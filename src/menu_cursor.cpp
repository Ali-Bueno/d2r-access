#include "menu_cursor.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <iterator>
#include <tuple>

namespace D2RAccess::MenuCursor {
namespace {
constexpr Direction All[] = {Direction::Left, Direction::Right, Direction::Up, Direction::Down};

bool IsGrid(const Target& target) { return target.grid.width > 0 && target.grid.height > 0; }
bool Drawn(const Target& target) { return target.widget && target.rect.width > 0 && target.rect.height > 0; }
bool Horizontal(Direction direction) { return direction == Direction::Left || direction == Direction::Right; }
Direction Opposite(Direction direction) {
	switch (direction) {
	case Direction::Left:  return Direction::Right;
	case Direction::Right: return Direction::Left;
	case Direction::Up:    return Direction::Down;
	default:               return Direction::Up;
	}
}

// The space between two spans of one axis, 0 where they overlap or touch.
int64_t Space(int64_t firstStart, int64_t firstLength, int64_t secondStart, int64_t secondLength) {
	return std::max<int64_t>({0, secondStart - (firstStart + firstLength), firstStart - (secondStart + secondLength)});
}

// How far `to` lies from `from` one way: `ahead` is how far its middle is
// that way, in half units and below 1 for what lies behind; `along` is the
// space between the two that way, `across` the space between them to the side, and
// `off` how far their near edges are apart to the side: a column, whatever a heading across it.
struct Distance { int64_t ahead = 0, along = 0, across = 0, off = 0; };
Distance Between(const Rect& from, const Rect& to, Direction direction) {
	const int64_t x = (2LL * to.x + to.width) - (2LL * from.x + from.width);
	const int64_t y = (2LL * to.y + to.height) - (2LL * from.y + from.height);
	const int64_t wide = Space(from.x, from.width, to.x, to.width), high = Space(from.y, from.height, to.y, to.height);
	switch (direction) {
	case Direction::Left:  return {-x, wide, high, std::llabs(static_cast<int64_t>(to.y) - from.y)};
	case Direction::Right: return {x, wide, high, std::llabs(static_cast<int64_t>(to.y) - from.y)};
	case Direction::Up:    return {-y, high, wide, std::llabs(static_cast<int64_t>(to.x) - from.x)};
	default:               return {y, high, wide, std::llabs(static_cast<int64_t>(to.x) - from.x)};
	}
}

bool Framed(const Target& target) { return target.frame.width > 0 && target.frame.height > 0; }

// The nearest thing one way from `origin`, or null. What lies more to one side
// than that way comes after everything that does not, and of those a thing of
// another group after the things of the group the step is made from. A row out
// of view is reached along its own list only; a step out of a list is made from
// the row in view, or from the frame for one that is not, and only straight out.
const Target* Nearest(std::span<const Target> targets, const Target* current, const Rect& origin, Direction direction) {
	const Target* best = nullptr;
	std::tuple<bool, bool, int64_t, int64_t, int64_t, int64_t, int, int> least;
	for (const auto& target : targets) {
		if (&target == current || !Drawn(target)) continue;
		const bool kin = current->group != 0 && target.group == current->group;
		if (!kin && Beyond(target) != 0) continue;
		const auto distance = Between(!kin && Beyond(*current) != 0 ? current->frame : origin, target.rect, direction);
		if (distance.ahead <= 0) continue;
		if (!kin && Framed(*current) && distance.across > distance.along) continue;
		// A list is entered at its nearest row, however far to one side the step came from.
		const bool aside = distance.across > distance.along && (kin || !Framed(target));
		// Only a step from a thing of a group keeps to its group: from a tab the skill below comes first.
		const std::tuple score{aside, current->group != 0 && target.group != current->group,
			distance.along * distance.along + distance.across * distance.across, distance.across, distance.off, distance.ahead,
			target.rect.y, target.rect.x};
		if (!best || score < least) { best = &target; least = score; }
	}
	return best;
}
bool Inside(const GridJump::Grid& grid, Cell cell) {
	return cell.x >= 0 && cell.y >= 0 && cell.x < grid.width && cell.y < grid.height;
}

// What stands on a cell: the item covering it from its top-left cell, or the cell alone.
Place OnCell(const Target& target, Cell cell) {
	const int item = GridJump::ItemAt(target.grid, cell);
	if (item == GridJump::Empty) return {target.widget, cell, cell};
	const auto& found = target.grid.items[static_cast<size_t>(item)];
	return {target.widget, {found.x, found.y}, cell};
}

// A span of cells along one axis of a grid, in the units the grid is drawn in.
int Edge(int origin, int length, int cells, int cell) {
	return origin + static_cast<int>(static_cast<int64_t>(length) * cell / cells);
}

// The cell of the grid a step from `origin` enters it at: on the near edge,
// as close to where the step came from as the grid has.
Cell Entry(const Target& target, const Rect& origin, Direction direction) {
	const auto& grid = target.grid;
	const auto nearest = [](int centre, int start, int length, int cells) {
		if (length <= 0) return 0;
		const int64_t cell = (static_cast<int64_t>(centre) - start) * cells / length;
		return static_cast<int>(std::clamp<int64_t>(cell, 0, cells - 1));
	};
	Cell cell{nearest(origin.x + origin.width / 2, target.rect.x, target.rect.width, grid.width),
		nearest(origin.y + origin.height / 2, target.rect.y, target.rect.height, grid.height)};
	switch (direction) {
	case Direction::Left:  cell.x = grid.width - 1; break;
	case Direction::Right: cell.x = 0; break;
	case Direction::Up:    cell.y = grid.height - 1; break;
	case Direction::Down:  cell.y = 0; break;
	}
	return cell;
}

// The place as the one thing it is, whatever cell of it was travelled through.
Place Anchored(const Place& place) { return {place.widget, place.anchor, place.anchor}; }
bool Known(const std::vector<Place>& places, const Place& place) {
	return std::find(places.begin(), places.end(), Anchored(place)) != places.end();
}
}

const Target* Find(std::span<const Target> targets, uintptr_t widget) {
	const auto found = std::find_if(targets.begin(), targets.end(), [widget](const Target& target) {
		return widget && target.widget == widget;
	});
	return found == targets.end() ? nullptr : &*found;
}

Rect RectOf(const Target& target, const Place& place) {
	if (!IsGrid(target) || !Inside(target.grid, place.anchor)) return target.rect;
	int width = 1, height = 1;
	if (const int item = GridJump::ItemAt(target.grid, place.anchor); item != GridJump::Empty) {
		const auto& found = target.grid.items[static_cast<size_t>(item)];
		if (found.x == place.anchor.x && found.y == place.anchor.y) {
			width = std::min(found.width, target.grid.width - found.x);
			height = std::min(found.height, target.grid.height - found.y);
		}
	}
	const int left = Edge(target.rect.x, target.rect.width, target.grid.width, place.anchor.x);
	const int top = Edge(target.rect.y, target.rect.height, target.grid.height, place.anchor.y);
	const int right = Edge(target.rect.x, target.rect.width, target.grid.width, place.anchor.x + width);
	const int bottom = Edge(target.rect.y, target.rect.height, target.grid.height, place.anchor.y + height);
	return {left, top, right - left, bottom - top};
}

Place Settle(std::span<const Target> targets, const Place& place) {
	const auto* target = Find(targets, place.widget);
	if (!target) return {};
	if (!IsGrid(*target)) return {target->widget, {}, {}};
	const Cell cell{std::clamp(place.travel.x, 0, target->grid.width - 1), std::clamp(place.travel.y, 0, target->grid.height - 1)};
	return OnCell(*target, cell);
}

int Beyond(const Target& target) {
	if (!Framed(target)) return 0;
	const int middle = target.rect.y + target.rect.height / 2;
	if (middle < target.frame.y) return middle - target.frame.y;
	const int end = target.frame.y + target.frame.height;
	return middle >= end ? middle - end + 1 : 0;
}

Place First(std::span<const Target> targets) {
	const Target* first = nullptr;
	for (const auto& target : targets) {
		if (!Drawn(target) || Beyond(target) != 0) continue;
		const bool before = !first || std::tuple(target.aside, target.rect.y, target.rect.x) < std::tuple(first->aside, first->rect.y, first->rect.x);
		if (before) first = &target;
	}
	if (!first) return {};
	return IsGrid(*first) ? OnCell(*first, {0, 0}) : Place{first->widget, {}, {}};
}

Place OnRow(std::span<const Target> targets, const Rect& rect) {
	const Target* best = nullptr;
	std::pair<int, int> least;
	for (const auto& target : targets) {
		if (!Drawn(target) || target.aside) continue;
		const std::pair score{std::abs(target.rect.y + target.rect.height / 2 - (rect.y + rect.height / 2)),
			std::abs(target.rect.x + target.rect.width / 2 - (rect.x + rect.width / 2))};
		if (!best || score < least) { best = &target; least = score; }
	}
	return best ? Settle(targets, Place{best->widget, {}, {}}) : Place{};
}

Place Move(std::span<const Target> targets, const Place& from, Direction direction, bool byCell, std::span<const Link> links) {
	const auto* current = Find(targets, from.widget);
	if (!current) return First(targets);
	for (auto link = links.rbegin(); link != links.rend(); ++link)
		if (link->direction == direction && link->from.widget == from.widget && link->from.anchor == from.anchor
			&& Find(targets, link->to.widget)) return link->to;
	if (IsGrid(*current) && Inside(current->grid, from.travel)) {
		if (byCell) {
			Cell next = from.travel;
			switch (direction) {
			case Direction::Left:  --next.x; break;
			case Direction::Right: ++next.x; break;
			case Direction::Up:    --next.y; break;
			case Direction::Down:  ++next.y; break;
			}
			if (Inside(current->grid, next)) return {current->widget, next, next};
		} else if (const auto step = GridJump::Move(current->grid, from.travel, direction); step.moved) {
			return {current->widget, step.anchor, step.cursor};
		}
	}
	// Nothing further in this grid, or not in one: the nearest thing that way.
	const Rect origin = byCell && IsGrid(*current)
		? RectOf(*current, {current->widget, from.travel, from.travel}) : RectOf(*current, from);
	const auto* chosen = Nearest(targets, current, origin, direction);
	if (!chosen) return from;
	if (!IsGrid(*chosen)) return {chosen->widget, {}, {}};
	const Cell cell = Entry(*chosen, origin, direction);
	return byCell ? Place{chosen->widget, cell, cell} : OnCell(*chosen, cell);
}

std::vector<Place> Everything(std::span<const Target> targets) {
	std::vector<Place> places;
	for (const auto& target : targets) {
		if (!target.widget || target.rect.width <= 0 || target.rect.height <= 0) continue;
		if (!IsGrid(target)) { places.push_back({target.widget, {}, {}}); continue; }
		for (int y = 0; y < target.grid.height; ++y) {
			for (int x = 0; x < target.grid.width; ++x) {
				const auto place = Anchored(OnCell(target, {x, y}));
				if (!Known(places, place)) places.push_back(place);
			}
		}
	}
	return places;
}

std::vector<Place> Reachable(std::span<const Target> targets, const Place& from, std::span<const Link> links) {
	std::vector<Place> met, seen, queue;
	if (!Find(targets, from.widget)) return met;
	met.push_back(Anchored(from));
	seen.push_back(from);
	queue.push_back(from);
	for (size_t head = 0; head < queue.size(); ++head) {
		for (const auto direction : All) {
			const auto next = Move(targets, Place(queue[head]), direction, false, links);
			// A cell travelled through is a state of its own: the same item
			// entered on another row goes on along that row.
			if (std::find(seen.begin(), seen.end(), next) != seen.end()) continue;
			seen.push_back(next);
			queue.push_back(next);
			if (!Known(met, next)) met.push_back(Anchored(next));
		}
	}
	return met;
}

namespace {
// Every place, and after them every other cell travelled through, with where
// each of the four steps leads from it.
struct Steps {
	std::vector<Place> states;
	std::vector<std::array<size_t, 4>> next;
};
Steps Walk(std::span<const Target> targets, const std::vector<Place>& all, std::span<const Link> links) {
	Steps steps{all, {}};
	for (size_t at = 0; at < steps.states.size(); ++at) {
		std::array<size_t, 4> leads{};
		for (size_t way = 0; way < std::size(All); ++way) {
			const auto to = Move(targets, Place(steps.states[at]), All[way], false, links);
			auto known = std::find(steps.states.begin(), steps.states.end(), to);
			if (known == steps.states.end()) known = steps.states.insert(steps.states.end(), to);
			leads[way] = static_cast<size_t>(known - steps.states.begin());
		}
		steps.next.push_back(leads);
	}
	return steps;
}

// The places the steps lead to from `first`, and the places they lead back to it from.
void Reach(const Steps& steps, const std::vector<Place>& all, size_t first, std::vector<bool>& reached, std::vector<bool>& returns) {
	const size_t count = steps.states.size();
	const auto place = [&](size_t state) {
		return static_cast<size_t>(std::find(all.begin(), all.end(), Anchored(steps.states[state])) - all.begin());
	};
	std::vector<bool> out(count, false), back(count, false);
	std::vector<size_t> queue{first};
	out[first] = true;
	for (size_t head = 0; head < queue.size(); ++head)
		for (const size_t to : steps.next[queue[head]]) if (!out[to]) { out[to] = true; queue.push_back(to); }
	// To be on the place is to be back, whatever cell of it was travelled through.
	queue.clear();
	for (size_t state = 0; state < count; ++state) if (place(state) == first) { back[state] = true; queue.push_back(state); }
	for (size_t head = 0; head < queue.size(); ++head)
		for (size_t from = 0; from < count; ++from) {
			if (back[from] || std::find(steps.next[from].begin(), steps.next[from].end(), queue[head]) == steps.next[from].end()) continue;
			back[from] = true;
			queue.push_back(from);
		}
	reached.assign(all.size(), false);
	returns.assign(all.size(), false);
	for (size_t state = 0; state < count; ++state) if (out[state]) reached[place(state)] = true;
	for (size_t state = 0; state < all.size(); ++state) returns[state] = back[state];
}
}

std::vector<Link> Mend(std::span<const Target> targets) {
	std::vector<Link> links;
	const auto all = Everything(targets);
	const auto entered = std::find(all.begin(), all.end(), Anchored(First(targets)));
	if (entered == all.end()) return links;
	const size_t first = static_cast<size_t>(entered - all.begin());
	std::vector<Rect> drawn;
	for (const auto& place : all) drawn.push_back(RectOf(*Find(targets, place.widget), place));
	std::vector<bool> reached, returns;
	// A round mends one place. One mended may cost another its only step, which
	// a later round mends, so there are more rounds than places to be had.
	for (size_t round = 0; round < 4 * all.size(); ++round) {
		Reach(Walk(targets, all, links), all, first, reached, returns);
		// A place missed is mended before one that does not lead back.
		const bool missing = std::find(reached.begin(), reached.end(), false) != reached.end();
		const auto& whole = missing ? reached : returns;
		size_t apart = all.size(), beside = all.size();
		int64_t least = 0;
		for (size_t lone = 0; lone < all.size(); ++lone) {
			if (whole[lone]) continue;
			for (size_t near = 0; near < all.size(); ++near) {
				if (!whole[near]) continue;
				const int64_t wide = Space(drawn[near].x, drawn[near].width, drawn[lone].x, drawn[lone].width);
				const int64_t high = Space(drawn[near].y, drawn[near].height, drawn[lone].y, drawn[lone].height);
				const int64_t distance = wide * wide + high * high;
				if (apart != all.size() && distance >= least) continue;
				apart = lone;
				beside = near;
				least = distance;
			}
		}
		if (apart == all.size()) break;
		// The step is made from the place that is reached to the one that is
		// not, or from the one that does not lead back to one that does.
		const size_t from = missing ? beside : apart, to = missing ? apart : beside;
		const int64_t x = (2LL * drawn[to].x + drawn[to].width) - (2LL * drawn[from].x + drawn[from].width);
		const int64_t y = (2LL * drawn[to].y + drawn[to].height) - (2LL * drawn[from].y + drawn[from].height);
		const Direction direction = std::llabs(x) >= std::llabs(y) ? (x >= 0 ? Direction::Right : Direction::Left)
			: (y >= 0 ? Direction::Down : Direction::Up);
		const Place on = Move(targets, all[from], direction, false, links);
		links.push_back({all[from], all[to], direction});
		if (!missing) continue;
		if (Anchored(on) != all[from] && Anchored(on) != all[to]) links.push_back({all[to], on, direction});
		links.push_back({all[to], all[from], Opposite(direction)});
	}
	return links;
}
}
