#pragma once
#include "ground_items.h"
#include <array>

namespace D2RAccess::ItemSpatial {
// X3DAudio speaker matrix; attenuation/pitch stay in the world-distance policy.
std::array<float, 2> StereoMatrix(const GroundItems::Voice& voice);
}
