#pragma once

// Forward declarations for all 17 dock model test functions.
// Tests defined in dock_model_tests.cpp (10 tests):
void test_default_layout();
void test_repeated_moves_preserve_uniqueness();
void test_active_tab_clamps_after_panel_removal();
void test_destination_active_tab_tracks_inserted_panel();
void test_set_active_tab_out_of_range_throws();
void test_empty_slot_behavior();
void test_all_panels_to_single_slot();
void test_move_same_slot_is_noop();
void test_active_tab_round_trips();
void test_panel_ordering_after_moves();

// Tests defined in dock_model_invariant_tests.cpp (7 tests):
void test_to_string_all_slots_and_panels();
void test_all_panel_ids_and_slots_counts();
void test_default_state_active_tabs_are_zero();
void test_move_out_of_range_index_throws();
void test_set_active_tab_zero_on_empty_slot_allowed();
void test_move_nullopt_index_appends_to_end();
void test_full_panel_shuffle_preserves_uniqueness();
