#include "settings_store.h"
#include "test_support.h"

using namespace D2RAccess;

int main() {
	const std::map<std::string, int> values{{"radar", 0}, {"auto-detail", 1}, {"mana-alerts", 2}};
	Check(SettingsStore::Decode(SettingsStore::Encode(values)) == values, "what is saved reads back the same");
	const auto read = SettingsStore::Decode("radar=0\r\nbroken\n=1\nfootsteps=x\nvitals=1\npan=4\nminus=-1\nlong=1234\nempty=");
	Check(read.size() == 3 && read.at("radar") == 0 && read.at("vitals") == 1 && read.at("pan") == 4,
		"Windows line ends read; settings keep their level; lines that are not a value are passed over");
	Check(SettingsStore::Decode("").empty(), "an empty file holds nothing");
	Passed();
	return 0;
}
