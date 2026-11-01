#pragma once
#include <string>

// The shared stash's page controls, resolved afresh each call. UI owner only.
namespace D2RAccess::StashPages {
// The page the game shows between the arrows; empty when the Shared tab is not shown.
std::string Label();
// Moves pages through the native command; -1 when the Shared page is not shown or the move fails.
int Change(int direction, int pages);
}
