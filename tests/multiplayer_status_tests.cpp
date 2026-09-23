#include "multiplayer_status.h"
#include "test_support.h"

int main() {
	using namespace D2RAccess::MultiplayerStatus;
	Reader reader;
	Snapshot host{"Hosting", "Issues", {}, {}};
	Check(reader.Update(host, 0).empty(), "host status waits for stable text");
	Check(reader.Update(host, 120) == std::vector<std::string>{"Hosting. Issues"}, "host status reads on entry");
	Check(reader.Update(host, 300).empty(), "stable status does not repeat");
	host.hostingValue = "Ready";
	Check(reader.Update(host, 400).empty(), "status change waits for settling");
	Check(reader.Update(host, 520) == std::vector<std::string>{"Hosting. Ready"}, "changed status reads once");
	host.browseEmpty = "No games are being advertised yet.";
	Check(reader.Update(host, 600).empty(), "browser empty state waits for settling");
	Check(reader.Update(host, 720) == std::vector<std::string>{host.browseEmpty}, "visible empty state is read");
	host.browseEmpty.clear();
	host.selectedDetails = {"Game Details", "Another game", "2 of 8 Players"};
	Check(reader.Update(host, 800).empty(), "selection change waits for settling");
	Check(reader.Update(host, 920) == std::vector<std::string>{"Game Details. Another game. 2 of 8 Players"}, "selected details are read together");
	host.selectedDetails[2] = "3 of 8 Players";
	Check(reader.Update(host, 1000).empty(), "changing details are not read immediately");
	host.selectedDetails[2] = "4 of 8 Players";
	Check(reader.Update(host, 1080).empty(), "rapid detail changes restart settling");
	Check(reader.Update(host, 1200) == std::vector<std::string>{"Game Details. Another game. 4 of 8 Players"}, "latest settled detail wins");
	reader.Reset();
	Check(reader.Update(host, 1300).empty(), "reset clears pending state");
	Check(reader.Update(host, 1420).size() == 2, "reopened panel reads current status and selection");
	Passed();
}
