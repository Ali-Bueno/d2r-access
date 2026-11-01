"""Authoring helpers for the cinematics' audio description (docs/cinematics-audio-description.md).

The game files come from tools/casc-extract into an extract folder (`<x>`):
    casc_extract extract <D2R folder> <x> "global\\video\\" "lng\\subtitles\\"

    audiodesc.py speech   <x> <out>             speech windows of every video (data/audiodesc/speech.tsv)
    audiodesc.py timeline <x> <video>           English lines and the stretches free in every language
    audiodesc.py sheets   <x> <video> <out dir> [--classic] [--fps 1] [--from s] [--to s] [--grid 4] [--width 400]
                          [--free]: only the stretches free of speech, 1 s either side
"""
import argparse, os, re, subprocess, sys

VIDEOS = {  # file stem -> path under global/video/ (hd/ for the 2021 videos)
    "d2intro": "d2intro", "Act02start": "act2/Act02start", "Act03start": "act3/Act03start",
    "Act04start": "act4/Act04start", "Act04end": "act4/Act04end", "D2x_Intro": "D2x_Intro",
    "D2x_Out": "act5/D2x_Out",
}
JOIN_GAP = 0.3   # speech lines closer than this are one window
FREE_MIN = 1.5   # shorter stretches are not worth a cue
TAG = re.compile(r"^\[[^\]]*\]$")
TIME = re.compile(r"(\d+):(\d+):(\d+),(\d+) --> (\d+):(\d+):(\d+),(\d+)")
FONT = r"C\:/Windows/Fonts/arial.ttf"


def video_file(x, video, classic=False):
    return os.path.join(x, "global" if classic else "hd/global", "video", VIDEOS[video] + ".webm")


def duration(path):
    out = subprocess.run(["ffprobe", "-v", "error", "-show_entries", "format=duration", "-of", "csv=p=0", path],
                         capture_output=True, text=True, check=True).stdout
    return float(out.strip())


def srt(path):
    """(start, end, text, spoken) per line; a line made only of [sound tags] is not spoken."""
    text = open(path, encoding="utf-8-sig").read().replace("\r", "")
    for block in text.split("\n\n"):
        m = TIME.search(block)
        if not m:
            continue
        g = list(map(int, m.groups()))
        start = g[0] * 3600 + g[1] * 60 + g[2] + g[3] / 1000
        end = g[4] * 3600 + g[5] * 60 + g[6] + g[7] / 1000
        lines = [l.strip() for l in block[m.end():].strip().split("\n") if l.strip()]
        yield start, end, " ".join(lines), not all(TAG.match(l) for l in lines)


def speech_windows(x, video):
    """Spoken stretches in any language's subtitles, merged."""
    root = os.path.join(x, "local/lng/subtitles")
    spans = sorted((s, e) for lang in os.listdir(root) if os.path.exists(os.path.join(root, lang, video + ".srt"))
                   for s, e, _, spoken in srt(os.path.join(root, lang, video + ".srt")) if spoken)
    merged = []
    for s, e in spans:
        if merged and s - merged[-1][1] < JOIN_GAP:
            merged[-1][1] = max(merged[-1][1], e)
        else:
            merged.append([s, e])
    return merged


def free_stretches(windows, length):
    out, cursor = [], 0.0
    for s, e in windows + [[length, length]]:
        if s - cursor >= FREE_MIN:
            out.append((cursor, s))
        cursor = max(cursor, e)
    return out


def cmd_speech(a):
    with open(a.out, "w", encoding="utf-8", newline="\n") as f:
        f.write("# Spoken stretches of each cinematic in any of the 13 languages' subtitles, from\n"
                "# tools/audiodesc.py speech. <video> TAB length TAB <seconds> | <video> TAB <start> TAB <end>\n")
        for video in VIDEOS:
            f.write("%s\tlength\t%.3f\n" % (video, duration(video_file(a.x, video))))
            for s, e in speech_windows(a.x, video):
                f.write("%s\t%.3f\t%.3f\n" % (video, s, e))
    print("wrote", a.out)


def cmd_timeline(a):
    length = duration(video_file(a.x, a.video))
    print("%s, %.2f s\n\nEnglish subtitles (start, end, text):" % (a.video, length))
    for s, e, text, _ in srt(os.path.join(a.x, "local/lng/subtitles/enUS", a.video + ".srt")):
        print("  %7.2f %7.2f  %s" % (s, e, text))
    print("\nFree of speech in every language (>= %.1f s):" % FREE_MIN)
    for s, e in free_stretches(speech_windows(a.x, a.video), length):
        print("  %7.2f - %7.2f  (%.1f s)" % (s, e, e - s))


def cmd_sheets(a):
    """4x4 tiles of 400 px, each stamped with its time in the video."""
    path = video_file(a.x, a.video, a.classic)
    end = min(a.t1 or 1e9, duration(path))
    os.makedirs(a.out, exist_ok=True)
    per = a.grid * a.grid / a.fps
    spans = [(a.t0, end)]
    if a.free:
        spans = [(max(0.0, s - 1), min(end, e + 1)) for s, e in free_stretches(speech_windows(a.x, a.video), end)]
    for start, stop in spans:
        t = start
        while t < stop - 1 / a.fps + 1e-6:  # a piece shorter than a frame has nothing to show
            vf = ("fps=%g,scale=%d:-1,drawtext=fontfile='%s':text='%%{pts\\:flt\\:%g}':x=4:y=4:fontsize=24:"
                  "fontcolor=yellow:box=1:boxcolor=black@0.6,tile=%dx%d:padding=2" % (a.fps, a.width, FONT, t, a.grid, a.grid))
            out = os.path.join(a.out, "%06.1f.jpg" % t)
            subprocess.run(["ffmpeg", "-v", "error", "-y", "-ss", "%.3f" % t, "-t", "%.3f" % min(per, stop - t), "-i", path,
                            "-vf", vf, "-frames:v", "1", "-q:v", "3", out], check=True)
            t += per
    print("sheets in", a.out)


def main():
    p = argparse.ArgumentParser()
    sub = p.add_subparsers(dest="cmd", required=True)
    s = sub.add_parser("speech"); s.add_argument("x"); s.add_argument("out"); s.set_defaults(fn=cmd_speech)
    s = sub.add_parser("timeline"); s.add_argument("x"); s.add_argument("video", choices=VIDEOS)
    s.set_defaults(fn=cmd_timeline)
    s = sub.add_parser("sheets"); s.add_argument("x"); s.add_argument("video", choices=VIDEOS); s.add_argument("out")
    s.add_argument("--classic", action="store_true"); s.add_argument("--fps", type=float, default=1.0)
    s.add_argument("--from", dest="t0", type=float, default=0.0); s.add_argument("--to", dest="t1", type=float)
    s.add_argument("--grid", type=int, default=4); s.add_argument("--width", type=int, default=400)
    s.add_argument("--free", action="store_true")
    s.set_defaults(fn=cmd_sheets)
    a = p.parse_args()
    a.fn(a)


if __name__ == "__main__":
    sys.exit(main())
