#include "dock_model_test_helpers.hpp"
#include "dock_model_test_registry.hpp"

#include "aura_shell/dock_model.hpp"

#include <cassert>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string_view>

void test_default_layout() {
    using aura::shell::DockSlot;
    using aura::shell::PanelId;

    const auto state = aura::shell::build_default_dock_state();
    assert_single_instance_per_panel(state);
    const auto left_panel = aura::shell::active_panel(state, DockSlot::Left);
    assert(left_panel.has_value());
    assert(*left_panel == PanelId::TelemetryOverview);
}

void test_repeated_moves_preserve_uniqueness() {
    using aura::shell::DockSlot;
    using aura::shell::PanelId;
    using aura::shell::PanelMoveRequest;

    auto state = aura::shell::build_default_dock_state();
    state = aura::shell::move_panel(
        state,
        PanelMoveRequest{
            .panel_id = PanelId::TelemetryOverview,
            .to_slot = DockSlot::Center,
            .to_index = std::nullopt,
        }
    );
    state = aura::shell::move_panel(
        state,
        PanelMoveRequest{
            .panel_id = PanelId::TopProcesses,
            .to_slot = DockSlot::Right,
            .to_index = std::nullopt,
        }
    );
    state = aura::shell::move_panel(
        state,
        PanelMoveRequest{
            .panel_id = PanelId::RenderSurface,
            .to_slot = DockSlot::Left,
            .to_index = std::nullopt,
        }
    );
    assert_single_instance_per_panel(state);
}

void test_active_tab_clamps_after_panel_removal() {
    using aura::shell::DockSlot;
    using aura::shell::PanelId;
    using aura::shell::PanelMoveRequest;

    // Right has {ProcessPanel, TopProcesses} -- set active to 1 (TopProcesses)
    auto state = aura::shell::build_default_dock_state();
    state = aura::shell::set_active_tab(state, DockSlot::Right, 1U);

    // Move TopProcesses out -- Right now has 1 panel, active should clamp to 0
    const auto moved = aura::shell::move_panel(
        state,
        PanelMoveRequest{
            .panel_id = PanelId::TopProcesses,
            .to_slot = DockSlot::Left,
            .to_index = std::nullopt,
        }
    );
    assert(moved.active_tab[slot_index(DockSlot::Right)] == 0U);
    const auto right_panel = aura::shell::active_panel(moved, DockSlot::Right);
    assert(right_panel.has_value());
    assert(*right_panel == PanelId::ProcessPanel);
}

void test_destination_active_tab_tracks_inserted_panel() {
    using aura::shell::DockSlot;
    using aura::shell::PanelId;
    using aura::shell::PanelMoveRequest;

    const auto state = aura::shell::build_default_dock_state();
    // Move TelemetryOverview to Right at index 0
    // Right was {ProcessPanel, TopProcesses} -> becomes {TelemetryOverview, ProcessPanel, TopProcesses}
    const auto moved = aura::shell::move_panel(
        state,
        PanelMoveRequest{
            .panel_id = PanelId::TelemetryOverview,
            .to_slot = DockSlot::Right,
            .to_index = 0U,
        }
    );
    assert(moved.active_tab[slot_index(DockSlot::Right)] == 0U);
    const auto right_panel = aura::shell::active_panel(moved, DockSlot::Right);
    assert(right_panel.has_value());
    assert(*right_panel == PanelId::TelemetryOverview);
}

void test_set_active_tab_out_of_range_throws() {
    using aura::shell::DockSlot;
    bool threw = false;
    try {
        static_cast<void>(aura::shell::set_active_tab(
            aura::shell::build_default_dock_state(),
            DockSlot::Left,
            1U
        ));
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);
}

void test_empty_slot_behavior() {
    using aura::shell::DockSlot;
    using aura::shell::PanelId;
    using aura::shell::PanelMoveRequest;

    const auto moved = aura::shell::move_panel(
        aura::shell::build_default_dock_state(),
        PanelMoveRequest{
            .panel_id = PanelId::TelemetryOverview,
            .to_slot = DockSlot::Right,
            .to_index = std::nullopt,
        }
    );
    assert(moved.slot_tabs[slot_index(DockSlot::Left)].empty());
    assert(!aura::shell::active_panel(moved, DockSlot::Left).has_value());
    static_cast<void>(aura::shell::set_active_tab(moved, DockSlot::Left, 0U));

    bool threw = false;
    try {
        static_cast<void>(aura::shell::set_active_tab(moved, DockSlot::Left, 1U));
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);
}

// ---------------------------------------------------------------------------
// Aggregate slot tests
// ---------------------------------------------------------------------------

// Move all 5 panels to Left. Left should contain exactly 5 panels.
// Center and Right must be empty. active_panel for each must resolve correctly.
void test_all_panels_to_single_slot() {
    using aura::shell::DockSlot;
    using aura::shell::PanelId;
    using aura::shell::PanelMoveRequest;

    auto state = aura::shell::build_default_dock_state();

    // Default: Left={TelemetryOverview}, Center={RenderSurface, DvrTimeline}, Right={ProcessPanel, TopProcesses}
    // Move RenderSurface -> Left
    state = aura::shell::move_panel(
        state,
        PanelMoveRequest{
            .panel_id = PanelId::RenderSurface,
            .to_slot = DockSlot::Left,
            .to_index = std::nullopt,
        }
    );
    // Move TopProcesses -> Left
    state = aura::shell::move_panel(
        state,
        PanelMoveRequest{
            .panel_id = PanelId::TopProcesses,
            .to_slot = DockSlot::Left,
            .to_index = std::nullopt,
        }
    );
    // Move DvrTimeline -> Left
    state = aura::shell::move_panel(
        state,
        PanelMoveRequest{
            .panel_id = PanelId::DvrTimeline,
            .to_slot = DockSlot::Left,
            .to_index = std::nullopt,
        }
    );
    // Move ProcessPanel -> Left
    state = aura::shell::move_panel(
        state,
        PanelMoveRequest{
            .panel_id = PanelId::ProcessPanel,
            .to_slot = DockSlot::Left,
            .to_index = std::nullopt,
        }
    );

    // All 5 panels must be in Left
    assert(state.slot_tabs[slot_index(DockSlot::Left)].size() == 5U);

    // Center and Right must be empty
    assert(state.slot_tabs[slot_index(DockSlot::Center)].empty());
    assert(state.slot_tabs[slot_index(DockSlot::Right)].empty());

    // active_panel for Center/Right returns nullopt
    assert(!aura::shell::active_panel(state, DockSlot::Center).has_value());
    assert(!aura::shell::active_panel(state, DockSlot::Right).has_value());

    // active_panel for Left returns the panel at active_tab[Left]
    const auto left_active = aura::shell::active_panel(state, DockSlot::Left);
    assert(left_active.has_value());

    // Uniqueness invariant still holds
    assert_single_instance_per_panel(state);

    // Each panel must appear exactly once across all slots
    for (const auto panel_id : aura::shell::all_panel_ids()) {
        assert(panel_count(state, panel_id) == 1U);
    }
}

// ---------------------------------------------------------------------------
// Moving a panel to its current slot is a no-op (same slot, no to_index)
// ---------------------------------------------------------------------------

// Move TelemetryOverview to Left (where it already is with no to_index).
// The state should be functionally identical: same panel count, same active_panel.
void test_move_same_slot_is_noop() {
    using aura::shell::DockSlot;
    using aura::shell::PanelId;
    using aura::shell::PanelMoveRequest;

    const auto original = aura::shell::build_default_dock_state();

    // TelemetryOverview starts in Left. Move it back to Left (no explicit index).
    const auto after_move = aura::shell::move_panel(
        original,
        PanelMoveRequest{
            .panel_id = PanelId::TelemetryOverview,
            .to_slot = DockSlot::Left,
            .to_index = std::nullopt,
        }
    );

    // Both before and after: Left has exactly 1 panel (TelemetryOverview)
    assert(after_move.slot_tabs[slot_index(DockSlot::Left)].size() == 1U);
    assert(after_move.slot_tabs[slot_index(DockSlot::Left)][0] == PanelId::TelemetryOverview);

    // Center and Right are unchanged
    assert(
        after_move.slot_tabs[slot_index(DockSlot::Center)].size() ==
        original.slot_tabs[slot_index(DockSlot::Center)].size()
    );
    assert(
        after_move.slot_tabs[slot_index(DockSlot::Right)].size() ==
        original.slot_tabs[slot_index(DockSlot::Right)].size()
    );

    // Active panel for Left is still TelemetryOverview
    const auto left_panel = aura::shell::active_panel(after_move, DockSlot::Left);
    assert(left_panel.has_value());
    assert(*left_panel == PanelId::TelemetryOverview);

    // Uniqueness preserved
    assert_single_instance_per_panel(after_move);
}

// ---------------------------------------------------------------------------
// Active tab round-trips through moves and re-assignments
// ---------------------------------------------------------------------------

// Set active tab for each slot, then perform unrelated moves,
// and verify active tabs survive or clamp correctly.
void test_active_tab_round_trips() {
    using aura::shell::DockSlot;
    using aura::shell::PanelId;
    using aura::shell::PanelMoveRequest;

    // Default: Left={TelemetryOverview[0]}, Center={RenderSurface[0], DvrTimeline[1]}, Right={ProcessPanel[0], TopProcesses[1]}
    auto state = aura::shell::build_default_dock_state();

    // Set Right active tab to 1 (TopProcesses)
    state = aura::shell::set_active_tab(state, DockSlot::Right, 1U);
    assert(state.active_tab[slot_index(DockSlot::Right)] == 1U);

    // Confirm active_panel reflects TopProcesses
    {
        const auto panel = aura::shell::active_panel(state, DockSlot::Right);
        assert(panel.has_value());
        assert(*panel == PanelId::TopProcesses);
    }

    // Move RenderSurface from Center to Center at explicit index 0 (stays there)
    state = aura::shell::move_panel(
        state,
        PanelMoveRequest{
            .panel_id = PanelId::RenderSurface,
            .to_slot = DockSlot::Center,
            .to_index = 0U,
        }
    );

    // Right active tab should still resolve to TopProcesses (unchanged by this move)
    {
        const auto panel = aura::shell::active_panel(state, DockSlot::Right);
        assert(panel.has_value());
        assert(*panel == PanelId::TopProcesses);
    }

    // Center active_tab was updated to 0 (insertion at 0); RenderSurface should still be active
    {
        const auto center_panel = aura::shell::active_panel(state, DockSlot::Center);
        assert(center_panel.has_value());
        assert(*center_panel == PanelId::RenderSurface);
    }

    // Reset: move Right active to 0
    state = aura::shell::set_active_tab(state, DockSlot::Right, 0U);
    assert(state.active_tab[slot_index(DockSlot::Right)] == 0U);
    {
        const auto panel = aura::shell::active_panel(state, DockSlot::Right);
        assert(panel.has_value());
        assert(*panel == PanelId::ProcessPanel);
    }

    // Uniqueness invariant
    assert_single_instance_per_panel(state);
}

// ---------------------------------------------------------------------------
// Panel ordering after a sequence of targeted index moves
// ---------------------------------------------------------------------------

// Build a specific tab order by inserting panels at explicit indices and
// verify that the resulting order matches expectations exactly.
void test_panel_ordering_after_moves() {
    using aura::shell::DockSlot;
    using aura::shell::PanelId;
    using aura::shell::PanelMoveRequest;

    // Start fresh
    auto state = aura::shell::build_default_dock_state();
    // Default: Left={TelemetryOverview}, Center={RenderSurface, DvrTimeline}, Right={ProcessPanel, TopProcesses}

    // Move TopProcesses from Right to Center at index 0
    // Center becomes: {TopProcesses, RenderSurface, DvrTimeline}
    state = aura::shell::move_panel(
        state,
        PanelMoveRequest{
            .panel_id = PanelId::TopProcesses,
            .to_slot = DockSlot::Center,
            .to_index = 0U,
        }
    );
    assert(state.slot_tabs[slot_index(DockSlot::Center)].size() == 3U);
    assert(state.slot_tabs[slot_index(DockSlot::Center)][0] == PanelId::TopProcesses);
    assert(state.slot_tabs[slot_index(DockSlot::Center)][1] == PanelId::RenderSurface);
    assert(state.slot_tabs[slot_index(DockSlot::Center)][2] == PanelId::DvrTimeline);

    // Move TelemetryOverview from Left to Center at index 1
    // Center becomes: {TopProcesses, TelemetryOverview, RenderSurface, DvrTimeline}
    state = aura::shell::move_panel(
        state,
        PanelMoveRequest{
            .panel_id = PanelId::TelemetryOverview,
            .to_slot = DockSlot::Center,
            .to_index = 1U,
        }
    );
    assert(state.slot_tabs[slot_index(DockSlot::Center)].size() == 4U);
    assert(state.slot_tabs[slot_index(DockSlot::Center)][0] == PanelId::TopProcesses);
    assert(state.slot_tabs[slot_index(DockSlot::Center)][1] == PanelId::TelemetryOverview);
    assert(state.slot_tabs[slot_index(DockSlot::Center)][2] == PanelId::RenderSurface);
    assert(state.slot_tabs[slot_index(DockSlot::Center)][3] == PanelId::DvrTimeline);

    // Move ProcessPanel from Right to Center at end
    state = aura::shell::move_panel(
        state,
        PanelMoveRequest{
            .panel_id = PanelId::ProcessPanel,
            .to_slot = DockSlot::Center,
            .to_index = std::nullopt,
        }
    );
    assert(state.slot_tabs[slot_index(DockSlot::Center)].size() == 5U);
    assert(state.slot_tabs[slot_index(DockSlot::Center)][4] == PanelId::ProcessPanel);

    // Left and Right are both empty
    assert(state.slot_tabs[slot_index(DockSlot::Left)].empty());
    assert(state.slot_tabs[slot_index(DockSlot::Right)].empty());

    // Confirm active_panel for Center points to the panel at active_tab[Center]
    {
        const std::size_t active = state.active_tab[slot_index(DockSlot::Center)];
        assert(active < state.slot_tabs[slot_index(DockSlot::Center)].size());
        const auto active_p = aura::shell::active_panel(state, DockSlot::Center);
        assert(active_p.has_value());
        assert(*active_p == state.slot_tabs[slot_index(DockSlot::Center)][active]);
    }

    // Uniqueness invariant
    assert_single_instance_per_panel(state);
}

int main() {
    // --- Foundation tests ---
    test_default_layout();
    test_repeated_moves_preserve_uniqueness();
    test_active_tab_clamps_after_panel_removal();
    test_destination_active_tab_tracks_inserted_panel();
    test_set_active_tab_out_of_range_throws();
    test_empty_slot_behavior();

    // --- Aggregate slot tests ---
    test_all_panels_to_single_slot();
    test_move_same_slot_is_noop();

    // --- Active tab round-trips ---
    test_active_tab_round_trips();

    // --- Panel ordering ---
    test_panel_ordering_after_moves();

    // --- Invariant tests (defined in dock_model_invariant_tests.cpp) ---
    test_to_string_all_slots_and_panels();
    test_all_panel_ids_and_slots_counts();
    test_default_state_active_tabs_are_zero();
    test_move_out_of_range_index_throws();
    test_set_active_tab_zero_on_empty_slot_allowed();
    test_move_nullopt_index_appends_to_end();
    test_full_panel_shuffle_preserves_uniqueness();

    return 0;
}
