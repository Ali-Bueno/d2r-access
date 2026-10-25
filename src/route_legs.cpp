#include "route_legs.h"

#include "entity_radar.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <functional>
#include <limits>
#include <numbers>
#include <queue>
#include <span>

namespace D2RAccess::RouteLegs {
namespace {
// Policy: a turn costs as much as walking this many units further, so of two
// ways nearly as long the one with fewer legs is said.
constexpr double TurnUnits = 3;
// Policy: legs keep within this many cells of the search's own way: they take
// the way it found, and the search stays small on a long one.
constexpr int CorridorCells = 8;
// Policy: a long way is laid out this many of its cells at a time, as the
// search grows faster than the ground it covers.
constexpr size_t StretchCells = 96;
// Policy: a stretch ends anywhere this near the way, so it turns no more than the ground asks.
constexpr int StretchSlack = CorridorCells / 2;
// Policy: a shut door is opened from any cell this near it.
constexpr int DoorReach = 1;
// Policy: past this many legs the rest is only counted; asked again on the way,
// the route starts from where the player stands.
constexpr size_t SpokenLegs = 6;

struct Cell {
	int x = 0, y = 0;
	bool operator==(const Cell&) const = default;
};
// The eight ways the stick and the arrows walk, as world steps (spoken_direction.h):
// north is the world's -X,-Y diagonal, and the stick's diagonals are the world axes.
constexpr std::array<Cell, 8> Ways{{{-1, -1}, {0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}}};
constexpr int NoWay = static_cast<int>(Ways.size()); // the way a walk heads before its first step
constexpr int Headings = NoWay + 1;

Cell CellAt(Position at) { return {static_cast<int>(std::lround(at.x)), static_cast<int>(std::lround(at.y))}; }
bool Diagonal(Cell way) { return way.x != 0 && way.y != 0; }
bool Contains(const End& end, Cell cell) {
	return cell.x >= end.minX && cell.x <= end.maxX && cell.y >= end.minY && cell.y <= end.maxY
		&& (!end.counts || end.counts(cell.x, cell.y));
}

// Whether walking on from `cell` the way `way` heads comes to a cell of `end`.
bool Ahead(Cell cell, Cell way, const End& end) {
	int low = 1, high = std::numeric_limits<int>::max();
	const auto along = [&low, &high](int at, int step, int min, int max) {
		if (step == 0) {
			if (at < min || at > max) high = 0;
			return;
		}
		const int a = (min - at) * step, b = (max - at) * step;
		low = std::max(low, std::min(a, b));
		high = std::min(high, std::max(a, b));
	};
	along(cell.x, way.x, end.minX, end.maxX);
	along(cell.y, way.y, end.minY, end.maxY);
	return low <= high;
}

// The ground round the search's own way that legs may cross, each cell asked
// of `passable` once, and numbered for the search.
class Corridor {
public:
	Corridor(std::span<const Cell> way, const End& end, const Passable& passable) : passable_(passable) {
		int minX = end.minX, minY = end.minY, maxX = end.maxX, maxY = end.maxY;
		for (const auto cell : way) {
			minX = std::min(minX, cell.x); minY = std::min(minY, cell.y);
			maxX = std::max(maxX, cell.x); maxY = std::max(maxY, cell.y);
		}
		x_ = minX - CorridorCells; y_ = minY - CorridorCells;
		width_ = maxX - minX + 1 + 2 * CorridorCells;
		height_ = maxY - minY + 1 + 2 * CorridorCells;
		marks_.assign(static_cast<size_t>(width_) * height_, Outside);
		for (const auto cell : way)
			for (int y = cell.y - CorridorCells; y <= cell.y + CorridorCells; ++y)
				std::fill_n(marks_.begin() + static_cast<std::ptrdiff_t>(Index({cell.x - CorridorCells, y})), 2 * CorridorCells + 1, Unasked);
		for (int y = end.minY; y <= end.maxY; ++y)
			std::fill_n(marks_.begin() + static_cast<std::ptrdiff_t>(Index({end.minX, y})), end.maxX - end.minX + 1, Unasked);
		ids_.assign(marks_.size(), -1);
		for (size_t i = 0; i < marks_.size(); ++i) {
			if (marks_[i] == Outside) continue;
			ids_[i] = static_cast<int32_t>(cells_.size());
			cells_.push_back({x_ + static_cast<int>(i % static_cast<size_t>(width_)), y_ + static_cast<int>(i / static_cast<size_t>(width_))});
		}
	}
	bool Open(Cell cell) {
		if (!Inside(cell)) return false;
		auto& mark = marks_[Index(cell)];
		if (mark == Unasked) mark = passable_(cell.x, cell.y) ? Clear : Shut;
		return mark == Clear;
	}
	// A cell's number: from 0 to Count() - 1 for a cell of the corridor, else -1.
	int32_t Id(Cell cell) const { return Inside(cell) ? ids_[Index(cell)] : -1; }
	Cell Of(int32_t id) const { return cells_[static_cast<size_t>(id)]; }
	size_t Count() const { return cells_.size(); }
private:
	enum Mark : uint8_t { Outside, Unasked, Clear, Shut };
	bool Inside(Cell cell) const { return cell.x >= x_ && cell.y >= y_ && cell.x < x_ + width_ && cell.y < y_ + height_; }
	size_t Index(Cell cell) const { return static_cast<size_t>(cell.y - y_) * width_ + static_cast<size_t>(cell.x - x_); }
	const Passable& passable_;
	int x_ = 0, y_ = 0, width_ = 0, height_ = 0;
	std::vector<uint8_t> marks_;
	std::vector<int32_t> ids_;
	std::vector<Cell> cells_;
};

// The steps from `start` to a cell of `end` other than `start`, each one of the
// eight ways, over open ground of the corridor: the fewest turns for the length
// (TurnUnits). With `corners`, a diagonal step needs the body to fit on both
// cells beside it, as at a wall's corner or a door frame, where the stick's
// diagonal walks into the frame; the way through a doorway is then along it.
// Empty when there is none.
std::vector<Cell> Search(Corridor& corridor, Cell start, const End& end, bool corners) {
	// The least length still to walk from each cell, turns and corners aside:
	// walked back from `end` over the same ground. The cell started from is
	// reached, never walked through.
	std::vector<double> rest(corridor.Count(), std::numeric_limits<double>::infinity());
	{
		using Entry = std::pair<double, int32_t>;
		std::priority_queue<Entry, std::vector<Entry>, std::greater<>> queue;
		for (int y = end.minY; y <= end.maxY; ++y)
			for (int x = end.minX; x <= end.maxX; ++x)
				if (corridor.Open({x, y})) queue.push({rest[static_cast<size_t>(corridor.Id({x, y}))] = 0, corridor.Id({x, y})});
		while (!queue.empty()) {
			const auto [walked, id] = queue.top();
			queue.pop();
			const Cell cell = corridor.Of(id);
			if (walked > rest[static_cast<size_t>(id)] || cell == start) continue;
			for (const auto step : Ways) {
				const Cell next{cell.x + step.x, cell.y + step.y};
				if (next != start && !corridor.Open(next)) continue;
				const double further = walked + (Diagonal(step) ? std::numbers::sqrt2 : 1.0);
				const auto at = static_cast<size_t>(corridor.Id(next));
				if (further >= rest[at]) continue;
				rest[at] = further;
				queue.push({further, static_cast<int32_t>(at)});
			}
		}
	}
	// What is still to walk at the least: that length, and a turn when the way
	// the walk heads misses `end`. Never more than the walk, so the first way to
	// `end` the search takes is the best.
	const auto estimate = [&end, &rest, &corridor](Cell cell, int heading) {
		const double length = rest[static_cast<size_t>(corridor.Id(cell))];
		return heading == NoWay || Ahead(cell, Ways[static_cast<size_t>(heading)], end) ? length : length + TurnUnits;
	};
	const size_t states = corridor.Count() * Headings;
	std::vector<double> cost(states, std::numeric_limits<double>::infinity());
	std::vector<uint32_t> from(states, 0);
	std::vector<uint8_t> done(states, 0);
	// Of two as good, the one walked further first: on open floor the search runs straight on.
	struct Entry {
		double estimate, walked;
		uint32_t state;
		bool operator>(const Entry& other) const {
			return estimate > other.estimate || (estimate == other.estimate && walked < other.walked);
		}
	};
	std::priority_queue<Entry, std::vector<Entry>, std::greater<>> queue;
	const auto first = static_cast<uint32_t>(corridor.Id(start) * Headings + NoWay);
	if (corridor.Id(start) < 0 || !std::isfinite(rest[static_cast<size_t>(corridor.Id(start))])) return {};
	cost[first] = 0;
	queue.push({estimate(start, NoWay), 0, first});
	while (!queue.empty()) {
		const uint32_t at = queue.top().state;
		queue.pop();
		if (done[at]) continue;
		done[at] = 1;
		const Cell cell = corridor.Of(static_cast<int32_t>(at / Headings));
		const int heading = static_cast<int>(at % Headings);
		if (cell != start && Contains(end, cell)) {
			std::vector<Cell> steps;
			for (uint32_t back = at; back != first; back = from[back]) steps.push_back(Ways[back % Headings]);
			std::reverse(steps.begin(), steps.end());
			return steps;
		}
		for (int way = 0; way < NoWay; ++way) {
			const Cell step = Ways[static_cast<size_t>(way)], next{cell.x + step.x, cell.y + step.y};
			if (!corridor.Open(next)) continue;
			if (corners && Diagonal(step) && (!corridor.Open({next.x, cell.y}) || !corridor.Open({cell.x, next.y}))) continue;
			const double walked = cost[at] + (Diagonal(step) ? std::numbers::sqrt2 : 1.0)
				+ (heading != NoWay && heading != way ? TurnUnits : 0.0);
			const auto to = static_cast<uint32_t>(corridor.Id(next) * Headings + way);
			if (done[to] || walked >= cost[to] || !std::isfinite(rest[static_cast<size_t>(corridor.Id(next))])) continue;
			cost[to] = walked;
			from[to] = at;
			queue.push({walked + estimate(next, way), walked, to});
		}
	}
	return {};
}

// The steps of `way` itself from `at`, one cell at a time: every step of the
// search's own way is one of the eight already.
std::vector<Cell> Retrace(Cell at, std::span<const Cell> way) {
	std::vector<Cell> steps;
	for (const auto cell : way) {
		while (at != cell) {
			const Cell step{std::clamp(cell.x - at.x, -1, 1), std::clamp(cell.y - at.y, -1, 1)};
			steps.push_back(step);
			at = {at.x + step.x, at.y + step.y};
		}
	}
	return steps;
}

// The steps from `at` to `end` over the ground round `way`, else along it.
std::vector<Cell> Steps(std::span<const Cell> way, Cell at, const End& end, const Passable& passable) {
	Corridor corridor(way, end, passable);
	auto steps = Search(corridor, at, end, true);
	// A way only the search's own diagonal steps get through, as past a pillar.
	if (steps.empty()) steps = Search(corridor, at, end, false);
	return steps.empty() ? Retrace(at, way) : steps;
}

// Steps in a row the same way are one leg.
void Join(const std::vector<Cell>& steps, std::vector<Leg>& legs) {
	for (size_t i = 0; i < steps.size();) {
		size_t run = i + 1;
		while (run < steps.size() && steps[run] == steps[i]) ++run;
		const auto count = static_cast<double>(run - i);
		legs.push_back({{steps[i].x * count, steps[i].y * count}, count * (Diagonal(steps[i]) ? std::numbers::sqrt2 : 1.0)});
		i = run;
	}
}

// From `at` to `end` as legs, the search's own way[first..last] being one way
// there; returns where they end. A long way is laid out a stretch at a time,
// each stretch's last leg again with the next, which sees where it turns.
Cell Walk(const std::vector<Cell>& way, size_t first, size_t last, Cell at, const End& end, const Passable& passable, std::vector<Leg>& legs) {
	const std::span<const Cell> all(way);
	std::vector<Cell> steps;
	while (last - first > StretchCells) {
		const size_t to = first + StretchCells;
		const Cell next = way[to];
		auto stretch = at == next ? std::vector<Cell>{} : Steps(all.subspan(first, to - first + 1), at, {next.x - StretchSlack, next.y - StretchSlack, next.x + StretchSlack, next.y + StretchSlack}, passable);
		size_t keep = stretch.size();
		while (keep > 0 && stretch[keep - 1] == stretch.back()) --keep;
		Cell corner = at, reached = at;
		for (size_t i = 0; i < stretch.size(); ++i) {
			reached = {reached.x + stretch[i].x, reached.y + stretch[i].y};
			if (i + 1 == keep) corner = reached;
		}
		// On from the cell of the way nearest that corner, if that is well along it.
		size_t from = first;
		for (size_t i = first, best = SIZE_MAX; i <= to; ++i) {
			const auto apart = static_cast<size_t>(std::max(std::abs(way[i].x - corner.x), std::abs(way[i].y - corner.y)));
			if (apart <= best) { best = apart; from = i; }
		}
		if (from < first + StretchCells / 2) { keep = stretch.size(); corner = reached; from = to; }
		steps.insert(steps.end(), stretch.begin(), stretch.begin() + static_cast<std::ptrdiff_t>(keep));
		at = corner;
		first = from;
	}
	const auto rest = Steps(all.subspan(first, last - first + 1), at, end, passable);
	steps.insert(steps.end(), rest.begin(), rest.end());
	for (const auto step : rest) at = {at.x + step.x, at.y + step.y};
	Join(steps, legs);
	return at;
}

// Neighbouring legs said in the same direction are one leg.
std::vector<Leg> Merge(const std::vector<Leg>& legs, SpokenDirection::Style style) {
	std::vector<Leg> merged;
	for (const auto& leg : legs) {
		auto* last = merged.empty() ? nullptr : &merged.back();
		if (last && !last->door && !leg.door
			&& SpokenDirection::Bucket(last->delta, style) == SpokenDirection::Bucket(leg.delta, style)) {
			last->delta = {last->delta.x + leg.delta.x, last->delta.y + leg.delta.y};
			last->length += leg.length;
		} else merged.push_back(leg);
	}
	return merged;
}

// How nearly two legs head the same way: the cosine between them.
double Along(const Leg& a, const Leg& b) {
	return (a.delta.x * b.delta.x + a.delta.y * b.delta.y) / (std::hypot(a.delta.x, a.delta.y) * std::hypot(b.delta.x, b.delta.y));
}

// Whether walking straight from `from` by `delta` crosses `ground` cells only.
bool Straight(Cell from, Position delta, const Passable& ground) {
	const int steps = std::max(1, static_cast<int>(std::ceil(std::hypot(delta.x, delta.y) * 2)));
	for (int i = 1; i <= steps; ++i) {
		const double t = static_cast<double>(i) / steps;
		const Cell cell = CellAt({from.x + delta.x * t, from.y + delta.y * t});
		if (!ground(cell.x, cell.y)) return false;
	}
	return true;
}

// Legs shorter than `minLeg`, shortest first, each folded into the neighbour heading most its
// way (never a door), which then runs straight from the one's start to the other's end. A fold
// whose straight line would cross a wall cell is not made: a pillar in Asheara's hut (a tester, 2026-10-24).
std::vector<Leg> Lengthen(std::vector<Leg> legs, double minLeg, SpokenDirection::Style style, Cell start, const Passable& ground) {
	std::vector<bool> kept(legs.size());
	for (;;) {
		const auto foldable = [&legs](size_t i) { return i < legs.size() && !legs[i].door; };
		size_t at = legs.size();
		for (size_t i = 0; i < legs.size(); ++i)
			if (!kept[i] && !legs[i].door && legs[i].length < minLeg && ((i > 0 && foldable(i - 1)) || foldable(i + 1))
				&& (at == legs.size() || legs[i].length < legs[at].length)) at = i;
		if (at == legs.size()) return legs;
		const auto fits = [&](size_t into) {
			Cell from = start;
			for (size_t i = 0; i < std::min(at, into); ++i)
				from = CellAt({from.x + legs[i].delta.x, from.y + legs[i].delta.y});
			return Straight(from, {legs[at].delta.x + legs[into].delta.x, legs[at].delta.y + legs[into].delta.y}, ground);
		};
		const bool before = at > 0 && foldable(at - 1), after = foldable(at + 1);
		const size_t preferred = !after || (before && Along(legs[at], legs[at - 1]) >= Along(legs[at], legs[at + 1])) ? at - 1 : at + 1;
		const size_t other = preferred < at ? (after ? at + 1 : legs.size()) : (before ? at - 1 : legs.size());
		size_t into = fits(preferred) ? preferred : other < legs.size() && fits(other) ? other : legs.size();
		if (into == legs.size()) {
			kept[at] = true;
			continue;
		}
		auto& leg = legs[into];
		leg.delta = {leg.delta.x + legs[at].delta.x, leg.delta.y + legs[at].delta.y};
		leg.length = std::hypot(leg.delta.x, leg.delta.y);
		legs.erase(legs.begin() + static_cast<std::ptrdiff_t>(at));
		if (into > at) --into;
		// Two legs that undo each other leave nothing to walk.
		if (legs[into].length < 1) legs.erase(legs.begin() + static_cast<std::ptrdiff_t>(into));
		legs = Merge(legs, style);
		kept.assign(legs.size(), false);
	}
}
}

std::vector<Leg> Describe(Position from, const std::vector<Position>& path, const std::vector<size_t>& doors,
	const Passable& passable, SpokenDirection::Style style, const End* end, double minLeg, const Passable& ground) {
	std::vector<Leg> legs;
	if (path.empty()) return legs;
	std::vector<Cell> way{CellAt(from)};
	for (const auto& point : path) way.push_back(CellAt(point));
	const End goal = end ? *end : End{way.back().x, way.back().y, way.back().x, way.back().y};
	Cell at = way.front();
	size_t first = 0, lastDoor = path.size();
	for (const size_t door : doors) {
		// A door is opened at the first of its cells on the way; the rest are crossed with it.
		const bool sameDoor = lastDoor != path.size() && door == lastDoor + 1;
		lastDoor = door;
		if (sameDoor || door >= path.size() || door + 1 <= first) continue;
		const Cell shut = way[door + 1];
		const End reach{shut.x - DoorReach, shut.y - DoorReach, shut.x + DoorReach, shut.y + DoorReach};
		if (!Contains(reach, at)) at = Walk(way, first, door + 1, at, reach, passable, legs);
		legs.push_back({{}, 0, true});
		first = door + 1;
	}
	// Opened from where the way already ends, a door is the last leg.
	if (first == 0 || !Contains(goal, at)) Walk(way, first, way.size() - 1, at, goal, passable, legs);
	return minLeg > 0 ? Lengthen(Merge(legs, style), minLeg, style, way.front(), ground ? ground : passable) : Merge(legs, style);
}

double Length(const std::vector<Leg>& legs) {
	double length = 0;
	for (const auto& leg : legs) length += leg.length;
	return length;
}

std::string Say(const Leg& leg, const Strings::Table& strings) {
	if (leg.door) return strings.routeDoor;
	char text[160]{};
	std::snprintf(text, sizeof text, strings.routeLeg, SpokenDirection::Of(leg.delta, strings), EntityRadar::SpokenCount(leg.length));
	return text;
}

std::string Speak(const std::string& name, const std::vector<Leg>& legs, bool unseen, Wording wording, const Strings::Table& strings) {
	std::string said;
	const size_t shown = wording.legs ? std::min(legs.size(), SpokenLegs) : 0;
	for (size_t i = 0; i < shown; ++i) {
		if (i) said += strings.routeThen;
		said += Say(legs[i], strings);
	}
	if (wording.legs && legs.size() > shown) {
		char more[96]{};
		std::snprintf(more, sizeof more, strings.routeMore, static_cast<int>(legs.size() - shown));
		said += strings.routeThen;
		said += more;
	}
	const auto length = EntityRadar::DistancePhrase(Length(legs), EntityRadar::Settings{}, strings);
	std::string text(name.size() + length.size() + said.size() + 128, '\0');
	int written = 0;
	if (wording.total && wording.legs) written = std::snprintf(text.data(), text.size(), strings.routeTo, name.c_str(), length.c_str(), said.c_str());
	else if (wording.legs) written = std::snprintf(text.data(), text.size(), strings.routeToLegs, name.c_str(), said.c_str());
	else if (wording.total) written = std::snprintf(text.data(), text.size(), strings.routeToLength, name.c_str(), length.c_str());
	else written = std::snprintf(text.data(), text.size(), strings.routeToName, name.c_str());
	text.resize(written > 0 ? std::min(static_cast<size_t>(written), text.size() - 1) : 0);
	if (unseen) {
		text += strings.terrainSeparator;
		text += strings.routeUnseen;
	}
	return text;
}
}
