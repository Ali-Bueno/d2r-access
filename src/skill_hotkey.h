#pragma once

// When a keyboard skill hotkey is worth a word: only when the skill differs from the last one named.
// See docs/skill-hotkeys.md.
namespace D2RAccess::SkillHotkey {
class Tracker {
public:
	// True when `skill` is a real id other than the one announced last; it becomes the one announced.
	bool Changed(int skill) {
		if (skill < 0 || skill == m_last) return false;
		m_last = skill;
		return true;
	}
	void Reset() { m_last = -1; }

private:
	int m_last = -1;
};
}
