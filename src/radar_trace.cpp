#include "radar_trace.h"
#include "async_log.h"
#include <cstdio>
#include <string>

namespace D2RAccess::RadarTrace {
const char* DirectionName(size_t direction) {
	constexpr const char* Names[] = {"up", "left", "right", "down"};
	return direction < Radar::DirectionCount ? Names[direction] : "unknown";
}
const char* StateName(Radar::State state) {
	switch (state) {
	case Radar::State::Open: return "open";
	case Radar::State::Blocked: return "blocked";
	default: return "unknown";
	}
}
void Snapshot(const D2RL::PluginContext* context, const Radar::World& world, const Radar::Engine& engine, uint64_t now) {
	// One snapshot per cue-producing tick, after submitting audio. Retain the
	// raw flags needed to replay footprint/terrain filtering, including unknown
	// coverage. Run-length encoding avoids dozens of disk flushes per map.
	const int radius=Radar::StructuralRadius({},world.bodyPattern), side=2*radius+1;
	const int left=static_cast<int>(world.player.x)-radius, top=static_cast<int>(world.player.y)-radius;
	struct Chunk { size_t start; std::string data; };
	std::vector<Chunk> chunks{{0,{}}};
	constexpr size_t MaxPayload=3000; // leave metadata space in AsyncLog's 4096-byte record
	constexpr uint16_t Blank=0x20; // native unknown subtile, same interpretation as World::Cell
	uint16_t last=0;
	size_t count=0, offset=0;
	const auto flush=[&] {
		if (!count) return;
		char run[32]{};
		std::snprintf(run,sizeof(run),"%04X:%zu,",last,count);
		if (chunks.back().data.size()+std::char_traits<char>::length(run)>MaxPayload) chunks.push_back({offset,{}});
		chunks.back().data+=run; offset+=count;
	};
	for (int y=0;y<side;++y) for (int x=0;x<side;++x) {
		uint16_t flags=0;
		if (world.Cell(left+x,top+y,Radar::Layer::Movement,&flags)==Radar::State::Unknown) flags=Blank;
		if (count && flags!=last) { flush(); count=0; }
		last=flags; ++count;
	}
	flush();
	for (size_t part=0;part<chunks.size();++part)
		AsyncLog::InfoF(context,"[radar-collision] event_ms=%llu session=%llu level=%d origin=%d,%d size=%d pattern=%d mask=%04X position=%.5f,%.5f part=%zu/%zu start=%zu data=%s",
			static_cast<unsigned long long>(now),static_cast<unsigned long long>(world.session),world.level,left,top,side,world.bodyPattern,world.walkingMask,
			world.player.x+world.subtileFraction.x,world.player.y+world.subtileFraction.y,part+1,chunks.size(),chunks[part].start,chunks[part].data.c_str());
	for (size_t direction=0;direction<Radar::DirectionCount;++direction) {
		const auto& sensor=engine.Sensors()[direction];
		const auto point=sensor.wallMemory.Point(), normal=sensor.wallMemory.Normal();
		const auto visit=sensor.openingVisit.Point(), target=sensor.openingVisit.Target();
		AsyncLog::InfoF(context,"[radar-reference] event_ms=%llu direction=%zu known=%u point=%.0f,%.0f normal=%.5f,%.5f hit=%.0f,%.0f new_exit=%u required=%.1f visit_known=%u visit=%.0f,%.0f target=%.1f,%.1f departed=%u revisit=%u",
			static_cast<unsigned long long>(now),direction,sensor.wallMemory.Known()?1u:0u,point.x,point.y,normal.x,normal.y,
			sensor.structure.hit.x,sensor.structure.hit.y,sensor.newOpening?1u:0u,sensor.wallMemory.RequiredDepth(),
			sensor.openingVisit.Known()?1u:0u,visit.x,visit.y,target.x,target.y,sensor.openingVisit.Departed()?1u:0u,sensor.revisitCue?1u:0u);
	}
}
void Audio(const D2RL::PluginContext* context, size_t direction, Radar::State state,
	const RadarAudio::CueSource& source, const char* stage, const char* reason,
	uint64_t now, unsigned result, uint64_t audioMs) {
	AsyncLog::InfoF(context,
		"[radar-audio] event_ms=%llu direction=%s state=%s stage=%s reason=%s at_ms=%llu elapsed_ms=%llu audio_ms=%llu session=%llu level=%d origin=%.0f,%.0f source=%s result=%u",
		static_cast<unsigned long long>(source.eventMs), DirectionName(direction), StateName(state), stage, reason,
		static_cast<unsigned long long>(now), static_cast<unsigned long long>(now >= source.eventMs ? now - source.eventMs : 0),
		static_cast<unsigned long long>(audioMs), static_cast<unsigned long long>(source.session), source.level,
		source.position.x, source.position.y, source.audition ? "audition" : "navigation", result);
}
}
