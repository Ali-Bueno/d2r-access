# Decode the user's OGG assets at build time. XAudio2/X3DAudio are Windows APIs;
# the installed plugin needs no decoder DLL and does no decoding on the UI thread.
find_program(D2R_FFMPEG ffmpeg REQUIRED)
# Each decoded WAV is embedded in the plugin DLL as RCDATA named FOLDER_STEM (spaces
# as underscores), which src/item_wave.cpp looks up for GroundItems::SoundFiles.
set(D2R_ITEM_WAV_DIR "${CMAKE_CURRENT_BINARY_DIR}/item-sounds")
set(D2R_ITEM_WAVS)
set(D2R_ITEM_RESOURCES)
# Requested quest onset spacing; short edge fades avoid clicks when the bell repeats.
set(D2R_QUEST_LOOP_SECONDS 0.7)
set(D2R_QUEST_ATTACK_SECONDS 0.005)
set(D2R_QUEST_RELEASE_START_SECONDS 0.68)
set(D2R_QUEST_RELEASE_SECONDS 0.02)
# User listening, 2026-09-26: the 0.3 s stash recording repeated too fast; silence pads each pass.
set(D2R_STASH_LOOP_SECONDS 1.0)
# These are short clicks: peak-normalized alone they play 7-10 dB under the other
# loops. The boost lifts their body; the limiter holds the click at full scale.
set(D2R_CLICK_SOUNDS "corpse" "gem" "scroll" "rune")
set(D2R_CLICK_BOOST_DB 24)
# User listening, 2026-09-26: at 24 dB the corpse click still played about 6 dB under the other
# loops; 42 dB brings its loudest 100 ms to the quest and weapon rack level (docs/ground-items.md).
set(D2R_CORPSE_BOOST_DB 42)
# `group` is the folder under sounds/: "item loop sounds" or "one shot sounds".
function(d2r_sounds group folder)
	foreach(name IN LISTS ARGN)
		set(source "${CMAKE_CURRENT_SOURCE_DIR}/sounds/${group}/${folder}/${name}.ogg")
		set(output "${D2R_ITEM_WAV_DIR}/${folder}/${name}.wav")
		set(loop_filter)
		if(name STREQUAL "quest item")
			set(loop_filter -af "apad=whole_dur=${D2R_QUEST_LOOP_SECONDS},atrim=duration=${D2R_QUEST_LOOP_SECONDS},afade=t=in:d=${D2R_QUEST_ATTACK_SECONDS},afade=t=out:st=${D2R_QUEST_RELEASE_START_SECONDS}:d=${D2R_QUEST_RELEASE_SECONDS}")
		elseif(name STREQUAL "stash")
			set(loop_filter -af "apad=whole_dur=${D2R_STASH_LOOP_SECONDS}")
		elseif(name IN_LIST D2R_CLICK_SOUNDS)
			set(boost ${D2R_CLICK_BOOST_DB})
			if(name STREQUAL "corpse")
				set(boost ${D2R_CORPSE_BOOST_DB})
			endif()
			set(loop_filter -af "aformat=sample_fmts=flt,volume=${boost}dB,alimiter=limit=0.9:level=false")
		endif()
		add_custom_command(OUTPUT "${output}"
			COMMAND "${CMAKE_COMMAND}" -E make_directory "${D2R_ITEM_WAV_DIR}/${folder}"
			COMMAND "${D2R_FFMPEG}" -nostdin -v error -y -i "${source}" -map_metadata -1
				-vn ${loop_filter} -ac 1 -ar 48000 -c:a pcm_s16le -fflags +bitexact -flags:a +bitexact "${output}"
			DEPENDS "${source}" VERBATIM)
		string(TOUPPER "${folder}_${name}" resource)
		string(REPLACE " " "_" resource "${resource}")
		string(APPEND D2R_ITEM_RESOURCES "${resource} RCDATA \"${output}\"\n")
		list(APPEND D2R_ITEM_WAVS "${output}")
	endforeach()
	set(D2R_ITEM_WAVS "${D2R_ITEM_WAVS}" PARENT_SCOPE)
	set(D2R_ITEM_RESOURCES "${D2R_ITEM_RESOURCES}" PARENT_SCOPE)
endfunction()
# "chest" is the container cue, taken from the author's DevilutionX accessibility
# fork (audio/chest.ogg there); the others are the user's own recordings.
d2r_sounds("item loop sounds" loot "gold" "potion" "scroll" "rune" "gem" "normal equipment" "magic equipment" "rare equipment" "legendary equipment"
	"chest" "quest item" "corpse" "weapon rack" "stash")
d2r_sounds("item loop sounds" exploration "portal" "wooden door" "metal door" "stone door" "slime door"
	"unexplored room" "zone transition" "inside unexplored room")
d2r_sounds("one shot sounds" exploration "discovered")
add_custom_target(d2r-item-sounds DEPENDS ${D2R_ITEM_WAVS})
set(D2R_ITEM_RC "${CMAKE_CURRENT_BINARY_DIR}/item-sounds.rc")
file(CONFIGURE OUTPUT "${D2R_ITEM_RC}" CONTENT "${D2R_ITEM_RESOURCES}" @ONLY)
# A changed recording recompiles the resources, relinks and redeploys the DLL.
set_property(SOURCE "${D2R_ITEM_RC}" APPEND PROPERTY OBJECT_DEPENDS ${D2R_ITEM_WAVS})
add_dependencies(d2r-access d2r-item-sounds)
target_sources(d2r-access PRIVATE "${D2R_ITEM_RC}" src/ground_items.cpp src/ground_item_text.cpp src/native_ground_items.cpp
	src/ground_item_service.cpp src/item_wave.cpp src/item_audio.cpp src/item_spatial.cpp)
target_link_libraries(d2r-access PRIVATE xaudio2 ole32)

if(BUILD_TESTING)
	add_executable(ground-item-tests tests/ground_items_tests.cpp src/ground_items.cpp src/ground_item_text.cpp src/speech_strings.cpp)
	add_executable(native-ground-item-tests tests/native_ground_items_tests.cpp src/native_ground_items.cpp
		src/ground_items.cpp src/ground_item_text.cpp src/native_memory.cpp src/ui_text.cpp src/speech_strings.cpp src/async_log.cpp)
	add_executable(item-audio-tests tests/item_audio_tests.cpp src/item_wave.cpp src/item_spatial.cpp)
	target_link_libraries(item-audio-tests PRIVATE xaudio2)
	add_dependencies(item-audio-tests d2r-access)
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
	add_test(NAME item-audio COMMAND item-audio-tests "$<TARGET_FILE:d2r-access>")
	add_test(NAME ground-item-service COMMAND ground-item-service-tests)
endif()
