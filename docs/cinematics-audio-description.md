# Audio description for the act cinematics (2026-11-01)

The user asked for narration of the act cinematics while nobody speaks, as in the Kakarot and
Spider-Man mods, for both the 2021 videos and the classic 2000 ones. This records what the game
offers, the script format, the writing guide and the runtime design.

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

## One timeline for every language and both editions (measured 2026-11-01)

- **Languages:** the 13 SRTs time their lines within a fraction of a second of each other (the dubs
  follow the same picture). The stretches free in *every* language (the union of their spoken lines)
  still leave about 880 s, so one timeline serves all languages; only the words are translated.
  Lines made only of sound tags (`[eerie music]`, English only) are not speech.
- **Classic videos:** `data/global/video/<path>.webm` (the 2000 renders, 1920x1080) with their own
  audio `data/local/video/<path>.flac` (per language under `locales/audio/<lang>/`). Same lengths as
  the 2021 ones (`d2intro` 1.04 s shorter at the end); the audio envelopes cross-correlate at lag 0
  minute by minute (`d2intro`, `Act02start`), so the 2021 speech windows hold for them too. The
  pictures differ, so each edition is described from its own frames.
- `D2x_Out` has subtitles in English only, all sound tags: it has no dialogue.

## Script format

- `data/audiodesc/<edition>/<locale>.txt`, edition `hd` (2021) or `classic` (2000); UTF-8, `#`
  notes, one cue per line: `<video>` TAB `<seconds>` TAB `<text>`; `<video>` is the file stem
  (`d2intro`, `Act02start`, `Act03start`, `Act04start`, `Act04end`, `D2x_Intro`, `D2x_Out`), any case.
  Parsed by `AudioDescription::Parse` (`src/audio_description_script.cpp`).
- `data/audiodesc/speech.tsv`: the spoken stretches of each video in any language, made by
  `tools/audiodesc.py speech` from the game's SRTs (times only, no game text).
- **Check:** `audio-description-tests` (CTest `audio-description`) reads every script: each line
  parses, English covers every video in both editions, and no cue starts in speech, runs into the next
  speech line (0.2 s margin) or the next cue. Length is estimated at 14 characters a second (English
  2.8 words/s), 7 for Japanese, 4.5 for Chinese, 6.5 for Korean. The exe reads the files at run time,
  so a script edit needs no rebuild.
- **Authoring:** `tools/audiodesc.py timeline <extract> <video>` (English lines, free stretches) and
  `sheets` (4x4 contact sheets at 1 fps, each frame stamped with its time).

## Writing guide (revised 2026-11-01 after the user's first listen)

The first pass (short captions, 3 to 9 words, from small 1 fps contact sheets) worked but sounded
generic to the user, like photos: "in the first video you hear footsteps and a door opening, but it
never says what is happening there". The model is the Witcher 3 mod
(`The Witcher 3/mods/modWitcherAccess/audiodesc/cutscenes_en.tsv`, e.g. `cs001_nightmare`,
`cs403_teleport`) and the Spider-Man one: narration of the story, not of pictures.
- **Events, not snapshots:** who does what, to whom, with what result, in story order. "A hooded
  stranger strides down the asylum hall and crushes a spider underfoot." beats "A spider. A robe."
- **Every sound gets its cause:** footsteps, doors, locks, chains, screams, laughter, impacts heard in
  a free stretch are explained by what is seen (the English SRT tags list many: `[lock turns]`,
  `[whip cracks, man screams]`, `[waiter screams]`).
- **Introduce, then keep track:** each place and person at first sight with what makes them
  recognizable (a gaunt, white-haired prisoner in rags; a hooded stranger in pale robes); the same
  label until the story names them; Marius, the Wanderer, Tyrael and Baal by name once the dialogue
  or an earlier video has named them, never before (the stranger is Baal only from `Act04end`'s
  reveal).
- **Sentences:** one or two full sentences a cue, usually 8 to 20 words. A long stretch gets a cue every
  4 to 8 s, each carrying a whole beat, rather than many fragments; a short stretch gets one short
  sentence. A still picture may be left in silence; filler ("Darkness.", "The letters fade.") never.
- Present tense, third person, no camera language ("we see", "close-up", "the shot"). Short on-screen
  text is read when it matters (the act titles).
- **Only what is visible, understood:** use the story of the cinematics to recognize people and
  actions, but say nothing the picture does not show. Fast or dark moments are checked on 2 fps
  full-size frames.
- **Timing:** each cue starts within about 1 s of what it describes and passes the test. Leave room
  for the translations where the stretch allows (estimated length x 1.2 still fits).

## Written so far (2026-11-01)

- **First pass** (replaced): 293 + 290 short captions by two Sonnet writers from 400 px sheets. The
  user listened to it (Videos list) and found it generic.
- **Narration** (current): English by an Opus writer to the revised guide, from 640 px sheets of the
  free stretches, the dialogue sheets for the story and 2 fps frames for unclear moments. 2021: 161
  cues (d2intro 43, Act02start 5, Act03start 27, Act04start 15, Act04end 18, D2x_Intro 35, D2x_Out
  18). Classic: 163, adapted from the 2021 text shot by shot to what the 2000 frames show (white bolts
  instead of green, beetles in a hall instead of a burning street, a grey-bearded warrior, the title
  "Diablo II Expansion"). Director fixes: the stranger "appears as an angel" (he is Baal in disguise),
  the Worldstone named in `D2x_Out`.
- Unsure moments the writers reported, the first places to fix if a tester hears something wrong:
  the fast, dark shots of the `d2intro` fire (who the bolts strike, the bats, the demon), `Act03start`
  142-171 (whose light burns the bridge), `Act04end` 21.6 (the blue shape leaving Diablo), the
  classic `D2x_Intro` title read from 1 fps sheets.
- The other 12 languages: Sonnet translators, same times, no cue dropped; about 20 cues a file lost a
  secondary detail to fit (mostly Chinese, Korean, German, French, Polish). The classic files reuse
  each language's 2021 wording where the English matches. Names come from each language's subtitles
  and string tables:

| | Wanderer | Tyrael | Infernal Gate |
|---|---|---|---|
| esES / esMX | el vagabundo / el Errante | Tyrael | las puertas del infierno / el Portal del Infierno |
| ptBR, itIT, frFR, deDE | o Errante, il Viandante, le Rôdeur, der Wanderer | Tyraël in frFR | o Portão Infernal, il Portale Infernale, la porte infernale, die Pforte des Infernos |
| plPL, ruRU | Wędrowiec, Странник | Тираэль | Piekielne Wrota, Врата Преисподней (composed from the subtitles) |
| jaJP, koKR | 放浪者, 방랑자 | ティラエル, 티리엘 | 地獄の門, 지옥문 |
| zhCN, zhTW | 流浪者 | 泰瑞尔, 泰瑞爾 | 地狱之门, 地獄之門 |

  Marius is 玛瑞斯 (zhCN, which also writes 马瑞斯) and 馬略斯 (zhTW); Tal Rasha 塔·拉夏 (zhCN, from
  levels) and 塔拉夏 (zhTW).

## Runtime (built 2026-11-01, unverified live)

- **Sealed:** `tools/seal-descriptions` (now any folder, recursive, key name as an argument) seals
  `data/audiodesc/*/` into RCDATA `AUDIODESC` under `Generated::AudioDescriptionKey`; the plugin
  opens it with the area descriptions' helper (`descriptions_bundle.cpp`). Names are
  `<edition>/<locale>` (`hd/enUS`).
- **Language:** the mod's language at each video's start (`Strings::Selected`); a video that language's
  file lacks is said in English (`CuesFor`).
- **Which video, from the menu:** the Videos list sends `Cinematics:Play` with the index (`Videos`
  order in `audio_description_script.h`); the classic box (`CinematicsModal/LegacyCheckbox`,
  `NativeUi::Toggled`) is read at that moment. A movie that starts within 5 s of the choice is that
  video. Confirmed live 2026-11-01 (d2intro hd and classic, Act02start).
- **Which video, in play** (unverified live): `InGameMoviePlayerPanel/ProgressionHelper`, its
  `+0x88` state (0 idle, 1 playing, 2 finished) and `+0x8C` index (RE below). Log:
  `[audiodesc] in play: ProgressionHelper state S, index N, legacy graphics L`. The edition is taken
  from `Settings.json`'s `Legacy Graphics Mode` (classic when on), an inference.
- **Clock:** time since `MoviePlayerMessage:Play` less the start delay, measured live 2026-11-01 by
  matching game screenshots (taken at known times after Play) to the video's frames with SSIM
  (0.998 to 1.0): 2021 intro 1.05 s (three shots, ±0.01), classic intro 0.70 s (three, ±0.05). A
  full 2021 intro also ended 428.3 s after Play for a 427.27 s video. `Scheduler` says
  each due cue once without interrupting (`ScreenReader::Speak(text, false)`) and drops one more than
  2 s late; `Cinematics:MovieComplete` (end or hold-to-skip) stops it. Log: one line at the start
  (video, edition, language, cue count), one per cue said, one at the end (said, dropped).
- **Switch:** `audio-description` (Ctrl+S, Alerts group), on by default.
- **Probe** (`movie_probe.cpp`, diagnostic): 0.5 s and 2.5 s into every movie it snapshots the
  `MoviePlayer` widget (0x1000 bytes) and every object its fields point to (0x400 bytes each), logs the
  strings that name a video file and every field that advanced like a clock; in play it also dumps
  `ProgressionHelper`'s first 0x100 bytes. **Live 2026-11-01 (four menu videos):** no video name and
  no clock at those depths (strings only at +0x8 name, +0x208 `Subtitle`, +0x438 `HoldToSkip`,
  +0x4B8 the hold-to-skip text; two junk clock hits inside the text's heap block).

## What the game does (RE of the 3.3.93787 dump, 2026-11-01)

Static, from `re/D2R_live.exe` (the installed game is 3.3.94995; RVAs below need a recheck there,
the class offsets are what the plugin uses). Sonnet RE agent; scratch decompiles not kept.
- `0xEEA70` builds the two paths of a movie (`Build(buf1[0x104], buf2[0x104], name, hd)`, format
  `%s/video/%s%s`, `.flac`/`.webm`), and the result goes as a **binary 0x208-byte payload** into
  `MoviePlayerMessage:Play`: that is why the UI event's text is empty (the SDK's `UiMessageEvent`
  carries only target, command, text). Callers: the Videos list (`0x1541330`), the in-play
  progression helper (`0xEF630`) and four unidentified classes.
- Names: `*(char**)(base + 0x22A0FF8 + i*0x18)`, 8 entries filled at run time (index 7 unknown).
- 2021 or classic: the Videos list uses a byte (`0x3BBEB24`, flipped by `LegacyToggle`); in play
  `FUN_140650210()` (`float @0x33ED6A0 > 0`), called from about 12 places, likely the graphics
  mode. Inferred.
- In play, `0xEF630` is the handler of a 0x90-byte object (vtable `0x1CC6270`): a base widget is
  0x88 bytes (a TextBoxWidget's text starts at +0x88), so it is `ProgressionVideoHelperWidget` plus
  `+0x88` state and `+0x8C` index, set from the progression message's `*(int*)payload`. Index 5 is
  followed by 6 (D2x_Intro then D2x_Out). The sender (game logic) was not found.
- Playback position: not found (no SRT parser located); probably in a decoder object deeper than the
  probe looked. Next: a depth-3 walk from `MoviePlayer` with snapshots 1 s apart.
