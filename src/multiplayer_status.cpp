#include "multiplayer_status.h"

namespace D2RAccess::MultiplayerStatus {
namespace {
constexpr uint64_t SettleMs = 120;

bool Equal(const Snapshot& left, const Snapshot& right) {
	return left.hostingLabel == right.hostingLabel && left.hostingValue == right.hostingValue
		&& left.browseEmpty == right.browseEmpty && left.selectedDetails == right.selectedDetails;
}

std::string Join(const std::vector<std::string>& parts) {
	std::string result;
	for (const auto& part : parts) {
		if (part.empty()) continue;
		if (!result.empty()) result += ". ";
		result += part;
	}
	return result;
}
}

std::vector<std::string> Reader::Update(const Snapshot& snapshot, uint64_t now) {
	if (!Equal(snapshot, candidate_)) {
		candidate_ = snapshot;
		candidateSince_ = now;
		pending_ = true;
		return {};
	}
	if (!pending_ || now - candidateSince_ < SettleMs) return {};
	pending_ = false;
	std::vector<std::string> result;
	if (!candidate_.hostingValue.empty()
		&& (candidate_.hostingValue != spoken_.hostingValue || candidate_.hostingLabel != spoken_.hostingLabel)) {
		result.push_back(Join({candidate_.hostingLabel, candidate_.hostingValue}));
	}
	if (!candidate_.browseEmpty.empty() && candidate_.browseEmpty != spoken_.browseEmpty)
		result.push_back(candidate_.browseEmpty);
	if (!candidate_.selectedDetails.empty() && candidate_.selectedDetails != spoken_.selectedDetails)
		result.push_back(Join(candidate_.selectedDetails));
	spoken_ = candidate_;
	return result;
}

void Reader::Reset() {
	candidate_ = {};
	spoken_ = {};
	candidateSince_ = 0;
	pending_ = false;
}

}
