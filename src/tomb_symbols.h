#pragma once

#include "entity_radar.h"

#include <optional>

// D2R 3.3.93787: the server sends staff-tomb level minus 66 at packet
// 0x50 +5 (0x516DEC), and the journal uses it directly as the image frame
// (0x14DEA20). Artwork and the complete derivation: docs/tal-rasha-symbols.md.
namespace D2RAccess::TombSymbols {
inline constexpr uint32_t FirstLevel = 66;
inline constexpr size_t Count = 7;
inline constexpr std::string_view WidgetName = "TalRashaSymbol";
inline constexpr std::string_view QuestWidget = "Quest11";

inline std::optional<size_t> Index(uint32_t destination) {
	if (destination < FirstLevel || destination >= FirstLevel + Count) return std::nullopt;
	return destination - FirstLevel;
}

// Both loaded warp units and remembered/preset entrances use this name.
// Portals and waypoints have no carving of their own. Keep the underlying
// localized HUD name and its string key for sight matching and disk memory.
inline void Mark(EntityRadar::Entity& entity, const Strings::Table& strings) {
	const auto index = Index(entity.destinationLevel);
	if (!index || entity.kind != EntityRadar::Kind::Exit || entity.category != EntityRadar::Category::Exit) return;
	if (entity.worldName.empty()) entity.worldName = entity.name;
	entity.name = entity.worldName + ": " + strings.tombSymbols[*index];
}
}
