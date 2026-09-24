#include "native_ground_items.h"
#include "ground_item_layout.h"
#include "localization.h"
#include "test_support.h"
#include <array>
#include <cstring>
#include <fstream>

using namespace D2RAccess;
namespace D2RAccess::Localization { std::string Text(uint32_t id) { return id == 100 ? "Poción" : "Hacha"; } }
namespace {
template<size_t N> struct Memory {
	std::array<uint8_t, N> data{};
	uintptr_t Address() const { return reinterpret_cast<uintptr_t>(data.data()); }
	template<class T> void At(size_t offset, T value) { std::memcpy(data.data() + offset, &value, sizeof(value)); }
};
struct Fixture {
	Memory<0x200> player, item;
	Memory<0x100> playerPath, itemPath, itemData;
	Memory<0xC0> room, otherRoom;
	Memory<0x1600> tables;
	Memory<0x1C0> itemTable;
	Memory<0xE8 * 5> types;
	std::array<uintptr_t, 2> neighbors{};
	Fixture() {
		player.At<uint32_t>(8, 42); player.At<uint32_t>(0xC, 1); player.At(0x38, playerPath.Address());
		playerPath.At<uint32_t>(0, 100u << 16); playerPath.At<uint32_t>(4, 100u << 16); playerPath.At(0x20, room.Address());
		room.At(0xA8, item.Address());
		item.At<uint32_t>(0, 4); item.At<uint32_t>(8, 9); item.At<uint32_t>(0xC, 3);
		item.At(0x10, itemData.Address()); item.At(0x38, itemPath.Address());
		itemPath.At(0, room.Address()); itemPath.At<int>(0x10, 101); itemPath.At<int>(0x14, 101);
		itemData.At<uint32_t>(0, 2);
		tables.At(0x15A0, itemTable.Address()); tables.At<uint64_t>(0x15A8, 1);
		tables.At(0x1348, types.Address()); tables.At<uint64_t>(0x1350, 5);
		itemTable.At<uint32_t>(0x80, D2RL::Items::MakeItemCode("hp1")); itemTable.At<uint16_t>(0xFC, 100); itemTable.At<uint16_t>(0x12E, 1);
		types.At<GroundItemLayout::TypeRecord>(0xE8, {D2RL::Items::MakeItemCode("hpot"), 2, 0});
		types.At<GroundItemLayout::TypeRecord>(0xE8 * 2, {D2RL::Items::MakeItemCode("poti"), 0, 0});
	}
	bool Read(GroundItems::Snapshot& result) { return NativeGroundItems::ReadPlayer(player.Address(), tables.Address(), 42, 7, result); }
};
}
int main(int argc, char** argv) {
	GroundItems::Snapshot snapshot;
	Fixture f;
	Check(f.Read(snapshot) && snapshot.items.size() == 1, "native nearby ground item is copied");
	Check(snapshot.items[0].id == 9 && snapshot.items[0].name == "Poción", "copied identity and game localization");
	Check(snapshot.items[0].sound == GroundItems::Sound::Potion, "consumables inherit parent type from the runtime table");
	Check(snapshot.player.x == 100 && snapshot.items[0].position.x == 101, "static and dynamic coordinates use their distinct native layouts");
	f.playerPath.At<uint32_t>(0, (100u << 16) + 32768);
	Check(f.Read(snapshot) && snapshot.player.x == 100.5, "fractional player motion is preserved");
	for (uint32_t mode : {0, 1, 2, 4, 6}) {
		f.item.At<uint32_t>(0xC, mode); Check(f.Read(snapshot) && snapshot.items.empty(), "carried, equipped and cursor items are excluded");
	}
	f.item.At<uint32_t>(0xC, GroundItemLayout::Dropping);
	Check(f.Read(snapshot) && snapshot.items.empty(), "falling items remain silent until the native landing transition");
	GroundItems::Tracker tracker;
	const auto falling = tracker.Tick(snapshot, 0, true);
	Check(falling.voices.empty() && falling.speech.empty(), "falling objects create neither a loop nor premature proximity speech");
	f.item.At<uint32_t>(0xC, GroundItemLayout::OnGround);
	Check(f.Read(snapshot) && snapshot.items.size() == 1, "landed item appears in the very next snapshot");
	const auto landed = tracker.Tick(snapshot, GroundItems::PollMs, true);
	Check(landed.voices.size() == 1 && landed.speech == "Poción", "landing starts the correct loop and permits nearby speech without an artificial delay");
	f.item.At<uint32_t>(0, 1);
	Check(f.Read(snapshot) && snapshot.items.empty(), "monsters are not loot"); f.item.At<uint32_t>(0, 4);
	f.itemPath.At(0, f.otherRoom.Address()); Check(f.Read(snapshot) && snapshot.items.empty(), "stale room linkage cannot make an item audible");
	f.itemPath.At(0, f.room.Address()); f.itemPath.At<int>(0x10, 150);
	Check(f.Read(snapshot) && snapshot.items.empty(), "remote room items outside audio range are not copied");
	f.itemPath.At<int>(0x10, 101);
	f.item.At<uint32_t>(4, 1); Check(f.Read(snapshot) && snapshot.items.empty(), "invalid item row fails locally"); f.item.At<uint32_t>(4, 0);
	f.itemData.At<uint32_t>(0, 7); f.itemTable.At<uint16_t>(0x12E, 0);
	Check(f.Read(snapshot) && snapshot.items[0].sound == GroundItems::Sound::Legendary, "unique equipment maps to legendary audio");
	f.itemTable.At<uint16_t>(0x12E, 1);
	f.types.At<GroundItemLayout::TypeRecord>(0xE8 * 2, {D2RL::Items::MakeItemCode("none"), 1, 2});
	Check(f.Read(snapshot) && snapshot.items.size() == 1, "cyclic item type inheritance is bounded");
	f.types.At<GroundItemLayout::TypeRecord>(0xE8 * 2, {D2RL::Items::MakeItemCode("scro"), 0, 0});
	Check(f.Read(snapshot) && snapshot.items[0].sound == GroundItems::Sound::Potion, "scrolls use potion audio regardless of quality");
	f.types.At<GroundItemLayout::TypeRecord>(0xE8 * 2, {D2RL::Items::MakeItemCode("ques"), 0, 0});
	Check(f.Read(snapshot) && snapshot.items[0].sound == GroundItems::Sound::Quest,
		"quest ancestry takes priority over consumable and unique item sounds");
	f.types.At<GroundItemLayout::TypeRecord>(0xE8 * 2, {D2RL::Items::MakeItemCode("hamm"), 0, 0});
	f.itemTable.At<uint8_t>(GroundItemLayout::ItemQuest, 4);
	Check(f.Read(snapshot) && snapshot.items[0].sound == GroundItems::Sound::Quest,
		"the native quest column also catches quest weapons with ordinary item types");
	f.item.At<uint32_t>(0xC, GroundItemLayout::Dropping);
	Check(f.Read(snapshot) && snapshot.items.empty(), "a falling quest item is silent");
	f.item.At<uint32_t>(0xC, GroundItemLayout::OnGround);
	Check(f.Read(snapshot) && snapshot.items[0].sound == GroundItems::Sound::Quest,
		"landing exposes the quest cue on the next snapshot");
	f.item.At<uint32_t>(0xC, 0);
	Check(f.Read(snapshot) && snapshot.items.empty(), "picking up a quest item removes its cue");
	f.item.At<uint32_t>(0xC, GroundItemLayout::OnGround);
	f.room.At<uintptr_t>(0xA8, 0); f.otherRoom.At(0xA8, f.item.Address()); f.itemPath.At(0, f.otherRoom.Address());
	f.neighbors = {f.room.Address(), f.otherRoom.Address()}; f.room.At(0, reinterpret_cast<uintptr_t>(f.neighbors.data())); f.room.At<uint32_t>(0x40, 2);
	Check(f.Read(snapshot) && snapshot.items.size() == 1, "neighbor rooms are traversed once including self links");
	f.item.At(0x160, f.item.Address()); Check(!f.Read(snapshot) && snapshot.items.empty(), "cyclic unit list discards the incomplete snapshot");
	f.item.At<uintptr_t>(0x160, 0); f.room.At<uint32_t>(0x40, UINT32_MAX);
	Check(!f.Read(snapshot), "corrupt neighbor counts fail before allocation"); f.room.At<uint32_t>(0x40, 2);
	f.player.At<uint32_t>(8, 99); Check(!f.Read(snapshot), "wrong local player identity is rejected"); f.player.At<uint32_t>(8, 42);
	f.player.At<uint32_t>(0xC, 17); Check(!f.Read(snapshot), "dead player cannot emit loot cues"); f.player.At<uint32_t>(0xC, 1);
	f.tables.At<uint64_t>(0x15A8, UINT64_MAX); Check(!f.Read(snapshot), "corrupt runtime table dimensions fail closed");
	Check(!NativeGroundItems::ReadPlayer(1, 1, 42, 7, snapshot), "unmapped pointers never crash the caller");
	if (argc > 1) {
		std::ifstream image(argv[1], std::ios::binary | std::ios::ate);
		Check(bool(image), "reference executable opens");
		std::vector<uint8_t> bytes(static_cast<size_t>(image.tellg())); image.seekg(0);
		image.read(reinterpret_cast<char*>(bytes.data()), bytes.size());
		D2RL::PluginContext context{}; context.exeBase = reinterpret_cast<uintptr_t>(bytes.data());
		Check(NativeGroundItems::Initialize(&context), "all native function guards match the actual installed executable capture");
		bytes[GroundItemLayout::LocalPlayer] ^= 1;
		Check(!NativeGroundItems::Initialize(&context), "changed native build disables the reader");
	}
	Passed();
}
