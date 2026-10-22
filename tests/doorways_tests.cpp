#include "doorways.h"
#include "native_level_exits.h"
#include "place_service.h"
#include "secret_doors.h"
#include "speech_strings.h"
#include "test_support.h"

using namespace D2RAccess;

namespace {
std::vector<Doorways::Box> buildings;
}
namespace D2RAccess::PlaceService { std::vector<Doorways::Box> Buildings() { return buildings; } }
namespace D2RAccess::NativeLevelExits {
uint64_t Identity(Radar::Position position) { return (static_cast<uint64_t>(position.x) << 16 | static_cast<uint64_t>(position.y)) + 1; }
}

namespace {
// LvlTypes.txt rows: the crypt, Tristram, and the Act I wilderness.
constexpr int32_t Crypt = 4, Tristram = 11, Wilderness = 2;

Rooms::Doorway At(double x, double y) { return {{x, y}, 5}; }

void WhereDoorwaysAre() {
	const std::vector<Rooms::Doorway> found{At(10, 10), At(100, 100)};
	Check(Doorways::Select(found, Crypt, {}, {}).size() == 2, "the crypt's halls have doorways everywhere");
	Check(Doorways::Select(found, Tristram, {}, {}).size() == 2, "so do Tristram's houses (a tester, 2026-10-22)");
	Check(Doorways::Select(found, Wilderness, {}, {}).empty(), "the open wilderness has none: a gap between trees is no doorway");
	const std::vector<Doorways::Box> cottage{{90, 90, 130, 130}};
	const auto inCottage = Doorways::Select(found, Wilderness, cottage, {});
	Check(inCottage.size() == 1 && inCottage[0].centre.x == 100, "but a cottage, the Tower's ruin or a burning house has its own");
	Check(Doorways::Select(found, Crypt, {}, {{12, 11}}).size() == 1, "a door object's own gap is the door's, not a doorway");
}

void ListedLikeADoor() {
	const auto entity = Doorways::Listed(At(20.5, 30.5));
	Check(entity.kind == EntityRadar::Kind::Object && entity.category == EntityRadar::Category::Interactable,
		"a doorway is listed with the doors");
	Check(entity.mapLandmark && entity.doorway && entity.placeId != 0, "as a place with no unit, known by where it is");
	Check(entity.name == Strings::Current().doorway, "named a doorway");
}

void DraftFeltOnce() {
	SecretDoors::Draft draft;
	const std::vector<Radar::Position> doors{{100, 100}};
	Check(!draft.Felt(doors, {100 + SecretDoors::FeltWithin + 1, 100}), "nothing is felt from afar");
	Check(draft.Felt(doors, {100 + SecretDoors::FeltWithin - 1, 100}), "a draft is felt walking past a shut secret door");
	Check(!draft.Felt(doors, {100, 101}), "once, not on every step beside it");
	Check(!draft.Felt(doors, {100 + SecretDoors::FeltAgainBeyond - 1, 100}), "nor on stepping back a little");
	Check(!draft.Felt(doors, {100 + SecretDoors::FeltAgainBeyond + 1, 100}), "walking away is quiet");
	Check(draft.Felt(doors, {100, 100}), "and coming back is felt again");
	Check(!draft.Felt({}, {100, 100}), "an opened door is a doorway, no longer a draft");
	Check(draft.Felt({{100, 100}, {104, 100}}, {102, 100}), "two doors side by side are felt as one draft");
}
}

int main() {
	WhereDoorwaysAre();
	ListedLikeADoor();
	DraftFeltOnce();
	Passed();
}
