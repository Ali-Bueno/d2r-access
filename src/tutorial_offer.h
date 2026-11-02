#pragma once

#include "tutorial.h"

#include <string>

namespace D2RL {
struct PluginContext;
}

// The question asked before a new character's tutorial (docs/tutorial.md): while it is open the
// game gets no input and the mod's own gestures do nothing; two presses of one answer give it.
namespace D2RAccess::TutorialOffer {
using Reply = Tutorial::Offer::Reply;
// Says `question` now, and again whenever the player does anything but answer.
void Open(std::string question);
bool IsOpen();
// Every update, right after InputDirection::Poll: while open, the update's input is the question's.
void Tick(const D2RL::PluginContext* context);
// Yes, No or Never once given, the question then closed; None until then.
Reply TakeAnswer();
// Closes it unanswered: the game left, the tutorial switched off.
void Close();
// At unload.
void Remove();
}
