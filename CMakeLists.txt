cmake_minimum_required(VERSION 3.28)

project(D2RAccess VERSION 0.1.0 LANGUAGES CXX RC)

# Where the game lives. The build copies the plugin into <D2R_GAME_DIR>/d2rloader/plugins
# and prism.dll into the d2r-access/ subfolder there (a DLL directly in plugins/ is
# treated as a plugin by D2RLoader and rejected), so a rebuild is immediately testable.
set(D2R_GAME_DIR "D:/games/steam/steamapps/common/Diablo II Resurrected" CACHE PATH "Diablo II: Resurrected install folder")
set(D2R_PLUGINS_DIR "${D2R_GAME_DIR}/d2rloader/plugins")

set(D2RLPLUGIN_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(D2RLPLUGIN_BUILD_TESTS OFF CACHE BOOL "" FORCE)
# D2RLoader PluginSDK: the git submodule in external/, or the shared checkout
# next to this repo when working inside the accessibility playbook tree.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/PluginSDK/CMakeLists.txt")
	set(D2RL_PLUGIN_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/PluginSDK")
else()
	set(D2RL_PLUGIN_SDK_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../external/PluginSDK")
endif()
add_subdirectory("${D2RL_PLUGIN_SDK_DIR}" "${CMAKE_BINARY_DIR}/PluginSDK")

set(PRISM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/prism")

# D2RLoader picks up a companion d2rl-<id>.mpq by this file name, so keep the
# "d2rl-" prefix and the plugin id from src/plugin.cpp in sync.
add_library(d2r-access SHARED
	src/async_log.cpp
	src/tab_reader.cpp
	src/character_saves.cpp
	src/code_dump.cpp
	src/control_state.cpp
	src/display_modes.cpp
	src/focus_tracker.cpp
	src/footsteps.cpp
	src/footstep_service.cpp
	src/native_footsteps.cpp
	src/game_text_reader.cpp
	src/input_direction.cpp
	src/interaction_reader.cpp
	src/plugin.cpp
	src/localization.cpp
	src/native_widgets.cpp
	src/native_memory.cpp
	src/native_world.cpp
	src/radar.cpp
	src/radar_wall_memory.cpp
	src/navigation_grid.cpp
	src/radar_passage.cpp
	src/radar_structure.cpp
	src/radar_grid.cpp
	src/radar_guidance.cpp
	src/radar_audio.cpp
	src/radar_trace.cpp
	src/radar_readout.cpp
	src/radar_synth.cpp
	src/radar_service.cpp
	src/radar_ui.cpp
	src/radar_config.cpp
	src/entity_radar.cpp
	src/entity_radar_service.cpp
	src/native_entities.cpp
	src/native_ui.cpp
	src/native_focus.cpp
	src/native_skill_navigation.cpp
	src/native_cursor_gesture.cpp
	src/skill_grid.cpp
	src/skill_navigation.cpp
	src/native_progression.cpp
	src/native_controller_help.cpp
	src/controller_text.cpp
	src/progression_text.cpp
	src/native_tooltips.cpp
	src/panel_state.cpp
	src/screen_reader.cpp
	src/settings_file.cpp
	src/speech_strings.cpp
	src/ui_message_monitor.cpp
	src/ui_thread_pump.cpp
	src/ui_text.cpp
	src/ui_notifications.cpp
	src/widgets.cpp
	plugin.rc
)
target_link_libraries(d2r-access PRIVATE D2RLPlugin::D2RLPlugin winmm)
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
	add_executable(radar-revisit-tests tests/radar_revisit_tests.cpp src/radar.cpp src/radar_wall_memory.cpp src/radar_passage.cpp src/navigation_grid.cpp src/radar_structure.cpp src/radar_grid.cpp src/radar_guidance.cpp)
	target_include_directories(radar-revisit-tests PRIVATE src)
	target_compile_features(radar-revisit-tests PRIVATE cxx_std_20)
	add_test(NAME radar-revisit COMMAND radar-revisit-tests)
	add_executable(radar-snapshot-tests tests/radar_snapshot_tests.cpp src/radar_trace.cpp src/radar_structure.cpp src/radar_grid.cpp)
	target_include_directories(radar-snapshot-tests PRIVATE src)
	target_link_libraries(radar-snapshot-tests PRIVATE D2RLPlugin::D2RLPlugin)
	target_compile_features(radar-snapshot-tests PRIVATE cxx_std_20)
	add_test(NAME radar-snapshot COMMAND radar-snapshot-tests)
	add_executable(radar-contour-tests tests/radar_contour_tests.cpp src/radar.cpp src/radar_wall_memory.cpp src/radar_passage.cpp src/navigation_grid.cpp src/radar_structure.cpp src/radar_grid.cpp src/radar_guidance.cpp)
	target_include_directories(radar-contour-tests PRIVATE src)
	target_compile_features(radar-contour-tests PRIVATE cxx_std_20)
	add_test(NAME radar-contour COMMAND radar-contour-tests)
	add_executable(radar-memory-update-tests tests/radar_memory_update_tests.cpp src/radar.cpp src/radar_wall_memory.cpp src/radar_passage.cpp src/navigation_grid.cpp src/radar_structure.cpp src/radar_grid.cpp src/radar_guidance.cpp)
	target_include_directories(radar-memory-update-tests PRIVATE src)
	target_compile_features(radar-memory-update-tests PRIVATE cxx_std_20)
	add_test(NAME radar-memory-update COMMAND radar-memory-update-tests)
	add_executable(radar-wall-memory-tests tests/radar_wall_memory_tests.cpp src/radar.cpp src/radar_wall_memory.cpp src/radar_passage.cpp src/navigation_grid.cpp src/radar_structure.cpp src/radar_grid.cpp src/radar_guidance.cpp)
	target_include_directories(radar-wall-memory-tests PRIVATE src)
	target_compile_features(radar-wall-memory-tests PRIVATE cxx_std_20)
	add_test(NAME radar-wall-memory COMMAND radar-wall-memory-tests)
	add_executable(radar-passage-tests tests/radar_passage_tests.cpp src/radar.cpp src/radar_wall_memory.cpp src/radar_passage.cpp src/navigation_grid.cpp src/radar_structure.cpp src/radar_grid.cpp src/radar_guidance.cpp)
	target_include_directories(radar-passage-tests PRIVATE src)
	target_compile_features(radar-passage-tests PRIVATE cxx_std_20)
	add_test(NAME radar-passage COMMAND radar-passage-tests)
	# Full-resolution native body connectivity, shared with the reactive radar.
	add_executable(navigation-grid-tests tests/navigation_grid_tests.cpp src/navigation_grid.cpp src/radar_grid.cpp)
	target_include_directories(navigation-grid-tests PRIVATE src)
	target_compile_features(navigation-grid-tests PRIVATE cxx_std_20)
	add_test(NAME navigation-grid COMMAND navigation-grid-tests)
	add_executable(radar-structure-tests tests/radar_structure_tests.cpp src/radar.cpp src/radar_wall_memory.cpp src/navigation_grid.cpp src/radar_passage.cpp src/radar_structure.cpp src/radar_grid.cpp src/radar_geometry.cpp src/radar_guidance.cpp)
	target_include_directories(radar-structure-tests PRIVATE src)
	target_compile_features(radar-structure-tests PRIVATE cxx_std_20)
	add_test(NAME radar-structure COMMAND radar-structure-tests)
	add_executable(radar-surface-tests tests/radar_surface_tests.cpp src/radar.cpp src/radar_wall_memory.cpp src/navigation_grid.cpp src/radar_passage.cpp src/radar_structure.cpp src/radar_grid.cpp src/radar_geometry.cpp src/radar_guidance.cpp)
	target_include_directories(radar-surface-tests PRIVATE src)
	target_compile_features(radar-surface-tests PRIVATE cxx_std_20)
	add_test(NAME radar-surface COMMAND radar-surface-tests)
	add_executable(controller-help-tests tests/controller_help_tests.cpp src/native_controller_help.cpp src/controller_text.cpp src/native_memory.cpp src/ui_text.cpp src/speech_strings.cpp)
	target_link_libraries(controller-help-tests PRIVATE D2RLPlugin::D2RLPlugin)
	target_include_directories(controller-help-tests PRIVATE src)
	target_compile_features(controller-help-tests PRIVATE cxx_std_20)
	target_compile_definitions(controller-help-tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
	add_test(NAME controller-help COMMAND controller-help-tests)
	add_executable(radar-precision-tests tests/radar_precision_tests.cpp src/radar.cpp src/radar_wall_memory.cpp src/navigation_grid.cpp src/radar_passage.cpp src/radar_structure.cpp src/radar_grid.cpp src/radar_geometry.cpp src/radar_guidance.cpp)
	target_include_directories(radar-precision-tests PRIVATE src)
	target_compile_features(radar-precision-tests PRIVATE cxx_std_20)
	add_test(NAME radar-precision COMMAND radar-precision-tests)
	add_executable(radar-geometry-tests src/radar_readout.cpp tests/radar_geometry_tests.cpp src/radar.cpp src/radar_wall_memory.cpp src/navigation_grid.cpp src/radar_passage.cpp src/radar_structure.cpp src/radar_grid.cpp src/radar_geometry.cpp src/radar_guidance.cpp)
	target_include_directories(radar-geometry-tests PRIVATE src)
	target_compile_features(radar-geometry-tests PRIVATE cxx_std_20)
	add_test(NAME radar-geometry COMMAND radar-geometry-tests)
	add_executable(native-cursor-gesture-tests tests/native_cursor_gesture_tests.cpp src/native_cursor_gesture.cpp)
	target_include_directories(native-cursor-gesture-tests PRIVATE src)
	target_compile_features(native-cursor-gesture-tests PRIVATE cxx_std_20)
	add_test(NAME native-cursor-gesture COMMAND native-cursor-gesture-tests)
	add_executable(skill-navigation-tests tests/skill_navigation_tests.cpp src/skill_grid.cpp src/skill_navigation.cpp src/progression_text.cpp src/ui_text.cpp src/speech_strings.cpp src/async_log.cpp)
	target_link_libraries(skill-navigation-tests PRIVATE D2RLPlugin::D2RLPlugin)
	target_include_directories(skill-navigation-tests PRIVATE src)
	target_compile_features(skill-navigation-tests PRIVATE cxx_std_20)
	add_test(NAME skill-navigation COMMAND skill-navigation-tests)
	add_executable(native-focus-tests tests/native_focus_tests.cpp src/native_focus.cpp)
	target_link_libraries(native-focus-tests PRIVATE D2RLPlugin::D2RLPlugin)
	target_include_directories(native-focus-tests PRIVATE src)
	target_compile_features(native-focus-tests PRIVATE cxx_std_20)
	add_test(NAME native-focus COMMAND native-focus-tests)
	add_executable(progression-tests tests/progression_tests.cpp src/native_progression.cpp src/progression_text.cpp src/controller_text.cpp src/ui_text.cpp src/speech_strings.cpp)
	target_link_libraries(progression-tests PRIVATE D2RLPlugin::D2RLPlugin)
	target_include_directories(progression-tests PRIVATE src)
	target_compile_features(progression-tests PRIVATE cxx_std_20)
	add_test(NAME progression COMMAND progression-tests)
	add_executable(footstep-tests tests/footstep_tests.cpp src/footsteps.cpp)
	target_include_directories(footstep-tests PRIVATE src)
	target_compile_features(footstep-tests PRIVATE cxx_std_20)
	add_test(NAME footsteps COMMAND footstep-tests)
	add_executable(radar-timing-tests tests/radar_timing_tests.cpp src/radar.cpp src/radar_wall_memory.cpp src/navigation_grid.cpp src/radar_passage.cpp src/radar_structure.cpp src/radar_grid.cpp src/radar_geometry.cpp src/radar_guidance.cpp)
	target_include_directories(radar-timing-tests PRIVATE src)
	target_compile_features(radar-timing-tests PRIVATE cxx_std_20)
	add_test(NAME radar-timing COMMAND radar-timing-tests)
	add_executable(radar-delivery-tests tests/radar_delivery_tests.cpp src/radar.cpp src/radar_wall_memory.cpp src/navigation_grid.cpp src/radar_passage.cpp src/radar_structure.cpp src/radar_grid.cpp src/radar_geometry.cpp src/radar_guidance.cpp src/radar_synth.cpp)
	target_link_libraries(radar-delivery-tests PRIVATE D2RLPlugin::D2RLPlugin)
	target_include_directories(radar-delivery-tests PRIVATE src)
	target_compile_features(radar-delivery-tests PRIVATE cxx_std_20)
	add_test(NAME radar-delivery COMMAND radar-delivery-tests)
	add_executable(radar-guidance-tests tests/radar_guidance_tests.cpp src/radar.cpp src/radar_wall_memory.cpp src/navigation_grid.cpp src/radar_passage.cpp src/radar_structure.cpp src/radar_grid.cpp src/radar_geometry.cpp src/radar_guidance.cpp)
	target_include_directories(radar-guidance-tests PRIVATE src)
	target_compile_features(radar-guidance-tests PRIVATE cxx_std_20)
	add_test(NAME radar-guidance COMMAND radar-guidance-tests)
	add_executable(entity-radar-tests tests/entity_radar_tests.cpp src/entity_radar.cpp src/radar_grid.cpp src/speech_strings.cpp)
	target_include_directories(entity-radar-tests PRIVATE src)
	target_compile_features(entity-radar-tests PRIVATE cxx_std_20)
	add_test(NAME entity-radar COMMAND entity-radar-tests)
	add_executable(native-entities-tests tests/native_entities_tests.cpp src/native_entities.cpp src/native_ground_items.cpp src/ground_items.cpp src/ground_item_text.cpp src/native_memory.cpp src/ui_text.cpp src/speech_strings.cpp src/async_log.cpp src/entity_radar.cpp src/radar_grid.cpp)
	target_link_libraries(native-entities-tests PRIVATE D2RLPlugin::D2RLPlugin)
	target_include_directories(native-entities-tests PRIVATE src)
	target_compile_features(native-entities-tests PRIVATE cxx_std_20)
	target_compile_definitions(native-entities-tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
	add_test(NAME native-entities COMMAND native-entities-tests)
	add_executable(radar-ui-tests tests/radar_ui_tests.cpp src/radar_ui.cpp src/panel_state.cpp)
	target_link_libraries(radar-ui-tests PRIVATE D2RLPlugin::D2RLPlugin)
	target_include_directories(radar-ui-tests PRIVATE src)
	target_compile_features(radar-ui-tests PRIVATE cxx_std_20)
	add_test(NAME radar-ui COMMAND radar-ui-tests)
	add_executable(radar-tests tests/radar_tests.cpp src/radar.cpp src/radar_wall_memory.cpp src/navigation_grid.cpp src/radar_passage.cpp src/radar_structure.cpp src/radar_grid.cpp src/radar_geometry.cpp src/radar_guidance.cpp src/radar_synth.cpp)
	target_link_libraries(radar-tests PRIVATE D2RLPlugin::D2RLPlugin)
	target_include_directories(radar-tests PRIVATE src)
	target_compile_features(radar-tests PRIVATE cxx_std_20)
	add_test(NAME radar COMMAND radar-tests)
	add_executable(native-world-tests tests/native_world_tests.cpp src/native_world.cpp src/native_memory.cpp src/radar_grid.cpp src/radar_config.cpp src/async_log.cpp)
	target_link_libraries(native-world-tests PRIVATE D2RLPlugin::D2RLPlugin)
	target_include_directories(native-world-tests PRIVATE src)
	target_compile_features(native-world-tests PRIVATE cxx_std_20)
	target_compile_definitions(native-world-tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
	add_test(NAME native-world COMMAND native-world-tests)
	add_executable(ui-text-tests tests/ui_text_tests.cpp src/ui_text.cpp src/speech_strings.cpp)
	target_include_directories(ui-text-tests PRIVATE src)
	target_compile_features(ui-text-tests PRIVATE cxx_std_20)
	add_test(NAME ui-text COMMAND ui-text-tests)
	add_executable(native-memory-tests tests/native_memory_tests.cpp src/native_memory.cpp)
	target_include_directories(native-memory-tests PRIVATE src)
	target_compile_features(native-memory-tests PRIVATE cxx_std_20)
	target_compile_definitions(native-memory-tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
	add_test(NAME native-memory COMMAND native-memory-tests)
	add_executable(native-ui-tests tests/native_ui_tests.cpp src/native_ui.cpp src/native_memory.cpp src/ui_text.cpp src/speech_strings.cpp)
	target_link_libraries(native-ui-tests PRIVATE D2RLPlugin::D2RLPlugin)
	target_include_directories(native-ui-tests PRIVATE src)
	target_compile_features(native-ui-tests PRIVATE cxx_std_20)
	target_compile_definitions(native-ui-tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
	add_test(NAME native-ui COMMAND native-ui-tests)
	add_executable(game-text-reader-tests tests/game_text_reader_tests.cpp src/async_log.cpp src/game_text_reader.cpp src/native_memory.cpp src/ui_text.cpp src/ui_notifications.cpp src/speech_strings.cpp)
	target_link_libraries(game-text-reader-tests PRIVATE D2RLPlugin::D2RLPlugin)
	target_include_directories(game-text-reader-tests PRIVATE src)
	target_compile_features(game-text-reader-tests PRIVATE cxx_std_20)
	target_compile_definitions(game-text-reader-tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
	add_test(NAME game-text-reader COMMAND game-text-reader-tests)
	add_executable(ui-notifications-tests tests/ui_notifications_tests.cpp src/ui_notifications.cpp src/ui_text.cpp src/speech_strings.cpp)
	target_include_directories(ui-notifications-tests PRIVATE src)
	target_compile_features(ui-notifications-tests PRIVATE cxx_std_20)
	add_test(NAME ui-notifications COMMAND ui-notifications-tests)
	add_executable(native-tooltips-tests tests/native_tooltips_tests.cpp src/native_tooltips.cpp src/native_memory.cpp)
	target_link_libraries(native-tooltips-tests PRIVATE D2RLPlugin::D2RLPlugin)
	target_include_directories(native-tooltips-tests PRIVATE src)
	target_compile_features(native-tooltips-tests PRIVATE cxx_std_20)
	target_compile_definitions(native-tooltips-tests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
	add_test(NAME native-tooltips COMMAND native-tooltips-tests)
	add_executable(interaction-reader-tests tests/interaction_reader_tests.cpp src/async_log.cpp src/interaction_reader.cpp src/progression_text.cpp src/ui_text.cpp src/speech_strings.cpp)
	target_link_libraries(interaction-reader-tests PRIVATE D2RLPlugin::D2RLPlugin)
	target_include_directories(interaction-reader-tests PRIVATE src)
	target_compile_features(interaction-reader-tests PRIVATE cxx_std_20)
	add_test(NAME interaction-reader COMMAND interaction-reader-tests)
	add_executable(async-log-tests tests/async_log_tests.cpp src/async_log.cpp)
	target_link_libraries(async-log-tests PRIVATE D2RLPlugin::D2RLPlugin)
	target_include_directories(async-log-tests PRIVATE src)
	target_compile_features(async-log-tests PRIVATE cxx_std_20)
	add_test(NAME async-log COMMAND async-log-tests)
	add_executable(tab-reader-tests tests/tab_reader_tests.cpp src/tab_reader.cpp src/ui_text.cpp src/speech_strings.cpp)
	target_include_directories(tab-reader-tests PRIVATE src)
	target_compile_features(tab-reader-tests PRIVATE cxx_std_20)
	add_test(NAME tab-reader COMMAND tab-reader-tests)
	if(MSVC)
		target_compile_options(radar-revisit-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(radar-snapshot-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(radar-contour-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(radar-memory-update-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(radar-wall-memory-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(radar-passage-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(radar-surface-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(radar-structure-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(navigation-grid-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(controller-help-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(radar-geometry-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(native-cursor-gesture-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(skill-navigation-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(native-focus-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(progression-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(footstep-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(radar-timing-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(radar-delivery-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(radar-guidance-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(radar-ui-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(radar-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(native-world-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(async-log-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(tab-reader-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(ui-text-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(native-memory-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(native-ui-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(game-text-reader-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(ui-notifications-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
		target_compile_options(native-tooltips-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
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
