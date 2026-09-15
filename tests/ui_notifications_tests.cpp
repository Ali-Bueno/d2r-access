#include "ui_notifications.h"
#include "test_support.h"

int main() {
	using namespace D2RAccess;
	using namespace UiText;
	const std::string warning = "Solo disponible en partidas de clásico y de expansión";
	Snapshot feed{1, 0, "HUDMessagePanel", Surface::NotificationFeed, {
		{1, -1, Kind::Other, "Root", "HUDMessagePanel", ""},
		{2, 0, Kind::NotificationList, "Messages", "ChatListWidget", ""},
		{3, 1, Kind::ListItem, "ChatEntryTemplate", "ListItemWidget", ""},
		{4, 2, Kind::LinkedText, "MessageText", "TextBoxWithLinksWidget", "ÿc1" + warning},
		{5, 0, Kind::List, "History", "ChatListWidget", ""},
		{6, 4, Kind::LinkedText, "MessageText", "TextBoxWithLinksWidget", "Old chat history"},
		{7, 0, Kind::Text, "Unrelated", "TextBoxWidget", "Unrelated HUD text"},
	}};
	auto messages = UiNotifications::Describe(feed);
	Check(messages == std::vector<std::string>{warning}, "reads notification rows despite interactive list ancestry; excludes history and unrelated HUD");
	feed.nodes.push_back({8, 2, Kind::Text, "Copy", "TextBoxWidget", warning});
	Check(UiNotifications::Describe(feed) == messages, "duplicate embedded text is spoken once");
	feed.nodes[1].kind = Kind::List;
	Check(UiNotifications::Describe(feed).empty(), "an ordinary list never becomes a notification feed");
	feed.nodes[1].kind = Kind::NotificationList;
	feed.nodes[2].parent = 9999;
	Check(UiNotifications::Describe(feed).empty(), "invalid ancestor indices are bounded");
	Snapshot notice{10, 12, "AnyNotice", Surface::Notice, {
		{10, -1, Kind::Other, "Root", "NotificationPanel", ""},
		{11, 0, Kind::Text, "Prompt", "TextBoxWidget", "An informative notice"},
		{12, 0, Kind::Button, "Confirm", "ButtonWidget", "Accept"},
	}};
	Check(UiNotifications::Describe(notice) == std::vector<std::string>{"An informative notice"}, "temporary notice reads prompt without enumerating buttons");
	UiNotifications::Reader reader;
	Check(reader.Update(messages, 0).empty(), "waits for notification text to settle");
	Check(reader.Update(messages, 80) == messages, "stable notification is delivered");
	Check(reader.Update(messages, 400).empty(), "persistent notification remains silent");
	Check(reader.Repeat() == warning, "current notification can be repeated");
	messages.push_back("A second message");
	(void)reader.Update(messages, 500);
	Check(reader.Update(messages, 580) == std::vector<std::string>{"A second message"}, "only newly added text is read");
	std::swap(messages[0], messages[1]);
	Check(reader.Update(messages, 650).empty(), "recycled row order does not repeat old text");
	messages.pop_back();
	Check(reader.Update(messages, 700).empty(), "expiration of an older message is silent");
	(void)reader.Update({}, 800);
	Check(reader.Repeat().empty(), "disappeared notices cannot be repeated as current");
	messages = {warning, warning};
	(void)reader.Update(messages, 900);
	Check(reader.Update(messages, 980) == std::vector<std::string>{warning}, "same warning reads again after disappearing, once for duplicate rows");
	reader.Reset();
	Check(reader.Repeat().empty() && reader.Update(messages, 1000).empty(), "session reset clears pending and spoken notices");
	(void)reader.Update({"partial"}, 1010);
	Check(reader.Update({"complete"}, 1080).empty(), "partially populated text is not spoken");
	Check(reader.Update({"complete"}, 1160) == std::vector<std::string>{"complete"}, "complete replacement text is delivered");

	feed.nodes[1].text = "ÿc1" + warning;
	feed.nodes[1].contentId = 700;
	const auto nativeDeliveries = UiNotifications::Deliveries(feed);
	Check(nativeDeliveries.size() == 1 && nativeDeliveries[0].source == 2 && nativeDeliveries[0].id == 700 && nativeDeliveries[0].text == warning, "delivery identity comes from the source list and native message, with colors removed");
	feed.nodes[1].kind = Kind::List;
	Check(UiNotifications::Deliveries(feed).empty(), "chat history cannot rearm notification speech");
	reader.Reset();
	messages = {warning, "An older notice"};
	std::vector<UiNotifications::Delivery> deliveries{{2, 100, warning}};
	(void)reader.Update(messages, 2000, deliveries);
	Check(reader.Update(messages, 2080, deliveries) == messages, "initial delivery reads visible text once");
	deliveries[0].id = 101;
	Check(reader.Update(messages, 2200, deliveries).empty(), "a second identical delivery settles before speech");
	Check(reader.Update(messages, 2280, deliveries) == std::vector<std::string>{warning}, "second delivery repeats only its own text, not older notices");
	Check(reader.Update(messages, 2400, deliveries).empty(), "unchanged message identity does not repeat between key presses");
	std::swap(messages[0], messages[1]);
	Check(reader.Update(messages, 2500, deliveries).empty(), "reordering recycled widgets does not create a delivery");
	deliveries[0].id = 100;
	(void)reader.Update(messages, 2600, deliveries);
	Check(reader.Update(messages, 2680, deliveries) == std::vector<std::string>{warning}, "a reused ring slot after intermediate deliveries still reads");
	deliveries[0] = {2, 102, "Invisible backing-store text"};
	Check(reader.Update(messages, 2800, deliveries).empty() && reader.Update(messages, 2880, deliveries).empty(), "a hidden or expired native message never repeats stale visible text");
	deliveries[0] = {2, 0, warning};
	Check(reader.Update(messages, 3000, deliveries).empty(), "unavailable native identity retains the text-only fallback");
	deliveries[0] = {2, 103, warning};
	(void)reader.Update(messages, 3100, deliveries);
	deliveries[0].id = 104;
	(void)reader.Update(messages, 3150, deliveries);
	Check(reader.Update(messages, 3230, deliveries) == std::vector<std::string>{warning}, "a sub-settling burst coalesces into its latest delivery");
	reader.Reset();
	Check(reader.Update(messages, 3300, deliveries).empty() && reader.Update(messages, 3380, deliveries) == messages, "session reset also clears delivery identities");
	Passed();
}
