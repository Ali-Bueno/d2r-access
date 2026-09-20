# Decode the user's OGG assets at build time. XAudio2/X3DAudio are Windows APIs;
# the installed plugin needs no decoder DLL and does no decoding on the UI thread.
find_program(D2R_FFMPEG ffmpeg REQUIRED)
# "chest" is the container cue, taken from the author's DevilutionX accessibility
# fork (audio/chest.ogg there); the others are the user's own loop beds.
set(D2R_ITEM_SOUND_NAMES "gold" "potion" "normal equipment" "magic equipment" "rare equipment" "legendary equipment" "portal" "chest")
set(D2R_ITEM_WAV_DIR "${CMAKE_CURRENT_BINARY_DIR}/item-sounds")
set(D2R_ITEM_WAVS)
foreach(name IN LISTS D2R_ITEM_SOUND_NAMES)
	set(source "${CMAKE_CURRENT_SOURCE_DIR}/sounds/item loop sounds/${name}.ogg")
	set(output "${D2R_ITEM_WAV_DIR}/${name}.wav")
	add_custom_command(OUTPUT "${output}"
		COMMAND "${CMAKE_COMMAND}" -E make_directory "${D2R_ITEM_WAV_DIR}"
		COMMAND "${D2R_FFMPEG}" -nostdin -v error -y -i "${source}" -map_metadata -1
			-vn -ac 1 -ar 48000 -c:a pcm_s16le -fflags +bitexact -flags:a +bitexact "${output}"
		DEPENDS "${source}" VERBATIM)
	list(APPEND D2R_ITEM_WAVS "${output}")
endforeach()
# This target also runs for asset-only rebuilds; POST_BUILD alone would miss them.
add_custom_target(d2r-item-sounds ALL
	COMMAND "${CMAKE_COMMAND}" -E make_directory "${D2R_PLUGINS_DIR}/d2r-access/sounds/item loop sounds"
	COMMAND "${CMAKE_COMMAND}" -E copy_if_different ${D2R_ITEM_WAVS} "${D2R_PLUGINS_DIR}/d2r-access/sounds/item loop sounds"
	DEPENDS ${D2R_ITEM_WAVS} VERBATIM)
add_dependencies(d2r-access d2r-item-sounds)
target_sources(d2r-access PRIVATE src/ground_items.cpp src/ground_item_text.cpp src/native_ground_items.cpp
	src/ground_item_service.cpp src/item_wave.cpp src/item_audio.cpp src/item_spatial.cpp)
target_link_libraries(d2r-access PRIVATE xaudio2 ole32)

if(BUILD_TESTING)
	add_executable(ground-item-tests tests/ground_items_tests.cpp src/ground_items.cpp src/ground_item_text.cpp src/speech_strings.cpp)
	add_executable(native-ground-item-tests tests/native_ground_items_tests.cpp src/native_ground_items.cpp
		src/ground_items.cpp src/ground_item_text.cpp src/native_memory.cpp src/ui_text.cpp src/speech_strings.cpp src/async_log.cpp)
	add_executable(item-audio-tests tests/item_audio_tests.cpp src/item_wave.cpp src/item_spatial.cpp)
	target_link_libraries(item-audio-tests PRIVATE xaudio2)
	add_dependencies(item-audio-tests d2r-item-sounds)
	add_executable(ground-item-service-tests tests/ground_item_service_tests.cpp src/ground_item_service.cpp
		src/ground_items.cpp src/ground_item_text.cpp src/speech_strings.cpp src/async_log.cpp src/radar_config.cpp)
	foreach(target ground-item-tests native-ground-item-tests item-audio-tests ground-item-service-tests)
		target_link_libraries(${target} PRIVATE D2RLPlugin::D2RLPlugin version)
		target_include_directories(${target} PRIVATE src)
		target_compile_features(${target} PRIVATE cxx_std_20)
		target_compile_definitions(${target} PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
		if(MSVC)
			target_compile_options(${target} PRIVATE /W4 /permissive- /EHsc /utf-8)
		endif()
	endforeach()
	add_test(NAME ground-items COMMAND ground-item-tests)
	add_test(NAME native-ground-items COMMAND native-ground-item-tests)
	add_test(NAME item-audio COMMAND item-audio-tests "${D2R_ITEM_WAV_DIR}")
	add_test(NAME ground-item-service COMMAND ground-item-service-tests)
endif()
