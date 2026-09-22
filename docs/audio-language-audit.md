# Audio cues and language — 2026-09-22

The cue classification, positioning and playback paths do not select sounds by
translated display names or by the game's text/voice locale. The mod's spoken
phrases follow the text locale through `Localization::DetectSpeechLanguage`.

| Cue | Source of its identity and sound |
|---|---|
| Radar wall, floor, door, exit and unexplored tail | Native collision, entity kinds, geometry and exploration state; procedural PCM (`radar_service.cpp`, `radar_synth.cpp`). |
| Target beacon | Target identity, world position and reachable route; procedural PCM (`beacon_service.cpp`, `beacon_audio.cpp`). |
| Ground items | Numeric quality, item type ancestry and internal item codes; bundled WAV files (`native_ground_items.cpp`, `ground_items.cpp`, `item_audio.cpp`). |
| Containers | Object interaction handler and current selectability; bundled `chest.wav` (`native_entities.cpp`, `ground_item_service.cpp`). |
| Portals, waypoints and other exits | Object subclass, OpenWarp, internal level keys or native warp records; bundled `portal.wav`. |
| Footstep filter | Local player identity, animation and position; preserves or suppresses the game's own HD footfalls (`native_footsteps.cpp`, `footsteps.cpp`). |

`RadarUi` uses native panel identity and foreground state, not translated menu
captions. The mod's WAV paths are fixed and do not use the game's voice packs.
Changing voice locale can change the game's own audio, including any native
assets; it does not change the mod's cue classification or synthesis.

## Corrections

- `NativeEntities::ReadObject` previously discarded otherwise valid containers
  and object exits when game localization returned an empty name. They now retain
  their numeric identity and position with generic labels from the mod's own
  language table. Ordinary unnamed objects keep their previous filtering. This
  removes a dependency on localization availability; no particular supported
  language was shown to fail during ordinary operation.
- The beacon previously stopped generating pings when its UI gate closed but
  left queued/current audio running. A transition now cancels audio on the next
  UI tick through the existing worker cancellation path, retains the target and
  avoids repeated cancellation logging. Guidance can resume when its gate opens.

## Verification and limits

The initial build and all 58 existing CTest suites passed. The installed plugin,
PRISM DLL and eight WAV assets matched the local build/vendor files by SHA-256.
Regression coverage added for missing object localization in all 13 selected mod
locales and for beacon service audio cancellation across UI transitions.
After the corrections, `./build.ps1` compiled and deployed successfully and
`ctest --test-dir build --output-on-failure --timeout 20` passed **59/59 suites**.
These counts describe the shared workspace, including its pre-existing
exploration changes. The corrected installed plugin and PRISM hashes also match.

The retained live log from 2026-09-21 identifies NVDA, `enUS` text, eight loaded
loops, radar buffers submitted/completed and an advancing portal voice. There
were no logged audio initialization failures. It also contains UI tick warnings
(31–62 ms) and an initial UI-pump retry; this audit does not diagnose those timings.

The loader currently specifies `enUS` for both text and voice. This audit leaves
that local configuration intact. It does not constitute a listening test of the
new build, an in-game sweep of all languages, or analog-controller validation.
Next: listen while approaching a container/exit and while opening/closing a
dialogue with beacon guidance active; repeat after changing the text/voice locale.
