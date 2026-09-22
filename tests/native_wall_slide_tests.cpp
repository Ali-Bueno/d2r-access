#include "native_wall_slide.h"
#include "test_support.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

using namespace D2RAccess;
namespace {
constexpr size_t ContextPath = 0x38, PathOwner = 0x40, PathType = 0x50, PathTarget = 0x70;
constexpr size_t UnitType = 0, UnitPath = 0x38;
using Context = std::array<unsigned char, ContextPath + sizeof(uintptr_t)>;
using Path = std::array<unsigned char, PathTarget + sizeof(uintptr_t)>;
using Unit = std::array<unsigned char, UnitPath + sizeof(uintptr_t)>;

bool prevent = false;
uintptr_t localPlayer = 0;
int calls = 0, forwardedDistance = -1;
NativeWallSlide::Probe forwarded{};

template<class T, class V> void Put(T& storage, size_t offset, V value) {
	std::memcpy(storage.data() + offset, &value, sizeof(value));
}
struct Fixture {
	alignas(uintptr_t) Context context{};
	alignas(uintptr_t) Path path{};
	alignas(uintptr_t) Unit unit{};
	alignas(uintptr_t) Unit target{};
	Fixture(uint32_t pathType = 17, uint32_t unitType = 0) {
		Put(context, ContextPath, reinterpret_cast<uintptr_t>(path.data()));
		Put(path, PathType, pathType);
		Put(path, PathOwner, reinterpret_cast<uintptr_t>(unit.data()));
		Put(path, PathTarget, reinterpret_cast<uintptr_t>(target.data()));
		Put(unit, UnitType, unitType);
		Put(unit, UnitPath, reinterpret_cast<uintptr_t>(path.data()));
		localPlayer = reinterpret_cast<uintptr_t>(unit.data());
	}
	uintptr_t Address() { return reinterpret_cast<uintptr_t>(context.data()); }
};
void Original(uintptr_t context, const float* start, const float* direction, int distance,
	uint32_t* point, float* travel, float* alignment, int16_t* collision) {
	++calls;
	forwarded = {context, start, direction, distance, point, travel, alignment, collision};
	forwardedDistance = distance;
	*point = 0xAABBCCDD;
	*travel = static_cast<float>(distance);
	*alignment = -1.0f;
	*collision = 0;
}
}
namespace D2RAccess::WallSlideService { bool Prevent(uintptr_t) noexcept { return prevent; } }
namespace D2RAccess::NativeWorld { bool IsLocalPlayer(uintptr_t player) noexcept { return player == localPlayer; } }
namespace D2RAccess::AsyncLog {
void Info(const D2RL::PluginContext*, const char*) {}
void Warn(const D2RL::PluginContext*, const char*) {}
void InfoF(const D2RL::PluginContext*, const char*, ...) {}
void WarnF(const D2RL::PluginContext*, const char*, ...) {}
}

int main() {
	using namespace NativeWallSlide;
	Fixture directional;
	Check(IsLocalDirectionalAttempt(directional.Address()), "local directional path is recognized from native ownership");
	Check(reinterpret_cast<uintptr_t>(directional.target.data()) != localPlayer,
		"a foreign target at Path+0x70 is not mistaken for the owner at Path+0x40");
	for (uint32_t type : {7u, 15u, 4u, 16u}) {
		Fixture other(type);
		Check(!IsLocalDirectionalAttempt(other.Address()), "mouse and special path types are not directional attempts");
	}
	Fixture monster(17, 1);
	Check(!IsLocalDirectionalAttempt(monster.Address()), "monster path is rejected");
	Fixture otherPlayer;
	localPlayer = 1;
	Check(!IsLocalDirectionalAttempt(otherPlayer.Address()), "another player's directional path is rejected");
	localPlayer = reinterpret_cast<uintptr_t>(directional.unit.data());
	Put(directional.path, PathOwner, uintptr_t{});
	Check(!IsLocalDirectionalAttempt(directional.Address()), "null path owner fails open");
	Put(directional.path, PathOwner, reinterpret_cast<uintptr_t>(directional.unit.data()));
	Put(directional.unit, UnitPath, uintptr_t{});
	Check(!IsLocalDirectionalAttempt(directional.Address()), "stale owner back-reference fails open");
	Check(!IsLocalDirectionalAttempt(0) && !IsLocalDirectionalAttempt(1), "missing and inaccessible contexts fail open");

	Fixture dispatch;
	float start[2]{1.0f, 2.0f}, direction[2]{3.0f, 4.0f}, travel = 99.0f, alignment = 99.0f;
	uint32_t point = 0;
	int16_t collision = -1;
	Probe probe{dispatch.Address(), start, direction, 10, &point, &travel, &alignment, &collision};
	const auto before = dispatch.context;
	prevent = false; calls = 0; forwardedDistance = -1;
	for (uintptr_t caller : {uintptr_t{0x3AB2A1}, uintptr_t{0x3AB2F6}, uintptr_t{0x3AB6B4}}) {
		calls = 0; forwardedDistance = -1;
		Dispatch(probe, caller, Original);
		Check(calls == 1 && forwardedDistance == 10, "disabled prevention preserves every fallback probe");
	}
	Check(forwarded.context == probe.context && forwarded.start == start && forwarded.direction == direction
		&& forwarded.point == &point && forwarded.travel == &travel && forwarded.alignment == &alignment
		&& forwarded.collision == &collision, "dispatch preserves every native pointer and the context");
	Check(point == 0xAABBCCDD && travel == 10.0f && alignment == -1.0f && collision == 0,
		"the original probe remains responsible for every output");

	prevent = true;
	for (uintptr_t caller : {uintptr_t{0x3AB24C}, uintptr_t{0xDEADBEEF}}) {
		calls = 0; forwardedDistance = -1;
		Dispatch(probe, caller, Original);
		Check(calls == 1 && forwardedDistance == 10, "raw and unrelated solver probes remain unchanged when prevention is enabled");
	}
	for (uintptr_t caller : {uintptr_t{0x3AB2A1}, uintptr_t{0x3AB2F6}, uintptr_t{0x3AB6B4}}) {
		calls = 0; forwardedDistance = -1; point = 0; travel = alignment = 99.0f; collision = -1;
		Dispatch(probe, caller, Original);
		Check(calls == 1 && forwardedDistance == 0, "each audited quantized fallback receives zero travel distance");
		Check(point == 0xAABBCCDD && travel == 0.0f && alignment == -1.0f && collision == 0,
			"zero-distance prevention still uses the original probe's normal output initialization");
	}
	Check(dispatch.context == before, "prevention leaves the native context untouched");
	Check(!Initialize(nullptr), "missing plugin context cannot install the hook");
	Passed();
}
