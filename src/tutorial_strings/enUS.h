#pragma once
#include "tutorial_text.h"

// English (enUS): the tutorial's words.
namespace D2RAccess::TutorialText::Tables {

inline constexpr Texts enUS{
	.welcome = {
		"D2R Access tutorial. I will show you how to find your way with the mod until the first quest is done. "
		"Each step asks you to do something; when you do it, a step-done sound plays and the next one comes. "
		"{view} and {rs} left or right go through these messages; {view} and {up} go back to the first. "
		"You can turn the tutorial off in the settings list, with {view} and {rb}.",
		"D2R Access tutorial. I will show you how to find your way with the mod until the first quest is done. "
		"Each step asks you to do something; when you do it, a step-done sound plays and the next one comes. "
		"K and L go through these messages; Shift plus K goes back to the first. "
		"You can turn the tutorial off in the settings list, with Control plus S."},
	.steps = {
		{"First step: your state. Press {view} and {x} to hear your life and mana.",
			"First step: your state. Press Z to hear your life, and X for your mana."},
		{"The radar sounds in four directions as your surroundings change; down sounds lower. I will now play its "
			"sounds, each after its name and what it means.",
			"The radar sounds in four directions as your surroundings change; down sounds lower. I will now play its "
			"sounds, each after its name and what it means."},
		{"Besides the radar, things nearby sound from where they are, louder the closer they are: people, places and "
			"what lies on the ground, the items in the sounds you chose in the settings list. Until now I kept them "
			"quiet so you could hear the radar alone. I will play each one after its name.",
			"Besides the radar, things nearby sound from where they are, louder the closer they are: people, places and "
			"what lies on the ground, the items in the sounds you chose in the settings list. Until now I kept them "
			"quiet so you could hear the radar alone. I will play each one after its name."},
		{"The scanner. It lists what is on the map by category: exits, NPCs, waypoints and more, nearest first, with "
			"how far and which way. With {rs}, up and down change the category, and left and right go through it. "
			"Find Warriv in the NPC category: he is the one nearest you.",
			"The scanner. It lists what is on the map by category: exits, NPCs, waypoints and more, nearest first, with "
			"how far and which way. Control plus Page Up and Control plus Page Down change the category; Page Up and "
			"Page Down go through it. Find Warriv in the NPC category: he is the one nearest you."},
		{"Talk to Warriv. He is a few steps away: walk up to him with {ls} the way the scanner said, press {a} beside "
			"him and listen to what he has to tell you. When he is done, close the dialogue with {b}.",
			"Talk to Warriv. He is a few steps away: walk up to him with the arrow keys the way the scanner said, press "
			"D beside him and listen to what he has to tell you. When he is done, close the dialogue with Escape."},
		{"Now Akara, the camp's healer: she has the first quest. She is farther away; find her in the scanner's NPC "
			"category.",
			"Now Akara, the camp's healer: she has the first quest. She is farther away; find her in the scanner's NPC "
			"category."},
		{"The route. It tells you the way to what the scanner named, in directions and distances, as you would plan it "
			"on the minimap; it does not walk for you. With Akara named, press {route}.",
			"The route. It tells you the way to what the scanner named, in directions and distances, as you would plan it "
			"on the minimap; it does not walk for you. With Akara named, press Control plus E."},
		{"The beacon. It pings toward what the scanner named and leads you along the way, but you do the walking. "
			"Press {view} and {lb} to follow Akara, and walk toward the pings until you reach her; another press lets go.",
			"The beacon. It pings toward what the scanner named and leads you along the way, but you do the walking. "
			"Press Control plus B to follow Akara, and walk with the arrow keys toward the pings until you reach her; "
			"another press lets go."},
		{"Talk to Akara: beside her, press {a}. She has a quest for you. "
			"When she is done, close the dialogue with {b}.",
			"Talk to Akara: beside her, press D. She has a quest for you. "
			"When she is done, close the dialogue with Escape."},
		{"Outside the camp there are monsters, and each kind sounds in its own way from where it is. Before you go, "
			"I will play them, each after its name.",
			"Outside the camp there are monsters, and each kind sounds in its own way from where it is. Before you go, "
			"I will play them, each after its name."},
		{"You are in {moor}. Outdoors, two questions place you: {view} and {b} give an overview of the terrain, and "
			"{view} and {rs} up the area's description. Try the overview.",
			"You are in {moor}. Control plus O gives you an overview of the terrain around you. Try it."},
		{"There are monsters here: they sound as you heard in the camp, and the scanner lists them in their own "
			"category. Attack with {a}; your other skills are on the buttons you gave them. If your life drops, drink a "
			"potion with {dpad}. What they drop sounds from where it lies: walk up to it and press {a} to pick it up; "
			"gold is picked up as you walk over it. Kill a monster.",
			"There are monsters here: they sound as you heard in the camp, and the scanner lists them in their own "
			"category. Tab picks the next monster; hold A to attack with your left skill, and W with your right one. If "
			"your life drops, the potions are on keys 1 to 4. What they drop sounds from where it lies: full stop picks "
			"the next item on the ground, comma the previous one, and D picks it up; gold is picked up as you walk over "
			"it. Kill a monster."},
		{"Find {den}. The map is made anew for each game, so its entrance may be near or far. Follow the radar's "
			"sounds, or the wall tones if you have them on: unexplored passages and floor lead to new ground, and the "
			"radar tells you when it logs a new exit, which the scanner then lists among its exits. Look there for "
			"{den}: once it is listed, the route and the beacon take you to its entrance. If you get lost, the scanner's "
			"unexplored area category shows you where to go.",
			"Find {den}. The map is made anew for each game, so its entrance may be near or far. Follow the radar's "
			"sounds, or the wall tones if you have them on: unexplored passages and floor lead to new ground, and the "
			"radar tells you when it logs a new exit, which the scanner then lists among its exits. Look there for "
			"{den}: once it is listed, the route and the beacon take you to its entrance. If you get lost, the scanner's "
			"unexplored area category shows you where to go."},
		{"You are in {den}. In caves and dungeons the mod knows the rooms: {view} and {y} say the ways out of the room "
			"you are in. Try it now.",
			"You are in {den}. In caves and dungeons the mod knows the rooms: Control plus Y says the ways out of the "
			"room you are in. Try it now."},
		{"As you enter each room you will hear whether parts of it are still unexplored. Kill every monster in {den}; "
			"the game says how many are left. The scanner's unexplored area category helps you miss none.",
			"As you enter each room you will hear whether parts of it are still unexplored. Kill every monster in {den}; "
			"the game says how many are left. The scanner's unexplored area category helps you miss none."},
		{"{den} is clear. Go back to the camp: open the inventory, use the town portal scroll, and step into the "
			"portal with {a}. You can also walk back with the route and the beacon.",
			"{den} is clear. Go back to the camp: open the inventory with I, go to the town portal scroll and press D "
			"to read it; close the inventory and press D beside the portal. You can also walk back with the route and "
			"the beacon."},
		{"Talk to Akara for your reward, a skill point. Then open the skill tree with {menu} and spend it.",
			"Talk to Akara for your reward, a skill point. Then open the skill tree with T and spend it."},
		{"One more thing the game does not tell you: waypoints keep your way. The game keeps the quests you have done, "
			"but each time you come back you start in the camp, and a far area is reached through its waypoint, which is "
			"yours only once you have touched it. The first one outside the camp is in {plains}, past {moor}. Find it in "
			"the scanner's waypoint category, walk up to it and press {a}: its menu opens, and it is yours. Close the "
			"menu with {b}.",
			"One more thing the game does not tell you: waypoints keep your way. The game keeps the quests you have done, "
			"but each time you come back you start in the camp, and a far area is reached through its waypoint, which is "
			"yours only once you have touched it. The first one outside the camp is in {plains}, past {moor}. Find it in "
			"the scanner's waypoint category, walk up to it and press D: its menu opens, and it is yours. Close the "
			"menu with Escape."},
		{"Tutorial complete. You know how to find your way: scanner, route, beacon and radar. In most menus, {rs} down "
			"tells you what the buttons do there: try it in every menu new to you, such as the stash. In the settings "
			"list, with {view} and {rb}, you choose what the mod says and plays. From here on, the game is yours.",
			"Tutorial complete. You know how to find your way: scanner, route, beacon and radar. In the settings list, "
			"with Control plus S, you choose what the mod says and plays. The keyboard also has shortcuts that walk "
			"for you: H goes to the nearest unexplored area, and Shift plus Home to what the scanner named. "
			"From here on, the game is yours."}},
	.radarTask = {
		"Now walk with {ls} until you hear a wall.",
		"Now walk with the arrow keys until you hear a wall."},
	.cues = {"Passage: an opening that leads to another room or to an exit",
		"Unexplored passage: the passage with two pips at the end; it leads to ground you have not seen yet",
		"Floor: you can walk that way; further on there may be anything, a passage too",
		"Unexplored floor: the floor with the two pips; ground you have not seen yet lies that way", "Wall: it blocks your way",
		"Door: it opens to let you through", "Small obstacle: something small in your way that you can walk around"},
	.worldCues = {"NPC, someone to talk to", "NPC with an exclamation mark: they have something for you",
		"Stash, the chest where you keep your items", "Stairs, entrance or portal", "Area border, the way into another area",
		"Unexplored area", "Gold on the ground", "A potion on the ground",
		"An item on the ground that you can wear, such as a weapon"},
	.worldTask = {
		"Now the ones around you will sound. If you do not know what a sound is, the scanner tells you what is there, "
		"and each sound can be turned off in the settings list.",
		"Now the ones around you will sound. If you do not know what a sound is, the scanner tells you what is there, "
		"and each sound can be turned off in the settings list."},
	.enemyCues = {"Monster: the nearest ones growl from where they are", "Monster that attacks from afar, such as an archer",
		"Shaman that brings dead monsters back to life: kill it first", "Elite: a champion, a unique monster or a boss",
		"In reach: a monster has come within reach of your attack"},
	.leaveTask = {
		"Now leave the camp on your own, with what you have learned: among the scanner's exits, find {moor}, "
		"ask for the route, follow the beacon and cross the exit.",
		"Now leave the camp on your own, with what you have learned: among the scanner's exits, find {moor}, "
		"ask for the route, follow the beacon and cross the exit."},
	.auditionAgain = {
		"They come one after another; you do not need to press anything. To hear a sound again, press {view} and {rs} "
		"left: the next one then waits until {view} and {rs} right. Later, going back through these messages plays "
		"each sound again.",
		"They come one after another; you do not need to press anything. To hear a sound again, press K: the next one "
		"then waits until L. Later, going back through these messages plays each sound again."},
	.auditionWait = {
		"After each sound, {view} and {rs} left plays it again, and {view} and {rs} right goes on to the next. Later, "
		"going back through these messages plays each sound again.",
		"After each sound, K plays it again, and L goes on to the next. Later, going back through these messages plays "
		"each sound again."},
	.portalButton = {
		"Quicker: {portal} opens a town portal at once, without the inventory, while you carry a town portal scroll "
		"or tome.",
		"Quicker: {portal} opens a town portal at once, without the inventory, while you carry a town portal scroll "
		"or tome."},
	.gold = {
		"You picked up gold; it is picked up as you walk over it. When you die, you lose part of the gold you carry, "
		"but gold in the stash is safe. Back in the camp, open the stash and deposit what you do not need.",
		"You picked up gold; it is picked up as you walk over it. When you die, you lose part of the gold you carry, "
		"but gold in the stash is safe. Back in the camp, open the stash and deposit what you do not need."},
	.shop = {
		"A shop. As soon as you have the gold, buy a {portaltome} and a {idtome}: each holds up to 20 scrolls in a "
		"single cell, and the scrolls of its kind you buy afterwards go into it. Loose scrolls take a cell each and "
		"soon fill your inventory. An identify scroll shows what an unidentified item is: use it, then choose the item.",
		"A shop. As soon as you have the gold, buy a {portaltome} and a {idtome}: each holds up to 20 scrolls in a "
		"single cell, and the scrolls of its kind you buy afterwards go into it. Loose scrolls take a cell each and "
		"soon fill your inventory. An identify scroll shows what an unidentified item is: use it, then choose the item."},
	.levelUp = {
		"You have gone up a level: you have attribute points and a skill point to spend. Press {menu} to open your "
		"character sheet; I will guide you inside.",
		"You have gone up a level: you have attribute points and a skill point to spend. Press C to open your "
		"character sheet; I will guide you inside."},
	.levelUpSheet = {
		"Your character sheet. You are on Strength's plus: {dpad} up and down go through the four attributes, and "
		"holding {a} spends a point on the one you are on. {rs} up and down read your statistics line by line, and "
		"right says the points you have left.",
		"Your character sheet. The arrow keys take the cursor to each attribute's plus, and A spends a point on it. "
		"J reads the description of the line you are on, and Shift with the up or down arrow goes through it line by "
		"line."},
	.levelUpSwitch = {
		"Your attribute points are spent. Now your skill point: press {rb} until you reach the skill tree; each menu "
		"says its name.",
		"Your attribute points are spent. Now your skill point: press T to open the skill tree beside your sheet."},
	.levelUpTree = {
		"The skill tree. LT and RT change the branch, {dpad} goes through its skills, and {rs} up reads the one you "
		"are on. {a} spends the point on it, and {rs} right says the points you have left. When you are done, close "
		"it with {b}.",
		"The skill tree. Page Up and Page Down change the branch, the arrow keys go through its skills, and J reads "
		"the one the cursor is on. A spends the point on it, and Control plus Page Down says the points you have "
		"left. When you are done, close it with Escape."},
	.levelUpTreeLater = {
		"You still have your skill point. Press {menu}: with the attributes spent, it opens the skill tree.",
		"You still have your skill point. Press T to open the skill tree."},
	.levelUpBind = {
		"To use a skill, put it on a button: in the skill tree, on the skill, press {x} and then the button you "
		"want it on. Each skill's reading says which button it is on. Passive skills, such as the masteries, work on "
		"their own: they need no button and cannot have one. With LT and RT, past your class's branches, the {general} "
		"page holds what every character has, such as Town Portal and Identify; you put them on buttons the same way.",
		"To use a skill, put it in a hand: S opens the list of skills for the right hand, and Shift plus S the one "
		"for the left; the arrow keys pick one and A puts it in that hand. With a list open, F1 to F8 bind the skill "
		"under the cursor to that key. Passive skills, such as the masteries, work on their own: they are not in "
		"these lists and need nothing more."},
	.kit = {
		"You have picked up something to wear. Press {menu} to open the menus; they open on your inventory, and I "
		"will guide you inside.",
		"You have picked up something to wear. Press I to open your inventory; I will guide you inside."},
	.kitOpen = {
		"Your inventory. {dpad} goes from cell to cell, through your backpack and what you have on, and the mod says "
		"what each holds. On an item, {rs} up reads its description, and {rs} down the buttons you can use with it, "
		"the one that equips it among them. An item that says unidentified cannot be worn until it is identified, "
		"with a scroll of identify, which Akara sells.",
		"Your inventory. The arrow keys take the cursor from cell to cell, through your backpack and what you have "
		"on, and the mod says what each holds. J reads an item's description, and Shift plus A equips it. An item "
		"that says unidentified cannot be worn until it is identified, with a scroll of identify, which Akara sells."},
	.kitWorn = {
		"You have it on. Close the inventory with {b} when you are done.",
		"You have it on. Close the inventory with Escape when you are done."},
	.kitDone = {
		"Done. Whatever you pick up, the inventory is where you put it on. Back to what you were doing:",
		"Done. Whatever you pick up, the inventory is where you put it on. Back to what you were doing:"},
	.levelUpDone = {
		"Done. Try your new skill once now: press the button you put it on, and hear how it sounds. Whenever you go up "
		"a level, {menu} opens your character sheet. Back to what you were doing:",
		"Done. Try your new skill once now: A uses your left hand's skill and W your right's; hear how it sounds. "
		"Whenever you go up a level, C opens your character sheet and T the skill tree. Back to what you were doing:"},
	.routeStick = "{view} and {rs} down",
	.routeButton = "{view} and {a}",
	.firstMessage = "First message",
	.lastMessage = "Last message",
	.den = "Den of Evil",
	.moor = "Blood Moor",
	.plains = "Cold Plains",
	.portalTome = "Tome of Town Portal",
	.identifyTome = "Tome of Identify",
	.generalSkills = "General Skills",
};
}
