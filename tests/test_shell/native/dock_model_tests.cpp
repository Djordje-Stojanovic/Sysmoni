#include "aura_shell/dock_model.hpp"

#include <cassert>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string_view>

namespace {

constexpr std::size_t slot_index(const aura::shell::DockSlot slot) {
    return static_cast<std::size_t>(slot);
}

std::size_t panel_count(const aura::shell::DockState& state, const aura::shell::PanelId panel_id) {
    std::size_t count = 0U;
    for (const auto slot : aura::shell::all_dock_slots()) {
        for (const auto slot_panel : state.slot_tabs[slot_index(slot)]) {
            if (slot_panel == panel_id) {
                ++count;
            }
        }
    }
    return count;
}

void assert_single_instance_per_panel(const aura::shell::DockState& state) {
    for (const auto panel_id : aura::shell::all_panel_ids()) {
        assert(panel_count(state, panel_id) == 1U);
    }
}

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

    auto state = aura::shell::build_default_dock_state();
    state = aura::shell::set_active_tab(state, DockSlot::Center, 1U);

    const auto moved = aura::shell::move_panel(
        state,
        PanelMoveRequest{
            .panel_id = PanelId::DvrTimeline,
            .to_slot = DockSlot::Left,
            .to_index = std::nullopt,
        }
    );
    assert(moved.active_tab[slot_index(DockSlot::Center)] == 0U);
    const auto center_panel = aura::shell::active_panel(moved, DockSlot::Center);
    assert(center_panel.has_value());
    assert(*center_panel == PanelId::TopProcesses);
}

void test_destination_active_tab_tracks_inserted_panel() {
    using aura::shell::DockSlot;
    using aura::shell::PanelId;
    using aura::shell::PanelMoveRequest;

    const auto state = aura::shell::build_default_dock_state();
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
// NEW: All panels moved to a single slot
// ---------------------------------------------------------------------------

// Move all 4 panels to Left. Left should contain exactly 4 panels.
// Center and Right must be empty. active_panel for each must resolve correctly.
void test_all_panels_to_single_slot() {
    using aura::shell::DockSlot;
    using aura::shell::PanelId;
    using aura::shell::PanelMoveRequest;

    auto state = aura::shell::build_default_dock_state();

    // Default has: Left={TelemetryOverview}, Center={TopProcesses, DvrTimeline}, Right={RenderSurface}
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
    // Move RenderSurface -> Left
    state = aura::shell::move_panel(
        state,
        PanelMoveRequest{
            .panel_id = PanelId::RenderSurface,
            .to_slot = DockSlot::Left,
            .to_index = std::nullopt,
        }
    );

    // All 4 panels must be in Left
    assert(state.slot_tabs[slot_index(DockSlot::Left)].size() == 4U);

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
// NEW: Moving a panel to its current slot is a no-op (same slot, no to_index)
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
// NEW: Active tab round-trips through moves and re-assignments
// ---------------------------------------------------------------------------

// Set active tab for each slot, then perform unrelated moves,
// and verify active tabs survive or clamp correctly.
void test_active_tab_round_trips() {
    using aura::shell::DockSlot;
    using aura::shell::PanelId;
    using aura::shell::PanelMoveRequest;

    // Default: Left={TelemetryOverview[0]}, Center={TopProcesses[0], DvrTimeline[1]}, Right={RenderSurface[0]}
    auto state = aura::shell::build_default_dock_state();

    // Set Center active tab to 1 (DvrTimeline)
    state = aura::shell::set_active_tab(state, DockSlot::Center, 1U);
    assert(state.active_tab[slot_index(DockSlot::Center)] == 1U);

    // Confirm active_panel reflects DvrTimeline
    {
        const auto panel = aura::shell::active_panel(state, DockSlot::Center);
        assert(panel.has_value());
        assert(*panel == PanelId::DvrTimeline);
    }

    // Move RenderSurface from Right to Right at explicit index 0 (stays there)
    state = aura::shell::move_panel(
        state,
        PanelMoveRequest{
            .panel_id = PanelId::RenderSurface,
            .to_slot = DockSlot::Right,
            .to_index = 0U,
        }
    );

    // Center active tab should still resolve to DvrTimeline (unchanged by this move)
    {
        const auto panel = aura::shell::active_panel(state, DockSlot::Center);
        assert(panel.has_value());
        assert(*panel == PanelId::DvrTimeline);
    }

    // Right active_tab was updated to 0 (insertion at 0); RenderSurface should still be active
    {
        const auto right_panel = aura::shell::active_panel(state, DockSlot::Right);
        assert(right_panel.has_value());
        assert(*right_panel == PanelId::RenderSurface);
    }

    // Reset: move Center active to 0
    state = aura::shell::set_active_tab(state, DockSlot::Center, 0U);
    assert(state.active_tab[slot_index(DockSlot::Center)] == 0U);
    {
        const auto panel = aura::shell::active_panel(state, DockSlot::Center);
        assert(panel.has_value());
        assert(*panel == PanelId::TopProcesses);
    }

    // Uniqueness invariant
    assert_single_instance_per_panel(state);
}

// ---------------------------------------------------------------------------
// NEW: Panel ordering after a sequence of targeted index moves
// ---------------------------------------------------------------------------

// Build a specific tab order by inserting panels at explicit indices and
// verify that the resulting order matches expectations exactly.
void test_panel_ordering_after_moves() {
    using aura::shell::DockSlot;
    using aura::shell::PanelId;
    using aura::shell::PanelMoveRequest;

    // Start fresh
    auto state = aura::shell::build_default_dock_state();
    // Default: Left={TelemetryOverview}, Center={TopProcesses, DvrTimeline}, Right={RenderSurface}

    // Move RenderSurface from Right to Center at index 0
    // Center becomes: {RenderSurface, TopProcesses, DvrTimeline}
    state = aura::shell::move_panel(
        state,
        PanelMoveRequest{
            .panel_id = PanelId::RenderSurface,
            .to_slot = DockSlot::Center,
            .to_index = 0U,
        }
    );
    assert(state.slot_tabs[slot_index(DockSlot::Center)].size() == 3U);
    assert(state.slot_tabs[slot_index(DockSlot::Center)][0] == PanelId::RenderSurface);
    assert(state.slot_tabs[slot_index(DockSlot::Center)][1] == PanelId::TopProcesses);
    assert(state.slot_tabs[slot_index(DockSlot::Center)][2] == PanelId::DvrTimeline);

    // Right is now empty
    assert(state.slot_tabs[slot_index(DockSlot::Right)].empty());

    // Move TelemetryOverview from Left to Center at index 1
    // Center becomes: {RenderSurface, TelemetryOverview, TopProcesses, DvrTimeline}
    state = aura::shell::move_panel(
        state,
        PanelMoveRequest{
            .panel_id = PanelId::TelemetryOverview,
            .to_slot = DockSlot::Center,
            .to_index = 1U,
        }
    );
    assert(state.slot_tabs[slot_index(DockSlot::Center)].size() == 4U);
    assert(state.slot_tabs[slot_index(DockSlot::Center)][0] == PanelId::RenderSurface);
    assert(state.slot_tabs[slot_index(DockSlot::Center)][1] == PanelId::TelemetryOverview);
    assert(state.slot_tabs[slot_index(DockSlot::Center)][2] == PanelId::TopProcesses);
    assert(state.slot_tabs[slot_index(DockSlot::Center)][3] == PanelId::DvrTimeline);

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

// ---------------------------------------------------------------------------
// NEW: to_string round-trip for all slots and all panels
// ---------------------------------------------------------------------------

void test_to_string_all_slots_and_panels() {
    using aura::shell::DockSlot;
    using aura::shell::PanelId;

    // Verify to_string returns non-empty, non-"unknown" for all valid values
    assert(aura::shell::to_string(DockSlot::Left)   == "left");
    assert(aura::shell::to_string(DockSlot::Center) == "center");
    assert(aura::shell::to_string(DockSlot::Right)  == "right");

    assert(aura::shell::to_string(PanelId::TelemetryOverview) == "telemetry_overview");
    assert(aura::shell::to_string(PanelId::TopProcesses)      == "top_processes");
    assert(aura::shell::to_string(PanelId::DvrTimeline)       == "dvr_timeline");
    assert(aura::shell::to_string(PanelId::RenderSurface)     == "render_surface");

    // None of the string representations should be "unknown"
    assert(aura::shell::to_string(DockSlot::Left)   != "unknown");
    assert(aura::shell::to_string(DockSlot::Center) != "unknown");
    assert(aura::shell::to_string(DockSlot::Right)  != "unknown");
    assert(aura::shell::to_string(PanelId::TelemetryOverview) != "unknown");
    assert(aura::shell::to_string(PanelId::TopProcesses)      != "unknown");
    assert(aura::shell::to_string(PanelId::DvrTimeline)       != "unknown");
    assert(aura::shell::to_string(PanelId::RenderSurface)     != "unknown");
}

// ---------------------------------------------------------------------------
// NEW: all_panel_ids() and all_dock_slots() coverage and ordering
// ---------------------------------------------------------------------------

void test_all_panel_ids_and_slots_counts() {
    const auto panels = aura::shell::all_panel_ids();
    assert(panels.size() == 4U);

    const auto slots = aura::shell::all_dock_slots();
    assert(slots.size() == 3U);

    // Verify all expected panels are present
    bool found_telemetry = false;
    bool found_processes = false;
    bool found_dvr = false;
    bool found_render = false;
    for (const auto panel_id : panels) {
        if (panel_id == aura::shell::PanelId::TelemetryOverview) { found_telemetry = true; }
        if (panel_id == aura::shell::PanelId::TopProcesses)      { found_processes = true; }
        if (panel_id == aura::shell::PanelId::DvrTimeline)       { found_dvr = true; }
        if (panel_id == aura::shell::PanelId::RenderSurface)     { found_render = true; }
    }
    assert(found_telemetry);
    assert(found_processes);
    assert(found_dvr);
    assert(found_render);

    // Verify all expected slots are present
    bool found_left = false;
    bool found_center = false;
    bool found_right = false;
    for (const auto slot : slots) {
        if (slot == aura::shell::DockSlot::Left)   { found_left = true; }
        if (slot == aura::shell::DockSlot::Center) { found_center = true; }
        if (slot == aura::shell::DockSlot::Right)  { found_right = true; }
    }
    assert(found_left);
    assert(found_center);
    assert(found_right);
}

// ---------------------------------------------------------------------------
// NEW: Default state initial active_tabs are all zero
// ---------------------------------------------------------------------------

void test_default_state_active_tabs_are_zero() {
    const auto state = aura::shell::build_default_dock_state();
    for (const auto slot : aura::shell::all_dock_slots()) {
        assert(state.active_tab[slot_index(slot)] == 0U);
    }
}

// ---------------------------------------------------------------------------
// NEW: Move to out-of-range to_index throws
// ---------------------------------------------------------------------------

void test_move_out_of_range_index_throws() {
    using aura::shell::DockSlot;
    using aura::shell::PanelId;
    using aura::shell::PanelMoveRequest;

    const auto state = aura::shell::build_default_dock_state();
    // Center has 2 panels (TopProcesses, DvrTimeline).
    // Valid to_index values are 0, 1, 2 (append). Index 3 is out of range.
    bool threw = false;
    try {
        static_cast<void>(aura::shell::move_panel(
            state,
            PanelMoveRequest{
                .panel_id = PanelId::TelemetryOverview,
                .to_slot = DockSlot::Center,
                .to_index = 10U,  // Far out of range
            }
        ));
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);
}

// ---------------------------------------------------------------------------
// NEW: set_active_tab with tab_index=0 on empty slot is allowed
// ---------------------------------------------------------------------------

void test_set_active_tab_zero_on_empty_slot_allowed() {
    using aura::shell::DockSlot;
    using aura::shell::PanelId;
    using aura::shell::PanelMoveRequest;

    // Create a state where Right has no panels
    const auto state = aura::shell::build_default_dock_state();
    const auto empty_state = aura::shell::move_panel(
        state,
        PanelMoveRequest{
            .panel_id = PanelId::RenderSurface,
            .to_slot = DockSlot::Center,
            .to_index = std::nullopt,
        }
    );
    assert(empty_state.slot_tabs[slot_index(DockSlot::Right)].empty());

    // Setting active_tab=0 on an empty slot must not throw
    bool threw = false;
    try {
        static_cast<void>(aura::shell::set_active_tab(empty_state, DockSlot::Right, 0U));
    } catch (...) {
        threw = true;
    }
    assert(!threw);

    // active_panel on empty slot should return nullopt
    assert(!aura::shell::active_panel(empty_state, DockSlot::Right).has_value());
}

// ---------------------------------------------------------------------------
// NEW: Append semantics: move with nullopt to_index appends to end
// ---------------------------------------------------------------------------

void test_move_nullopt_index_appends_to_end() {
    using aura::shell::DockSlot;
    using aura::shell::PanelId;
    using aura::shell::PanelMoveRequest;

    // Default: Center={TopProcesses[0], DvrTimeline[1]}
    const auto state = aura::shell::build_default_dock_state();

    // Move TelemetryOverview to Center with nullopt → should be appended at index 2
    const auto moved = aura::shell::move_panel(
        state,
        PanelMoveRequest{
            .panel_id = PanelId::TelemetryOverview,
            .to_slot = DockSlot::Center,
            .to_index = std::nullopt,
        }
    );

    // Center now has 3 panels: {TopProcesses, DvrTimeline, TelemetryOverview}
    assert(moved.slot_tabs[slot_index(DockSlot::Center)].size() == 3U);
    assert(moved.slot_tabs[slot_index(DockSlot::Center)][0] == PanelId::TopProcesses);
    assert(moved.slot_tabs[slot_index(DockSlot::Center)][1] == PanelId::DvrTimeline);
    assert(moved.slot_tabs[slot_index(DockSlot::Center)][2] == PanelId::TelemetryOverview);

    // Destination active_tab should point to the newly inserted panel at index 2
    assert(moved.active_tab[slot_index(DockSlot::Center)] == 2U);
    const auto center_active = aura::shell::active_panel(moved, DockSlot::Center);
    assert(center_active.has_value());
    assert(*center_active == PanelId::TelemetryOverview);

    // Left is now empty
    assert(moved.slot_tabs[slot_index(DockSlot::Left)].empty());

    assert_single_instance_per_panel(moved);
}

// ---------------------------------------------------------------------------
// NEW: Full shuffle: move every panel to a different slot
// ---------------------------------------------------------------------------

// Perform a complete 4-move shuffle so every panel ends up in a different slot
// than it started in, and verify the invariant at every step.
void test_full_panel_shuffle_preserves_uniqueness() {
    using aura::shell::DockSlot;
    using aura::shell::PanelId;
    using aura::shell::PanelMoveRequest;

    // Default:
    //   Left = { TelemetryOverview }
    //   Center = { TopProcesses, DvrTimeline }
    //   Right = { RenderSurface }

    auto state = aura::shell::build_default_dock_state();
    assert_single_instance_per_panel(state);

    // Step 1: TelemetryOverview -> Right
    state = aura::shell::move_panel(
        state, PanelMoveRequest{PanelId::TelemetryOverview, DockSlot::Right, std::nullopt}
    );
    assert_single_instance_per_panel(state);

    // Step 2: RenderSurface -> Left
    state = aura::shell::move_panel(
        state, PanelMoveRequest{PanelId::RenderSurface, DockSlot::Left, std::nullopt}
    );
    assert_single_instance_per_panel(state);

    // Step 3: TopProcesses -> Right
    state = aura::shell::move_panel(
        state, PanelMoveRequest{PanelId::TopProcesses, DockSlot::Right, std::nullopt}
    );
    assert_single_instance_per_panel(state);

    // Step 4: DvrTimeline -> Left
    state = aura::shell::move_panel(
        state, PanelMoveRequest{PanelId::DvrTimeline, DockSlot::Left, std::nullopt}
    );
    assert_single_instance_per_panel(state);

    // End state:
    //   Left = { RenderSurface, DvrTimeline }
    //   Center = {} (empty)
    //   Right = { TelemetryOverview, TopProcesses }
    assert(state.slot_tabs[slot_index(DockSlot::Left)].size() == 2U);
    assert(state.slot_tabs[slot_index(DockSlot::Center)].empty());
    assert(state.slot_tabs[slot_index(DockSlot::Right)].size() == 2U);

    // Left[0] == RenderSurface (moved there first)
    assert(state.slot_tabs[slot_index(DockSlot::Left)][0] == PanelId::RenderSurface);
    assert(state.slot_tabs[slot_index(DockSlot::Left)][1] == PanelId::DvrTimeline);

    // Right[0] == TelemetryOverview (moved there first)
    assert(state.slot_tabs[slot_index(DockSlot::Right)][0] == PanelId::TelemetryOverview);
    assert(state.slot_tabs[slot_index(DockSlot::Right)][1] == PanelId::TopProcesses);

    // active_panel for Center must be nullopt
    assert(!aura::shell::active_panel(state, DockSlot::Center).has_value());
}

}  // namespace

int main() {
    // --- Existing tests (unchanged) ---
    test_default_layout();
    test_repeated_moves_preserve_uniqueness();
    test_active_tab_clamps_after_panel_removal();
    test_destination_active_tab_tracks_inserted_panel();
    test_set_active_tab_out_of_range_throws();
    test_empty_slot_behavior();

    // --- New: Aggregate slot tests ---
    test_all_panels_to_single_slot();
    test_move_same_slot_is_noop();

    // --- New: Active tab round-trips ---
    test_active_tab_round_trips();

    // --- New: Panel ordering ---
    test_panel_ordering_after_moves();

    // --- New: to_string coverage ---
    test_to_string_all_slots_and_panels();

    // --- New: all_panel_ids / all_dock_slots ---
    test_all_panel_ids_and_slots_counts();

    // --- New: Default state invariants ---
    test_default_state_active_tabs_are_zero();

    // --- New: Error path coverage ---
    test_move_out_of_range_index_throws();
    test_set_active_tab_zero_on_empty_slot_allowed();

    // --- New: Append semantics ---
    test_move_nullopt_index_appends_to_end();

    // --- New: Full shuffle invariant ---
    test_full_panel_shuffle_preserves_uniqueness();

    return 0;
}
