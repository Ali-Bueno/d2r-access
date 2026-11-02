"""Authoring helpers for the cinematics' audio description (docs/cinematics-audio-description.md).

The game files come from tools/casc-extract into an extract folder (`<x>`):
    casc_extract extract <D2R folder> <x> "global\\video\\" "lng\\subtitles\\"

    audiodesc.py speech   <x> <out>             speech windows of every video (data/audiodesc/speech.tsv)
    audiodesc.py subtitles <x> <out>            when each subtitle line starts, per video and language, sound
                                                tags included (data/audiodesc/subtitles.tsv; times only)
    audiodesc.py timeline <x> <video>           English lines and the stretches free in every language
    audiodesc.py sheets   <x> <video> <out dir> [--classic] [--fps 1] [--from s] [--to s] [--grid 4] [--width 400]
                          [--free]: only the stretches free of speech, 1 s either side
    audiodesc.py check    <script> [--locale enUS] [--headroom 1.2]
                          the CTest timing rule on any script file (speech.tsv beside this tool's data),
                          each cue's estimated length times the headroom; then per video the free time,
                          the share of it narrated and the silences of 3 s or more left in it
    audiodesc.py review   <x> <hd|classic> <out>
                          a script a tester can read and edit: every English cue with its time and the
                          characters that fit, the English subtitle lines between them for context
    audiodesc.py apply    <edited review> <hd|classic>
                          takes the tester's text back into data/audiodesc/<edition>/enUS.txt (same
                          cues and times; DELETE removes one), lists what changed and every [note]
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


def cmd_subtitles(a):
    """The plugin follows the video's own clock by them: each line is shown when the video reaches it."""
    root = os.path.join(a.x, "local/lng/subtitles")
    with open(a.out, "w", encoding="utf-8", newline="\n") as f:
        f.write("# When each subtitle line starts, in seconds, per video and language (sound tags included),\n"
                "# from tools/audiodesc.py subtitles. <video> TAB <locale> TAB <start>,<start>,...\n")
        for video in VIDEOS:
            for lang in sorted(os.listdir(root)):
                path = os.path.join(root, lang, video + ".srt")
                if os.path.exists(path):
                    starts = sorted(s for s, _, _, _ in srt(path))
                    f.write("%s\t%s\t%s\n" % (video, lang, ",".join("%.3f" % s for s in starts)))
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


SPEECH_TSV = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "data", "audiodesc", "speech.tsv")
PACE = {"jaJP": 10.0, "zhCN": 6.4, "zhTW": 6.4, "koKR": 9.3}  # characters a second; src/audio_description_script.h
ALPHABETIC_PACE, SPEECH_MARGIN, SILENCE_REPORT = 20.0, 0.2, 3.0


def cmd_check(a):
    length, speech = read_speech()
    cues, problems = {}, 0
    for n, line in enumerate(open(a.script, encoding="utf-8-sig"), 1):
        if line.startswith("#") or not line.strip():
            continue
        parts = line.rstrip("\r\n").split("\t")
        if len(parts) != 3 or parts[0] not in VIDEOS:
            print("line %d: not <video> TAB <seconds> TAB <text>" % n); problems += 1; continue
        cues.setdefault(parts[0], []).append((float(parts[1]), parts[2]))
    pace = PACE.get(a.locale, ALPHABETIC_PACE)
    for video, raw in cues.items():
        cue = sorted((t, t + len(text) / pace * a.headroom) for t, text in raw)
        for i, (t, end) in enumerate(cue):
            where = "%s %.2f" % (video, t)
            if end > length[video]:
                print(where, "ends after the video"); problems += 1
            if i + 1 < len(cue) and end > cue[i + 1][0]:
                print(where, "runs into the next cue (ends about %.2f)" % end); problems += 1
            for s, e in speech.get(video, []):
                if s <= t < e:
                    print(where, "starts while someone speaks (%.2f-%.2f)" % (s, e)); problems += 1; break
                if s > t and end > s - SPEECH_MARGIN:
                    print(where, "ends about %.2f, over the speech at %.2f" % (end, s)); problems += 1; break
        free, said, silences = 0.0, 0.0, []
        for s, e in free_stretches([list(w) for w in speech.get(video, [])], length[video]):
            free += e - s
            cursor = s
            for t, end in cue:
                if s <= t < e:
                    if t - cursor >= SILENCE_REPORT:
                        silences.append("%.1f-%.1f" % (cursor, t))
                    said += min(end, e) - t
                    cursor = max(cursor, end)
            if e - cursor >= SILENCE_REPORT:
                silences.append("%.1f-%.1f" % (cursor, e))
        print("%s: %d cues, free %.0f s, narrated %.0f s (%.0f%%), silences >= %.0f s: %s" % (
            video, len(cue), free, said, 100 * said / free if free else 0, SILENCE_REPORT, " ".join(silences) or "none"))
    print("%d problems" % problems)
    return 1 if problems else 0


TITLES = {  # the Videos list's English names
    "d2intro": "Sisters' Lament (the game intro)", "Act02start": "Desert Journey (Act II)",
    "Act03start": "Mephisto's Jungle (Act III)", "Act04start": "Enter Hell (Act IV)",
    "Act04end": "Terror's End (after Diablo)", "D2x_Intro": "Search for Baal (Act V)",
    "D2x_Out": "Destruction's End (after Baal)",
}
SCRIPT_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "data", "audiodesc")
REVIEW_HEAD = """Diablo II: Resurrected cinematics: audio description script, {edition} edition, English

How to edit
- Each narration line starts with a line like "@ d2intro 10.40 | 0:10 | fits 95 characters". Keep
  that line as it is: it tells me which cue you changed. Change the text under it freely.
- To remove a line, replace its text with DELETE.
- "fits" is how many characters can be read, at a fast screen reader's pace, before the next
  narration line or the next spoken line. Going over is fine; I will trim it or move it.
- If a line should come earlier or later, or anything else, write a note in [square brackets].
- Lines starting with ">" are what is said or heard in the video at that moment (the English
  subtitles), only there so you know where you are. Do not edit them.
- The narration says only what is seen, never what is heard; names only once the story has given
  them (the hooded stranger is Baal only from Terror's End).
"""


def read_script(edition):
    cues = {}
    for line in open(os.path.join(SCRIPT_DIR, edition, "enUS.txt"), encoding="utf-8-sig"):
        parts = line.rstrip("\r\n").split("\t")
        if len(parts) == 3 and not line.startswith("#"):
            cues.setdefault(parts[0], []).append((parts[1], parts[2]))
    return cues


def read_speech():
    length, speech = {}, {}
    for line in open(SPEECH_TSV, encoding="utf-8"):
        if line.startswith("#") or not line.strip():
            continue
        video, first, second = line.rstrip("\n").split("\t")
        if first == "length":
            length[video] = float(second)
        else:
            speech.setdefault(video, []).append((float(first), float(second)))
    return length, speech


def clock(seconds):
    return "%d:%04.1f" % (int(seconds) // 60, seconds % 60)


def cmd_review(a):
    cues, (length, speech) = read_script(a.edition), read_speech()
    name = "2021" if a.edition == "hd" else "classic (2000)"
    out = [REVIEW_HEAD.format(edition=name)]
    for video in VIDEOS:
        out.append("\n== %s: %s ==\n" % (video, TITLES[video]))
        lines = [(s, "> %s %s" % (clock(s), text)) for s, _, text, _ in
                 srt(os.path.join(a.x, "local/lng/subtitles/enUS", video + ".srt"))]
        listed = cues.get(video, [])
        for i, (at, text) in enumerate(listed):
            start = float(at)
            limit = length[video]
            if i + 1 < len(listed): limit = min(limit, float(listed[i + 1][0]))
            for s, _ in speech.get(video, []):
                if s > start: limit = min(limit, s - SPEECH_MARGIN); break
            fits = int((limit - start) * ALPHABETIC_PACE)
            lines.append((start, "\n@ %s %s | %s | fits %d characters\n%s\n" % (video, at, clock(start), fits, text)))
        out += [text for _, text in sorted(lines, key=lambda line: line[0])]
    with open(a.out, "w", encoding="utf-8", newline="\r\n") as f:
        f.write(re.sub(r"\n{3,}", "\n\n", "\n".join(out)).strip() + "\n")
    print("wrote", a.out, "with", sum(len(v) for v in cues.values()), "cues")


def cmd_apply(a):
    edited, current, notes = {}, None, []
    for raw in open(a.review, encoding="utf-8-sig"):
        line = raw.rstrip("\r\n")
        m = re.match(r"^@ (\S+) (\S+) \|", line)
        if m:
            current = (m.group(1), m.group(2)); edited[current] = []
        elif line.startswith("==") or line.startswith(">"):
            current = None
        elif current and line.strip():
            edited[current].append(line.strip())
    path = os.path.join(SCRIPT_DIR, a.edition, "enUS.txt")
    lines = open(path, encoding="utf-8").read().split("\n")
    changed = removed = 0
    for i, line in enumerate(lines):
        parts = line.split("\t")
        if len(parts) != 3 or line.startswith("#") or (parts[0], parts[1]) not in edited:
            continue
        text = " ".join(edited.pop((parts[0], parts[1])))
        for note in re.findall(r"\[[^\]]*\]", text):
            notes.append("%s %s %s" % (parts[0], parts[1], note))
        text = re.sub(r"\s*\[[^\]]*\]\s*", " ", text).strip()
        if text.upper() == "DELETE":
            lines[i] = None; removed += 1; print("removed %s %s: %s" % (parts[0], parts[1], parts[2]))
        elif text and text != parts[2]:
            lines[i] = "\t".join([parts[0], parts[1], text]); changed += 1
            print("changed %s %s:\n  was: %s\n  now: %s" % (parts[0], parts[1], parts[2], text))
    with open(path, "w", encoding="utf-8", newline="") as f:
        f.write("\n".join(l for l in lines if l is not None))
    for key in edited:
        print("not in the script (time edited?):", *key)
    for note in notes:
        print("note:", note)
    print("%d changed, %d removed, %d notes; run check, then retranslate those cues" % (changed, removed, len(notes)))


def main():
    p = argparse.ArgumentParser()
    sub = p.add_subparsers(dest="cmd", required=True)
    s = sub.add_parser("speech"); s.add_argument("x"); s.add_argument("out"); s.set_defaults(fn=cmd_speech)
    s = sub.add_parser("subtitles"); s.add_argument("x"); s.add_argument("out"); s.set_defaults(fn=cmd_subtitles)
    s = sub.add_parser("timeline"); s.add_argument("x"); s.add_argument("video", choices=VIDEOS)
    s.set_defaults(fn=cmd_timeline)
    s = sub.add_parser("sheets"); s.add_argument("x"); s.add_argument("video", choices=VIDEOS); s.add_argument("out")
    s.add_argument("--classic", action="store_true"); s.add_argument("--fps", type=float, default=1.0)
    s.add_argument("--from", dest="t0", type=float, default=0.0); s.add_argument("--to", dest="t1", type=float)
    s.add_argument("--grid", type=int, default=4); s.add_argument("--width", type=int, default=400)
    s.add_argument("--free", action="store_true")
    s.set_defaults(fn=cmd_sheets)
    s = sub.add_parser("check"); s.add_argument("script"); s.add_argument("--locale", default="enUS")
    s.add_argument("--headroom", type=float, default=1.0); s.set_defaults(fn=cmd_check)
    s = sub.add_parser("review"); s.add_argument("x"); s.add_argument("edition", choices=["hd", "classic"])
    s.add_argument("out"); s.set_defaults(fn=cmd_review)
    s = sub.add_parser("apply"); s.add_argument("review"); s.add_argument("edition", choices=["hd", "classic"])
    s.set_defaults(fn=cmd_apply)
    a = p.parse_args()
    return a.fn(a)


if __name__ == "__main__":
    sys.exit(main())
