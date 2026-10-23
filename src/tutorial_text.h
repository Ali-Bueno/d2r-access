#pragma once

#include "speech_strings.h"
#include "tutorial.h"

#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

// The tutorial's words, for the pad and for the keyboard, one table per language
// in src/tutorial_strings/ (docs/tutorial.md).
namespace D2RAccess::TutorialText {
struct Lines { const char* pad; const char* keys; };
// The radar's sounds in the radar step, the world's in the sounds step and the monsters' before
// leaving the camp, each after its name.
inline constexpr size_t CueCount = 7, WorldCueCount = 9, EnemyCueCount = 6;
struct Texts {
	Lines welcome;
	Lines steps[Tutorial::StepCount + 1]; // Finished's is the farewell
	Lines radarTask;                      // said after the radar's sounds
	const char* cues[CueCount];           // passage, unexplored passage, floor, unexplored floor, wall, door, small
	                                      // obstacle: each name and its meaning
	const char* worldCues[WorldCueCount]; // NPC, NPC with a "!", stash, stairs or portal, area border, unexplored room,
	                                      // gold, a potion, an item to wear: the items in the player's sound set
	Lines worldTask;                      // said after the world's sounds, as they are let in
	const char* enemyCues[EnemyCueCount]; // monster, ranged monster, resurrecting shaman, elite, in reach, crosshair
	Lines leaveTask;                      // said after the monsters' sounds: the way out of the camp
	Lines auditionAgain;                  // before a lesson's sounds: how to hear one again
	Lines auditionWait;                   // the same when each sound waits for the player (tutorial-sound-wait)
	Lines portalButton;                   // after the way back: the button the town portal is on, as {portal}
	Lines gold;                           // gold picked up: the stash keeps it safe
	Lines shop;                           // a shop opened: the tomes
	Lines levelUp;                        // the level-up lesson, at the first calm moment after it
	Lines levelUpSheet;                   // as the character sheet opens
	Lines levelUpSwitch;                  // in the sheet, its points spent: the way on to the skill tree
	Lines levelUpTree;                    // as the skill tree opens
	Lines levelUpTreeLater;               // the sheet shut before the tree
	Lines levelUpBind;                    // the skill point spent: putting a skill on a button
	Lines kit;                            // something to wear picked up: the inventory lesson
	Lines kitOpen;                        // as the inventory opens
	Lines kitWorn;                        // that item worn
	Lines kitDone;                        // back in play from the inventory
	Lines levelUpDone;                    // back in play from the tree
	const char* routeStick;               // the pad's route chord, as {route}
	const char* routeButton;              // the same with the switch pad-route-on-a
	const char* firstMessage;
	const char* lastMessage;
	// {den}, {moor}, {plains}, {portaltome}, {idtome} and {general} when the game's own names cannot be read.
	const char* den;
	const char* moor;
	const char* plains;
	const char* portalTome;
	const char* identifyTome;
	const char* generalSkills;
};
const Texts& For(Strings::Language language);

// The game's own names of places, tomes and the skill tree's general page, which differ between
// Spain's Spanish and Mexico's; an empty one takes the text's own. `portal` is the button the
// town portal is bound to.
struct Names { std::string den, moor, plains, portalTome, identifyTome, generalSkills, portal; };

// Replaces {den}, {moor}, {plains}, {portaltome}, {idtome}, {general} and {portal} with the game's
// names and the button, and {view}, {menu}, {a}, {rs} and the other pad tokens with the button
// names `button` gives for a controller glyph; {route} first, as its chord can move.
std::string Render(std::string_view text, const Texts& texts, bool routeOnA, const Names& names,
	const std::function<std::string(uint32_t)>& button);
}
