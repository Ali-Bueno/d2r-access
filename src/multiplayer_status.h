#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace D2RAccess::MultiplayerStatus {

struct Snapshot {
	std::string hostingLabel;
	std::string hostingValue;
	std::string browseEmpty;
	std::vector<std::string> selectedDetails;
};

class Reader {
public:
	std::vector<std::string> Update(const Snapshot& snapshot, uint64_t now);
	void Reset();
private:
	Snapshot candidate_, spoken_;
	uint64_t candidateSince_ = 0;
	bool pending_ = false;
};

}
