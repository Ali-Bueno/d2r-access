#include "settings_store.h"
#include "test_support.h"

using namespace D2RAccess;

int main() {
	const std::map<std::string, bool> values{{"radar", false}, {"auto-detail", true}};
	Check(SettingsStore::Decode(SettingsStore::Encode(values)) == values, "what is saved reads back the same");
	const auto read = SettingsStore::Decode("radar=0\r\nbroken\n=1\nfootsteps=2\nvitals=1");
	Check(read.size() == 2 && !read.at("radar") && read.at("vitals"), "Windows line ends read; lines that are not a switch are passed over");
	Check(SettingsStore::Decode("").empty(), "an empty file holds nothing");
	Passed();
	return 0;
}
