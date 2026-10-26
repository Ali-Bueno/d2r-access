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
	src/death_place.cpp
	src/death_place_service.cpp
	src/boss_life.cpp
	src/boss_life_service.cpp
	src/tutorial.cpp
	src/tutorial_records.cpp
	src/tutorial_service.cpp
	src/tutorial_text.cpp
	src/native_preset_map.cpp
	src/passages.cpp
	src/places.cpp
	src/place_service.cpp
	src/doorways.cpp
	src/secret_doors.cpp
	src/inventory_arrivals.cpp
	src/inventory_arrival_service.cpp
	src/cube_state.cpp
	src/cube_notice.cpp
	src/item_hand.cpp
	src/attack_reach.cpp
	src/crosshair.cpp
	src/native_attack.cpp
	src/async_log.cpp
	src/auto_detail.cpp
	src/control_hints.cpp
	src/tab_reader.cpp
	src/tooltip_text.cpp
	src/text_field.cpp
	src/text_field_edit.cpp
	src/grid_edge.cpp
	src/grid_edge_service.cpp
	src/stash_count.cpp
	src/sound_glossary.cpp
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
	src/pad_typing.cpp
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
	src/mod_sounds.cpp
	src/navigation_policy.cpp
	src/radar.cpp
	src/navigation_grid.cpp
	src/radar_passage.cpp
	src/radar_structure.cpp
	src/radar_grid.cpp
	src/radar_audio.cpp
	src/radar_trace.cpp
	src/radar_readout.cpp
	src/spoken_direction.cpp
	src/route_legs.cpp
	src/route_follow.cpp
	src/step_counter.cpp
	src/crossing_memory.cpp
	src/radar_synth.cpp
	src/radar_service.cpp
	src/wall_bump.cpp
	src/wall_tones.cpp
	src/wall_tone_audio.cpp
	src/wall_tone_service.cpp
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
	src/native_pets.cpp
	src/town_npcs.cpp
	src/monster_plate.cpp
	src/monster_names.cpp
	src/native_cairn.cpp
	src/cairn_scroll_service.cpp
	src/native_level_exits.cpp
	src/native_level_areas.cpp
	src/level_areas.cpp
	src/native_drlg.cpp
	src/native_rooms.cpp
	src/room_graph.cpp
	src/room_catalogue.cpp
	src/corpse_memory.cpp
	src/rooms.cpp
	src/room_service.cpp
	src/room_sides.cpp
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
	src/quest_seals.cpp
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
	src/low_life_audio.cpp
	src/status_effects.cpp
	src/speech_priority.cpp
	src/minion_report.cpp
	src/party_reader.cpp
	src/equipment_census.cpp
	src/native_weapon_set.cpp
	src/weapon_swap.cpp
	src/weapon_swap_service.cpp
	src/native_trade.cpp
	src/trade.cpp
	src/trade_service.cpp
	src/belt_reader.cpp
	src/native_belt.cpp
	src/skill_bar.cpp
	src/native_skill_bar.cpp
	src/skill_bar_service.cpp
	src/native_status.cpp
	src/widgets.cpp
	src/auto_walk.cpp
	src/level_route.cpp
	src/pad_links.cpp
	src/combat_target.cpp
	src/explore_goal.cpp
	src/grid_jump.cpp
	src/menu_cursor.cpp
	src/click_words.cpp
	src/menu_mouse.cpp
	src/native_sort.cpp
	src/native_class_choice.cpp
	src/native_list_rows.cpp
	src/native_skill_hand.cpp
	src/native_skill_icons.cpp
	src/native_swap.cpp
	src/native_hover.cpp
	src/native_hit_chance.cpp
	src/native_waypoints.cpp
	src/native_quest_flags.cpp
	src/descriptions.cpp
	src/native_value_controls.cpp
	src/native_belt_rows.cpp
	src/native_belt_fill.cpp
	src/native_actions.cpp
	src/keyboard_service.cpp
	src/keyboard_menu.cpp
	src/settings_list.cpp
	src/settings_store.cpp
	plugin.rc
)
target_link_libraries(d2r-access PRIVATE D2RLPlugin::D2RLPlugin winmm version)
d2rlplugin_embed_config(TARGET d2r-access FILE "${CMAKE_CURRENT_SOURCE_DIR}/radar.toml")
# The descriptions that come with the mod, read under the player's own file (docs/descriptions.md).
set(D2R_DESCRIPTIONS "${CMAKE_CURRENT_SOURCE_DIR}/data/descriptions.txt")
set(D2R_DESCRIPTIONS_RC "${CMAKE_CURRENT_BINARY_DIR}/descriptions.rc")
file(CONFIGURE OUTPUT "${D2R_DESCRIPTIONS_RC}" CONTENT "DESCRIPTIONS RCDATA \"${D2R_DESCRIPTIONS}\"\n")
set_property(SOURCE "${D2R_DESCRIPTIONS_RC}" APPEND PROPERTY OBJECT_DEPENDS "${D2R_DESCRIPTIONS}")
target_sources(d2r-access PRIVATE "${D2R_DESCRIPTIONS_RC}")
target_include_directories(d2r-access PRIVATE "${PRISM_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}/src")
set_target_properties(d2r-access PROPERTIES
	CXX_STANDARD 20
	CXX_STANDARD_REQUIRED TRUE
	CXX_EXTENSIONS FALSE
	OUTPUT_NAME "d2rl-d2r-access"
)
target_compile_definitions(d2r-access PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX UNICODE _UNICODE)
# The test release name spoken at start (tools/test-release.ps1 passes its tag); empty for a local build.
set(D2R_ACCESS_RELEASE "" CACHE STRING "Release name spoken at start; empty for a local build")
set_property(SOURCE src/plugin.cpp APPEND PROPERTY COMPILE_DEFINITIONS "D2R_ACCESS_RELEASE=\"${D2R_ACCESS_RELEASE}\"")
if(MSVC)
	target_compile_options(d2r-access PRIVATE /W4 /permissive- /EHsc /utf-8)
endif()

include(CTest)
include(cmake/GroundItems.cmake)
# Each shared src/*.cpp is compiled once as an OBJECT library; a test links the objects of exactly
# the src files it lists, so stubs for unlisted sources keep working. Other sources stay direct.
function(d2r_test name test_source)
	set(direct ${test_source})
	set(objects)
	foreach(source IN LISTS ARGN)
		if(source MATCHES "^src/(.+)\\.cpp$")
			set(object "testobj-${CMAKE_MATCH_1}")
			if(NOT TARGET ${object})
				add_library(${object} OBJECT ${source})
				target_include_directories(${object} PRIVATE src)
				target_link_libraries(${object} PRIVATE D2RLPlugin::D2RLPlugin)
				target_compile_features(${object} PRIVATE cxx_std_20)
				target_compile_definitions(${object} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
				if(MSVC)
					target_compile_options(${object} PRIVATE /W4 /permissive- /EHsc /utf-8)
				endif()
			endif()
			list(APPEND objects ${object})
		else()
			list(APPEND direct ${source})
		endif()
	endforeach()
	add_executable(${name} ${direct})
	target_include_directories(${name} PRIVATE src)
	target_compile_features(${name} PRIVATE cxx_std_20)
	target_compile_definitions(${name} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
	if(MSVC)
		target_compile_options(${name} PRIVATE /W4 /permissive- /EHsc /utf-8)
	endif()
	if(objects)
		target_link_libraries(${name} PRIVATE ${objects})
	endif()
endfunction()

if(BUILD_TESTING)
	d2r_test(callback-boundary-tests tests/callback_boundary_tests.cpp tests/allocation_failure.cpp src/navigation_grid.cpp src/radar_grid.cpp)
	add_test(NAME callback-boundary COMMAND callback-boundary-tests)
	d2r_test(ui-thread-pump-tests tests/ui_thread_pump_tests.cpp tests/allocation_failure.cpp src/ui_thread_pump.cpp src/async_log.cpp)
	target_link_libraries(ui-thread-pump-tests PRIVATE D2RLPlugin::D2RLPlugin)
	add_test(NAME ui-thread-pump COMMAND ui-thread-pump-tests)
	add_library(process-lifetime-fixture SHARED tests/process_lifetime_fixture.cpp)
	target_include_directories(process-lifetime-fixture PRIVATE src)
	target_compile_features(process-lifetime-fixture PRIVATE cxx_std_20)
	add_executable(process-lifetime-tests tests/process_lifetime_tests.cpp)
	target_include_directories(process-lifetime-tests PRIVATE src)
	target_compile_features(process-lifetime-tests PRIVATE cxx_std_20)
	add_dependencies(process-lifetime-tests process-lifetime-fixture)
	add_test(NAME process-lifetime COMMAND process-lifetime-tests $<TARGET_FILE:process-lifetime-fixture>)
	if(MSVC)
		foreach(target callback-boundary-tests ui-thread-pump-tests process-lifetime-fixture process-lifetime-tests)
			target_compile_options(${target} PRIVATE /W4 /permissive- /EHsc /utf-8)
			target_compile_definitions(${target} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
		endforeach()
	endif()
	d2r_test(attack-reach-tests tests/attack_reach_tests.cpp src/attack_reach.cpp src/entity_cues.cpp src/combat_target.cpp src/auto_walk.cpp src/navigation_grid.cpp src/entity_radar.cpp src/radar_grid.cpp src/speech_strings.cpp)
	add_test(NAME attack-reach COMMAND attack-reach-tests)
	d2r_test(crosshair-tests tests/crosshair_tests.cpp src/crosshair.cpp src/entity_cues.cpp src/combat_target.cpp src/auto_walk.cpp src/navigation_grid.cpp src/entity_radar.cpp src/radar_grid.cpp src/speech_strings.cpp)
	add_test(NAME crosshair COMMAND crosshair-tests)
	d2r_test(death-place-tests tests/death_place_tests.cpp src/death_place.cpp)
	add_test(NAME death-place COMMAND death-place-tests)
	d2r_test(boss-life-tests tests/boss_life_tests.cpp src/boss_life.cpp src/vitals.cpp)
	add_test(NAME boss-life COMMAND boss-life-tests)
	d2r_test(tutorial-tests tests/tutorial_tests.cpp src/tutorial.cpp src/tutorial_text.cpp)
	add_test(NAME tutorial COMMAND tutorial-tests)
	d2r_test(inventory-arrivals-tests tests/inventory_arrivals_tests.cpp src/inventory_arrivals.cpp)
	add_test(NAME inventory-arrivals COMMAND inventory-arrivals-tests)
	d2r_test(item-hand-tests tests/item_hand_tests.cpp src/item_hand.cpp)
	add_test(NAME item-hand COMMAND item-hand-tests)
	d2r_test(passages-tests tests/passages_tests.cpp src/passages.cpp)
	add_test(NAME passages COMMAND passages-tests)
	d2r_test(places-tests tests/places_tests.cpp src/places.cpp)
	add_test(NAME places COMMAND places-tests)
	d2r_test(doorways-tests tests/doorways_tests.cpp src/doorways.cpp src/secret_doors.cpp src/rooms.cpp src/radar_grid.cpp src/speech_strings.cpp)
	target_link_libraries(doorways-tests PRIVATE D2RLPlugin::D2RLPlugin)
	add_test(NAME doorways COMMAND doorways-tests)
	# Full-resolution native body connectivity, shared with the reactive radar.
	d2r_test(beacon-tests tests/beacon_tests.cpp src/beacon.cpp src/radar_grid.cpp)
	add_test(NAME beacon COMMAND beacon-tests)
	d2r_test(beacon-service-tests tests/beacon_service_tests.cpp src/beacon_service.cpp src/crossing_memory.cpp src/route_legs.cpp src/route_follow.cpp src/spoken_direction.cpp src/beacon.cpp src/beacon_navigation.cpp src/level_route.cpp src/navigation_mode.cpp src/navigation_policy.cpp src/navigation_grid.cpp src/radar_grid.cpp src/room_graph.cpp src/level_areas.cpp src/speech_strings.cpp src/async_log.cpp)
	target_link_libraries(beacon-service-tests PRIVATE D2RLPlugin::D2RLPlugin)
	add_test(NAME beacon-service COMMAND beacon-service-tests)
	# The exploration memory (seen/visited rooms), its disk form and the open-ground frontier.
	d2r_test(exploration-tests tests/exploration_tests.cpp src/explored_rooms.cpp src/explored_store.cpp src/explored_store_worker.cpp src/frontier.cpp src/navigation_grid.cpp src/radar_grid.cpp src/native_rooms.cpp src/native_memory.cpp)
	add_test(NAME exploration COMMAND exploration-tests)
	d2r_test(exploration-cues-tests tests/exploration_cues_tests.cpp src/exploration_cues.cpp src/explored_rooms.cpp)
	add_test(NAME exploration-cues COMMAND exploration-cues-tests)
	d2r_test(exploration-cue-service-tests tests/exploration_cue_service_tests.cpp src/exploration_cue_service.cpp src/exploration_cues.cpp src/explored_rooms.cpp src/ground_items.cpp src/ground_item_text.cpp src/speech_strings.cpp src/tutorial.cpp)
	target_link_libraries(exploration-cue-service-tests PRIVATE D2RLPlugin::D2RLPlugin version)
	add_test(NAME exploration-cue-service COMMAND exploration-cue-service-tests)
	d2r_test(zone-transitions-tests tests/zone_transitions_tests.cpp src/zone_transitions.cpp src/passages.cpp src/rooms.cpp src/radar_grid.cpp)
	add_test(NAME zone-transitions COMMAND zone-transitions-tests)
	d2r_test(navigation-grid-tests tests/navigation_grid_tests.cpp src/navigation_grid.cpp src/radar_grid.cpp)
	add_test(NAME navigation-grid COMMAND navigation-grid-tests)
	d2r_test(rooms-tests tests/rooms_tests.cpp src/rooms.cpp src/radar_grid.cpp)
	add_test(NAME rooms COMMAND rooms-tests)
	d2r_test(room-sides-tests tests/room_sides_tests.cpp src/room_sides.cpp src/spoken_direction.cpp src/speech_strings.cpp)
	add_test(NAME room-sides COMMAND room-sides-tests)
	d2r_test(terrain-overview-tests tests/terrain_overview_tests.cpp src/terrain_overview.cpp src/spoken_direction.cpp src/explored_rooms.cpp src/entity_radar.cpp src/radar_grid.cpp src/speech_strings.cpp)
	add_test(NAME terrain-overview COMMAND terrain-overview-tests)
	# The sonar radar: passage scan, sensor state machine, structural filtering,
	# the cue synthesizer and the spoken readout.
	set(RADAR_ENGINE_SOURCES src/radar.cpp src/radar_passage.cpp src/navigation_grid.cpp src/radar_structure.cpp src/radar_grid.cpp src/rooms.cpp)
	d2r_test(radar-engine-tests tests/radar_engine_tests.cpp ${RADAR_ENGINE_SOURCES})
	add_test(NAME radar-engine COMMAND radar-engine-tests)
	d2r_test(radar-passage-tests tests/radar_passage_tests.cpp ${RADAR_ENGINE_SOURCES})
	add_test(NAME radar-passage COMMAND radar-passage-tests)
	d2r_test(radar-structure-tests tests/radar_structure_tests.cpp ${RADAR_ENGINE_SOURCES})
	add_test(NAME radar-structure COMMAND radar-structure-tests)
	# The snapshot test supplies its own AsyncLog sink, so it must not link the real one.
	d2r_test(radar-snapshot-tests tests/radar_snapshot_tests.cpp src/radar_trace.cpp src/radar_structure.cpp src/radar_grid.cpp)
	target_link_libraries(radar-snapshot-tests PRIVATE D2RLPlugin::D2RLPlugin version)
	add_test(NAME radar-snapshot COMMAND radar-snapshot-tests)
	d2r_test(radar-audio-tests tests/radar_audio_tests.cpp src/radar_synth.cpp)
	target_link_libraries(radar-audio-tests PRIVATE D2RLPlugin::D2RLPlugin version)
	add_test(NAME radar-audio COMMAND radar-audio-tests)
	d2r_test(radar-readout-tests tests/radar_readout_tests.cpp src/radar_readout.cpp src/spoken_direction.cpp ${RADAR_ENGINE_SOURCES})
	add_test(NAME radar-readout COMMAND radar-readout-tests)
	d2r_test(wall-bump-tests tests/wall_bump_tests.cpp src/wall_bump.cpp src/radar_grid.cpp)
	add_test(NAME wall-bump COMMAND wall-bump-tests)
	d2r_test(wall-tones-tests tests/wall_tones_tests.cpp src/wall_tones.cpp src/radar_grid.cpp)
	add_test(NAME wall-tones COMMAND wall-tones-tests)
	d2r_test(controller-help-tests tests/controller_help_tests.cpp src/native_controller_help.cpp src/controller_text.cpp src/native_memory.cpp src/ui_text.cpp src/speech_strings.cpp)
	target_link_libraries(controller-help-tests PRIVATE D2RLPlugin::D2RLPlugin version)
	add_test(NAME controller-help COMMAND controller-help-tests)
	d2r_test(skill-bar-tests tests/skill_bar_tests.cpp src/skill_bar.cpp src/controller_text.cpp src/ui_text.cpp src/speech_strings.cpp)
	target_link_libraries(skill-bar-tests PRIVATE D2RLPlugin::D2RLPlugin)
	add_test(NAME skill-bar COMMAND skill-bar-tests)
	d2r_test(native-cursor-gesture-tests tests/native_cursor_gesture_tests.cpp src/native_cursor_gesture.cpp)
	add_test(NAME native-cursor-gesture COMMAND native-cursor-gesture-tests)
	d2r_test(skill-navigation-tests tests/skill_navigation_tests.cpp src/skill_grid.cpp src/skill_navigation.cpp src/progression_text.cpp src/ui_text.cpp src/speech_strings.cpp src/async_log.cpp)
	target_link_libraries(skill-navigation-tests PRIVATE D2RLPlugin::D2RLPlugin version)
	add_test(NAME skill-navigation COMMAND skill-navigation-tests)
	d2r_test(native-focus-tests tests/native_focus_tests.cpp src/native_focus.cpp)
	d2r_test(frontend-native-fallback-tests tests/frontend_native_fallback_tests.cpp src/frontend_native_fallback.cpp src/ui_text.cpp src/speech_strings.cpp)
	target_include_directories(frontend-native-fallback-tests PRIVATE src tests)
	add_test(NAME frontend-native-fallback-tests COMMAND frontend-native-fallback-tests)
	d2r_test(multiplayer-status-tests tests/multiplayer_status_tests.cpp src/multiplayer_status.cpp)
	target_include_directories(multiplayer-status-tests PRIVATE src tests)
	add_test(NAME multiplayer-status COMMAND multiplayer-status-tests)
	d2r_test(menu-navigation-policy-tests tests/menu_navigation_policy_tests.cpp src/menu_navigation_policy.cpp)
	target_include_directories(menu-navigation-policy-tests PRIVATE src tests)
	add_test(NAME menu-navigation-policy COMMAND menu-navigation-policy-tests)
	d2r_test(menu-navigation-tests tests/menu_navigation_tests.cpp src/menu_navigation.cpp src/menu_navigation_policy.cpp)
	target_link_libraries(menu-navigation-tests PRIVATE D2RLPlugin::D2RLPlugin)
	target_include_directories(menu-navigation-tests PRIVATE src tests)
	add_test(NAME menu-navigation COMMAND menu-navigation-tests)
	target_link_libraries(native-focus-tests PRIVATE D2RLPlugin::D2RLPlugin version)
	add_test(NAME native-focus COMMAND native-focus-tests)
	d2r_test(progression-tests tests/progression_tests.cpp src/native_progression.cpp src/progression_text.cpp src/tooltip_text.cpp src/controller_text.cpp src/ui_text.cpp src/speech_strings.cpp)
	target_link_libraries(progression-tests PRIVATE D2RLPlugin::D2RLPlugin version)
	add_test(NAME progression COMMAND progression-tests)
	d2r_test(footstep-tests tests/footstep_tests.cpp src/footsteps.cpp)
	add_test(NAME footsteps COMMAND footstep-tests)
	d2r_test(native-wall-slide-tests tests/native_wall_slide_tests.cpp src/native_wall_slide.cpp src/native_memory.cpp)
	target_link_libraries(native-wall-slide-tests PRIVATE D2RLPlugin::D2RLPlugin version)
	add_test(NAME native-wall-slide COMMAND native-wall-slide-tests)
	d2r_test(wall-slide-service-tests tests/wall_slide_service_tests.cpp src/wall_slide_service.cpp src/speech_strings.cpp)
	target_link_libraries(wall-slide-service-tests PRIVATE D2RLPlugin::D2RLPlugin)
	add_test(NAME wall-slide-service COMMAND wall-slide-service-tests)
	d2r_test(entity-cues-tests tests/entity_cues_tests.cpp src/entity_cues.cpp src/combat_target.cpp src/auto_walk.cpp src/navigation_grid.cpp src/entity_radar.cpp src/radar_grid.cpp src/speech_strings.cpp)
	add_test(NAME entity-cues COMMAND entity-cues-tests)
	d2r_test(entity-cue-audio-tests tests/entity_cue_audio_tests.cpp src/entity_cue_synth.cpp src/radar_synth.cpp)
	target_link_libraries(entity-cue-audio-tests PRIVATE D2RLPlugin::D2RLPlugin)
	add_test(NAME entity-cue-audio COMMAND entity-cue-audio-tests)
	d2r_test(entity-cue-service-tests tests/entity_cue_service_tests.cpp src/entity_cue_service.cpp src/attack_reach.cpp src/crosshair.cpp src/entity_cues.cpp src/combat_target.cpp src/auto_walk.cpp src/navigation_grid.cpp src/radar_grid.cpp src/speech_strings.cpp src/async_log.cpp src/tutorial.cpp)
	target_link_libraries(entity-cue-service-tests PRIVATE D2RLPlugin::D2RLPlugin)
	add_test(NAME entity-cue-service COMMAND entity-cue-service-tests)
	d2r_test(entity-radar-tests tests/entity_radar_tests.cpp src/entity_radar.cpp src/radar_grid.cpp src/speech_strings.cpp)
	add_test(NAME entity-radar COMMAND entity-radar-tests)
	d2r_test(town-npcs-tests tests/town_npcs_tests.cpp src/town_npcs.cpp src/entity_radar.cpp src/radar_grid.cpp src/speech_strings.cpp)
	add_test(NAME town-npcs COMMAND town-npcs-tests)
	d2r_test(map-survey-tests tests/map_survey_tests.cpp src/map_survey.cpp src/npc_proximity.cpp src/spoken_direction.cpp src/entity_radar.cpp src/navigation_grid.cpp src/radar_grid.cpp src/speech_strings.cpp src/native_rooms.cpp src/native_memory.cpp)
	add_test(NAME map-survey COMMAND map-survey-tests)
	d2r_test(npc-proximity-tests tests/npc_proximity_tests.cpp src/npc_proximity.cpp src/spoken_direction.cpp src/entity_radar.cpp src/radar_grid.cpp src/speech_strings.cpp)
	add_test(NAME npc-proximity COMMAND npc-proximity-tests)
	d2r_test(target-sight-tests tests/target_sight_tests.cpp src/target_sight.cpp src/entity_radar.cpp src/radar_grid.cpp src/speech_strings.cpp)
	add_test(NAME target-sight COMMAND target-sight-tests)
	d2r_test(native-entities-tests tests/native_entities_tests.cpp src/native_entities.cpp src/native_pets.cpp src/town_npcs.cpp src/quest_seals.cpp src/monster_plate.cpp src/native_level_exits.cpp src/native_level_areas.cpp src/level_areas.cpp src/room_graph.cpp src/native_rooms.cpp src/native_drlg.cpp src/navigation_grid.cpp src/native_ground_items.cpp src/ground_items.cpp src/ground_item_text.cpp src/native_memory.cpp src/ui_text.cpp src/speech_strings.cpp src/async_log.cpp src/entity_radar.cpp src/radar_grid.cpp src/native_quest_flags.cpp)
	target_link_libraries(native-entities-tests PRIVATE D2RLPlugin::D2RLPlugin version)
	add_test(NAME native-entities COMMAND native-entities-tests)
	d2r_test(radar-ui-tests tests/radar_ui_tests.cpp src/radar_ui.cpp src/panel_state.cpp)
	target_link_libraries(radar-ui-tests PRIVATE D2RLPlugin::D2RLPlugin version)
	add_test(NAME radar-ui COMMAND radar-ui-tests)
	d2r_test(native-world-tests tests/native_world_tests.cpp src/native_world.cpp src/native_memory.cpp src/radar_grid.cpp src/radar_config.cpp src/async_log.cpp)
	target_link_libraries(native-world-tests PRIVATE D2RLPlugin::D2RLPlugin version)
	add_test(NAME native-world COMMAND native-world-tests)
	# The speech tables: one per game locale, matched against English.
	d2r_test(speech-strings-tests tests/speech_strings_tests.cpp src/speech_strings.cpp)
	add_test(NAME speech-strings COMMAND speech-strings-tests)

	d2r_test(lootables-tests tests/lootables_tests.cpp)
	add_test(NAME lootables COMMAND lootables-tests)
	d2r_test(item-sizes-tests tests/item_sizes_tests.cpp)
	add_test(NAME item-sizes COMMAND item-sizes-tests)
	d2r_test(cairn-stones-tests tests/cairn_stones_tests.cpp)
	add_test(NAME cairn-stones COMMAND cairn-stones-tests)
	d2r_test(native-cairn-cache-tests tests/native_cairn_cache_tests.cpp)
	add_test(NAME native-cairn-cache COMMAND native-cairn-cache-tests)
	d2r_test(quest-seals-tests tests/quest_seals_tests.cpp src/quest_seals.cpp src/speech_strings.cpp)
	add_test(NAME quest-seals COMMAND quest-seals-tests)
	d2r_test(cairn-scroll-service-tests tests/cairn_scroll_service_tests.cpp src/cairn_scroll_service.cpp src/speech_strings.cpp)
	target_link_libraries(cairn-scroll-service-tests PRIVATE D2RLPlugin::D2RLPlugin)
	add_test(NAME cairn-scroll-service COMMAND cairn-scroll-service-tests)
	d2r_test(ui-text-tests tests/ui_text_tests.cpp src/ui_text.cpp src/speech_strings.cpp)
	d2r_test(native-level-exits-tests tests/native_level_exits_tests.cpp src/native_level_exits.cpp src/native_memory.cpp)
	add_test(NAME native-level-exits COMMAND native-level-exits-tests)
	d2r_test(native-level-areas-tests tests/native_level_areas_tests.cpp src/native_level_areas.cpp src/level_areas.cpp src/room_graph.cpp src/native_rooms.cpp src/native_memory.cpp src/navigation_grid.cpp src/radar_grid.cpp)
	add_test(NAME native-level-areas COMMAND native-level-areas-tests)
	d2r_test(native-rooms-tests tests/native_rooms_tests.cpp src/native_rooms.cpp src/native_memory.cpp)
	add_test(NAME native-rooms COMMAND native-rooms-tests)
	d2r_test(room-graph-tests tests/room_graph_tests.cpp src/room_graph.cpp src/native_rooms.cpp src/native_memory.cpp src/navigation_grid.cpp src/radar_grid.cpp)
	add_test(NAME room-graph COMMAND room-graph-tests)
	d2r_test(room-catalogue-tests tests/room_catalogue_tests.cpp src/room_catalogue.cpp src/room_graph.cpp src/navigation_grid.cpp src/native_rooms.cpp src/native_memory.cpp src/entity_radar.cpp src/radar_grid.cpp src/speech_strings.cpp)
	add_test(NAME room-catalogue COMMAND room-catalogue-tests)
	d2r_test(navigation-policy-tests tests/navigation_policy_tests.cpp src/navigation_policy.cpp src/beacon_navigation.cpp src/level_route.cpp src/beacon.cpp src/room_graph.cpp src/native_rooms.cpp src/native_memory.cpp src/navigation_grid.cpp src/radar_grid.cpp src/speech_strings.cpp)
	add_test(NAME navigation-policy COMMAND navigation-policy-tests)
	d2r_test(native-drlg-tests tests/native_drlg_tests.cpp src/native_drlg.cpp src/native_memory.cpp)
	add_test(NAME native-drlg COMMAND native-drlg-tests)
	# The stock automap's current layer: cell decoding and the guarded tree walk.
	d2r_test(automap-tests tests/automap_tests.cpp src/automap_cells.cpp)
	add_test(NAME automap COMMAND automap-tests)
	d2r_test(corpse-memory-tests tests/corpse_memory_tests.cpp src/corpse_memory.cpp src/room_graph.cpp src/navigation_grid.cpp src/native_rooms.cpp src/native_memory.cpp src/entity_radar.cpp src/radar_grid.cpp src/speech_strings.cpp)
	add_test(NAME corpse-memory COMMAND corpse-memory-tests)
	add_test(NAME ui-text COMMAND ui-text-tests)
	d2r_test(native-memory-tests tests/native_memory_tests.cpp src/native_memory.cpp)
	add_test(NAME native-memory COMMAND native-memory-tests)
	d2r_test(native-ui-tests tests/native_ui_tests.cpp src/native_ui.cpp src/native_memory.cpp src/ui_text.cpp src/speech_strings.cpp src/localization.cpp src/quest_log.cpp)
	target_link_libraries(native-ui-tests PRIVATE D2RLPlugin::D2RLPlugin version)
	add_test(NAME native-ui COMMAND native-ui-tests)
	d2r_test(game-text-reader-tests tests/game_text_reader_tests.cpp src/async_log.cpp src/game_text_reader.cpp src/speech_priority.cpp src/native_memory.cpp src/ui_text.cpp src/ui_notifications.cpp src/speech_strings.cpp src/descriptions.cpp src/controller_text.cpp)
	target_link_libraries(game-text-reader-tests PRIVATE D2RLPlugin::D2RLPlugin version)
	add_test(NAME game-text-reader COMMAND game-text-reader-tests)
	d2r_test(ui-notifications-tests tests/ui_notifications_tests.cpp src/ui_notifications.cpp src/ui_text.cpp src/speech_strings.cpp src/controller_text.cpp)
	add_test(NAME ui-notifications COMMAND ui-notifications-tests)
	d2r_test(native-tooltips-tests tests/native_tooltips_tests.cpp src/native_tooltips.cpp src/native_memory.cpp)
	target_link_libraries(native-tooltips-tests PRIVATE D2RLPlugin::D2RLPlugin version)
	add_test(NAME native-tooltips COMMAND native-tooltips-tests)
	d2r_test(tooltip-delay-tests tests/tooltip_delay_tests.cpp src/tooltip_delay.cpp src/native_memory.cpp src/async_log.cpp)
	target_link_libraries(tooltip-delay-tests PRIVATE D2RLPlugin::D2RLPlugin version)
	add_test(NAME tooltip-delay COMMAND tooltip-delay-tests)
	d2r_test(character-stats-tests tests/character_stats_tests.cpp src/character_stats.cpp src/character_stats_reader.cpp src/mercenary_stats.cpp src/native_memory.cpp src/tooltip_text.cpp src/controller_text.cpp src/ui_text.cpp src/speech_strings.cpp)
	target_link_libraries(character-stats-tests PRIVATE D2RLPlugin::D2RLPlugin version)
	add_test(NAME character-stats COMMAND character-stats-tests)
	d2r_test(advanced-stats-tests tests/advanced_stats_tests.cpp src/advanced_stats_reader.cpp src/mercenary_stats.cpp src/character_stats.cpp src/character_stats_reader.cpp src/tooltip_text.cpp src/controller_text.cpp src/ui_text.cpp src/speech_strings.cpp)
	target_link_libraries(advanced-stats-tests PRIVATE D2RLPlugin::D2RLPlugin version)
	add_test(NAME advanced-stats COMMAND advanced-stats-tests)
	d2r_test(hire-stats-tests tests/hire_stats_tests.cpp src/hire_stats.cpp src/tooltip_text.cpp src/controller_text.cpp src/ui_text.cpp src/speech_strings.cpp src/async_log.cpp)
	target_link_libraries(hire-stats-tests PRIVATE D2RLPlugin::D2RLPlugin version)
	add_test(NAME hire-stats COMMAND hire-stats-tests)
	d2r_test(interaction-reader-tests tests/interaction_reader_tests.cpp src/async_log.cpp src/interaction_reader.cpp src/speech_priority.cpp src/cube_state.cpp src/monster_names.cpp src/item_hand.cpp src/click_words.cpp src/equip_attempt.cpp src/ground_item_text.cpp src/panel_state.cpp src/control_hints.cpp src/progression_text.cpp src/tooltip_text.cpp src/ui_text.cpp src/speech_strings.cpp src/belt_reader.cpp src/controller_text.cpp src/stash_count.cpp)
	target_link_libraries(interaction-reader-tests PRIVATE D2RLPlugin::D2RLPlugin version)
	add_test(NAME interaction-reader COMMAND interaction-reader-tests)
	d2r_test(inventory-cursor-tests tests/inventory_cursor_tests.cpp src/inventory_cursor.cpp)
	target_link_libraries(inventory-cursor-tests PRIVATE D2RLPlugin::D2RLPlugin version)
	add_test(NAME inventory-cursor COMMAND inventory-cursor-tests)
	d2r_test(async-log-tests tests/async_log_tests.cpp tests/allocation_failure.cpp src/async_log.cpp)
	target_link_libraries(async-log-tests PRIVATE D2RLPlugin::D2RLPlugin version)
	add_test(NAME async-log COMMAND async-log-tests)
	d2r_test(equip-attempt-tests tests/equip_attempt_tests.cpp src/equip_attempt.cpp)
	add_test(NAME equip-attempt COMMAND equip-attempt-tests)
	d2r_test(tooltip-text-tests tests/tooltip_text_tests.cpp src/tooltip_text.cpp src/controller_text.cpp src/ui_text.cpp src/speech_strings.cpp)
	add_test(NAME tooltip-text COMMAND tooltip-text-tests)
	d2r_test(tab-reader-tests tests/tab_reader_tests.cpp src/tab_reader.cpp src/ui_text.cpp src/speech_strings.cpp)
	target_link_libraries(tab-reader-tests PRIVATE D2RLPlugin::D2RLPlugin)
	add_test(NAME tab-reader COMMAND tab-reader-tests)
	d2r_test(text-field-edit-tests tests/text_field_edit_tests.cpp src/text_field_edit.cpp)
	add_test(NAME text-field-edit COMMAND text-field-edit-tests)
	d2r_test(grid-edge-tests tests/grid_edge_tests.cpp src/grid_edge.cpp)
	add_test(NAME grid-edge COMMAND grid-edge-tests)
	d2r_test(stash-count-tests tests/stash_count_tests.cpp src/stash_count.cpp)
	target_link_libraries(stash-count-tests PRIVATE D2RLPlugin::D2RLPlugin)
	add_test(NAME stash-count COMMAND stash-count-tests)
	d2r_test(quest-log-tests tests/quest_log_tests.cpp src/quest_log.cpp)
	add_test(NAME quest-log COMMAND quest-log-tests)
	d2r_test(quest-progress-tests tests/quest_progress_tests.cpp src/quest_progress.cpp src/quest_log.cpp src/ui_text.cpp src/speech_strings.cpp)
	add_test(NAME quest-progress COMMAND quest-progress-tests)
	d2r_test(vitals-tests tests/vitals_tests.cpp src/vitals.cpp)
	add_test(NAME vitals COMMAND vitals-tests)
	d2r_test(status-effects-tests tests/status_effects_tests.cpp src/status_effects.cpp)
	add_test(NAME status-effects COMMAND status-effects-tests)
	d2r_test(speech-priority-tests tests/speech_priority_tests.cpp)
	add_test(NAME speech-priority COMMAND speech-priority-tests)
	d2r_test(minion-report-tests tests/minion_report_tests.cpp src/minion_report.cpp)
	add_test(NAME minion-report COMMAND minion-report-tests)
	d2r_test(weapon-swap-tests tests/weapon_swap_tests.cpp src/weapon_swap.cpp src/speech_strings.cpp)
	add_test(NAME weapon-swap COMMAND weapon-swap-tests)
	d2r_test(trade-tests tests/trade_tests.cpp src/trade.cpp src/speech_strings.cpp)
	add_test(NAME trade COMMAND trade-tests)
	d2r_test(quest-service-tests tests/quest_service_tests.cpp src/quest_service.cpp src/speech_priority.cpp src/quest_seals.cpp src/quest_progress.cpp src/quest_log.cpp src/native_memory.cpp src/ui_text.cpp src/speech_strings.cpp)
	target_link_libraries(quest-service-tests PRIVATE D2RLPlugin::D2RLPlugin version)
	add_test(NAME quest-service COMMAND quest-service-tests)
	d2r_test(modal-reader-tests tests/modal_reader_tests.cpp src/modal_reader.cpp src/control_hints.cpp src/controller_text.cpp src/ui_text.cpp src/speech_strings.cpp)
	target_link_libraries(modal-reader-tests PRIVATE D2RLPlugin::D2RLPlugin)
	add_test(NAME modal-reader COMMAND modal-reader-tests)
	d2r_test(pad-filter-tests tests/pad_filter_tests.cpp src/pad_filter.cpp)
	target_link_libraries(pad-filter-tests PRIVATE D2RLPlugin::D2RLPlugin)
	add_test(NAME pad-filter COMMAND pad-filter-tests)
	# Keyboard play: the routes and choices behind the walking and fighting keys.
	d2r_test(auto-walk-tests tests/auto_walk_tests.cpp src/auto_walk.cpp src/beacon.cpp src/radar_grid.cpp)
	add_test(NAME auto-walk COMMAND auto-walk-tests)
	d2r_test(route-legs-tests tests/route_legs_tests.cpp src/route_legs.cpp src/spoken_direction.cpp src/entity_radar.cpp src/radar_grid.cpp src/speech_strings.cpp)
	add_test(NAME route-legs COMMAND route-legs-tests)
	d2r_test(step-counter-tests tests/step_counter_tests.cpp src/step_counter.cpp)
	add_test(NAME step-counter COMMAND step-counter-tests)
	d2r_test(route-follow-tests tests/route_follow_tests.cpp src/route_follow.cpp)
	add_test(NAME route-follow COMMAND route-follow-tests)
	d2r_test(crossing-memory-tests tests/crossing_memory_tests.cpp src/crossing_memory.cpp)
	add_test(NAME crossing-memory COMMAND crossing-memory-tests)
	d2r_test(level-route-tests tests/level_route_tests.cpp src/level_route.cpp src/beacon.cpp src/navigation_grid.cpp src/radar_grid.cpp src/native_rooms.cpp src/native_memory.cpp)
	add_test(NAME level-route COMMAND level-route-tests)
	# The beacon's guidance over walkable ground: the window's route, the remembered map's way, or none.
	d2r_test(beacon-guidance-tests tests/beacon_guidance_tests.cpp src/beacon_navigation.cpp src/level_route.cpp src/beacon.cpp src/navigation_policy.cpp src/room_graph.cpp src/navigation_grid.cpp src/radar_grid.cpp src/native_rooms.cpp src/native_memory.cpp)
	add_test(NAME beacon-guidance COMMAND beacon-guidance-tests)
	d2r_test(pad-links-tests tests/pad_links_tests.cpp src/pad_links.cpp src/native_rooms.cpp src/native_memory.cpp)
	add_test(NAME pad-links COMMAND pad-links-tests)
	d2r_test(combat-target-tests tests/combat_target_tests.cpp src/combat_target.cpp src/auto_walk.cpp src/entity_radar.cpp src/navigation_grid.cpp src/radar_grid.cpp src/speech_strings.cpp)
	add_test(NAME combat-target COMMAND combat-target-tests)
	d2r_test(explore-goal-tests tests/explore_goal_tests.cpp src/explore_goal.cpp src/room_graph.cpp src/native_rooms.cpp src/native_memory.cpp src/navigation_grid.cpp src/radar_grid.cpp)
	add_test(NAME explore-goal COMMAND explore-goal-tests)
	d2r_test(grid-jump-tests tests/grid_jump_tests.cpp src/grid_jump.cpp)
	add_test(NAME grid-jump COMMAND grid-jump-tests)
	d2r_test(menu-cursor-tests tests/menu_cursor_tests.cpp src/menu_cursor.cpp src/grid_jump.cpp)
	add_test(NAME menu-cursor COMMAND menu-cursor-tests)
	d2r_test(click-words-tests tests/click_words_tests.cpp src/click_words.cpp src/speech_strings.cpp)
	add_test(NAME click-words COMMAND click-words-tests)
	d2r_test(skill-jump-tests tests/skill_jump_tests.cpp src/skill_grid.cpp)
	add_test(NAME skill-jump COMMAND skill-jump-tests)
	d2r_test(skill-attacks-tests tests/skill_attacks_tests.cpp)
	add_test(NAME skill-attacks COMMAND skill-attacks-tests)
	d2r_test(monster-plate-tests tests/monster_plate_tests.cpp src/monster_plate.cpp)
	add_test(NAME monster-plate COMMAND monster-plate-tests)
	d2r_test(monster-names-tests tests/monster_names_tests.cpp src/monster_names.cpp)
	add_test(NAME monster-names COMMAND monster-names-tests)
	d2r_test(settings-list-tests tests/settings_list_tests.cpp src/settings_list.cpp)
	add_test(NAME settings-list COMMAND settings-list-tests)
	d2r_test(settings-store-tests tests/settings_store_tests.cpp src/settings_store.cpp)
	add_test(NAME settings-store COMMAND settings-store-tests)
	d2r_test(descriptions-tests tests/descriptions_tests.cpp src/descriptions.cpp src/async_log.cpp)
	target_link_libraries(descriptions-tests PRIVATE D2RLPlugin::D2RLPlugin)
	add_test(NAME descriptions COMMAND descriptions-tests)
endif()

add_custom_command(TARGET d2r-access POST_BUILD
	COMMAND "${CMAKE_COMMAND}" -E make_directory "${D2R_PLUGINS_DIR}"
	COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:d2r-access>" "${D2R_PLUGINS_DIR}/"
	COMMAND "${CMAKE_COMMAND}" -E make_directory "${D2R_PLUGINS_DIR}/d2r-access"
	COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${PRISM_DIR}/bin/prism.dll" "${D2R_PLUGINS_DIR}/d2r-access/"
	COMMENT "Deploying d2r-access plugin and prism.dll to ${D2R_PLUGINS_DIR}"
	VERBATIM
)
