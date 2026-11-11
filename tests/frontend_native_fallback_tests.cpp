#include "frontend_native_fallback.h"
#include "test_support.h"

int main() {
	using namespace D2RAccess;
	using UiText::Kind;
	UiText::Snapshot panel;
	panel.name = "D2RLoaderMultiplayerPanel";
	panel.focus = 3;
	panel.nodes = {
		{1, -1, Kind::Other, "D2RLoaderMultiplayerPanel", "Panel"},
		{2, 0, Kind::Text, "Title", "TextBoxWidget", "Multijugador"},
		{3, 0, Kind::Button, "Host", "ButtonWidget", "Crear partida"},
	};
	auto readout = FrontendNativeFallback::Describe(panel);
	Check(readout.heading == "Multijugador" && readout.text == "Crear partida", "new panel exposes heading and focused button separately");
	Check(FrontendNativeFallback::Describe(panel).identity == readout.identity, "unchanged focus has stable identity");
	panel.nodes.push_back({11, 2, Kind::Text, "HostText", "TextBoxWidget", "Dirigir partida"});
	panel.nodes[2].text = "Empieza una sesión";
	Check(FrontendNativeFallback::Describe(panel).text == "Dirigir partida, Empieza una sesión", "button caption precedes its tooltip");
	panel.nodes.pop_back();
	panel.nodes[2].text = "Crear partida";

	panel.focus = 5;
	panel.nodes.push_back({4, 0, Kind::Text, "GameNameTitle", "TextBoxWidget", "Nombre"});
	panel.nodes.push_back({5, 0, Kind::TextInput, "GameNameInput", "InputTextBoxWidget", "Partida de Ana"});
	readout = FrontendNativeFallback::Describe(panel);
	Check(readout.text == "Nombre", "text field uses the visible label while TextField owns its value");
	Check(readout.contents == "Partida de Ana", "text field exposes its contents for the arrival phrase");
	const auto previousIdentity = readout.identity;
	panel.nodes.back().text = "Partida de ambos";
	Check(FrontendNativeFallback::Describe(panel).identity == previousIdentity, "typing does not repeat the field label");

	panel.focus = 7;
	panel.nodes.push_back({6, 0, Kind::Text, "PasswordTitle", "TextBoxWidget", "Contraseña"});
	panel.nodes.push_back({7, 0, Kind::TextInput, "PasswordInput", "MaskedInputTextBoxWidget", "secret"});
	readout = FrontendNativeFallback::Describe(panel);
	Check(readout.text == "Contraseña", "masked text field never speaks a password");
	Check(readout.contents.empty(), "masked text field never exposes its contents");
	Check(readout.identity.find("secret") == std::string::npos, "masked password does not enter logging identity");

	panel.focus = 9;
	panel.nodes.push_back({8, 0, Kind::ListItem, "ListItem0", "ListItemWidget"});
	panel.nodes.push_back({9, 7, Kind::Text, "Name", "TextBoxWidget", "Partida de Luis"});
	readout = FrontendNativeFallback::Describe(panel);
	Check(readout.text == "Partida de Luis", "focus on a child reads its list row");
	panel.nodes.back().text = "Partida de María";
	Check(FrontendNativeFallback::Describe(panel).text == "Partida de María", "recycled row reads live contents");

	panel.focus = 10;
	panel.nodes.push_back({10, 0, Kind::Button, "Join", "ButtonWidget", "Unirse", false});
	readout = FrontendNativeFallback::Describe(panel);
	Check(!readout.enabled, "disabled state remains available for localized announcement");
	panel.nodes.back().kind = Kind::Toggle;
	panel.nodes.back().toggled = false;
	const auto offIdentity = FrontendNativeFallback::Describe(panel).identity;
	panel.nodes.back().toggled = true;
	Check(FrontendNativeFallback::Describe(panel).identity != offIdentity, "toggle state changes dedup identity");

	UiText::Snapshot overlay;
	overlay.name = "D2RLoaderMultiplayerPanel";
	overlay.focus = 22; // browse input remains focused beneath the overlay
	overlay.nodes = {
		{20, -1, Kind::Other, "D2RLoaderMultiplayerPanel", "Panel"},
		{21, 0, Kind::Tabs, "MainTabs", "TabBarWidget", "Buscar partida"},
		{22, 0, Kind::TextInput, "SearchInput", "InputTextBoxWidget"},
		{23, 0, Kind::Other, "DirectJoinModal", "Widget"},
		{24, 3, Kind::Text, "DirectJoinTitle", "TextBoxWidget", "Unirse directamente"},
		{25, 3, Kind::Text, "DirectJoinAddressText", "TextBoxWidget", "Dirección IP"},
		{26, 3, Kind::TextInput, "DirectJoinAddressInput", "InputTextBoxWidget"},
	};
	readout = FrontendNativeFallback::Describe(overlay);
	Check(readout.heading == "Unirse directamente" && readout.text == "Dirección IP", "visible modal takes context and field from covered focus");
	Check(readout.context.find("23") != std::string::npos, "modal identity distinguishes overlay from its parent tab");
	overlay.nodes.resize(3);
	readout = FrontendNativeFallback::Describe(overlay);
	Check(readout.heading == "Buscar partida", "selected tab supplies context after overlay closes");

	UiText::Snapshot browse;
	browse.name = "D2RLoaderMultiplayerPanel";
	browse.focus = 31;
	browse.nodes = {
		{30, -1, Kind::Other, "Panel", "Panel"},
		{31, 0, Kind::List, "BrowseResultsList", "ListWidget"},
		{32, 1, Kind::ListItem, "ListItem0", "ListItemWidget"},
		{33, 2, Kind::Text, "Name", "TextBoxWidget", "Partida uno"},
		{34, 1, Kind::ListItem, "ListItem1", "ListItemWidget"},
		{35, 4, Kind::Text, "Name", "TextBoxWidget", "Partida dos"},
	};
	readout = FrontendNativeFallback::Describe(browse);
	Check(readout.text.find("Partida uno") == std::string::npos && readout.text.find("Partida dos") == std::string::npos,
		"focused list container never enumerates its child rows");
	browse.focus = 32;
	Check(FrontendNativeFallback::Describe(browse).text == "Partida uno", "focused list item reads only its own text");

	UiText::Snapshot filter;
	filter.name = "D2RLoaderMultiplayerPanel";
	filter.focus = 42;
	filter.nodes = {
		{40, -1, Kind::Other, "Panel", "Panel"},
		{41, 0, Kind::Text, "DifficultyLabel", "TextBoxWidget", "Dificultad de la partida"},
		{42, 0, Kind::ValueControl, "BrowseDifficultyFilterDropdown", "DropdownListWidget", "Normal", true, 0, std::nullopt, "Filter by difficulty"},
	};
	readout = FrontendNativeFallback::Describe(filter);
	Check(readout.text == "Dificultad de la partida, Normal, Filter by difficulty" && readout.value == "Normal",
		"dropdown reads label, selected caption, then tooltip");
	const auto oldFilterIdentity = readout.identity;
	filter.nodes.back().text = "Pesadilla";
	Check(FrontendNativeFallback::Describe(filter).identity != oldFilterIdentity,
		"selected value changes the dropdown identity");

	UiText::Snapshot counter;
	counter.name = "D2RLoaderMultiplayerPanel";
	counter.focus = 54;
	counter.nodes = {
		{50, -1, Kind::Other, "Panel", "Panel"},
		{51, 0, Kind::Other, "MaxPlayers", "Widget"},
		{52, 1, Kind::Text, "MaxPlayersLabel", "TextBoxWidget", "Máximo de jugadores"},
		{53, 1, Kind::Text, "MaxPlayersValue", "TextBoxWidget", "8"},
		{54, 1, Kind::Button, "MaxPlayersIncrementButton", "ButtonWidget", "Controla el máximo de jugadores"},
		{55, 1, Kind::Button, "MaxPlayersDecrementButton", "ButtonWidget", "Controla el máximo de jugadores"},
	};
	readout = FrontendNativeFallback::Describe(counter);
	Check(readout.text == "Máximo de jugadores, 8, Controla el máximo de jugadores" && readout.value == "8",
		"increment button reads its sibling label and current value");
	const auto eightIdentity = readout.identity;
	counter.nodes[3].text = "7";
	Check(FrontendNativeFallback::Describe(counter).identity != eightIdentity,
		"counter value changes speech identity on the same button");
	counter.focus = 55;
	Check(FrontendNativeFallback::Describe(counter).value == "7", "decrement button shares the same visible value");
	counter.focus = 59;
	counter.nodes.push_back({56, 0, Kind::Other, "LevelDifferenceToggle", "ToggleButtonWidget"});
	counter.nodes.push_back({57, 6, Kind::Text, "LevelDifferenceHostLabel", "TextBoxWidget", "Diferencia de nivel"});
	counter.nodes.push_back({58, 0, Kind::Text, "LevelDifferenceValue", "TextBoxWidget", "10"});
	counter.nodes.push_back({59, 0, Kind::Button, "LevelDifferenceIncrementButton", "ButtonWidget", "Controla la diferencia"});
	Check(FrontendNativeFallback::Describe(counter).text.starts_with("Diferencia de nivel, 10"),
		"counter label can live inside a related toggle subtree");
	UiText::Snapshot icons;
	icons.name = "D2RLoaderMultiplayerPanel";
	icons.focus = 61;
	icons.nodes = {{60, -1, Kind::Other, "Panel", "Panel"},
		{61, 0, Kind::Button, "RefreshButton", "ButtonWidget"}};
	Check(FrontendNativeFallback::Describe(icons).labelKey == "d2rloader:D2RLoaderMultiplayerRefreshGames",
		"captionless loader icon uses its shipped localization key");
	icons.name = "OtherPanel";
	Check(FrontendNativeFallback::Describe(icons).labelKey.empty(), "loader label override does not rename other panels");
	// Hit areas with no words of their own (live log, 2026-11-01).
	UiText::Snapshot inventory;
	inventory.name = "PlayerInventoryExpansionLayout";
	inventory.focus = 21;
	inventory.nodes = {
		{20, -1, Kind::Other, "PlayerInventoryExpansionLayout", "Panel"},
		{21, 0, Kind::Other, "weaponswap_left_arm", "FocusableWidget"},
	};
	Check(FrontendNativeFallback::Describe(inventory).labelKey == "Cfgswapweapons", "a weapon-swap tab is called by the game's string for the swap");
	UiText::Snapshot tree;
	tree.name = "SkillsTreePanel";
	tree.focus = 32;
	tree.nodes = {
		{30, -1, Kind::Other, "SkillsTreePanel", "Panel"},
		{31, 0, Kind::Text, "SkillCounter", "TextBoxWidget", "0 skill choices left"},
		{32, 1, Kind::Other, "Tooltip", "FocusableWidget"},
	};
	tree.nodes.back().hint = "Spend skill points to raise your skills.";
	Check(FrontendNativeFallback::Describe(tree).text == "0 skill choices left, Spend skill points to raise your skills.",
		"a hit area with no words is called by what it covers, then by its tooltip");
	tree.nodes[1].text.clear();
	Check(FrontendNativeFallback::Describe(tree).text == "Spend skill points to raise your skills.", "or by its tooltip alone");
	// A control of the game's own class snapshotted alone (ChronicleRewardsPanelHD.json): every word
	// it draws, its button's caption included.
	UiText::Snapshot reward;
	reward.name = "ChronicleRewardsPanel";
	reward.nodes = {
		{60, -1, Kind::Focusable, "UniquesRewards", "ChronicleRewardsPanelEntry"},
		{61, 0, Kind::Text, "EntryCompletionText", "TextBoxWidget", "45% completado"},
		{62, 0, Kind::Toggle, "ToggleSkillCheck", "ToggleButtonWidget"},
		{63, 2, Kind::Text, "Caption", "TextBoxWidget", "Activa"},
	};
	Check(FrontendNativeFallback::DescribeRow(reward).text == "45% completado, Activa", "a composite control reads every word it draws, its buttons' included");

	// Legacy graphics, 2026-11-11: an option was read as its value alone ("ALTA"), a slider as its widget's name.
	UiText::Snapshot video;
	video.name = "VideoOptionsPanel";
	video.focus = 73;
	video.nodes = {
		{70, -1, Kind::Other, "VideoOptionsPanel", "VideoOptionsPanel"},
		{71, 0, Kind::Other, "Row LightingQuality", "TableRowWidget"},
		{72, 1, Kind::Text, "LightingQualityText", "TextBoxWidget", "CALIDAD DE LUZ"},
		{73, 1, Kind::ValueControl, "LightingQuality", "CarouselWidget"},
		{74, 3, Kind::Text, "Text", "TextBoxWidget", "ALTA"},
	};
	Check(FrontendNativeFallback::Describe(video).text == "CALIDAD DE LUZ, ALTA", "a legacy option row says its caption before its value");

	Passed();
}
