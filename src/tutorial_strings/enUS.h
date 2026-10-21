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
			"sounds, each after its name and what it means; you do not need to press anything.",
			"The radar sounds in four directions as your surroundings change; down sounds lower. I will now play its "
			"sounds, each after its name and what it means; you do not need to press anything."},
		{"Besides the radar, things nearby sound from where they are, louder the closer they are. Until now I kept "
			"them quiet so you could hear the radar alone. I will play each one after its name.",
			"Besides the radar, things nearby sound from where they are, louder the closer they are. Until now I kept "
			"them quiet so you could hear the radar alone. I will play each one after its name."},
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
		{"Now leave the camp on your own, with what you have learned: among the scanner's exits, find {moor}, "
			"ask for the route, follow the beacon and cross the exit.",
			"Now leave the camp on your own, with what you have learned: among the scanner's exits, find {moor}, "
			"ask for the route, follow the beacon and cross the exit."},
		{"You are in {moor}. Three questions place you: {view} and {y} say the ways out of the area you are in; "
			"{view} and {b}, an overview of the terrain; {view} and {rs} up, the area's description. Try the first two.",
			"You are in {moor}. Two questions place you: Control plus Y says the ways out of the area you are in, "
			"and Control plus O, an overview of the terrain. Try both."},
		{"There are monsters here. They growl as they come near, and the scanner lists them in their own category. "
			"Attack with {a}; your other skills are on the buttons you gave them. If your life drops, drink a potion "
			"with {dpad}. Kill a monster and pick up the gold it drops: go up to it and press {a}.",
			"There are monsters here. They growl as they come near, and the scanner lists them in their own category. "
			"Tab picks the next monster; hold A to attack with your left skill, and W with your right one. If your life "
			"drops, the potions are on keys 1 to 4. Kill a monster and pick up its gold: full stop picks the next item "
			"on the ground and comma the previous one, and D picks it up."},
		{"Find {den}. The map is made anew for each game, so its entrance may be near or far. Follow the radar's "
			"sounds, or the wall tones if you have them on: unexplored passages and floor lead to new ground, and the "
			"radar tells you when it logs a new exit, which the scanner then lists among its exits. If you get lost, "
			"the scanner's unexplored room category shows you where to go. Go into the cave.",
			"Find {den}. The map is made anew for each game, so its entrance may be near or far. Follow the radar's "
			"sounds, or the wall tones if you have them on: unexplored passages and floor lead to new ground, and the "
			"radar tells you when it logs a new exit, which the scanner then lists among its exits. If you get lost, "
			"the scanner's unexplored room category shows you where to go. Go into the cave."},
		{"You are in {den}. As you enter each room you will hear whether parts of it are still unexplored. Kill every "
			"monster in the cave; the game says how many are left. The scanner's unexplored room category helps you miss none.",
			"You are in {den}. As you enter each room you will hear whether parts of it are still unexplored. Kill every "
			"monster in the cave; the game says how many are left. The scanner's unexplored room category helps you miss none."},
		{"The cave is clear. Go back to the camp: open the inventory, use the town portal scroll, and step into the "
			"portal with {a}. You can also walk back with the route and the beacon.",
			"The cave is clear. Go back to the camp: open the inventory with I, go to the town portal scroll and press D "
			"to read it; close the inventory and press D beside the portal. You can also walk back with the route and "
			"the beacon."},
		{"Talk to Akara for your reward, a skill point. Then open the skill tree with {menu} and spend it.",
			"Talk to Akara for your reward, a skill point. Then open the skill tree with T and spend it."},
		{"Tutorial complete. You know how to find your way: scanner, route, beacon and radar. In the settings list, "
			"with {view} and {rb}, you choose what the mod says and plays. From here on, the game is yours.",
			"Tutorial complete. You know how to find your way: scanner, route, beacon and radar. In the settings list, "
			"with Control plus S, you choose what the mod says and plays. The keyboard also has shortcuts that walk "
			"for you: H goes to the nearest unexplored room, and Shift plus Home to what the scanner named. "
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
		"Unexplored room"},
	.worldTask = {
		"Now the ones around you will sound. If you do not know what a sound is, the scanner tells you what is there, "
		"and each sound can be turned off in the settings list.",
		"Now the ones around you will sound. If you do not know what a sound is, the scanner tells you what is there, "
		"and each sound can be turned off in the settings list."},
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
		"want it on. Each skill's reading says which button it is on.",
		"To use a skill, put it in a hand: S opens the list of skills for the right hand, and Shift plus S the one "
		"for the left; the arrow keys pick one and A puts it in that hand. With a list open, F1 to F8 bind the skill "
		"under the cursor to that key."},
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
		"Done. Whenever you go up a level, {menu} opens your character sheet. Back to what you were doing:",
		"Done. Whenever you go up a level, C opens your character sheet and T the skill tree. Back to what you were "
		"doing:"},
	.routeStick = "{view} and {rs} down",
	.routeButton = "{view} and {a}",
	.firstMessage = "First message",
	.lastMessage = "Last message",
	.den = "Den of Evil",
	.moor = "Blood Moor",
};
}
