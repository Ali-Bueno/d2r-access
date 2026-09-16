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
	src/game_text_reader.cpp
	src/input_direction.cpp
	src/interaction_reader.cpp
	src/plugin.cpp
	src/localization.cpp
	src/native_widgets.cpp
	src/native_memory.cpp
	src/native_world.cpp
	src/radar.cpp
	src/radar_grid.cpp
	src/radar_guidance.cpp
	src/radar_audio.cpp
	src/radar_trace.cpp
	src/radar_synth.cpp
	src/radar_service.cpp
	src/radar_ui.cpp
	src/radar_config.cpp
	src/native_ui.cpp
	src/native_progression.cpp
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
if(BUILD_TESTING)
	add_executable(progression-tests tests/progression_tests.cpp src/native_progression.cpp src/progression_text.cpp src/ui_text.cpp src/speech_strings.cpp)
	target_link_libraries(progression-tests PRIVATE D2RLPlugin::D2RLPlugin)
	target_include_directories(progression-tests PRIVATE src)
	target_compile_features(progression-tests PRIVATE cxx_std_20)
	add_test(NAME progression COMMAND progression-tests)
	add_executable(radar-guidance-tests tests/radar_guidance_tests.cpp src/radar.cpp src/radar_grid.cpp src/radar_guidance.cpp)
	target_include_directories(radar-guidance-tests PRIVATE src)
	target_compile_features(radar-guidance-tests PRIVATE cxx_std_20)
	add_test(NAME radar-guidance COMMAND radar-guidance-tests)
	add_executable(radar-ui-tests tests/radar_ui_tests.cpp src/radar_ui.cpp src/panel_state.cpp)
	target_link_libraries(radar-ui-tests PRIVATE D2RLPlugin::D2RLPlugin)
	target_include_directories(radar-ui-tests PRIVATE src)
	target_compile_features(radar-ui-tests PRIVATE cxx_std_20)
	add_test(NAME radar-ui COMMAND radar-ui-tests)
	add_executable(radar-tests tests/radar_tests.cpp src/radar.cpp src/radar_grid.cpp src/radar_guidance.cpp src/radar_synth.cpp)
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
	add_executable(game-text-reader-tests tests/game_text_reader_tests.cpp src/async_log.cpp src/game_text_reader.cpp src/ui_text.cpp src/ui_notifications.cpp src/speech_strings.cpp)
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
		target_compile_options(progression-tests PRIVATE /W4 /permissive- /EHsc /utf-8)
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
