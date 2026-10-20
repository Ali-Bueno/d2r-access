# Complete feature and source index

Reviewed against the production `.cpp` inventory, central UI dispatch and
settings/input registrations during the 2026-10-19 upstream integration.
Each module below has one primary topic; shared native/output contracts are
in [runtime services](runtime-services.md) and [architecture](architecture.md).
Topic pages preserve source/function/native evidence, behavior, controls,
lifecycle, safe extension and their own verification limits. Current build
results and unresolved gameplay checks are in [STATUS](../STATUS.md).

The inventory includes upstream additions through 7352a4a and retained local
work through 4c88812. Historical tester reports remain attributed reports,
not confirmations by this user. See [integration record](upstream-2026-10-19.md).

Current scoped corrections: [skill targeting and stationary attacks](battle/skill-casting.md)
(keyboard_service/native_actions, battle/skill_cast.h and generated metadata),
and [menu gesture lifetime](menus/mouse-lifetime.md) (keyboard_menu/menu_mouse/
panel_state). These supplement their primary action/menu topics below.
Shift+S's HUD entry uses the verified [left-hand skill selector](menus/left-skill-menu.md)
callback, keeping menu-only queue cancellation intact during play.

| Feature or subsystem | Primary implementation modules | Authoritative topic |
| --- | --- | --- |
| Runtime, settings, speech and localization | [plugin](../src/plugin.cpp), [ui_thread_pump](../src/ui_thread_pump.cpp), [async_log](../src/async_log.cpp), [screen_reader](../src/screen_reader.cpp), [localization](../src/localization.cpp), [speech_strings](../src/speech_strings.cpp), [settings_file](../src/settings_file.cpp), [settings_list](../src/settings_list.cpp), [settings_store](../src/settings_store.cpp), [control_state](../src/control_state.cpp), [control_hints](../src/control_hints.cpp), [mod_sounds](../src/mod_sounds.cpp), [radar_config](../src/radar_config.cpp) | [runtime-services](runtime-services.md) |
| Native widget tree and front-end focus | [native_widgets](../src/native_widgets.cpp), [native_memory](../src/native_memory.cpp), [native_ui](../src/native_ui.cpp), [native_focus](../src/native_focus.cpp), [widgets](../src/widgets.cpp), [focus_tracker](../src/focus_tracker.cpp), [frontend_native_fallback](../src/frontend_native_fallback.cpp), [panel_state](../src/panel_state.cpp), [ui_message_monitor](../src/ui_message_monitor.cpp), [display_modes](../src/display_modes.cpp), [character_saves](../src/character_saves.cpp), [widget_probe](../src/widget_probe.cpp), [code_dump](../src/code_dump.cpp) | [native-ui](native-ui.md) |
| Gameplay text and notices | [game_text_reader](../src/game_text_reader.cpp), [ui_text](../src/ui_text.cpp), [ui_notifications](../src/ui_notifications.cpp), [native_tooltips](../src/native_tooltips.cpp), [native_list_rows](../src/native_list_rows.cpp), [tooltip_delay](../src/tooltip_delay.cpp), [tooltip_text](../src/tooltip_text.cpp), [interaction_reader](../src/interaction_reader.cpp), [controller_text](../src/controller_text.cpp), [click_words](../src/click_words.cpp), [auto_detail](../src/auto_detail.cpp) | [native-tooltips](native-tooltips.md) |
| Modal and deletion confirmation | [modal_reader](../src/modal_reader.cpp) | [delete-character-modal](delete-character-modal.md) |
| Keyboard play and actions | [keyboard_service](../src/keyboard_service.cpp), [native_actions](../src/native_actions.cpp), [native_attack](../src/native_attack.cpp), [native_hover](../src/native_hover.cpp), [combat_target](../src/combat_target.cpp), [auto_walk](../src/auto_walk.cpp), [native_keyboard](../src/input/native_keyboard.cpp) | [keyboard-actions](keyboard-actions.md), [native Character binding](menus/mouse-lifetime.md) |
| Keyboard menu cursor and clicks | [keyboard_menu](../src/keyboard_menu.cpp), [menu_cursor](../src/menu_cursor.cpp), [menu_mouse](../src/menu_mouse.cpp), [grid_jump](../src/grid_jump.cpp), [inventory_cursor](../src/inventory_cursor.cpp), [native_sort](../src/native_sort.cpp) | [keyboard-play](keyboard-play.md) |
| Controller focus, cursor and text entry | [input_direction](../src/input_direction.cpp), [pad_filter](../src/pad_filter.cpp), [native_skill_navigation](../src/native_skill_navigation.cpp), [native_cursor_gesture](../src/native_cursor_gesture.cpp), [menu_navigation](../src/menu_navigation.cpp), [menu_navigation_policy](../src/menu_navigation_policy.cpp), [pad_typing](../src/pad_typing.cpp), [text_field](../src/text_field.cpp), [text_field_edit](../src/text_field_edit.cpp) | [controller-navigation](controller-navigation.md) |
| Skills, prerequisites, statistics and skill-tree navigation | [native_progression](../src/native_progression.cpp), [progression_text](../src/progression_text.cpp), [native_skill_bar](../src/native_skill_bar.cpp), [native_skill_icons](../src/native_skill_icons.cpp), [native_class_choice](../src/native_class_choice.cpp), [skill_grid](../src/skill_grid.cpp), [skill_navigation](../src/skill_navigation.cpp) | [progression](progression.md) |
| Bound skills, hotkeys and weapon hands | [skill_bar](../src/skill_bar.cpp), [skill_bar_service](../src/skill_bar_service.cpp), [skill_hotkey_service](../src/skill_hotkey_service.cpp), [native_skill_hotkeys](../src/native_skill_hotkeys.cpp), [native_skill_hand](../src/native_skill_hand.cpp) | [skill-bar](skill-bar.md) |
| Native controller help and tabs | [native_controller_help](../src/native_controller_help.cpp), [native_value_controls](../src/native_value_controls.cpp), [tab_reader](../src/tab_reader.cpp), [tab_highlight](../src/tab_highlight.cpp), [inventory_census](../src/inventory_census.cpp) | [navigation-latency](navigation-latency.md) |
| Inventory actions, hand outcomes and arrivals | [equipment_census](../src/equipment_census.cpp), [equip_attempt](../src/equip_attempt.cpp), [inventory_arrivals](../src/inventory_arrivals.cpp), [inventory_arrival_service](../src/inventory_arrival_service.cpp), [item_hand](../src/item_hand.cpp) | [inventory-npc-followup](inventory-npc-followup.md) |
| Belt and stash cells/pages | [belt_reader](../src/belt_reader.cpp), [native_belt](../src/native_belt.cpp), [native_belt_rows](../src/native_belt_rows.cpp), [native_belt_fill](../src/native_belt_fill.cpp), [stash_count](../src/stash_count.cpp), [stash_pages](../src/menus/stash_pages.cpp), [native_stash_pages](../src/menus/native_stash_pages.cpp) | [belt](belt.md), [keyboard tabs and shared pages](menus/stash-pages.md) |
| Grid edge and compartment boundary cues | [grid_edge](../src/grid_edge.cpp), [grid_edge_service](../src/grid_edge_service.cpp) | [native-tooltips](native-tooltips.md) |
| Character and advanced statistics | [character_stats](../src/character_stats.cpp), [character_stats_reader](../src/character_stats_reader.cpp), [advanced_stats_reader](../src/advanced_stats_reader.cpp) | [character-panel](character-panel.md) |
| Mercenary, hire list, summons and usable corpses | [mercenary_stats](../src/mercenary_stats.cpp), [hire_stats](../src/hire_stats.cpp), [native_pets](../src/native_pets.cpp), [minion_report](../src/minion_report.cpp) | [summons-and-corpses-re](summons-and-corpses-re.md) |
| Life, mana, broken equipment and low-life audio | [vitals](../src/vitals.cpp), [vitals_service](../src/vitals_service.cpp), [native_status](../src/native_status.cpp), [low_life_audio](../src/low_life_audio.cpp) | [vitals](vitals.md) |
| Boss life and attack-reach feedback | [boss_life](../src/boss_life.cpp), [boss_life_service](../src/boss_life_service.cpp), [attack_reach](../src/attack_reach.cpp) | [boss-life](boss-life.md) |
| Native HD footsteps and replay volume | [footsteps](../src/footsteps.cpp), [footstep_service](../src/footstep_service.cpp), [native_footsteps](../src/native_footsteps.cpp) | [footsteps](footsteps.md) |
| Wall-slide input gate | [native_wall_slide](../src/native_wall_slide.cpp), [wall_slide_service](../src/wall_slide_service.cpp) | [wall-sliding](wall-sliding.md) |
| Native world, rooms and collision | [native_world](../src/native_world.cpp), [native_rooms](../src/native_rooms.cpp), [radar_grid](../src/radar_grid.cpp), [navigation_grid](../src/navigation_grid.cpp) | [collision-semantics](collision-semantics.md) |
| Quest journal, updates and seal counts | [quest_log](../src/quest_log.cpp), [quest_progress](../src/quest_progress.cpp), [quest_service](../src/quest_service.cpp), [native_quest_flags](../src/native_quest_flags.cpp), [quest_seals](../src/quest_seals.cpp) | [quests](quests.md) |
| Cairn Stone images and decoded scroll | [cairn_scroll_service](../src/cairn_scroll_service.cpp), [native_cairn](../src/native_cairn.cpp) | [cairn-stones-re](cairn-stones-re.md) |
| Horadric Cube state and feedback | [cube_notice](../src/cube_notice.cpp), [cube_state](../src/cube_state.cpp) | [horadric-cube](horadric-cube.md) |
| Waypoint row availability and terror marker | [native_waypoints](../src/native_waypoints.cpp) | [waypoints](waypoints.md) |
| Ground item identity and spatial sound | [ground_items](../src/ground_items.cpp), [ground_item_service](../src/ground_item_service.cpp), [ground_item_text](../src/ground_item_text.cpp), [native_ground_items](../src/native_ground_items.cpp), [item_audio](../src/item_audio.cpp), [item_spatial](../src/item_spatial.cpp), [item_wave](../src/item_wave.cpp) | [ground-items](ground-items.md) |
| Entity scans, catalogues, NPC presence and targets | [entity_radar](../src/entity_radar.cpp), [entity_radar_service](../src/entity_radar_service.cpp), [native_entities](../src/native_entities.cpp), [map_survey](../src/map_survey.cpp), [map_survey_service](../src/map_survey_service.cpp), [town_npcs](../src/town_npcs.cpp) | [entity-scan](entity-scan.md) |
| Enemy labels, rank, immunities and visibility | [monster_plate](../src/monster_plate.cpp), [monster_names](../src/monster_names.cpp), [target_sight](../src/target_sight.cpp) | [target-sight](target-sight.md) |
| NPC proximity speech | [npc_proximity](../src/npc_proximity.cpp), [npc_speech](../src/npc_speech.cpp) | [npc-proximity](npc-proximity.md) |
| Entity, quest-marker and corpse audio cues | [entity_cues](../src/entity_cues.cpp), [entity_cue_service](../src/entity_cue_service.cpp), [entity_cue_audio](../src/entity_cue_audio.cpp), [entity_cue_synth](../src/entity_cue_synth.cpp) | [entity-cues](entity-cues.md) |
| Terrain radar geometry, cues and wall tones | [radar](../src/radar.cpp), [radar_passage](../src/radar_passage.cpp), [radar_structure](../src/radar_structure.cpp), [radar_trace](../src/radar_trace.cpp), [radar_service](../src/radar_service.cpp), [radar_readout](../src/radar_readout.cpp), [radar_audio](../src/radar_audio.cpp), [radar_synth](../src/radar_synth.cpp), [wall_bump](../src/wall_bump.cpp), [wall_tones](../src/wall_tones.cpp), [wall_tone_service](../src/wall_tone_service.cpp), [wall_tone_audio](../src/wall_tone_audio.cpp), [radar_ui](../src/radar_ui.cpp) | [radar-readings](radar-readings.md) |
| Map memory, continuous exploration and coverage | [terrain_service](../src/terrain_service.cpp), [terrain_overview](../src/terrain_overview.cpp), [explored_rooms](../src/explored_rooms.cpp), [explored_store](../src/explored_store.cpp), [explored_store_worker](../src/explored_store_worker.cpp), [frontier](../src/frontier.cpp), [explore_goal](../src/explore_goal.cpp), [exploration_cues](../src/exploration_cues.cpp), [exploration_cue_service](../src/exploration_cue_service.cpp), [navigation_mode](../src/navigation_mode.cpp), [navigation_policy](../src/navigation_policy.cpp), [level_route](../src/level_route.cpp) | [exploration](exploration.md) |
| Room catalogue, exits and connectivity | [rooms](../src/rooms.cpp), [room_graph](../src/room_graph.cpp), [room_catalogue](../src/room_catalogue.cpp), [room_service](../src/room_service.cpp), [exit_review](../src/exit_review.cpp) | [rooms](rooms.md) |
| Area borders and measured crossings | [level_areas](../src/level_areas.cpp), [native_level_areas](../src/native_level_areas.cpp), [native_level_exits](../src/native_level_exits.cpp), [zone_transitions](../src/zone_transitions.cpp), [crossing_memory](../src/crossing_memory.cpp) | [level-exits](level-exits.md) |
| Teleport-pad links | [pad_links](../src/pad_links.cpp) | [keyboard-play](keyboard-play.md) |
| Beacon guidance and route narration | [beacon](../src/beacon.cpp), [beacon_service](../src/beacon_service.cpp), [beacon_navigation](../src/beacon_navigation.cpp), [beacon_audio](../src/beacon_audio.cpp), [route_legs](../src/route_legs.cpp), [route_follow](../src/route_follow.cpp), [spoken_direction](../src/spoken_direction.cpp) | [beacon](beacon.md) |
| Automap cells, reveal and quest markers | [native_automap](../src/native_automap.cpp), [automap_cells](../src/automap_cells.cpp), [automap_service](../src/automap_service.cpp) | [automap-reader](automap-reader.md) |
| Preset landmarks, interiors and corridor descriptions | [native_drlg](../src/native_drlg.cpp), [native_preset_map](../src/native_preset_map.cpp), [passages](../src/passages.cpp), [places](../src/places.cpp), [place_service](../src/place_service.cpp) | [places](places.md) |
| Remembered death location and corpse ownership | [death_place](../src/death_place.cpp), [death_place_service](../src/death_place_service.cpp), [corpse_memory](../src/corpse_memory.cpp) | [death-place](death-place.md) |
| Multiplayer UI, party roster and trade | [multiplayer_reader](../src/multiplayer_reader.cpp), [multiplayer_status](../src/multiplayer_status.cpp), [party_reader](../src/party_reader.cpp), [trade](../src/trade.cpp), [trade_service](../src/trade_service.cpp), [native_trade](../src/native_trade.cpp) | [multiplayer](multiplayer.md) |
| Weapon swapping | [weapon_swap](../src/weapon_swap.cpp), [weapon_swap_service](../src/weapon_swap_service.cpp), [native_swap](../src/native_swap.cpp), [native_weapon_set](../src/native_weapon_set.cpp) | [weapon-swap](weapon-swap.md) |
| Status-effect names and transitions | [status_effects](../src/status_effects.cpp) | [status-effects](status-effects.md) |
| Custom area/landmark descriptions | [descriptions](../src/descriptions.cpp) | [descriptions](descriptions.md) |
| Sound glossary and previews | [sound_glossary](../src/sound_glossary.cpp) | [entity-cues](entity-cues.md) |

## Supporting catalogs and evidence

Generated layout/item/level/skill/place/quest catalogs live under `src/generated`;
their `tools/gen_*.py` generators and feature-specific layout headers preserve
data provenance and numeric guards. Shared fixed buffers, callback boundaries,
process-lifetime storage, loop pace, percentage steps, sound volumes and menu
policies are header-only helpers indexed by their owning topics above. Native
offsets belong in the relevant layout header/topic, not this retrieval index.

Additional focused evidence: [Tal Rasha symbols](tal-rasha-symbols.md),
[quest objects](quest-objects.md), [portals](portals.md),
[route descriptions](route-description.md), [attack reach](attack-reach.md),
[combat status](combat-status-evidence.md), [stability](stability.md),
[audio and language](audio-language-audit.md), [decompile provenance](decompile.md),
[testing](testing.md) and [local decisions](decisions.md).

## Documentation and engineering limits

Inventory coverage: 213 production `.cpp` modules mapped above; no
unassigned production module. This checks discoverability and module ownership;
it does not convert archived test claims into current runtime validation.
Existing explanatory topics are reused; changed merge/native/presence paths
are reviewed directly and documented with the current task. Tests exercise
production native readers with constructed memory and policy fixtures.

Known broader requirements still have gaps: independent persistent mod-language
override, complete HRTF output coverage, whole-pipeline runtime baseline
measurements, and source organization of the five large central context owners.
Their actual boundaries are recorded in [runtime services](runtime-services.md).
These are retained implementation limits, not features claimed completed by
this merge. New gameplay and listening confirmation remain user-led.
