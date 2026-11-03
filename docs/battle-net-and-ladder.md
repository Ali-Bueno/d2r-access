# Battle.net, Ladder and Seasons

The mod cannot be played on Battle.net, so Ladder and Seasons are out of reach. This is a property of
D2RLoader, which runs the game offline and switches Battle.net online play off on purpose
([release-launch.md](release-launch.md)). With the mod a player has no Ladder or Season, no ladder
rankings, no online (server-stored) characters, and no public Battle.net games or trading. What
remains: offline characters, alone or with friends through D2RLoader's TCP/IP multiplayer
([multiplayer.md](multiplayer.md)). The only online step is the 30-day license check, run by the
launcher on the unmodified game ([launcher.md](launcher.md)).

Announced to players in the 1.0 posts (2026-11-02, `../announcement-2026-11-02-*`), after a tester
pointed out the missing disclaimer.

## Decision: no mod on the online client

Loading the mod into the online client is out of scope. It would need getting past Blizzard's
client scanning, it risks the players' accounts, and on Ladder the scanner and radar would read as an
unfair advantage, close to a map hack.

## Research, 2026-11-02

Web research by a subagent; confidence per line. Re-check before quoting any of it to Blizzard.

| Question | Finding | Confidence |
|---|---|---|
| Blizzard terms | The [EULA](https://www.blizzard.com/en-us/legal/fba4d00f-c7e4-4883-b8b9-1b4500a402ea/blizzard-end-user-license-agreement) (rev. 2024-03-21) bans unauthorized modification, software that reads or mines game data, and unauthorized third-party connections. Section 4 lets the client scan for unauthorized programs, in or out of process. No accessibility exemption. | Primary source, wording paraphrased |
| External tools (OCR, audio only) | Not exempt: the out-of-process scanning clause and the broad data-mining clause would cover them. No precedent either way. | Unclear |
| Precedent | Hearthstone Access (blind players, online). On 2022-11-08 Blizzard said it was in constant contact with its developer and had eased development, while moving toward native accessibility ([forum post](https://us.forums.blizzard.com/en/hearthstone/t/97531/1)). Tolerance and collaboration, no written approval found. | Verified post |
| Native accessibility | Diablo IV has a built-in screen reader for menus and text (2023). D2R's announced 2021 TTS plans are unconfirmed in the shipped game. | Secondary |
| Contact | accessibility@blizzard.com ([support article](https://support.blizzard.com/article/000244753)); Customer Support does not monitor it. | Secondary |
| D2RLoader | Changelog (1.3.1-beta, 2026-09-24) never mentions Battle.net or ladder; 1.3.0 gives TCP/IP and local games the offline Terror Zone rotation. No ladder-content toggle found; docs.html and Discord not checked. | Verified changelog |
| Ladder content offline | Ladder-only runewords have historically reached offline play in later patches (2.6). Patch 3.0 (2026-02-11) added five ladder-only runewords; whether they are offline now is unknown. | Secondary / unclear |

## Possible routes

1. Ask Blizzard in writing (accessibility@blizzard.com): permission for this mod or a read-only tool
   online, or native screen-reader support in D2R. Hearthstone Access is the precedent to cite.
2. Track which ladder content reaches offline play, and check D2RLoader for a ladder-content option.
