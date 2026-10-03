#include "grid_jump.h"

namespace D2RAccess::GridJump {
namespace {
bool Covers(const Item& item, Cell cell) {
	return cell.x >= item.x && cell.y >= item.y && cell.x < item.x + item.width && cell.y < item.y + item.height;
}
// What stands on a cell, as a rectangle: the item covering it, or the cell alone.
Item Footprint(const Grid& grid, Cell cell) {
	const int item = ItemAt(grid, cell);
	return item == Empty ? Item{cell.x, cell.y, 1, 1} : grid.items[static_cast<size_t>(item)];
}
}

int ItemAt(const Grid& grid, Cell cell) {
	for (size_t i = 0; i < grid.items.size(); ++i)
		if (Covers(grid.items[i], cell)) return static_cast<int>(i);
	return Empty;
}

Step Move(const Grid& grid, Cell cursor, Direction direction) {
	Step step;
	step.cursor = cursor;
	const Item from = Footprint(grid, cursor);
	Cell next = cursor;
	switch (direction) {
	case Direction::Left:  next.x = from.x - 1; break;
	case Direction::Right: next.x = from.x + from.width; break;
	case Direction::Up:    next.y = from.y - 1; break;
	case Direction::Down:  next.y = from.y + from.height; break;
	}
	step.item = ItemAt(grid, cursor);
	step.anchor = {from.x, from.y};
	if (next.x < 0 || next.y < 0 || next.x >= grid.width || next.y >= grid.height) return step;
	const Item to = Footprint(grid, next);
	step.moved = true;
	step.cursor = next;
	step.item = ItemAt(grid, next);
	step.anchor = {to.x, to.y};
	return step;
}
}
