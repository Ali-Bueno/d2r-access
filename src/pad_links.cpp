#include "pad_links.h"

#include <algorithm>
#include <cmath>

namespace D2RAccess::LevelRoute {
namespace {
Radar::Position At(const Pad& pad) { return {static_cast<double>(pad.x), static_cast<double>(pad.y)}; }
}

bool Pads::Note(const Pad& pad) {
	if (!pad.padClass || std::find(pads_.begin(), pads_.end(), pad) != pads_.end()) return false;
	pads_.push_back(pad);
	++revision_;
	return true;
}

bool Pads::Used(const Pad& from, Radar::Position landing, Pad& to) {
	const Pad* nearest = nullptr;
	double best = LandNear;
	for (const auto& pad : pads_) {
		if (pad == from) continue;
		const double distance = std::hypot(pad.x - landing.x, pad.y - landing.y);
		if (distance <= best) { best = distance; nearest = &pad; }
	}
	if (!nearest) return false;
	to = *nearest;
	(void)Note(from);
	for (const auto& pad : {from, to})
		if (!Been(pad)) been_.push_back(pad);
	const auto known = std::find_if(used_.begin(), used_.end(), [&](const Link& link) { return link.from == from; });
	if (known == used_.end()) used_.push_back({from, to, true});
	else if (known->to == to) return true;
	else known->to = to;
	++revision_;
	return true;
}

bool Pads::Stood(Radar::Position player) {
	bool added = false;
	for (const auto& pad : pads_) {
		if (std::hypot(pad.x - player.x, pad.y - player.y) > LandNear || Been(pad)) continue;
		been_.push_back(pad);
		added = true;
	}
	return added;
}

bool Pads::Been(const Pad& pad) const { return std::find(been_.begin(), been_.end(), pad) != been_.end(); }

std::optional<Pad> Pads::Nearest(Radar::Position at, double within) const {
	std::optional<Pad> nearest;
	double best = within;
	for (const auto& pad : pads_) {
		const double distance = std::hypot(pad.x - at.x, pad.y - at.y);
		if (distance <= best) { best = distance; nearest = pad; }
	}
	return nearest;
}

std::optional<Link> Pads::Lead(const Pad& from, const std::vector<NativeRooms::Room>& rooms) const {
	const auto seen = std::find_if(used_.begin(), used_.end(), [&](const Link& link) { return link.from == from; });
	if (seen != used_.end()) return *seen;
	const size_t room = NativeRooms::Containing(rooms, At(from));
	if (room == NativeRooms::None) return std::nullopt;
	const auto bounds = NativeRooms::Subtiles(rooms[room]);
	const Pad* other = nullptr;
	for (const auto& pad : pads_) {
		if (pad == from || pad.padClass != from.padClass || !NativeRooms::Contains(bounds, pad.x, pad.y)) continue;
		if (other) return std::nullopt; // more than one: the game's pick depends on an order not read here
		other = &pad;
	}
	if (!other) return std::nullopt;
	return Link{from, *other, false};
}

Destination Pads::Leads(const Pad& from, const std::vector<NativeRooms::Room>& rooms) const {
	if (const auto link = Lead(from, rooms)) return Been(link->to) ? Destination::Been : Destination::New;
	const size_t room = NativeRooms::Containing(rooms, At(from));
	if (room == NativeRooms::None) return Destination::Unknown;
	const auto bounds = NativeRooms::Subtiles(rooms[room]);
	const bool other = std::any_of(pads_.begin(), pads_.end(), [&](const Pad& pad) {
		return !(pad == from) && pad.padClass == from.padClass && NativeRooms::Contains(bounds, pad.x, pad.y); });
	return other ? Destination::Unknown : Destination::New;
}

std::vector<Link> Pads::Links(const std::vector<NativeRooms::Room>& rooms) const {
	std::vector<Link> links;
	for (const auto& pad : pads_)
		if (const auto link = Lead(pad, rooms)) links.push_back(*link);
	return links;
}
}
