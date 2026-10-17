D2R ACCESS - COMPLETE TESTER PACKAGE
Test build of {{DATE}} (commit {{COMMIT}})


WHAT IT IS

An accessibility mod for Diablo II: Resurrected. It reads the game through your
screen reader (NVDA, JAWS or, when neither is running, the Windows voice) and
plays orientation sounds: a wall radar, items, exits, chests, monsters.
It runs on D2RLoader {{LOADER}}, which comes in this package.


{{NEWS}}
REQUIREMENTS

- Diablo II: Resurrected from Steam or Battle.net, on 64-bit Windows 10 or 11.
- NVDA or JAWS running before you start the game.


INSTALLATION

1. Close the game.
2. Extract everything in this zip into the game folder, the one that has
   D2R.exe. On Steam: right-click the game, Manage, Browse local files.
   It is usually:
   C:\Program Files (x86)\Steam\steamapps\common\Diablo II Resurrected
   If it asks whether to replace files, say yes.
3. Next to D2R.exe you must now have: D2RLoader.exe, D2RCore.dll,
   d2rloader.mpq, four Microsoft DLLs (msvcp140.dll, msvcp140_atomic_wait.dll,
   vcruntime140.dll, vcruntime140_1.dll) and the d2rloader folder.
   Do not rename or replace D2R.exe.


UPDATING

The latest version can always be downloaded from this link (you must be signed
in to GitHub with an account that has access to the repository):

{{LATEST_URL}}

To update, close the game and extract the new zip on top, just like the first
time. If the game then starts in the wrong language again, repeat the LANGUAGE
step.


STARTING THE GAME

Steam (recommended): in the game's Properties, General tab, Launch options,
type the path of D2RLoader.exe in quotes followed by %command%. For example:

"C:\Program Files (x86)\Steam\steamapps\common\Diablo II Resurrected\D2RLoader.exe" %command%

Then play with Steam's Play button, as usual.

Battle.net: with Battle.net open and signed in, run D2RLoader.exe from the
game folder.

When the mod starts it says "Diablo 2 accessibility loaded".

If the loader warns that there was no sign-in for 30 days and starts without
mods, that is a Blizzard check the loader keeps on purpose. Start from Steam as
explained above. With a new account or installation you may need to open the
game once without the loader, connected to the internet.


LANGUAGE

The mod speaks in the game's text language. The loader may ignore the language
chosen in Steam and start the game in English. To choose it, open the file
d2rloader\config\d2rloader.toml with Notepad and find these two lines:

text_locale = ""
audio_locale = ""

Put the language code between the quotes, in both: enUS English, deDE German,
frFR French, itIT Italian, esES Spanish (Spain), esMX Spanish (Latin America),
plPL Polish, ptBR Portuguese (Brazil), ruRU Russian, jaJP Japanese,
koKR Korean, zhCN Chinese (simplified), zhTW Chinese (traditional).
Save and start the game again.


CONTROLS

Controller (the game's own controller mode). The game keeps its whole layout;
the mod adds these:

In play:
- Right stick up / down: scanner, previous / next category (exits, NPCs,
  enemies, chests, items, rooms...). Right stick left / right: previous / next
  thing in that category, with its distance and direction (north, northeast...,
  or clock hours: see HOW THE MOD DESCRIBES THE MAP).
- BACK + right stick down: the route to what the beacon follows, else to what
  the scanner named last, as a sighted player plans it off the minimap:
  "Route to Akara, 85 units: north, 20; then door; then east, 30". Nothing
  walks for you; ask again on the way and it starts from where you stand.
  While you walk it, the next leg is said as you come to each turn ("Now
  east, 30"), and "Off the route" once if you leave it.
- BACK + right stick up: the description of the area you are in, when there is
  one. Write your own in d2rloader\plugins\d2r-access\descriptions.txt, one
  line each: the area's name as the game says it, an equals sign, the words
  ("Rogue Encampment = A ramshackle collection of tents..."). Landmarks work
  the same way ("Hangman's Tree = ..."): their words are said instead of their
  name the first time they come into reach. Updates never replace that file.
  "Area and landmark descriptions" (Navigation) turns off the landmark words
  and the hint said the first time you enter a described area.
- BACK+LB: follow or release what the scanner named (beacon). A ping leads you
  there along a real route and the mod says when you arrive.
- BACK+X: life, mana, stamina (when not full), the arrows or throwing
  weapons left in hand, any poison, freezing, chill, stun or curse on you.
  A Necromancer also hears how many usable corpses are nearby. The mercenary
  is BACK+RT's now ("Mercenary in the life report", Alerts, brings it back).
- BACK+RT: one press at a time, your mercenary ("Mercenary life: 80 percent,
  Level 12, 6 units, north"), then each of your summons as
  the game's portraits group them ("Raven: 5", "Oak Sage: life 35 percent",
  a Warlock's demons one by one, "Tainted 2: life 75 percent"), then each
  party member ("Raine: life 80 percent"), with how far and where each one is,
  or the area a party member is in. After the last it starts again; after a
  pause of 10 seconds it starts again at the mercenary.
- BACK+LT: the main skill bar, as holding LT reads the other one.
- BACK+Y: the passages out of the room you are in.
- BACK+A: passage finder on or off. While on, the right stick names the
  passages on one wall of the room instead of scanning: flick up-right for the
  northeast wall (2 o'clock), down-right southeast (4), down-left southwest
  (8), up-left northwest (10). The room's exit line names passages by the same
  walls. "Passages by the map's sides" (Ctrl+S, Navigation, on) turned off
  gives the screen's sides instead.
- BACK+B: area overview: the area, where you stand in it ("northeast edge",
  "centre", "south corner"), how much you have seen, then each side of the
  area (northeast, southeast, southwest, northwest: Diablo's areas are
  diamonds on screen) with how far it reaches, where unexplored ground
  starts or the area beyond it, then the places the radar has logged here,
  nearest first ("Waypoint, 40 units, northeast").
- Lobby (host game, direct join): A on a text box starts typing there, for a
  keyboard. With the controller alone, while a box is being typed into, the
  right stick up and down step through the letters, digits, space and a few
  signs (each one said), right types the one reached and left deletes the
  last one; B ends as before. "Typing with the right stick" (Controller)
  turns the stick part off.
- LB (alone, in play): the audible loot filter on or off ("Loot filter on").
  While it is on, only the drop loops you keep in the Loot filter group of
  the settings sound (see SETTINGS); the game still plays each item's own
  drop sound as it lands.
- BACK+RB: the mod's settings list, open or close (the keyboard's Ctrl+S).
  It is made of groups (Sounds, Volumes, Enemies and NPCs, Navigation, Items
  and menus, Alerts, Controller, Settings list, Loot filter, Sound glossary),
  each said as a sub menu ("Sounds sub menu"). While it is open the right
  stick moves through it: up and down step, right opens a group and then
  changes the setting (or plays the sound again in the glossary), left goes
  back to the groups. LB and RB lower and raise a setting of several values
  (they are the list's while it is open). Past the last entry the list goes
  round to the first with a click (or stops there with a knock: see
  SETTINGS). It closes by itself after 30 seconds unused.
- Holding LT for a moment: reads the skill bar shown with LT.
- L3 is the game's walk/run toggle: the mod says "Walking" or "Running" when
  you press it, and warns as stamina runs low. R3 is the game's weapon swap:
  the mod names the weapons in hand.
During play BACK alone does nothing (the game's map stays shut; see the
settings list). Every other button does what the game does.

In menus:
- Right stick up: detail of what has focus (an item's full description, a
  skill's text). Right stick down: the buttons this screen offers (the game's
  own button legend). Right stick right: extra text; on a skill, left opens
  its lines one by one (B or left again closes them).
- On an inventory item, hold Y and move the left stick: left reads the item,
  right the equipped one it compares with, up detail, down controls. To hear
  the equipped one as soon as Y shows it, turn on "Equipped item comparison
  read aloud" in the settings list (Items and menus).
- In the skill tree the D-pad moves one skill at a time.
- On the character screen with points to spend, focus starts on Strength's
  plus button; the right stick reads the statistics.

Keyboard: the full list is in KEYBOARD.txt. To begin with:
- Ctrl+S: list of the mod's switches (Page Up / Page Down to move, Space to
  change).
- Page Up / Page Down: scanner, previous / next object.
  Ctrl+Page Up / Ctrl+Page Down: previous / next category.
- D: walk to the nearest usable thing and use it.
- Ctrl+G: radar on or off. Ctrl+T: radar status.
- Z and X: life and mana.


HOW THE MOD DESCRIBES THE MAP

Directions are screen directions said as compass points: north is the top of
the screen, south the bottom, east and west its right and left sides, and
northeast, southeast, southwest and northwest the corners between. Diablo II
draws its world turned 45 degrees, so a straight corridor on screen can run
diagonally on the game's own map; the mod always follows the screen. In the
Ctrl+S list (Navigation) "Directions" can say clock hours instead: 12 o'clock
is the top of the screen, 3 o'clock the right, 6 the bottom, 9 the left.

Distances are in units. A unit is the game's smallest map cell (a subtile; a
full tile is five subtiles), not a step of your character. Distances are
straight lines from your character to the thing's position, so a large object
can be a few units away while you stand at its edge.

A room counts as explored as soon as you step into it, anywhere: everything
in it joins the scanner, as a sighted player sees a whole room on arriving.
Whatever the scanner lists can be routed to and followed with the beacon, even
in a room you never entered; when the way has to cross rooms you have not
explored, the route says "Part of it crosses ground not seen yet", and the
beacon leads you into the first of them. A room is reached at your first step
inside it.

The scanner lists a monster once it has been within 15 units of you with
nothing blocking the view, or once it is in the room you stand in. It stays listed after it leaves the screen,
follows the monster while the game has it loaded, and is dropped when it dies.
Monsters you never came near are not listed, unless the Ctrl+S list is set to
"Full assistance": then every monster the game has loaded around you is listed,
which helps to find the last ones when an area must be cleared.

Exits are the ways out of an area: stairs, cave entrances, area borders,
portals. Passages are the openings between rooms inside one area (doorways,
cave necks); the room announcements and the exit finder use that word.


SETTINGS

The in-game switches are in the Ctrl+S list (BACK+RB on the controller), in
groups, each spoken as it stands ("Radar enabled"), and they are remembered
between sessions. A setting of several values goes up with right (wrapping
round to the lowest) or one step down or up with LB and RB. New ones worth
knowing:
- Directions (Navigation): compass points (the default) or clock hours, for
  every direction the mod says.
- Route turn prompts (Navigation, on): the next leg of a route you asked for,
  said at each turn.
- Passages by the map's sides (Navigation, on): the passage finder and the
  room's exit line by the room's walls, northeast, southeast, southwest and
  northwest, and the finder's stick turned to match.
- Enemy class sounds: layered (the default) or classic. Every enemy now
  growls at the same pitch, so pitch only tells north or south; a champion adds a
  raspy buzz, a boss a metallic ring, a shaman a wail, an archer a whistle.
- NPC exclamation mark chime (on): a small bell when the "!" appears over a
  town NPC, also when it appears while you stand near, and after its name from the
  scanner or the NPC announcer. The game only shows the mark within about 15
  units of the NPC.
- Footstep volume (in Volumes): your own footsteps from 25 to 300 percent of the game's, in
  steps of 25; above 100 percent the game's step is played again on top of itself.
- Pan and pitch strengths (enemies, guidance): 0 to 400 percent in steps of 25.
- Item sounds: Diablo IV (the default), Last Epoch gold and potions (the rest
  stays Diablo IV), or Diablo II drops: every item on the ground repeats the
  sound the game makes when it lands (a sword, a ring, a potion...). Drop
  sound speed, 50 to 300 percent in steps of 25, sets how often they repeat.
- Volumes: radar and walls (with the wall bump and wall tones), enemies and
  NPCs, items and places, guidance; each from 0 to 200 percent in steps of 25.
- Usable corpse rattle (Enemies and NPCs, on, Necromancer only): a short
  bone rattle toward the nearest corpse when corpses you can raise or explode
  come near. Corpses already raised or exploded do not count. "Usable corpse
  rattle every 3 seconds" below it repeats it while they stay near: once,
  or every 2, 3, 5 or 8 seconds.
- Minions in the life report (Alerts, off): BACK+X also lists your summons.
- Mercenary in the life report (Alerts, off): BACK+X also says the mercenary.
- Settings list (its own group): "Settings list ends" (wrap with a click, or
  stop with a knock) and "Settings list opens at its first entry" (off: it
  opens where you left it).
- Loot filter (its own group): "Loot filter" (off; LB in play flips it),
  "Potion loops" (on), "Gem and rune loops" (on) and "Equipment loops: all /
  magic and better / rare and better / set and unique / off". With the
  filter on, only those loops sound; gold, scrolls and quest items always
  do, and a white item the game draws gray (socketed or ethereal, "silver")
  loops at every level but off.
- Health and mana potions after other loot (Sounds, on): when more items lie
  near than can sound at once, health and mana potions sound only once the
  rest of the loot has its sound.
- Town NPCs and exits listed from anywhere in town (on): now the exits too.
- The party panel (multiplayer) reads with the right stick: the game's name,
  then each player. The mercenary's sheet keeps the right stick down for its
  statistics while an item is focused on it or in your backpack beside it;
  up still reads the item. The chat box (Enter) says "Text chat" and echoes
  what you type, as the name box does, then "Text chat closed".
- Your own summons (and other players') are no longer announced as NPCs, and
  a Warlock's bound demon is no longer taken for an enemy.
- Status effect and curse alerts (Alerts, on): "You are poisoned", "You are
  frozen", "You are chilled", "You are stunned", "You are slowed", "You are
  afflicted by Amplify Damage" (any curse), said at once as they begin; their
  end follows ("You are no longer frozen").
- Zero points (Items and menus, on): opening the character screen or the skill
  tree with nothing to spend says "0 Stat Points Remaining" or "0 Skill
  Choices Remaining".
- Items moved to the stash named (Items and menus, on): an item sent from the
  backpack into the stash says "Short Sword moved to the personal stash" or
  "... to the shared stash", as gold does.
- Route on BACK+A (Controller, off): swaps them: BACK+A says the route and
  BACK + right stick down turns the passage finder on or off.
- Experience in the life report (on): BACK+X also says how far you are into
  your level, as the bar between the orbs shows.
- Mercenary's lines named (on): "Mercenary says: I feel much stronger now".
- Walk, run and stamina alerts (on).
- Ammunition alerts (on): arrows, bolts or throwing weapons running low.
- Weapon set number (on): the weapon swap also says "Weapon set 1" or "2".
- Town NPCs listed from anywhere in town (on): every town NPC can be scanned
  and followed from anywhere in town, even before you have seen them.
- Inventory edge bump (on): a knock when the inventory cursor hits the edge, or leaves the
  backpack or its equipment slots for the vendor or stash.
- Silver items (on): items whose name the game draws in silver (socketed or
  ethereal) say "silver" after the name, as magic items say "magic".
- Focus on stat points (on): the character screen starts on Strength's plus
  button when you have points to spend.
- Screen hints (on): short hints on character select and the name box.
- Wall bump sound (on): a thud when you push into a wall and stop moving; your
  footsteps also go quiet while you push (the footstep filter, Ctrl+F).
- Small obstacle sound (on): the radar gives rocks, trees, pillars and other
  clutter you can walk around their own knock, unlike a wall.
- Wall tones (off): a soft continuous sound for each direction, louder as a
  wall gets closer, as in the Grim Dawn and Last Epoch mods.
- Mana alerts: every change, low mana only, or off. Left and right on the
  right stick (Space on the keyboard) step through the three.
- Enemy sound panning and pitch, guidance panning and pitch: 0 to 400 percent;
  100 is the default.
- Shrine and well sounds (on): a sound for each shrine or well you can still
  use, while it is in reach.
- Back button for the mod only (on): during play the game no longer opens or
  zooms its map with BACK. Turn it off to give BACK back to the map.

d2rloader\config\d2r-access.toml holds the numbers: the radar (on, volume,
diagonals), and for enemy sounds [entity_cues]: on_screen (growls start when
an enemy is on screen), range (used when on_screen is false), max_voices (how
many enemies can growl, 1 to 8), pan_strength and pitch_strength (percent,
100 is the default, up to 400), and the same two strengths under [beacon] for
the guidance ping; [wall_tones] sets the wall tones' volume and range. Edit it
with Notepad; changes apply the next time you start the game. A strength
changed in the Ctrl+S list wins over the file.


SOUND GLOSSARY

The Sound glossary group of the Ctrl+S list holds the mod's sounds, one per
entry: moving onto one names it and plays it, Space (right on the controller)
plays it again. Its last entry, Diablo II drop sounds, is a group of its own
(right opens it, left comes back) with each drop sound, named after an item
that makes it.


PLAYING ONLINE WITH SOMEONE ELSE

Both players must use this same package (the same loader, {{LOADER}}).


IF SOMETHING GOES WRONG

Copy the file d2rloader\logs\d2r-access.log right after the problem happens,
before starting the game again, and send it with what you were doing. If the
game closed by itself, also send what is in the d2rloader\crashes folder.


LICENSES

In the licenses folder: the mod's (MIT) and PRISM's, the speech library.
D2RLoader belongs to d2rloader.net. The four Microsoft DLLs are the Visual C++
runtime (x64), which the loader and the mod need.
