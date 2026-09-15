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
	src/code_dump.cpp
	src/control_state.cpp
	src/display_modes.cpp
	src/focus_tracker.cpp
	src/input_direction.cpp
	src/plugin.cpp
	src/localization.cpp
	src/panel_state.cpp
	src/screen_reader.cpp
	src/settings_file.cpp
	src/speech_strings.cpp
	src/ui_message_monitor.cpp
	src/ui_thread_pump.cpp
	src/widgets.cpp
	plugin.rc
)
target_link_libraries(d2r-access PRIVATE D2RLPlugin::D2RLPlugin)
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

add_custom_command(TARGET d2r-access POST_BUILD
	COMMAND "${CMAKE_COMMAND}" -E make_directory "${D2R_PLUGINS_DIR}"
	COMMAND "${CMAKE_COMMAND}" -E copy_if_different "$<TARGET_FILE:d2r-access>" "${D2R_PLUGINS_DIR}/"
	COMMAND "${CMAKE_COMMAND}" -E make_directory "${D2R_PLUGINS_DIR}/d2r-access"
	COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${PRISM_DIR}/bin/prism.dll" "${D2R_PLUGINS_DIR}/d2r-access/"
	COMMENT "Deploying d2r-access plugin and prism.dll to ${D2R_PLUGINS_DIR}"
	VERBATIM
)
