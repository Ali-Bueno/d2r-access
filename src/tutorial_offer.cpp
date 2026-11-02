#include "tutorial_offer.h"

#include "async_log.h"
#include "input_direction.h"
#include "item_audio.h"
#include "message_hold.h"
#include "pad_filter.h"
#include "screen_reader.h"

#include <windows.h>
#include <xinput.h>

#include <utility>

namespace D2RAccess::TutorialOffer {
namespace {
using Press = Tutorial::Offer::Press;

// XInput's A, B and Y: PlayStation's cross, circle and triangle.
constexpr uint16_t YesButton = XINPUT_GAMEPAD_A, NoButton = XINPUT_GAMEPAD_B, NeverButton = XINPUT_GAMEPAD_Y;
constexpr uint32_t YesKey = VK_RETURN, NoKey = VK_ESCAPE, NeverKey = VK_BACK;

Tutorial::Offer g_offer;
std::string g_question;
bool g_open = false;
Reply g_answer = Reply::None;
uint64_t g_sayAt = 0; // when the question's words follow its sound; 0 once said

// The screen reader's own keys (Ctrl stops speech, Insert and Caps Lock are its modifiers) and the
// other modifiers: kept from the game, but no answer and no reason to ask again.
bool Ignored(uint32_t key) {
	switch (key) {
	case VK_SHIFT: case VK_LSHIFT: case VK_RSHIFT: case VK_CONTROL: case VK_LCONTROL: case VK_RCONTROL:
	case VK_MENU: case VK_LMENU: case VK_RMENU: case VK_LWIN: case VK_RWIN: case VK_INSERT: case VK_CAPITAL: case VK_NUMLOCK:
		return true;
	default: return false;
	}
}

Press FromKey(uint32_t key) {
	return key == YesKey ? Press::Yes : key == NoKey ? Press::No : key == NeverKey ? Press::Never : Press::Other;
}

// One update's pad presses: one answer button alone is that answer; anything else, Other.
Press FromPad(const InputDirection::PadPresses& presses) {
	if (presses.moved) return Press::Other;
	switch (presses.buttons) {
	case YesButton: return Press::Yes;
	case NoButton: return Press::No;
	case NeverButton: return Press::Never;
	default: return Press::Other;
	}
}

// The tutorial's message sound, then the question (the user, 2026-11-02: a sound as it comes).
void Ask(uint64_t now) {
	ScreenReader::Stop();
	ItemAudio::PlayTutorialMessage();
	g_sayAt = now + Tutorial::ChimeMs;
}

void Heard(Reply reply, const D2RL::PluginContext* context, uint64_t now) {
	if (reply == Reply::None) return;
	if (reply == Reply::Again) {
		AsyncLog::Info(context, "[tutorial] the question asked again");
		Ask(now);
		return;
	}
	ScreenReader::Stop();
	ItemAudio::PlayTutorialStep();
	Close();
	g_answer = reply;
	AsyncLog::InfoF(context, "[tutorial] the question answered: %s",
		reply == Reply::Yes ? "yes" : reply == Reply::No ? "not now" : "never ask again");
}
}

void Open(std::string question) {
	g_question = std::move(question);
	g_offer.Start(GetDoubleClickTime());
	g_open = true;
	g_answer = Reply::None;
	(void)InputDirection::TakePadPresses();
	(void)MessageHold::TakePresses();
	Ask(GetTickCount64());
}

bool IsOpen() { return g_open; }

void Tick(const D2RL::PluginContext* context) {
	if (!g_open) return;
	const uint64_t now = GetTickCount64();
	PadFilter::HoldAll(now);
	MessageHold::Hold(context, now);
	const auto pad = InputDirection::TakePadPresses();
	InputDirection::DropGestures();
	for (const uint32_t key : MessageHold::TakePresses()) {
		if (Ignored(key)) continue;
		Heard(g_offer.Feed(FromKey(key), now), context, now);
		if (!g_open) return;
	}
	if (pad.buttons || pad.moved) Heard(g_offer.Feed(FromPad(pad), now), context, now);
	if (g_open) Heard(g_offer.Tick(now), context, now);
	if (g_open && g_sayAt && now >= g_sayAt) {
		ScreenReader::Speak(g_question, true);
		g_sayAt = 0;
	}
}

Reply TakeAnswer() { return std::exchange(g_answer, Reply::None); }

void Close() {
	g_open = false;
	g_sayAt = 0;
	g_answer = Reply::None;
	PadFilter::ReleaseAll();
	MessageHold::Release();
}

void Remove() {
	Close();
	MessageHold::Remove();
}
}
