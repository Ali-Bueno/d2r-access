#include "allocation_failure.h"
#include "callback_boundary.h"
#include "navigation_grid.h"
#include "test_support.h"

int main() {
	using namespace D2RAccess;
	Radar::World world;
	world.player = {100, 100};
	CallbackBoundary boundary;
	bool moving = true, pending = true, selected = true, unwound = false;
	CallbackFault fault = CallbackFault::None;
	int calls = 0, recovered = 0;
	struct Scope { bool& unwound; ~Scope() { unwound = true; } };
	auto recover = [&](CallbackFault why) noexcept {
		++recovered; fault = why;
		moving = pending = selected = false;
	};
	TestAllocation::minimum = 301u * 301u * sizeof(Navigation::Cell);
	TestAllocation::fail = true;
	const bool finished = boundary.Run(100, [&] {
		++calls;
		Scope scope{unwound};
		const auto grid = Navigation::Build(world, Navigation::MaxRadius, Radar::Layer::Terrain, false);
	}, recover);
	TestAllocation::fail = false;
	Check(!finished && fault == CallbackFault::Allocation, "real maximum-radius grid allocation failure is contained");
	Check(TestAllocation::rejected >= TestAllocation::minimum, "injection reaches the allocation implicated by the crash dump");
	Check(unwound && !moving && !pending && !selected, "snapshot unwinds and recovery cancels actions without allocating");
	Check(boundary.Failures() == 1 && recovered == 1, "failure state is retained for later diagnostics");
	Check(!boundary.Run(1099, [&] { ++calls; }, recover) && calls == 1, "memory failure backs off instead of reallocating every frame");
	Check(boundary.Run(1100, [&] { ++calls; }, recover) && calls == 2, "normal callbacks resume after the retry interval");
	Check(!boundary.Run(1200, [] { throw 42; }, recover) && fault == CallbackFault::Unexpected, "other C++ exceptions also cannot escape a noexcept callback");
	boundary.Reset();
	Check(boundary.Run(0, [] {}, recover) && boundary.Failures() == 0, "service restart clears recovery state");
	Passed();
}
