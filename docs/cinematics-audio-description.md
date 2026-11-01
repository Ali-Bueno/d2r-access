# Audio description for the act cinematics (research, 2026-11-01)

The user asked for narration of the act cinematics while nobody speaks, as in the Kakarot and
Spider-Man mods. Not implemented; this records what the game offers and the planned design.

## What the game ships

Seen in the CASC data (extracted with `tools/casc-extract`, 2026-11-01). Durations from ffprobe.

| Video (`data/hd/global/video/`) | When | Length | Subtitled (enUS) | Free of subtitles |
|---|---|---|---|---|
| `d2intro.webm` | game intro, Act I | 427.3 s | 251.5 s | ~176 s |
| `act2/Act02start.webm` | Act II start | 195.8 s | 151.3 s | ~44 s |
| `act3/Act03start.webm` | Act III start | 261.8 s | 144.9 s | ~117 s |
| `act4/Act04start.webm` | Act IV start | 211.8 s | 120.5 s | ~91 s |
| `act4/Act04end.webm` | Act IV end | 188.7 s | 98.4 s | ~90 s |
| `D2x_Intro.webm` | Act V start | 245.6 s | 66.1 s | ~179 s |
| `act5/D2x_Out.webm` | Act V end | 127.4 s | 27.2 s | ~100 s |

- Video: VP8 WebM 1920x1080, shared by every language. Audio: 6-channel FLAC per language,
  `hd/local/video/<path>.flac` and `locales/audio/<lang>/data/hd/local/video/<path>.flac`.
- **Subtitles:** SRT with start and end per line, `data/local/lng/subtitles/<lang>/<video>.srt`, in
  the 13 languages the mod speaks. They carry speaker tags (`[Marius] We traveled east,`) and sound
  tags (`[eerie music]`). `D2x_Out.srt` was only seen for enUS; the other languages are unchecked.
  Each language has its own voice track, so the free stretches come from that language's SRT.
- Mapping video to act is from the file names (inferred).

## What the plugin sees

- Start `MoviePlayerMessage:Play`, end `Cinematics:MovieComplete` (`ui_message_monitor.cpp`, cues
  already pause on them). The event's `text` field (`shared_events.h`) was the only place a movie
  name could be; it comes empty (below).
- **Seen live 2026-11-01:** the Videos list sends `Cinematics:Play` with the option's index as
  text (`"0"` Act I ... `"6"` the end of Act V; labels "LLANTO DE LAS HERMANAS" ... "FIN DE LA
  DESTRUCCIÓN"), then `MoviePlayerMessage:Play` with an **empty** text: the movie's name is not
  given. In play, the movie has to come from the act entered or completed.
- The list's "View classic videos" checkbox (`LegacyCheckbox`, `Cinematics:LegacyToggle`) plays the
  original 2000 videos instead, with other timings: the scripts must know which is playing, or say
  nothing for the classic ones.
- No pause. Hold-to-skip (`holdToSkipDurationMilliseconds` 500, `InGameMoviePlayerPanel.json`); a
  skip should end with `MovieComplete`, which stops the narration.
- No playback position is read: the clock would start at `Play`, so the video's load delay shifts
  every cue. Calibrate it live, or find the movie player's position by RE.

## The model: Kakarot and Spider-Man

- **Kakarot** (`D:\code\unreal\dragon ball kakarot access`, `reference/dbz-kakarot/notes/dbz-kakarot-cutscene-descriptions.md`):
  a cue catalog per movie (`at` seconds, text); a scheduler on the movie's own playback clock that
  drops cues on a seek and those more than 0.35 s late; a due cue is dropped, never delayed, when a
  subtitle is on screen; spoken without interrupting and never requeued. Drafts were AI-made from
  frames (one per second) and retimed by hand; 13 languages, English as each cue's fallback.
- **Spider-Man Remastered** (`D:\code\marvel's Spiderman remastered access`,
  `reference/engines/insomniac/audio-description.md`): a TSV per cinematic and language,
  `data/audiodesc/<lang>/<cinematic>.tsv`, `<cinematic> TAB <seconds> TAB <text>`. Its writing guide,
  confirmed by the user: fill every free stretch (one cue every 2.5 to 4 s), 3 to 9 words, present
  tense, third person, place set at a scene's start, no camera language, only what is visible,
  names as the game writes them in that language.

## Planned design

- `data/audiodesc/<lang>/<video>.tsv` in the Spider-Man format, embedded (and sealed with the
  descriptions, if that is done).
- A gap check at build or test time: each cue, at about 2.8 words a second, must end before the next
  subtitle line of that language's SRT; a test fails otherwise.
- The SRT timings are fixed per language, so the build check is the gate; at runtime a cue is
  dropped only when it comes late (clock drift, a skip).
- First live step: log `MoviePlayerMessage:Play`'s `text` and the time to the first frame.
