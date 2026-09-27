cmake_minimum_required(VERSION 3.28)

project(D2RAccess VERSION 0.1.0 LANGUAGES CXX RC)

# Where the game lives. The build copies the plugin into <D2R_GAME_DIR>/d2rloader/plugins
# and prism.dll into the d2r-access/ subfolder there (a DLL directly in plugins/ is
# treated as a plugin by D2RLoader and rejected), so a rebuild is immediately testable.
set(D2R_GAME_DIR "D:/games/steam/steamapps/common/Diablo II Resurrected" CACHE PATH "Diablo II: Resurrected install folder")
set(D2R_PLUGINS_DIR "${D2R_GAME_DIR}/d2rloader/plugins")

set(D2RLPLUGIN_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(D2RLPLUGIN_BUILD_TESTS OFF CACHE BOOL "" FORCE)
# D2RLoader PluginSDK: git submodule in external/ (`git submodule update --init`).
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/external/PluginSDK" "${CMAKE_BINARY_DIR}/PluginSDK")

set(PRISM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/prism")

# D2RLoader picks up a companion d2rl-<id>.mpq by this file name, so keep the
# "d2rl-" prefix and the plugin id from src/plugin.cpp in sync.
add_library(d2r-access SHARED
	src/async_log.cpp
	src/auto_detail.cpp
	src/control_hints.cpp
	src/tab_reader.cpp
	src/tooltip_text.cpp
	src/text_field.cpp
	src/character_saves.cpp
	src/code_dump.cpp
	src/control_state.cpp
	src/equip_attempt.cpp
	src/npc_speech.cpp
	src/display_modes.cpp
	src/focus_tracker.cpp
	src/frontend_native_fallback.cpp
	src/multiplayer_reader.cpp
	src/multiplayer_status.cpp
	src/menu_navigation.cpp
	src/menu_navigation_policy.cpp
	src/footsteps.cpp
	src/footstep_service.cpp
	src/native_footsteps.cpp
	src/wall_slide_service.cpp
	src/native_wall_slide.cpp
	src/game_text_reader.cpp
	src/input_direction.cpp
	src/modal_reader.cpp
	src/interaction_reader.cpp
	src/hire_stats.cpp
	src/inventory_census.cpp
	src/inventory_cursor.cpp
	src/plugin.cpp
	src/localization.cpp
	src/native_widgets.cpp
	src/native_memory.cpp
	src/native_world.cpp
	src/beacon.cpp
	src/beacon_audio.cpp
	src/beacon_service.cpp
	src/beacon_navigation.cpp
	src/navigation_mode.cpp
	src/navigation_policy.cpp
	src/radar.cpp
	src/navigation_grid.cpp
	src/radar_passage.cpp
	src/radar_structure.cpp
	src/radar_grid.cpp
	src/radar_audio.cpp
	src/radar_trace.cpp
	src/radar_readout.cpp
	src/radar_synth.cpp
	src/radar_service.cpp
	src/radar_ui.cpp
	src/radar_config.cpp
	src/entity_radar.cpp
	src/entity_radar_service.cpp
	src/entity_cues.cpp
	src/entity_cue_audio.cpp
	src/entity_cue_synth.cpp
	src/entity_cue_service.cpp
	src/npc_proximity.cpp
	src/target_sight.cpp
	src/native_entities.cpp
	src/native_cairn.cpp
	src/cairn_scroll_service.cpp
	src/native_level_exits.cpp
	src/native_level_areas.cpp
	src/level_areas.cpp
	src/native_drlg.cpp
	src/native_rooms.cpp
	src/room_graph.cpp
	src/corpse_memory.cpp
	src/rooms.cpp
	src/room_service.cpp
	src/exit_review.cpp
	src/explored_rooms.cpp
	src/explored_store.cpp
	src/explored_store_worker.cpp
	src/frontier.cpp
	src/exploration_cues.cpp
	src/exploration_cue_service.cpp
	src/zone_transitions.cpp
	src/terrain_overview.cpp
	src/terrain_service.cpp
	src/map_survey.cpp
	src/map_survey_service.cpp
	src/automap_cells.cpp
	src/native_automap.cpp
	src/automap_service.cpp
	src/native_ui.cpp
	src/pad_filter.cpp
	src/widget_probe.cpp
	src/native_focus.cpp
	src/native_skill_navigation.cpp
	src/native_cursor_gesture.cpp
	src/skill_grid.cpp
	src/skill_navigation.cpp
	src/native_progression.cpp
	src/native_controller_help.cpp
	src/controller_text.cpp
	src/progression_text.cpp
	src/character_stats.cpp
	src/character_stats_reader.cpp
	src/mercenary_stats.cpp
	src/advanced_stats_reader.cpp
	src/native_tooltips.cpp
	src/tooltip_delay.cpp
	src/panel_state.cpp
	src/quest_log.cpp
	src/quest_progress.cpp
	src/quest_service.cpp
	src/screen_reader.cpp
	src/settings_file.cpp
	src/speech_strings.cpp
	src/tab_highlight.cpp
	src/ui_message_monitor.cpp
	src/ui_thread_pump.cpp
	src/ui_text.cpp
	src/ui_notifications.cpp
	src/vitals.cpp
	src/vitals_service.cpp
	src/equipment_census.cpp
	src/native_weapon_set.cpp
	src/weapon_swap.cpp
	src/weapon_swap_service.cpp
	src/native_trade.cpp
	src/trade.cpp
	src/trade_service.cpp
	src/native_belt.cpp
	src/skill_bar.cpp
	src/native_skill_bar.cpp
	src/skill_bar_service.cpp
	src/native_status.cpp
	src/widgets.cpp
	src/retreat.cpp
	src/auto_walk.cpp
	src/combat_target.cpp
	src/explore_goal.cpp
	src/grid_jump.cpp
	src/menu_cursor.cpp
	src/click_words.cpp
	src/menu_mouse.cpp
	src/native_sort.cpp
	src/native_swap.cpp
	src/native_belt_rows.cpp
	src/native_belt_fill.cpp
	src/native_actions.cpp
	src/keyboard_service.cpp
	src/keyboard_menu.cpp
	src/settings_list.cpp
	plugin.rc
)
target_link_libraries(d2r-access PRIVATE D2RLPlugin::D2RLPlugin winmm version)
d2rlplugin_embed_config(TARGET d2r-access FILE "${CMAKE_CURRENT_SOURCE_DIR}/radar.toml")
target_include_directories(d2r-access PRIVATE "${PRISM_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/src")
set_target_properties(d2r-access PROPERTIES
	CXX_STANDARD 20
	CXX_STANDARD_REQUIRED TRUE
	CXX_EXTENSIONS FALSE
	OUTPUT_NAME "d2rl-d2r-access"
)
target_compile_definitions(d2r-access PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
if(MSVC)
	target_compile_options(d2r-access PRIVATE /W4 /permissive- /EHsc /utf-8)
endif()

include(CTest)
include(cmake/GroundItems.cmake)
if(BUILD_TESTING)
	# Full-resolution native body connectivity, shared with the reactive radar.
	add_executable(beacon-tests tests/beacon_tests.cpp src/beacon.cpp src/radar_grid.cpp)
	target_include_directories(beacon-tests PRIVATE src)
	target_compile_features(beacon-tests PRIVATE cxx_std_20)
	add_test(NAME beacon COMMAND beacon-tests)
	add_executable(beacon-service-tests tests/beacon_service_tests.cpp src/beacon_service.cpp src/beacon.cpp
		src/beacon_navigation.cpp src/navigation_mode.cpp src/navigation_policy.cpp
		src/navigation_grid.cpp src/radar_grid.cpp src/room_graph.cpp src/speech_strings.cpp src/async_log.cpp)
	target_include_directories(beacon-service-tests PRIVATE src)
	target_link_libraries(beacon-service-tests PRIVATE D2RLPlugin::D2RLPlugin)
	target_compile_features(beacon-service-tests PRIVATE cxx_std_20)
	target_compile_definitions(beacon-service-tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
	add_test(NAME beacon-service COMMAND beacon-service-tests)
	# The exploration memory (seen/visited rooms), its disk form and the open-ground frontier.
	add_executable(exploration-tests tests/exploration_tests.cpp src/explored_rooms.cpp src/explored_store.cpp src/explored_store_worker.cpp src/frontier.cpp src/navigation_grid.cpp src/radar_grid.cpp src/native_rooms.cpp src/native_memory.cpp)
	target_include_directories(exploration-tests PRIVATE src)
	target_compile_features(exploration-tests PRIVATE cxx_std_20)
	target_compile_definitions(exploration-tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
	add_test(NAME exploration COMMAND exploration-tests)
	add_executable(exploration-cues-tests tests/exploration_cues_tests.cpp src/exploration_cues.cpp src/explored_rooms.cpp)
	target_include_directories(exploration-cues-tests PRIVATE src)
	target_compile_features(exploration-cues-tests PRIVATE cxx_std_20)
	add_test(NAME exploration-cues COMMAND exploration-cues-tests)
	add_executable(exploration-cue-service-tests tests/exploration_cue_service_tests.cpp src/exploration_cue_service.cpp
		src/exploration_cues.cpp src/explored_rooms.cpp src/ground_items.cpp src/ground_item_text.cpp src/speech_strings.cpp)
	target_include_directories(exploration-cue-service-tests PRIVATE src)
	target_compile_features(exploration-cue-service-tests PRIVATE cxx_std_20)
	target_compile_definitions(exploration-cue-service-tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
	target_link_libraries(exploration-cue-service-tests PRIVATE D2RLPlugin::D2RLPlugin version)
	add_test(NAME exploration-cue-service COMMAND exploration-cue-service-tests)
	add_executable(zone-transitions-tests tests/zone_transitions_tests.cpp src/zone_transitions.cpp src/rooms.cpp src/radar_grid.cpp)
	target_include_directories(zone-transitions-tests PRIVATE src)
	target_compile_features(zone-transitions-tests PRIVATE cxx_std_20)
	add_test(NAME zone-transitions COMMAND zone-transitions-tests)
	add_executable(navigation-grid-tests tests/navigation_grid_tests.cpp src/navigation_grid.cpp src/radar_grid.cpp)
	target_include_directories(navigation-grid-tests PRIVATE src)
	target_compile_features(navigation-grid-tests PRIVATE cxx_std_20)
	add_test(NAME navigation-grid COMMAND navigation-grid-tests)
	add_executable(rooms-tests tests/rooms_tests.cpp src/rooms.cpp src/radar_grid.cpp)
	target_include_directories(rooms-tests PRIVATE src)
	target_compile_features(rooms-tests PRIVATE cxx_std_20)
	add_test(NAME rooms COMMAND rooms-tests)
	add_executable(exit-review-tests tests/exit_review_tests.cpp src/exit_review.cpp src/entity_radar.cpp src/radar_grid.cpp src/speech_strings.cpp)
	target_include_directories(exit-review-tests PRIVATE src)
	target_compile_features(exit-review-tests PRIVATE cxx_std_20)
	add_test(NAME exit-review COMMAND exit-review-tests)
	add_executable(terrain-overview-tests tests/terrain_overview_tests.cpp src/terrain_overview.cpp src/explored_rooms.cpp src/entity_radar.cpp src/radar_grid.cpp src/speech_strings.cpp)
	target_include_directories(terrain-overview-tests PRIVATE src)
	target_compile_features(terrain-overview-tests PRIVATE cxx_std_20)
	add_test(NAME terrain-overview COMMAND terrain-overview-tests)
	# The sonar radar: passage scan, sensor state machine, structural filtering,
	# the cue synthesizer and the spoken readout.
	set(RADAR_ENGINE_SOURCES src/radar.cpp src/radar_passage.cpp src/navigation_grid.cpp src/radar_structure.cpp src/radar_grid.cpp src/rooms.cpp)
	add_executable(radar-engine-tests tests/radar_engine_tests.cpp ${RADAR_ENGINE_SOURCES})
	target_include_directories(radar-engine-tests PRIVATE src)
	target_compile_features(radar-engine-tests PRIVATE cxx_std_20)
	add_test(NAME radar-engine COMMAND radar-engine-tests)
	add_executable(radar-passage-tests tests/radar_passage_tests.cpp ${RADAR_ENGINE_SOURCES})
	target_include_directories(radar-passage-tests PRIVATE src)
	target_compile_features(radar-passage-tests PRIVATE cxx_std_20)
	add_test(NAME radar-passage COMMAND radar-passage-tests)
	add_executable(radar-structure-tests tests/radar_structure_tests.cpp ${RADAR_ENGINE_SOURCES})
	target_include_directories(radar-structure-tests PRIVATE src)
	target_compile_features(radar-structure-tests PRIVATE cxx_std_20)
	add_test(NAME radar-structure COMMAND radar-structure-tests)
	# The snapshot test supplies its own AsyncLog sink, so it must not link the real one.
	add_executable(radar-snapshot-tests tests/radar_snapshot_tests.cpp src/radar_trace.cpp src/radar_structure.cpp src/radar_grid.cpp)
	target_link_libraries(radar-snapshot-tests PRIVATE D2RLPlugin::D2RLPlugin version)
	target_include_directories(radar-snapshot-tests PRIVATE src)
	target_compile_features(radar-snapshot-tests PRIVATE cxx_std_20)
	add_test(NAME radar-snapshot COMMAND radar-snapshot-tests)
	add_executable(radar-audio-tests tests/radar_audio_tests.cpp src/radar_synth.cpp)
	target_link_libraries(radar-audio-tests PRIVATE D2RLPlugin::D2RLPlugin version)
	target_include_directories(radar-audio-tests PRIVATE src)
	target_compile_features(radar-audio-tests PRIVATE cxx_std_20)
	add_test(NAME radar-audio COMMAND radar-audio-tests)
	add_executable(radar-readout-tests tests/radar_readout_tests.cpp src/radar_readout.cpp ${RADAR_ENGINE_SOURCES})
	target_include_directories(radar-readout-tests PRIVATE src)
	target_compile_features(radar-readout-tests PRIVATE cxx_std_20)
	add_test(NAME radar-readout COMMAND radar-readout-tests)
	add_executable(controller-help-tests tests/controller_help_tests.cpp src/native_controller_help.cpp src/controller_text.cpp src/native_memory.cpp src/ui_text.cpp src/speech_strings.cpp)
	target_link_libraries(controller-help-tests PRIVATE D2RLPlugin::D2RLPlugin version)
	target_include_directories(controller-help-tests PRIVATE src)
	target_compile_features(controller-help-tests PRIVATE cxx_std_20)
	target_compile_definitions(controller-help-tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
	add_test(NAME controller-help COMMAND controller-help-tests)
	add_executable(skill-bar-tests tests/skill_bar_tests.cpp src/skill_bar.cpp src/controller_text.cpp src/ui_text.cpp src/speech_strings.cpp)
	target_link_libraries(skill-bar-tests PRIVATE D2RLPlugin::D2RLPlugin)
	target_include_directories(skill-bar-tests PRIVATE src)
	target_compile_features(skill-bar-tests PRIVATE cxx_std_20)
	add_test(NAME skill-bar COMMAND skill-bar-tests)
	add_executable(native-cursor-gesture-tests tests/native_cursor_gesture_tests.cpp src/native_cursor_gesture.cpp)
	target_include_directories(native-cursor-gesture-tests PRIVATE src)
	target_compile_features(native-cursor-gesture-tests PRIVATE cxx_std_20)
	add_test(NAME native-cursor-gesture COMMAND native-cursor-gesture-tests)
	add_executable(skill-navigation-tests tests/skill_navigation_tests.cpp src/skill_grid.cpp src/skill_navigation.cpp src/progression_text.cpp src/ui_text.cpp src/speech_strings.cpp src/async_log.cpp)
	target_link_libraries(skill-navigation-tests PRIVATE D2RLPlugin::D2RLPlugin version)
	target_include_directories(skill-navigation-tests PRIVATE src)
	target_compile_features(skill-navigation-tests PRIVATE cxx_std_20)
	add_test(NAME skill-navigation COMMAND skill-navigation-tests)
	add_executable(native-focus-tests tests/native_focus_tests.cpp src/native_focus.cpp)
	add_executable(frontend-native-fallback-tests tests/frontend_native_fallback_tests.cpp src/frontend_native_fallback.cpp src/ui_text.cpp src/speech_strings.cpp)
	target_include_directories(frontend-native-fallback-tests PRIVATE src tests)
	target_compile_features(frontend-native-fallback-tests PRIVATE cxx_std_20)
	add_test(NAME frontend-native-fallback-tests COMMAND frontend-native-fallback-tests)
	add_executable(multiplayer-status-tests tests/multiplayer_status_tests.cpp src/multiplayer_status.cpp)
	target_include_directories(multiplayer-status-tests PRIVATE src tests)
	target_compile_features(multiplayer-status-tests PRIVATE cxx_std_20)
	add_test(NAME multiplayer-status COMMAND multiplayer-status-tests)
	add_executable(menu-navigation-policy-tests tests/menu_navigation_policy_tests.cpp src/menu_navigation_policy.cpp)
	target_include_directories(menu-navigation-policy-tests PRIVATE src tests)
	target_compile_features(menu-navigation-policy-tests PRIVATE cxx_std_20)
	add_test(NAME menu-navigation-policy COMMAND menu-navigation-policy-tests)
	add_executable(menu-navigation-tests tests/menu_navigation_tests.cpp src/menu_navigation.cpp src/menu_navigation_policy.cpp)
	target_link_libraries(menu-navigation-tests PRIVATE D2RLPlugin::D2RLPlugin)
	target_include_directories(menu-navigation-tests PRIVATE src tests)
	target_compile_features(menu-navigation-tests PRIVATE cxx_std_20)
	add_test(NAME menu-navigation COMMAND menu-navigation-tests)
	target_link_libraries(native-focus-tests PRIVATE D2RLPlugin::D2RLPlugin version)
	target_include_directories(native-focus-tests PRIVATE src)
	target_compile_features(native-focus-tests PRIVATE cxx_std_20)
	add_test(NAME native-focus COMMAND native-focus-tests)
	add_executable(progression-tests tests/progression_tests.cpp src/native_progression.cpp src/progression_text.cpp src/tooltip_text.cpp src/controller_text.cpp src/ui_text.cpp src/speech_strings.cpp)
	target_link_libraries(progression-tests PRIVATE D2RLPlugin::D2RLPlugin version)
	target_include_directories(progression-tests PRIVATE src)
	target_compile_features(progression-tests PRIVATE cxx_std_20)
	add_test(NAME progression COMMAND progression-tests)
	add_executable(footstep-tests tests/footstep_tests.cpp src/footsteps.cpp)
	target_include_directories(footstep-tests PRIVATE src)
	target_compile_features(footstep-tests PRIVATE cxx_std_20)
	add_test(NAME footsteps COMMAND footstep-tests)
	add_executable(native-wall-slide-tests tests/native_wall_slide_tests.cpp src/native_wall_slide.cpp src/native_memory.cpp)
	target_include_directories(native-wall-slide-tests PRIVATE src)
	target_link_libraries(native-wall-slide-tests PRIVATE D2RLPlugin::D2RLPlugin version)
	target_compile_features(native-wall-slide-tests PRIVATE cxx_std_20)
	target_compile_definitions(native-wall-slide-tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
	add_test(NAME native-wall-slide COMMAND native-wall-slide-tests)
	add_executable(wall-slide-service-tests tests/wall_slide_service_tests.cpp src/wall_slide_service.cpp src/speech_strings.cpp)
	target_include_directories(wall-slide-service-tests PRIVATE src)
	target_link_libraries(wall-slide-service-tests PRIVATE D2RLPlugin::D2RLPlugin)
	target_compile_features(wall-slide-service-tests PRIVATE cxx_std_20)
	target_compile_definitions(wall-slide-service-tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
	add_test(NAME wall-slide-service COMMAND wall-slide-service-tests)
	add_executable(entity-cues-tests tests/entity_cues_tests.cpp src/entity_cues.cpp src/entity_radar.cpp src/radar_grid.cpp src/speech_strings.cpp)
	target_include_directories(entity-cues-tests PRIVATE src)
	target_compile_features(entity-cues-tests PRIVATE cxx_std_20)
	add_test(NAME entity-cues COMMAND entity-cues-tests)
	add_executable(entity-cue-audio-tests tests/entity_cue_audio_tests.cpp src/entity_cue_synth.cpp src/radar_synth.cpp)
	target_include_directories(entity-cue-audio-tests PRIVATE src)
	target_link_libraries(entity-cue-audio-tests PRIVATE D2RLPlugin::D2RLPlugin)
	target_compile_features(entity-cue-audio-tests PRIVATE cxx_std_20)
	add_test(NAME entity-cue-audio COMMAND entity-cue-audio-tests)
	add_executable(entity-cue-service-tests tests/entity_cue_service_tests.cpp src/entity_cue_service.cpp src/entity_cues.cpp src/speech_strings.cpp src/async_log.cpp)
	target_include_directories(entity-cue-service-tests PRIVATE src)
	target_link_libraries(entity-cue-service-tests PRIVATE D2RLPlugin::D2RLPlugin)
	target_compile_features(entity-cue-service-tests PRIVATE cxx_std_20)
	target_compile_definitions(entity-cue-service-tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
	add_test(NAME entity-cue-service COMMAND entity-cue-service-tests)
	add_executable(entity-radar-tests tests/entity_radar_tests.cpp src/entity_radar.cpp src/radar_grid.cpp src/speech_strings.cpp)
	target_include_directories(entity-radar-tests PRIVATE src)
	target_compile_features(entity-radar-tests PRIVATE cxx_std_20)
	add_test(NAME entity-radar COMMAND entity-radar-tests)
	add_executable(map-survey-tests tests/map_survey_tests.cpp src/map_survey.cpp src/npc_proximity.cpp src/entity_radar.cpp src/navigation_grid.cpp src/radar_grid.cpp src/speech_strings.cpp
		src/native_rooms.cpp src/native_memory.cpp)
	target_include_directories(map-survey-tests PRIVATE src)
	target_compile_features(map-survey-tests PRIVATE cxx_std_20)
	target_compile_definitions(map-survey-tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
	add_test(NAME map-survey COMMAND map-survey-tests)
	add_executable(npc-proximity-tests tests/npc_proximity_tests.cpp src/npc_proximity.cpp src/entity_radar.cpp src/radar_grid.cpp src/speech_strings.cpp)
	target_include_directories(npc-proximity-tests PRIVATE src)
	target_compile_features(npc-proximity-tests PRIVATE cxx_std_20)
	add_test(NAME npc-proximity COMMAND npc-proximity-tests)
	add_executable(target-sight-tests tests/target_sight_tests.cpp src/target_sight.cpp src/entity_radar.cpp src/radar_grid.cpp src/speech_strings.cpp)
	target_include_directories(target-sight-tests PRIVATE src)
	target_compile_features(target-sight-tests PRIVATE cxx_std_20)
	add_test(NAME target-sight COMMAND target-sight-tests)
	add_executable(native-entities-tests tests/native_entities_tests.cpp src/native_entities.cpp src/native_level_exits.cpp src/native_level_areas.cpp src/level_areas.cpp src/room_graph.cpp src/native_rooms.cpp src/native_drlg.cpp src/navigation_grid.cpp src/native_ground_items.cpp src/ground_items.cpp src/ground_item_text.cpp src/native_memory.cpp src/ui_text.cpp src/speech_strings.cpp src/async_log.cpp src/entity_radar.cpp src/radar_grid.cpp)
	target_link_libraries(native-entities-tests PRIVATE D2RLPlugin::D2RLPlugin version)
	target_include_directories(native-entities-tests PRIVATE src)
	target_compile_features(native-entities-tests PRIVATE cxx_std_20)
	target_compile_definitions(native-entities-tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
	add_test(NAME native-entities COMMAND native-entities-tests)
	add_executable(radar-ui-tests tests/radar_ui_tests.cpp src/radar_ui.cpp src/panel_state.cpp)
	target_link_libraries(radar-ui-tests PRIVATE D2RLPlugin::D2RLPlugin version)
	target_include_directories(radar-ui-tests PRIVATE src)
	target_compile_features(radar-ui-tests PRIVATE cxx_std_20)
	add_test(NAME radar-ui COMMAND radar-ui-tests)
	add_executable(native-world-tests tests/native_world_tests.cpp src/native_world.cpp src/native_memory.cpp src/radar_grid.cpp src/radar_config.cpp src/async_log.cpp)
	target_link_libraries(native-world-tests PRIVATE D2RLPlugin::D2RLPlugin version)
	target_include_directories(native-world-tests PRIVATE src)
	target_compile_features(native-world-tests PRIVATE cxx_std_20)
	target_compile_definitions(native-world-tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
	add_test(NAME native-world COMMAND native-world-tests)
	# The speech tables: one per game locale, matched against English.
	add_executable(speech-strings-tests tests/speech_strings_tests.cpp src/speech_strings.cpp)
	target_include_directories(speech-strings-tests PRIVATE src)
	target_compile_features(speech-strings-tests PRIVATE cxx_std_20)
	add_test(NAME speech-strings COMMAND speech-strings-tests)

	add_executable(lootables-tests tests/lootables_tests.cpp)
	target_include_directories(lootables-tests PRIVATE src)
	target_compile_features(lootables-tests PRIVATE cxx_std_20)
	add_test(NAME lootables COMMAND lootables-tests)
	add_executable(item-sizes-tests tests/item_sizes_tests.cpp)
	target_include_directories(item-sizes-tests PRIVATE src)
	target_compile_features(item-sizes-tests PRIVATE cxx_std_20)
	add_test(NAME item-sizes COMMAND item-sizes-tests)
	add_executable(cairn-stones-tests tests/cairn_stones_tests.cpp)
	target_include_directories(cairn-stones-tests PRIVATE src)
	target_compile_features(cairn-stones-tests PRIVATE cxx_std_20)
	add_test(NAME cairn-stones COMMAND cairn-stones-tests)
	add_executable(native-cairn-cache-tests tests/native_cairn_cache_tests.cpp)
	target_include_directories(native-cairn-cache-tests PRIVATE src)
	target_compile_features(native-cairn-cache-tests PRIVATE cxx_std_20)
	add_test(NAME native-cairn-cache COMMAND native-cairn-cache-tests)
	add_executable(cairn-scroll-service-tests tests/cairn_scroll_service_tests.cpp src/cairn_scroll_service.cpp src/speech_strings.cpp)
	target_link_libraries(cairn-scroll-service-tests PRIVATE D2RLPlugin::D2RLPlugin)
	target_include_directories(cairn-scroll-service-tests PRIVATE src)
	target_compile_features(cairn-scroll-service-tests PRIVATE cxx_std_20)
	target_compile_definitions(cairn-scroll-service-tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
	add_test(NAME cairn-scroll-service COMMAND cairn-scroll-service-tests)
	add_executable(ui-text-tests tests/ui_text_tests.cpp src/ui_text.cpp src/speech_strings.cpp)
	add_executable(native-level-exits-tests tests/native_level_exits_tests.cpp src/native_level_exits.cpp src/native_memory.cpp)
	target_include_directories(native-level-exits-tests PRIVATE src)
	target_compile_features(native-level-exits-tests PRIVATE cxx_std_20)
	target_compile_definitions(native-level-exits-tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
	add_test(NAME native-level-exits COMMAND native-level-exits-tests)
	add_executable(native-level-areas-tests tests/native_level_areas_tests.cpp src/native_level_areas.cpp src/level_areas.cpp src/room_graph.cpp src/native_rooms.cpp src/native_memory.cpp src/navigation_grid.cpp src/radar_grid.cpp)
	target_include_directories(native-level-areas-tests PRIVATE src)
	target_compile_features(native-level-areas-tests PRIVATE cxx_std_20)
	target_compile_definitions(native-level-areas-tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
	add_test(NAME native-level-areas COMMAND native-level-areas-tests)
	add_executable(native-rooms-tests tests/native_rooms_tests.cpp src/native_rooms.cpp src/native_memory.cpp)
	target_include_directories(native-rooms-tests PRIVATE src)
	target_compile_features(native-rooms-tests PRIVATE cxx_std_20)
	target_compile_definitions(native-rooms-tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
	add_test(NAME native-rooms COMMAND native-rooms-tests)
	add_executable(room-graph-tests tests/room_graph_tests.cpp src/room_graph.cpp src/native_rooms.cpp src/native_memory.cpp src/navigation_grid.cpp src/radar_grid.cpp)
	target_include_directories(room-graph-tests PRIVATE src)
	target_compile_features(room-graph-tests PRIVATE cxx_std_20)
	target_compile_definitions(room-graph-tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
	add_test(NAME room-graph COMMAND room-graph-tests)
	add_executable(navigation-policy-tests tests/navigation_policy_tests.cpp src/navigation_policy.cpp src/beacon_navigation.cpp
		src/beacon.cpp src/room_graph.cpp src/native_rooms.cpp src/native_memory.cpp src/navigation_grid.cpp src/radar_grid.cpp src/speech_strings.cpp)
	target_include_directories(navigation-policy-tests PRIVATE src)
	target_compile_features(navigation-policy-tests PRIVATE cxx_std_20)
	target_compile_definitions(navigation-policy-tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
	add_test(NAME navigation-policy COMMAND navigation-policy-tests)
	add_executable(native-drlg-tests tests/native_drlg_tests.cpp src/native_drlg.cpp src/native_memory.cpp)
	target_include_directories(native-drlg-tests PRIVATE src)
	target_compile_features(native-drlg-tests PRIVATE cxx_std_20)
	target_compile_definitions(native-drlg-tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
	add_test(NAME native-drlg COMMAND native-drlg-tests)
	# The stock automap's current layer: cell decoding and the guarded tree walk.
	add_executable(automap-tests tests/automap_tests.cpp src/automap_cells.cpp)
	target_include_directories(automap-tests PRIVATE src)
	target_compile_features(automap-tests PRIVATE cxx_std_20)
	target_compile_definitions(automap-tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
	add_test(NAME automap COMMAND automap-tests)
	add_executable(corpse-memory-tests tests/corpse_memory_tests.cpp src/corpse_memory.cpp src/room_graph.cpp src/navigation_grid.cpp src/native_rooms.cpp src/native_memory.cpp src/entity_radar.cpp src/radar_grid.cpp src/speech_strings.cpp)
	target_include_directories(corpse-memory-tests PRIVATE src)
	target_compile_features(corpse-memory-tests PRIVATE cxx_std_20)
	target_compile_definitions(corpse-memory-tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
	add_test(NAME corpse-memory COMMAND corpse-memory-tests)
	target_include_directories(ui-text-tests PRIVATE src)
	target_compile_features(ui-text-tests PRIVATE cxx_std_20)
	add_test(NAME ui-text COMMAND ui-text-tests)
	add_executable(native-memory-tests tests/native_memory_tests.cpp src/native_memory.cpp)
	target_include_directories(native-memory-tests PRIVATE src)
	target_compile_features(native-memory-tests PRIVATE cxx_std_20)
	target_compile_definitions(native-memory-tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
	add_test(NAME native-memory COMMAND native-memory-tests)
	add_executable(native-ui-tests tests/native_ui_tests.cpp src/native_ui.cpp src/native_memory.cpp src/ui_text.cpp src/speech_strings.cpp src/localization.cpp src/quest_log.cpp)
	target_link_libraries(native-ui-tests PRIVATE D2RLPlugin::D2RLPlugin version)
	target_include_directories(native-ui-tests PRIVATE src)
	target_compile_features(native-ui-tests PRIVATE cxx_std_20)
	target_compile_definitions(native-ui-tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
	add_test(NAME native-ui COMMAND native-ui-tests)
	add_executable(game-text-reader-tests tests/game_text_reader_tests.cpp src/async_log.cpp src/game_text_reader.cpp src/native_memory.cpp src/ui_text.cpp src/ui_notifications.cpp src/speech_strings.cpp)
	target_link_libraries(game-text-reader-tests PRIVATE D2RLPlugin::D2RLPlugin version)
	target_include_directories(game-text-reader-tests PRIVATE src)
	target_compile_features(game-text-reader-tests PRIVATE cxx_std_20)
	target_compile_definitions(game-text-reader-tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
	add_test(NAME game-text-reader COMMAND game-text-reader-tests)
	add_executable(ui-notifications-tests tests/ui_notifications_tests.cpp src/ui_notifications.cpp src/ui_text.cpp src/speech_strings.cpp)
	target_include_directories(ui-notifications-tests PRIVATE src)
	target_compile_features(ui-notifications-tests PRIVATE cxx_std_20)
	add_test(NAME ui-notifications COMMAND ui-notifications-tests)
	add_executable(native-tooltips-tests tests/native_tooltips_tests.cpp src/native_tooltips.cpp src/native_memory.cpp)
	target_link_libraries(native-tooltips-tests PRIVATE D2RLPlugin::D2RLPlugin version)
	target_include_directories(native-tooltips-tests PRIVATE src)
	target_compile_features(native-tooltips-tests PRIVATE cxx_std_20)
	target_compile_definitions(native-tooltips-tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
	add_test(NAME native-tooltips COMMAND native-tooltips-tests)
	add_executable(tooltip-delay-tests tests/tooltip_delay_tests.cpp src/tooltip_delay.cpp src/native_memory.cpp src/async_log.cpp)
	target_link_libraries(tooltip-delay-tests PRIVATE D2RLPlugin::D2RLPlugin version)
	target_include_directories(tooltip-delay-tests PRIVATE src)
	target_compile_features(tooltip-delay-tests PRIVATE cxx_std_20)
	target_compile_definitions(tooltip-delay-tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
	add_test(NAME tooltip-delay COMMAND tooltip-delay-tests)
	add_executable(character-stats-tests tests/character_stats_tests.cpp src/character_stats.cpp src/character_stats_reader.cpp src/ui_text.cpp src/speech_strings.cpp)
	target_link_libraries(character-stats-tests PRIVATE D2RLPlugin::D2RLPlugin version)
	target_include_directories(character-stats-tests PRIVATE src)
	target_compile_features(character-stats-tests PRIVATE cxx_std_20)
	add_test(NAME character-stats COMMAND character-stats-tests)
	add_executable(advanced-stats-tests tests/advanced_stats_tests.cpp src/advanced_stats_reader.cpp src/mercenary_stats.cpp src/character_stats.cpp src/character_stats_reader.cpp src/tooltip_text.cpp src/ui_text.cpp src/speech_strings.cpp)
	target_link_libraries(advanced-stats-tests PRIVATE D2RLPlugin::D2RLPlugin version)
	target_include_directories(advanced-stats-tests PRIVATE src)
	target_compile_features(advanced-stats-tests PRIVATE cxx_std_20)
	add_test(NAME advanced-stats COMMAND advanced-stats-tests)
	add_executable(hire-stats-tests tests/hire_stats_tests.cpp src/hire_stats.cpp src/tooltip_text.cpp src/ui_text.cpp src/speech_strings.cpp src/async_log.cpp)
	target_link_libraries(hire-stats-tests PRIVATE D2RLPlugin::D2RLPlugin version)
	target_include_directories(hire-stats-tests PRIVATE src)
	target_compile_features(hire-stats-tests PRIVATE cxx_std_20)
	add_test(NAME hire-stats COMMAND hire-stats-tests)
	add_executable(interaction-reader-tests tests/interaction_reader_tests.cpp src/async_log.cpp src/interaction_reader.cpp src/click_words.cpp src/equip_attempt.cpp src/ground_item_text.cpp src/panel_state.cpp src/control_hints.cpp src/progression_text.cpp src/tooltip_text.cpp src/ui_text.cpp src/speech_strings.cpp)
	target_link_libraries(interaction-reader-tests PRIVATE D2RLPlugin::D2RLPlugin version)
	target_include_directories(interaction-reader-tests PRIVATE src)
	target_compile_features(interaction-reader-tests PRIVATE cxx_std_20)
	add_test(NAME interaction-reader COMMAND interaction-reader-tests)
	add_executable(inventory-cursor-tests tests/inventory_cursor_tests.cpp src/inventory_cursor.cpp)
	target_link_libraries(inventory-cursor-tests PRIVATE D2RLPlugin::D2RLPlugin version)
	target_include_directories(inventory-cursor-tests PRIVATE src)
	target_compile_features(inventory-cursor-tests PRIVATE cxx_std_20)
	add_test(NAME inventory-cursor COMMAND inventory-cursor-tests)
	add_executable(async-log-tests tests/async_log_tests.cpp src/async_log.cpp)
	target_link_libraries(async-log-tests PRIVATE D2RLPlugin::D2RLPlugin version)
	target_include_directories(async-log-tests PRIVATE src)
	target_compile_features(async-log-tests PRIVATE cxx_std_20)
	add_test(NAME async-log COMMAND async-log-tests)
	add_executable(equip-attempt-tests tests/equip_attempt_tests.cpp src/equip_attempt.cpp)
	target_include_directories(equip-attempt-tests PRIVATE src)
	target_compile_features(equip-attempt-tests PRIVATE cxx_std_20)
	add_test(NAME equip-attempt COMMAND equip-attempt-tests)
	add_executable(tooltip-text-tests tests/tooltip_text_tests.cpp src/tooltip_text.cpp src/ui_text.cpp src/speech_strings.cpp)
	target_include_directories(tooltip-text-tests PRIVATE src)
	target_compile_features(tooltip-text-tests PRIVATE cxx_std_20)
	add_test(NAME tooltip-text COMMAND tooltip-text-tests)
	add_executable(tab-reader-tests tests/tab_reader_tests.cpp src/tab_reader.cpp src/ui_text.cpp src/speech_strings.cpp)
	target_link_libraries(tab-reader-tests PRIVATE D2RLPlugin::D2RLPlugin)
	target_include_directories(tab-reader-tests PRIVATE src)
	target_compile_features(tab-reader-tests PRIVATE cxx_std_20)
	add_test(NAME tab-reader COMMAND tab-reader-tests)
	add_executable(quest-log-tests tests/quest_log_tests.cpp src/quest_log.cpp)
	target_include_directories(quest-log-tests PRIVATE src)
	target_compile_features(quest-log-tests PRIVATE cxx_std_20)
	add_test(NAME quest-log COMMAND quest-log-tests)
	add_executable(quest-progress-tests tests/quest_progress_tests.cpp src/quest_progress.cpp src/quest_log.cpp src/ui_text.cpp src/speech_strings.cpp)
	target_include_directories(quest-progress-tests PRIVATE src)
	target_compile_features(quest-progress-tests PRIVATE cxx_std_20)
	add_test(NAME quest-progress COMMAND quest-progress-tests)
	add_executable(vitals-tests tests/vitals_tests.cpp src/vitals.cpp)
	target_include_directories(vitals-tests PRIVATE src)
	target_compile_features(vitals-tests PRIVATE cxx_std_20)
	add_test(NAME vitals COMMAND vitals-tests)
	add_executable(weapon-swap-tests tests/weapon_swap_tests.cpp src/weapon_swap.cpp src/speech_strings.cpp)
	target_include_directories(weapon-swap-tests PRIVATE src)
	target_compile_features(weapon-swap-tests PRIVATE cxx_std_20)
	add_test(NAME weapon-swap COMMAND weapon-swap-tests)
	add_executable(trade-tests tests/trade_tests.cpp src/trade.cpp src/speech_strings.cpp)
	target_include_directories(trade-tests PRIVATE src)
	target_compile_features(trade-tests PRIVATE cxx_std_20)
	add_test(NAME trade COMMAND trade-tests)
	add_executable(quest-service-tests tests/quest_service_tests.cpp src/quest_service.cpp src/quest_progress.cpp src/quest_log.cpp src/native_memory.cpp src/ui_text.cpp src/speech_strings.cpp)
	target_link_libraries(quest-service-tests PRIVATE D2RLPlugin::D2RLPlugin version)
	target_include_directories(quest-service-tests PRIVATE src)
	target_compile_features(quest-service-tests PRIVATE cxx_std_20)
	target_compile_definitions(quest-service-tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
	add_test(NAME quest-service COMMAND quest-service-tests)
	add_executable(modal-reader-tests tests/modal_reader_tests.cpp src/modal_reader.cpp src/control_hints.cpp src/ui_text.cpp src/speech_strings.cpp)
	target_link_libraries(modal-reader-tests PRIVATE D2RLPlugin::D2RLPlugin)
	target_include_directories(modal-reader-tests PRIVATE src)
	target_compile_features(modal-reader-tests PRIVATE cxx_std_20)
	add_test(NAME modal-reader COMMAND modal-reader-tests)
	add_executable(pad-filter-tests tests/pad_filter_tests.cpp src/pad_filter.cpp)
	target_link_libraries(pad-filter-tests PRIVATE D2RLPlugin::D2RLPlugin)
	target_include_directories(pad-filter-tests PRIVATE src)
	target_compile_features(pad-filter-tests PRIVATE cxx_std_20)
	target_compile_definitions(pad-filter-tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
	add_test(NAME pad-filter COMMAND pad-filter-tests)
	# Keyboard play: the routes and choices behind the walking and fighting keys.
	add_executable(retreat-tests tests/retreat_tests.cpp src/retreat.cpp src/navigation_grid.cpp src/radar_grid.cpp)
	target_include_directories(retreat-tests PRIVATE src)
	target_compile_features(retreat-tests PRIVATE cxx_std_20)
	add_test(NAME retreat COMMAND retreat-tests)
	add_executable(auto-walk-tests tests/auto_walk_tests.cpp src/auto_walk.cpp src/beacon.cpp src/radar_grid.cpp)
	target_include_directories(auto-walk-tests PRIVATE src)
	target_compile_features(auto-walk-tests PRIVATE cxx_std_20)
	add_test(NAME auto-walk COMMAND auto-walk-tests)
	add_executable(combat-target-tests tests/combat_target_tests.cpp src/combat_target.cpp src/auto_walk.cpp src/entity_radar.cpp src/navigation_grid.cpp src/radar_grid.cpp src/speech_strings.cpp)
	target_include_directories(combat-target-tests PRIVATE src)
	target_compile_features(combat-target-tests PRIVATE cxx_std_20)
	add_test(NAME combat-target COMMAND combat-target-tests)
	add_executable(explore-goal-tests tests/explore_goal_tests.cpp src/explore_goal.cpp src/room_graph.cpp src/native_rooms.cpp src/native_memory.cpp src/navigation_grid.cpp src/radar_grid.cpp)
	target_include_directories(explore-goal-tests PRIVATE src)
	target_compile_features(explore-goal-tests PRIVATE cxx_std_20)
	target_compile_definitions(explore-goal-tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
	add_test(NAME explore-goal COMMAND explore-goal-tests)
	add_executable(grid-jump-tests tests/grid_jump_tests.cpp src/grid_jump.cpp)
	target_include_directories(grid-jump-tests PRIVATE src)
	target_compile_features(grid-jump-tests PRIVATE cxx_std_20)
	add_test(NAME grid-jump COMMAND grid-jump-tests)
	add_executable(menu-cursor-tests tests/menu_cursor_tests.cpp src/menu_cursor.cpp src/grid_jump.cpp)
	target_include_directories(menu-cursor-tests PRIVATE src)
	target_compile_features(menu-cursor-tests PRIVATE cxx_std_20)
	add_test(NAME menu-cursor COMMAND menu-cursor-tests)
	add_executable(click-words-tests tests/click_words_tests.cpp src/click_words.cpp src/speech_strings.cpp)
	target_include_directories(click-words-tests PRIVATE src)
	target_compile_features(click-words-tests PRIVATE cxx_std_20)
	add_test(NAME click-words COMMAND click-words-tests)
	add_executable(skill-jump-tests tests/skill_jump_tests.cpp src/skill_grid.cpp)
	target_include_directories(skill-jump-tests PRIVATE src)
	target_compile_features(skill-jump-tests PRIVATE cxx_std_20)
	add_test(NAME skill-jump COMMAND skill-jump-tests)
	add_executable(skill-attacks-tests tests/skill_attacks_tests.cpp)
	target_include_directories(skill-attacks-tests PRIVATE src)
	target_compile_features(skill-attacks-tests PRIVATE cxx_std_20)
	add_test(NAME skill-attacks COMMAND skill-attacks-tests)
	add_executable(settings-list-tests tests/settings_list_tests.cpp src/settings_list.cpp)
	target_include_directories(settings-list-tests PRIVATE src)
	target_compile_features(settings-list-tests PRIVATE cxx_std_20)
	add_test(NAME settings-list COMMAND settings-list-tests)
	if(MSVC)
		target_compile_options(settings-list-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(skill-attacks-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(skill-jump-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(grid-jump-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(menu-cursor-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(click-words-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(explore-goal-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(combat-target-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(retreat-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(auto-walk-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(entity-cues-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(entity-cue-audio-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(entity-cue-service-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(modal-reader-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(rooms-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(exit-review-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(terrain-overview-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(pad-filter-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(quest-service-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(vitals-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(character-stats-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(hire-stats-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(quest-progress-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(beacon-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(beacon-service-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(navigation-grid-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(radar-engine-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(radar-passage-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(radar-structure-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(radar-snapshot-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(radar-audio-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(radar-readout-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(controller-help-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(skill-bar-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(native-cursor-gesture-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(skill-navigation-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(native-focus-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(progression-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(footstep-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(native-wall-slide-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(wall-slide-service-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(radar-ui-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(native-world-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(async-log-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(tab-reader-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(map-survey-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(quest-log-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(ui-text-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(native-level-exits-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(native-level-areas-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(native-rooms-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(room-graph-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(navigation-policy-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(corpse-memory-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(speech-strings-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(lootables-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(item-sizes-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(cairn-stones-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(native-cairn-cache-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(cairn-scroll-service-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(native-memory-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(automap-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(native-ui-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(game-text-reader-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(ui-notifications-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(native-tooltips-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(tooltip-delay-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(interaction-reader-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
	endif()
endif()

add_custom_command(TARGET d2r-access POST_BUILD
	COMMAND "${CMAKE_COMMAND}" -E make_directory "${D2R_PLUGINS_DIR}"
	COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:d2r-access>" "${D2R_PLUGINS_DIR}/"
	COMMAND "${CMAKE_COMMAND}" -E make_directory "${D2R_PLUGINS_DIR}/d2r-access"
	COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${PRISM_DIR}/bin/prism.dll" "${D2R_PLUGINS_DIR}/d2r-access/"
	COMMENT "Deploying d2r-access plugin and prism.dll to ${D2R_PLUGINS_DIR}"
	VERBATIM
)
