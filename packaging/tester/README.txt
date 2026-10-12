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
  thing in that category, with its distance and direction.
- BACK+LB: follow or release what the scanner named (beacon). A ping leads you
  there along a real route and the mod says when you arrive.
- BACK+X: life, mana, stamina (when not full) and the arrows or throwing
  weapons left in hand.
- BACK+Y: the passages out of the room you are in.
- BACK+A: passage finder on or off (then the right stick names passages by
  screen side instead of scanning).
- BACK+B: terrain overview.
- BACK+RB: the mod's settings list, open or close (the keyboard's Ctrl+S).
  While it is open the right stick moves through it: up and down step, left or
  right changes the setting (or plays the sound again in the glossary). It
  closes by itself after 30 seconds unused.
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
  right the equipped one it compares with, up detail, down controls.
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

Directions are always screen directions, never compass points: up is the top
of the screen, down the bottom, left and right the screen's sides. Diablo II
draws its world turned 45 degrees, so a straight corridor on screen can run
diagonally on the game's own map; the mod always follows the screen.

A step is the game's smallest map unit (a subtile; a full tile is five
subtiles). Distances are straight lines from your character to the thing's
position, so a large object can be a few steps away while you stand at its
edge. Running covers more steps per second than walking; the count of steps
does not change.

The scanner lists a monster once it has been within 15 steps of you with
nothing blocking the view. It stays listed after it leaves the screen,
follows the monster while the game has it loaded, and is dropped when it dies.
Monsters you never came near are not listed, unless the Ctrl+S list is set to
"Full assistance": then every monster the game has loaded around you is listed,
which helps to find the last ones when an area must be cleared.

Exits are the ways out of an area: stairs, cave entrances, area borders,
portals. Passages are the openings between rooms inside one area (doorways,
cave necks); the room announcements and the exit finder use that word.


SETTINGS

The in-game switches are in the Ctrl+S list (BACK+RB on the controller), each
spoken as it stands ("Radar enabled"), and they are remembered between
sessions. New ones worth knowing:
- Walk, run and stamina alerts (on).
- Ammunition alerts (on): arrows, bolts or throwing weapons running low.
- Weapon set number (on): the weapon swap also says "Weapon set 1" or "2".
- Town NPCs listed from anywhere in town (on): every town NPC can be scanned
  and followed from anywhere in town, even before you have seen them.
- Pickup range click (on): a click when the game's pickup prompt shows an item.
- Inventory edge bump (on): a knock when the inventory cursor hits the edge.
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
- Enemy sound panning and pitch, guidance panning and pitch: 0 to 200 percent
  in steps of 25; 100 is the default.
- Potion and gold sounds: Diablo or Last Epoch (each potion kind has its own
  glass sound in the Last Epoch set).
- Shrine and well sounds (on): a sound for each shrine or well you can still
  use, while it is in reach.
- Back button for the mod only (on): during play the game no longer opens or
  zooms its map with BACK. Turn it off to give BACK back to the map.

d2rloader\config\d2r-access.toml holds the numbers: the radar (on, volume,
diagonals), and for enemy sounds [entity_cues]: on_screen (growls start when
an enemy is on screen), range (used when on_screen is false), max_voices (how
many enemies can growl, 1 to 8), pan_strength and pitch_strength (percent,
100 is the default, up to 200), and the same two strengths under [beacon] for
the guidance ping; [wall_tones] sets the wall tones' volume and range. Edit it
with Notepad; changes apply the next time you start the game. A strength
changed in the Ctrl+S list wins over the file.


SOUND GLOSSARY

At the end of the Ctrl+S list are the mod's sounds, one per entry: moving
onto one names it and plays it, Space plays it again. Page Up from the top of
the list jumps straight to the last sounds.


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
