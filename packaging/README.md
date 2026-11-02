# Diablo II: Resurrected Access

Version {{VERSION}} (commit {{COMMIT}})

## What it is

An accessibility mod for Diablo II: Resurrected. It reads the game through your screen reader (NVDA, JAWS or, when neither is running, the Windows voice) and plays orientation sounds: a wall radar, items, exits, chests, monsters. It runs on D2RLoader {{LOADER}}, which comes in this package.

{{NEWS}}

## Requirements

- Diablo II: Resurrected 3.3.x from Steam or Battle.net, on 64-bit Windows 10 or 11.
- On Steam, your Battle.net account linked to Steam (https://account.battle.net/connections): the game needs that link to run and to renew its license online.
- NVDA or JAWS running before you start the game. Without either, the mod speaks with the Windows voice.

## Installation

### With the installer (recommended)

Download `D2RAccessInstaller.exe` from this link, which always gives the latest one:

{{INSTALLER_URL}}

Close the game and run it. It finds the game on Steam or Battle.net (or lets you choose the folder), downloads the latest version of the mod, installs it and asks for the game language (see [Language](#language)). On Steam it asks whether Steam's Play button should start the mod, and then it offers a desktop shortcut to `D2RAccess.exe`. If Windows does not let it write in the game folder, it offers to start again as administrator. Your settings are kept. Run it again at any time to update, reinstall or uninstall the mod.

### By hand, with the zip

1. Close the game.
2. Extract everything in this zip into the game folder, the one that has `D2R.exe`. On Steam: right-click the game, Manage, Browse local files. It is usually: `C:\Program Files (x86)\Steam\steamapps\common\Diablo II Resurrected`. If it asks whether to replace files, say yes.
3. Next to `D2R.exe` you must now have: `D2RLoader.exe`, `D2RCore.dll`, `d2rloader.mpq`, four Microsoft DLLs (`msvcp140.dll`, `msvcp140_atomic_wait.dll`, `vcruntime140.dll`, `vcruntime140_1.dll`) and the `d2rloader` folder. Do not rename or replace `D2R.exe`.

## Updating

The easiest way is to say yes when `D2RAccess.exe` offers a new version as you start the game (see [Starting the game](#starting-the-game)), or to run the installer again.

To update by hand, the latest zip can always be downloaded from this link:

{{LATEST_URL}}

Close the game and extract the new zip on top, just like the first time. Your settings stay as they are: the zip carries no settings files, so the loader's options (language, extra shared stash pages and the rest) and the mod's `d2r-access.toml` are never replaced.

## Starting the game

Start the game with `D2RAccess.exe`, in the game folder (the installer offers to put a shortcut to it on the desktop). It starts the game with the mod and, when the last online sign-in is about 20 days old, first validates the license online by itself (about half a minute, it tells you), so the 30-day offline block never reaches you. At each start it also checks for a newer version and asks before installing it; turn that off with the "update-check" switch in the settings list (Ctrl+S).

**Steam:** the installer offers to make Steam's Play button start the mod. To do it yourself, in the game's Properties, General tab, Launch options, type the path of `D2RAccess.exe` in quotes followed by `%command%`. For example:

```
"C:\Program Files (x86)\Steam\steamapps\common\Diablo II Resurrected\D2RAccess.exe" %command%
```

**Battle.net:** with Battle.net open and signed in, run `D2RAccess.exe` from the game folder.

When the mod starts it says "Diablo 2 accessibility loaded".

If the loader warns that there was no sign-in for 30 days and starts without mods, that is a Blizzard check the loader keeps on purpose. `D2RAccess.exe` avoids it by validating online in time; if it says it could not validate, check the internet connection and start it again. The launcher writes `d2rloader\logs\d2r-access-launcher.log`. `D2RLoader.exe` still works on its own.

## Language

The mod speaks in the game's text language, descriptions included. The loader starts the game in English unless it is told the language, whatever Steam says: the installer's "Game language" tells it, for the text and the voices. To change it later, run the installer again, or open `d2rloader\config\d2rloader.toml` with Notepad and put the language code in both lines:

```
text_locale = "esES"
audio_locale = "esES"
```

Codes: enUS English, deDE German, frFR French, itIT Italian, esES Spanish (Spain), esMX Spanish (Latin America), plPL Polish, ptBR Portuguese (Brazil), ruRU Russian, jaJP Japanese, koKR Korean, zhCN Chinese (simplified), zhTW Chinese (traditional). Save and start the game again. With `text_locale` set, the game's own "Text language" option does not change the text; with it empty (`text_locale = ""`) that option does, and the mod follows it at once.

## Controls

BFBeast666's [commands and tutorial](GUIDE.html), next to this file: every control for the Xbox and PlayStation pads and the keyboard, in tables, with how the menus, the belt, the skills and the stash work. Open it in your web browser.

### Controller

The game's own controller mode. The game keeps its whole layout; the mod adds these:

#### In play

- **Right stick up / down:** scanner, previous / next category (exits, NPCs, enemies, chests, items, rooms...). Right stick left / right: previous / next thing in that category, with its distance and direction (north, northeast..., or clock hours: see [How the mod describes the map](#how-the-mod-describes-the-map)).
- **BACK + right stick down:** the route to what the beacon follows, else to what the scanner named last, as a sighted player plans it off the minimap: "Route to Akara, 85 units: north, 20; then door; then east, 30". Nothing walks for you; ask again on the way and it starts from where you stand. While you walk it, the next leg is said as you come to each turn ("Now east, 30"), and "Off the route" once if you leave it.
- **BACK + right stick up:** the description of the area you are in, in the game's language. While the Tainted Sun quest runs, every Act II area's description ends with "An unnatural darkness blankets the land". Landmarks have words too (the Hangman's Tree, the Hellforge): they are said instead of the landmark's name the first time it comes into reach. "Landmark descriptions" (Navigation) turns off the landmark words. New or changed descriptions come with updates of the mod: send them to the developer. Keyboard: Shift+Y.
- **BACK+LB:** follow or release what the scanner named (beacon). A ping leads you there along a real route and the mod says when you arrive.
- **BACK+X:** life, mana, stamina (when not full), the arrows or throwing weapons left in hand, any poison, freezing, chill, stun or curse on you. A Necromancer also hears how many usable corpses are nearby. The mercenary is BACK+RT's now ("Mercenary in the life report", Alerts, brings it back). "Running skills in the life report" (Alerts, on) adds the auras and skills running on you: "Prayer aura active. Bone Armor active".
- **BACK+RT:** one press at a time, your mercenary ("Mercenary life: 80 percent, Level 12, 6 units, north"), then each of your summons as the game's portraits group them ("Raven: 5", "Oak Sage: life 35 percent", a Warlock's demons one by one, "Tainted 2: life 75 percent"), then each party member ("Raine: life 80 percent"), with how far and where each one is, or the area a party member is in. After the last it starts again; after a pause of 10 seconds it starts again at the mercenary. Keyboard: Shift+Z.
- **BACK+LT:** the main skill bar, as holding LT reads the other one.
- **BACK+Y:** the passages out of the room you are in.
- **BACK+B:** area overview: the area, where you stand in it ("northeast edge", "centre", "south corner"), how much you have seen, then each side of the area (northeast, southeast, southwest, northwest: Diablo's areas are diamonds on screen) with how far it reaches, where unexplored ground starts or the area beyond it, then the places the radar has logged here, nearest first ("Waypoint, 40 units, northeast").
- **Lobby (host game, direct join):** a list. D-pad or left stick down and up step one control at a time; left and right only along the same row. A on a text box starts typing there, for a keyboard, and the mod says "Editing". With the controller alone, while a box is being typed into, the right stick walks a QWERTY keyboard: up and down change row (digits, q to p, a to l, z to m, then Caps Lock, space, - _ . : and Backspace), left and right change key, each said; A types it; B ends as before. "QWERTY keyboard on the right stick" (Controller) off: up steps through the letters, down through the digits, right types, left deletes. "Typing with the right stick" turns the stick part off.
- **LB (alone, in play):** the audible loot filter on or off ("Loot filter on"). While it is on, only the drop loops you keep in the Loot filter group of the settings sound (see [Settings](#settings)); the game still plays each item's own drop sound as it lands.
- **BACK+RB:** the mod's settings list, open or close (the keyboard's Ctrl+S). It is made of groups (Sounds, Volumes, Enemies and NPCs, Navigation, Items and menus, Alerts, Controller, Settings list, Loot filter, Sound glossary), each said as a sub menu ("Sounds sub menu"). While it is open the right stick moves through it: up and down step, right opens a group and then changes the setting (or plays the sound again in the glossary), left goes back to the groups. LB and RB lower and raise a setting of several values (they are the list's while it is open). Past the last entry the list goes round to the first with a click (or stops there with a knock: see [Settings](#settings)). It closes by itself after 30 seconds unused.
- **Holding LT for a moment:** reads the skill bar shown with LT.
- **BACK + right stick left / right:** the tutorial's previous / next message; BACK + D-pad up: its first message. A new level 1 character gets a hands-on tutorial (switch "Tutorial", Alerts).
- **L3 and R3:** L3 is the game's walk/run toggle: the mod says "Walking" or "Running" when you press it, and warns as stamina runs low. R3 is the game's weapon swap: the mod names the weapons in hand.

During play BACK alone does nothing (the game's map stays shut; see the settings list). Every other button does what the game does.

#### In menus

- **Right stick up:** detail of what has focus (an item's full description, a skill's text). Right stick down: the buttons this screen offers (the game's own button legend). Right stick right: extra text; on a skill, left opens its lines one by one (B or left again closes them).
- **On an inventory item, hold Y (triangle on a PlayStation controller):** and, without letting go, move the LEFT stick: left reads the item, right the equipped one it compares with, up detail, down controls. The right stick keeps its usual reading and does not compare; releasing Y ends it. Holding Y says "Comparing" and how the left stick reads it ("Compare hint while Y is held" in the settings list turns that off). To hear the equipped one as soon as Y shows it, turn on "Equipped item comparison read aloud" in the settings list (Items and menus).
- **In the skill tree:** the D-pad moves one skill at a time.
- **On the character screen with points to spend:** focus starts on Strength's plus button; the right stick reads the statistics.

The keyboard keys are listed in the [Keyboard](#keyboard) section.

## Keyboard

Mod keyboard commands (bladestorm360's scheme).

### In play

| Key | What it does |
|---|---|
| Arrows, held | walk. |
| A, held | fight with the left skill (only against a monster on screen). A buff, a summon or a spell that needs no target is cast once where you stand. |
| W, held | fight with the right skill, the same way. |
| Shift+A, held | attack in place with the left skill: no walking up to the monster and no falling back. With no monster it strikes where you stand. |
| F1 to F8 | the game's skill keys; the mod says the skill they select, only when it changes ("Skill hotkeys" in the Ctrl+S list). |
| Y | swap weapons. |
| Shift+S | left-hand skill list (the game's S opens the right-hand one). |
| Tab | next monster on screen. |
| Shift+Tab | previous monster. |
| Period | next object on screen, nearest first. It counts everything: ground items, chests, urns, barrels, corpses, shrines, levers, doors, the stash and your corpse. Then D uses that one. |
| Comma | previous object. |
| Right bracket | next NPC on screen. Then D talks to that one. |
| Left bracket | previous NPC. |
| D | walk to the nearest usable thing and use it. |
| H | walk to the nearest unexplored room. |
| Shift+Home | walk to what the scanner named. If it is an exit, take it (stairs, area border). |
| Ctrl+R | retreat: go back the way you entered the level and stay next to it. |
| End | stop. |
| Z | life. |
| X | mana. |
| Shift+Z | your companions, one at a time: mercenary, summons, party members, with their life (BACK+RT on the controller). |
| Page Up / Page Down | scanner, previous / next object. |
| Ctrl+Page Up / Ctrl+Page Down | scanner, previous / next category. |
| Home or backslash | repeat the scanner's object. |
| Ctrl+Numpad 4 / Ctrl+Numpad 6 | scanner, previous / next object. |
| Ctrl+Numpad 8 / Ctrl+Numpad 2 | scanner, previous / next category. |
| Ctrl+B | follow or release what the scanner named (beacon; BACK+LB on the controller). |
| Ctrl+E | the route to what the beacon follows or the scanner named (BACK + right stick down). |
| Ctrl+Y | the passages out of this room. |
| Shift+Y | the description of the area you are in (BACK + right stick up). |
| Ctrl+O | terrain overview. |
| Ctrl+T | radar status. |
| K / L | the tutorial's previous / next message; Shift+K: its first (BACK + right stick left / right, BACK + D-pad up). |

### In a menu

| Key | What it does |
|---|---|
| Arrows | move the cursor to the nearest thing in that direction. Held, repeats. |
| Left / Right on a slider or a stepped option | change its value. |
| A | left click (choose, pick up, place). On a stepped option: its next state. On character select: enter the game. |
| Enter | the game's own. On character select it starts with the character under the cursor. |
| D | right click (use, read, open, buy, drink). |
| Shift+A | Shift+left click (equip, unequip, potion to the belt). |
| Ctrl+A | Ctrl+left click (sell, drop, move to the stash or the cube). |
| Alt+A | Alt+left click (the stat and skill reset, where the game allows it). |
| Shift+D | Shift+right click. |
| Ctrl+D | Ctrl+right click. |
| Alt+D | Alt+right click. |
| W | nothing (taken from the game). |
| Y | swap weapons. |
| Shift+S | left-hand skill list, open or close. |
| Backslash | repeat. |
| J | description of what is under the cursor. On the character screen it reads the stat's tooltip. On a skill of the skill tree it ends with the skills it needs a point in. |
| Ctrl+Up / Ctrl+Down | previous / next tooltip line in the game's Ctrl view (ranges, for example "+(1-3) to Sacrifice"). |
| Ctrl+Left / Ctrl+Right | first / last line, the same. |
| Shift+Up / Shift+Down | previous / next line of the normal tooltip. After comparing, they walk the equipped item's lines. |
| Shift+Left / Shift+Right | first / last line, the same. |
| Page Up / Page Down | previous / next tab. In a shop, empty categories are skipped. |
| Alt+Page Up / Alt+Page Down | shared stash, previous / next page. |
| Ctrl+Shift+Page Up / Ctrl+Shift+Page Down | shared stash, ten pages back / forward. |
| Home / End | previous / next menu. |
| Ctrl+Page Up | a skill's lines, open and close. |
| Ctrl+Page Down | an item's size; the skill points left. |
| Shift+Page Up | compare: the whole item. |
| Shift+Page Down | compare: the equipped item. |
| Ctrl+R | repair all. |
| Shift+R | repair. |
| Alt+T | transmute (Horadric Cube). |
| Alt+S | sort items. |
| Alt+F | fill the belt from the inventory (with an inventory open). |

**In a text box (a name, an amount of gold, a game name):** the typing keys belong to the box; left and right move through the text; up and down leave the box; keys with Ctrl or Alt still belong to the mod.

### Everywhere

| Key | What it does |
|---|---|
| Ctrl+S | list of the mod's switches, open and close (BACK+RB on the controller). With that list open: Page Up / Page Down previous / next, Space opens a group or changes the value, Shift+Space lowers it, Backspace goes back to the groups. |
| Ctrl+H | read the controls with autodetail, on or off. |
| Ctrl+F | footstep filter. |
| Ctrl+U | scanner filter: only list what can be reached on foot. |
| Ctrl+N | NPC announcer. |
| Ctrl+M | room announcements. |
| Ctrl+I | speech for nearby items. |
| Ctrl+L | item sounds. |
| Ctrl+P | exit sounds. |
| Ctrl+K | container sounds. |
| Ctrl+J | automap diagnostic watch. |

Whatever has no key of its own is in the Ctrl+S list. Its last group is the sound glossary: each entry names a sound and plays it; Space plays it again.

### Game keys the mod takes

| Key | What it does |
|---|---|
| W | in the game it swaps weapons; with the mod it is the right skill. Y swaps weapons. |
| Tab | in the game it is the automap; with the mod, next monster. |
| H | in the game it is help; with the mod, explore. |
| Z | in the game it is the portraits; with the mod, life. |
| A | in the game it opens the character screen; with the mod it attacks. Use C for the character screen. |
| Space | closes the panels, except with the Ctrl+S list open. |

Every other game key (I, B, T, Q, O, P, M, S, F1 to F8, 1 to 4, R, Esc, Enter...) still reaches the game.

## How the mod describes the map

Directions are screen directions said as compass points: north is the top of the screen, south the bottom, east and west its right and left sides, and northeast, southeast, southwest and northwest the corners between. Diablo II draws its world turned 45 degrees, so a straight corridor on screen can run diagonally on the game's own map; the mod always follows the screen. In the Ctrl+S list (Navigation) "Directions" can say clock hours instead: 12 o'clock is the top of the screen, 3 o'clock the right, 6 the bottom, 9 the left.

Distances are in units. A unit is the game's smallest map cell (a subtile; a full tile is five subtiles), not a step of your character. Distances are straight lines from your character to the thing's position, so a large object can be a few units away while you stand at its edge.

A room counts as explored as soon as you step into it, anywhere: everything in it joins the scanner, as a sighted player sees a whole room on arriving. Whatever the scanner lists can be routed to and followed with the beacon, even in a room you never entered; when the way has to cross rooms you have not explored, the route says "Part of it crosses ground not seen yet", and the beacon leads you into the first of them. A room is reached at your first step inside it.

The scanner lists a monster once it has been within 15 units of you with nothing blocking the view, or once it is in the room you stand in. It stays listed after it leaves the screen, follows the monster while the game has it loaded, and is dropped when it dies. Monsters you never came near are not listed, unless the Ctrl+S list is set to "Full assistance": then every monster the game has loaded around you is listed, which helps to find the last ones when an area must be cleared.

Exits are the ways out of an area: stairs, cave entrances, area borders, portals. Passages are the openings between rooms inside one area (doorways, cave necks); the room announcements use that word.

## Settings

The in-game switches are in the Ctrl+S list (BACK+RB on the controller), in groups, each spoken as it stands ("Radar enabled"), and they are remembered between sessions. A setting of several values goes up with right (wrapping round to the lowest) or one step down or up with LB and RB. New ones worth knowing:

- **Directions** (Navigation): compass points (the default) or clock hours, for every direction the mod says.
- **Route turn prompts** (Navigation, on): the next leg of a route you asked for, said at each turn.
- **Route total distance** (Navigation, on): turn it off and a route says only its legs, without the whole length first.
- **Route legs** (Navigation, on): turn it off and a route says only its target and its whole length, without the legs ("east, 11; then ...").
- **Passages by the map's sides** (Navigation, on): the room's exit line by the room's walls, northeast, southeast, southwest and northwest.
- **Enemy class sounds:** layered (the default) or classic. Every enemy now growls at the same pitch, so pitch only tells north or south; a champion adds a raspy buzz, a boss a metallic ring, a shaman a wail, an archer a whistle.
- **NPC exclamation mark chime** (on): a small bell when the "!" appears over a town NPC, also when it appears while you stand near, and after its name from the scanner or the NPC announcer. The game only shows the mark within about 15 units of the NPC.
- **Footstep volume** (in Volumes): your own footsteps from 25 to 300 percent of the game's, in steps of 25; above 100 percent the game's step is played again on top of itself.
- **Pan and pitch strengths** (enemies, guidance): 0 to 400 percent in steps of 25.
- **Item sounds:** Diablo II drops (the default): every item on the ground repeats the sound the game makes when it lands (a sword, a ring, a potion...). Drop sound speed, 50 to 300 percent in steps of 25, sets how often they repeat. The other two sets: Diablo IV, or Last Epoch gold and potions (the rest stays Diablo IV).
- **Volumes:** radar and walls (with the wall bump and wall tones), enemies and NPCs, items and places, guidance; each from 0 to 200 percent in steps of 25.
- **Usable corpse rattle** (Enemies and NPCs, on, Necromancer only): a short bone rattle toward the nearest corpse when corpses you can raise or explode come near. Corpses already raised or exploded do not count. "Usable corpse rattle every 3 seconds" below it repeats it while they stay near: once, or every 2, 3, 5 or 8 seconds.
- **Minions in the life report** (Alerts, off): BACK+X also lists your summons.
- **Mercenary in the life report** (Alerts, off): BACK+X also says the mercenary.
- **Settings list** (its own group): "Settings list ends" (wrap with a click, or stop with a knock) and "Settings list opens at its first entry" (off: it opens where you left it).
- **Loot filter** (its own group): "Loot filter" (off; LB in play flips it), "Potion loops" (on), "Gem and rune loops" (on) and "Equipment loops: all / magic and better / rare and better / set and unique / off". With the filter on, only those loops sound; gold, scrolls and quest items always do, and a white item the game draws gray (socketed or ethereal, "silver") loops at every level but off.
- **Health and mana potions after other loot** (Sounds, on): when more items lie near than can sound at once, health and mana potions sound only once the rest of the loot has its sound.
- **Town NPCs and exits listed from anywhere in town** (on): now the exits too.
- **The party panel** (multiplayer) reads with the right stick: the game's name, then each player. The mercenary's sheet keeps the right stick down for its statistics while an item is focused on it or in your backpack beside it; up still reads the item. The chat box (Enter) says "Text chat" and echoes what you type, as the name box does, then "Text chat closed".
- **Your own summons** (and other players') are no longer announced as NPCs, and a Warlock's bound demon is no longer taken for an enemy.
- **Status effect and curse alerts** (Alerts, on): "You are poisoned", "You are frozen", "You are chilled", "You are stunned", "You are slowed", "You are afflicted by Amplify Damage" (any curse), said at once as they begin; their end follows ("You are no longer frozen"). A stamina potion says "Stamina frozen at 100 percent"; an antidote or thawing potion "You are fortified", after the poison or cold it cured, with no notice when it wears off.
- **Zero points** (Items and menus, on): opening the character screen or the skill tree with nothing to spend says "0 Stat Points Remaining" or "0 Skill Choices Remaining".
- **Items moved to the stash named** (Items and menus, on): an item sent from the backpack into the stash says "Short Sword moved to the personal stash" or "... to the shared stash", as gold does.
- **Horadric Cube:** "Cube opened" when its window opens, "Horadric Cube" when the cursor enters it; items moved say "Moved Wirt's Leg into the Cube", "Moved ... into inventory" or, with Y from a stash tab, "Moved 3 Chipped Topaz to Cube"; Transmute says "New item in Cube: Flawed Topaz". A stash tab beside a carried, open cube says "Runes - Cube open".
- **Route on BACK+A** (Controller, off): BACK+A says the route instead of BACK + right stick down.
- **Experience in the life report** (on): BACK+X also says how far you are into your level, as the bar between the orbs shows.
- **Mercenary's lines named** (on): "Mercenary says: I feel much stronger now".
- **Walk, run and stamina alerts** (on).
- **Ammunition alerts** (on): arrows, bolts or throwing weapons running low.
- **Assassin charge alerts** (Alerts, on): each new martial-arts charge says the skill and the count ("Tiger Strike 2"); charges that run out unused say "charges lost".
- **Charge chord** (Alerts): a soft chord, one note per charge (A, then A and C, then A, C and E: A minor). Struck with each new charge (the default), held while the charges last, or off. Its volume is in Volumes.
- **Weapon set number** (on): the weapon swap also says "Weapon set 1" or "2".
- **Skill hotkeys** (on): on the keyboard, F1 to F8 say the skill they select, only when it changes.
- **Town NPCs listed from anywhere in town** (on): every town NPC can be scanned and followed from anywhere in town, even before you have seen them.
- **Inventory edge bump** (on): a knock when the inventory cursor hits the edge, or leaves the backpack or its equipment slots for the vendor or stash.
- **Silver items** (on): items whose name the game draws in silver (socketed or ethereal) say "silver" after the name, as magic items say "magic".
- **Focus on stat points** (on): the character screen starts on Strength's plus button when you have points to spend.
- **Screen hints** (on): short hints on character select and the name box.
- **Wall bump sound** (on): a thud when you push into a wall and stop moving; your footsteps also go quiet while you push (the footstep filter, Ctrl+F).
- **Small obstacle sound** (on): the radar gives rocks, trees, pillars and other clutter you can walk around their own knock, unlike a wall.
- **Wall tones** (off): a soft continuous sound for each direction, louder as a wall gets closer, as in the Grim Dawn and Last Epoch mods.
- **Mana alerts:** every change, low mana only, or off. Left and right on the right stick (Space on the keyboard) step through the three.
- **Enemy sound panning and pitch, guidance panning and pitch:** 0 to 400 percent; 100 is the default.
- **Shrine and well sounds** (on): a sound for each shrine or well you can still use, while it is in reach.
- **Doorway sounds** (on): in the Crypt, the Mausoleum and the Tower Cellar, a creaking wood sound at the middle of each doorway between two rooms.
- **Opened door sounds** (off): doors keep their sound after you open them.
- **Walked distance clicks** (off): a click every 1, 5 or 10 units you walk ("Units per click", next to it); its volume is in Volumes.
- **Back button for the mod only** (on): during play the game no longer opens or zooms its map with BACK. Turn it off to give BACK back to the map.

### The settings file

`d2rloader\config\d2r-access.toml` holds the numbers: the radar (on, volume, diagonals), and for enemy sounds `[entity_cues]`: `on_screen` (growls start when an enemy is on screen), `range` (used when `on_screen` is false), `max_voices` (how many enemies can growl, 1 to 8), `pan_strength` and `pitch_strength` (percent, 100 is the default, up to 400), and the same two strengths under `[beacon]` for the guidance ping; `[wall_tones]` sets the wall tones' volume and range. The loader creates it the first time the game starts with the mod, and updates never replace it. Edit it with Notepad; changes apply the next time you start the game. A strength changed in the Ctrl+S list wins over the file.

## Sound glossary

The Sound glossary group of the Ctrl+S list holds the mod's sounds, one per entry: moving onto one names it and plays it, Space (right on the controller) plays it again. Its last entry, Diablo II drop sounds, is a group of its own (right opens it, left comes back) with each drop sound, named after an item that makes it.

## Playing online with someone else

Both players must use this same package (the same loader, {{LOADER}}).

## If something goes wrong

Copy the file `d2rloader\logs\d2r-access.log` right after the problem happens, before starting the game again, and send it with what you were doing. If the game closed by itself, also send what is in the `d2rloader\crashes` folder.

## Licenses

In the `licenses` folder: the mod's (MIT) and PRISM's, the speech library. D2RLoader belongs to d2rloader.net. The four Microsoft DLLs are the Visual C++ runtime (x64), which the loader and the mod need.
