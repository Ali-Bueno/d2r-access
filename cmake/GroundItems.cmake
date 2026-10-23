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
# Two Last Epoch pickups are short clinks too: peak-normalized alone the gold and the
# other potion sat 6-7 dB under the Diablo gold loop; 18 dB through the limiter brings
# their loudest 100 ms level with it (measured 2026-10-12). The other three need nothing.
set(D2R_EPOCH_CLICKS "last epoch/gold" "last epoch/other potion")
set(D2R_EPOCH_BOOST_DB 18)
# The game's drop sounds are short clinks and thuds as well; peak-normalized alone the
# herb, quiver and key sat at the near-item floor (measured 2026-10-13). 9 dB through the
# limiter lifts their bodies and leaves the loud ones at full scale.
set(D2R_DROP_BOOST_DB 9)
# A tester, 2026-10-20: the doorway creak was very soft in the Tower Cellar mix. Peak-normalized,
# its loudest 100 ms sat 15 dB under its peak (the doors 4-12); trimmed to the creak, 6 dB
# through the limiter brings it to 9.5, with the metal door.
set(D2R_DOORWAY_BOOST_DB 6)
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
		elseif(name STREQUAL "doorway")
			set(loop_filter -af "aformat=sample_fmts=flt,volume=${D2R_DOORWAY_BOOST_DB}dB,alimiter=limit=0.9:level=false")
		elseif(folder STREQUAL "d2 drops")
			set(loop_filter -af "aformat=sample_fmts=flt,volume=${D2R_DROP_BOOST_DB}dB,alimiter=limit=0.9:level=false")
		elseif("${folder}/${name}" IN_LIST D2R_EPOCH_CLICKS)
			set(loop_filter -af "aformat=sample_fmts=flt,volume=${D2R_EPOCH_BOOST_DB}dB,alimiter=limit=0.9:level=false")
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
# "doorway" is a tester's creaking-wood recording (2026-10-20), mono, cut to the creak before its
# clacks (2026-10-21: they flapped on every pass) and padded to a two-second pass.
d2r_sounds("item loop sounds" exploration "portal" "wooden door" "metal door" "stone door" "slime door"
	"unexplored room" "zone transition" "inside unexplored room" "shrine" "well" "doorway")
# Taken from the author's Last Epoch accessibility mod: its gold loop and the game's
# four potion pickups, one per kind; each file is one pass of the loop.
d2r_sounds("item loop sounds" "last epoch" "gold" "health potion" "mana potion" "rejuvenation potion" "other potion")
d2r_sounds("one shot sounds" exploration "discovered")
d2r_sounds("one shot sounds" tutorial "message" "step done")
# The sounds of the units walked, made with ffmpeg: "step" a 40 ms 1250 Hz beep, peak -1 dB, fading
# from 6 ms; "click" 30 ms of white noise decaying at exp(-220 t), band 900-6500 Hz, soft-clipped.
d2r_sounds("one shot sounds" movement "step" "click")
# The Diablo IV low-life start and heartbeat, from the author's Last Epoch mod (src/low_life_audio.cpp).
d2r_sounds("one shot sounds" vitals "low life start")
d2r_sounds("item loop sounds" vitals "low life")
# The game's own drop sounds, the alternative item set (tools/gen_drop_sounds.py).
include("${CMAKE_CURRENT_LIST_DIR}/DropSounds.cmake")
d2r_sounds("item loop sounds" "d2 drops" ${D2R_DROP_SOUNDS})
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
	add_executable(native-ground-item-tests tests/native_ground_items_tests.cpp src/native_ground_items.cpp src/monster_plate.cpp
		src/ground_items.cpp src/ground_item_text.cpp src/native_memory.cpp src/ui_text.cpp src/speech_strings.cpp src/async_log.cpp)
	add_executable(item-audio-tests tests/item_audio_tests.cpp src/item_wave.cpp src/item_spatial.cpp)
	target_link_libraries(item-audio-tests PRIVATE xaudio2)
	add_dependencies(item-audio-tests d2r-access)
	add_executable(ground-item-service-tests tests/ground_item_service_tests.cpp src/ground_item_service.cpp
		src/ground_items.cpp src/ground_item_text.cpp src/speech_strings.cpp src/async_log.cpp src/radar_config.cpp src/speech_priority.cpp
		src/step_counter.cpp src/tutorial.cpp src/secret_doors.cpp src/entity_radar.cpp src/radar_grid.cpp)
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
