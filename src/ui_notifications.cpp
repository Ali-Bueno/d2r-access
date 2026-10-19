#include "ui_notifications.h"
#include "controller_text.h"

#include <algorithm>

namespace D2RAccess::UiNotifications {
namespace {
constexpr uint64_t SettleMs = 80;

bool InNotificationList(const UiText::Snapshot& snapshot, size_t index) {
	for (int parent = snapshot.nodes[index].parent; parent >= 0 && parent < static_cast<int>(index);) {
		const auto& node = snapshot.nodes[static_cast<size_t>(parent)];
		if (node.kind == UiText::Kind::NotificationList) return true;
		const int next = node.parent;
		if (next >= parent) break;
		parent = next;
	}
	return false;
}
}

std::vector<std::string> Describe(const UiText::Snapshot& snapshot) {
	if (snapshot.surface == UiText::Surface::Notice) {
		auto text = UiText::Describe(snapshot).body;
		return text.empty() ? std::vector<std::string>{} : std::vector<std::string>{std::move(text)};
	}
	std::vector<std::string> result;
	for (size_t i = 0; i < snapshot.nodes.size(); ++i) {
		const auto& node = snapshot.nodes[i];
		if (node.kind != UiText::Kind::Text && node.kind != UiText::Kind::LinkedText) continue;
		if (!InNotificationList(snapshot, i)) continue;
		// "You've leveled up. Press <Menu> to assign Stat Points." (a tester, 2026-10-19: buttons left out).
		auto text = ControllerText::Read(node.text);
		if (!text.empty() && std::find(result.begin(), result.end(), text) == result.end()) result.push_back(std::move(text));
	}
	return result;
}

std::vector<Delivery> Deliveries(const UiText::Snapshot& snapshot) {
	std::vector<Delivery> result;
	for (const auto& node : snapshot.nodes) {
		if (node.kind != UiText::Kind::NotificationList || !node.contentId) continue;
		auto text = ControllerText::Read(node.text);
		if (!text.empty()) result.push_back({node.id, node.contentId, std::move(text)});
	}
	return result;
}

std::vector<std::string> Reader::Update(const std::vector<std::string>& visible, uint64_t now, const std::vector<Delivery>& deliveries) {
	// Recycling/scrolling can move the same visible message to another object.
	// Diff by content, not address or row index; fading old rows must stay silent.
	std::erase_if(entries_, [&](const Entry& entry) {
		return std::find(visible.begin(), visible.end(), entry.text) == visible.end();
	});
	if (std::find(visible.begin(), visible.end(), last_) == visible.end()) last_.clear();
	for (const auto& delivery : deliveries) {
		// An expired/hidden message in the backing store must never be spoken.
		if (!delivery.source || !delivery.id || std::find(visible.begin(), visible.end(), delivery.text) == visible.end()) continue;
		auto previous = std::find_if(deliveries_.begin(), deliveries_.end(), [&](const Delivery& item) { return item.source == delivery.source; });
		if (previous == deliveries_.end()) deliveries_.push_back(delivery);
		else if (previous->id != delivery.id || previous->text != delivery.text) {
			*previous = delivery;
			// Only the newly delivered text is rearmed; older visible notices stay
			// silent. Ring slots may wrap, so compare with the last delivery only.
			for (auto& entry : entries_) if (entry.text == delivery.text) {
				entry.spoken = false;
				entry.since = now;
			}
		}
	}
	std::vector<std::string> result;
	for (const auto& text : visible) {
		if (text.empty()) continue;
		auto it = std::find_if(entries_.begin(), entries_.end(), [&](const Entry& entry) { return entry.text == text; });
		if (it == entries_.end()) entries_.push_back({text, now, false});
		else if (!it->spoken && now - it->since >= SettleMs) {
			it->spoken = true;
			last_ = text;
			result.push_back(text);
		}
	}
	return result;
}

void Reader::Reset() { entries_.clear(); deliveries_.clear(); last_.clear(); }
}
