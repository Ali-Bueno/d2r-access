#include "passages.h"
#include "test_support.h"

#include <string_view>
#include <vector>

using namespace D2RAccess;
using Passages::End;
using Passages::Ground;
using Passages::Shape;

namespace {
// '#' wall, '?' ground never streamed, anything else floor; outside the plan is wall.
struct Plan {
	std::vector<std::string_view> rows;
	Ground operator()(int x, int y) const {
		if (y < 0 || y >= static_cast<int>(rows.size()) || x < 0 || x >= static_cast<int>(rows[static_cast<size_t>(y)].size()))
			return Ground::Blocked;
		const char c = rows[static_cast<size_t>(y)][static_cast<size_t>(x)];
		return c == '#' ? Ground::Blocked : c == '?' ? Ground::Unknown : Ground::Open;
	}
};
constexpr Passages::Settings Small{.minWidth = 2, .maxWidth = 3, .minLength = 5, .slack = 1, .reach = 30};

// A room opening into a corridor three wide that ends in a wall.
const Plan DeadEnd{{
	"##############",
	"#......#######",
	"#......#######",
	"#.............",
	"#.............",
	"#.............",
	"#......#######",
	"#......#######",
	"##############",
}};

void Corridors() {
	const auto in = Passages::Classify(DeadEnd, 10, 4, Small);
	Check(in.shape == Shape::Corridor && in.alongX, "three cells between two walls, eleven long, is a corridor along X");
	Check(in.ends[0].end == End::Opens, "its west end opens into the room");
	Check(in.ends[1].end == End::Closed || in.ends[1].end == End::Unknown, "its east end is the plan's edge");
	Check(Passages::Classify(DeadEnd, 3, 4, Small).shape == Shape::None, "the room is not a corridor, even beside its walls");
	Check(Passages::Classify(DeadEnd, 0, 0, Small).shape == Shape::None, "a wall is nothing");

	const Plan closed{{
		"#########",
		"#.......#",
		"#.......#",
		"#########",
	}};
	const auto shut = Passages::Classify(closed, 4, 1, Small);
	Check(shut.shape == Shape::Corridor && shut.DeadEnd(), "a corridor walled at both ends is a dead end");
	const Plan streaming{{
		"#########",
		"#.......?",
		"#.......?",
		"#########",
	}};
	const auto open = Passages::Classify(streaming, 4, 1, Small);
	Check(open.shape == Shape::Corridor && open.ends[1].end == End::Unknown,
		"ground not streamed yet is not a wall: that end is unknown, not closed");
	const Plan unproven{{
		"?????????",
		"#.......#",
		"#.......#",
		"#########",
	}};
	Check(Passages::Classify(unproven, 4, 1, Small).shape == Shape::None,
		"a side not streamed yet proves nothing about the width");
}

void Doorways() {
	const Plan door{{
		"###########",
		"#....#....#",
		"#....#....#",
		"#.........#",
		"#....#....#",
		"#....#....#",
		"###########",
	}};
	Check(Passages::Classify(door, 5, 3, Small).shape == Shape::None, "a doorway through a wall is not a corridor");
	const Plan slit{{
		"##########",
		"#........#",
		"##########",
		"#........#",
		"#........#",
		"#........#",
		"#........#",
		"##########",
	}};
	Check(Passages::Classify(slit, 4, 1, Small).shape == Shape::None, "a slit narrower than a corridor is not one");
	const Plan wide{{
		"############",
		"#..........#",
		"#..........#",
		"#..........#",
		"#..........#",
		"#..........#",
		"############",
	}};
	Check(Passages::Classify(wide, 5, 3, Small).shape == Shape::None, "a hall wider than a corridor is not one");
}

void Nodes() {
	const Plan cross{{
		"#####...#####",
		"#####...#####",
		"#####...#####",
		"#####...#####",
		"#####...#####",
		".............",
		".............",
		".............",
		"#####...#####",
		"#####...#####",
		"#####...#####",
		"#####...#####",
		"#####...#####",
	}};
	const auto middle = Passages::Classify(cross, 6, 6, Small);
	Check(middle.shape == Shape::Junction && middle.arms == 4, "four corridors meeting are a junction");
	Check(Passages::Classify(cross, 6, 1, Small).shape == Shape::Corridor, "each arm is a corridor");
	const Plan tee{{
		"#############",
		"#############",
		".............",
		".............",
		".............",
		"#####...#####",
		"#####...#####",
		"#####...#####",
		"#####...#####",
		"#####...#####",
	}};
	Check(Passages::Classify(tee, 6, 3, Small).shape == Shape::Junction, "three corridors meeting are a junction");
	const Plan bend{{
		"#####...",
		"#####...",
		"#####...",
		"#####...",
		"#####...",
		"........",
		"........",
		"........",
		"########",
	}};
	const auto corner = Passages::Classify(bend, 6, 6, Small);
	Check(corner.shape == Shape::Bend && corner.arms == 2, "two corridors meeting at a right angle are a bend");
}
}

int main() {
	Corridors();
	Doorways();
	Nodes();
	Passed();
}
