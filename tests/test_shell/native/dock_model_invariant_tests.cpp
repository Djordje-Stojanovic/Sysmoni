#include "dock_model_test_helpers.hpp"

#include "aura_shell/dock_model.hpp"

#include <cassert>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string_view>

// ---------------------------------------------------------------------------
// to_string round-trip for all slots and all panels
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
    assert(aura::shell::to_string(PanelId::ProcessPanel)      == "process_panel");

    // None of the string representations should be "unknown"
    assert(aura::shell::to_string(DockSlot::Left)   != "unknown");
    assert(aura::shell::to_string(DockSlot::Center) != "unknown");
    assert(aura::shell::to_string(DockSlot::Right)  != "unknown");
    assert(aura::shell::to_string(PanelId::TelemetryOverview) != "unknown");
    assert(aura::shell::to_string(PanelId::TopProcesses)      != "unknown");
    assert(aura::shell::to_string(PanelId::DvrTimeline)       != "unknown");
    assert(aura::shell::to_string(PanelId::RenderSurface)     != "unknown");
    assert(aura::shell::to_string(PanelId::ProcessPanel)      != "unknown");
}

// ---------------------------------------------------------------------------
// all_panel_ids() and all_dock_slots() coverage and ordering
// ---------------------------------------------------------------------------

void test_all_panel_ids_and_slots_counts() {
    const auto panels = aura::shell::all_panel_ids();
    assert(panels.size() == 5U);

    const auto slots = aura::shell::all_dock_slots();
    assert(slots.size() == 3U);

    // Verify all expected panels are present
    bool found_telemetry = false;
    bool found_processes = false;
    bool found_dvr = false;
    bool found_render = false;
    bool found_process_panel = false;
    for (const auto panel_id : panels) {
        if (panel_id == aura::shell::PanelId::TelemetryOverview) { found_telemetry = true; }
        if (panel_id == aura::shell::PanelId::TopProcesses)      { found_processes = true; }
        if (panel_id == aura::shell::PanelId::DvrTimeline)       { found_dvr = true; }
        if (panel_id == aura::shell::PanelId::RenderSurface)     { found_render = true; }
        if (panel_id == aura::shell::PanelId::ProcessPanel)      { found_process_panel = true; }
    }
    assert(found_telemetry);
    assert(found_processes);
    assert(found_dvr);
    assert(found_render);
    assert(found_process_panel);

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
// Default state initial active_tabs are all zero
// ---------------------------------------------------------------------------

void test_default_state_active_tabs_are_zero() {
    const auto state = aura::shell::build_default_dock_state();
    for (const auto slot : aura::shell::all_dock_slots()) {
        assert(state.active_tab[slot_index(slot)] == 0U);
    }
}

// ---------------------------------------------------------------------------
// Move to out-of-range to_index throws
// ---------------------------------------------------------------------------

void test_move_out_of_range_index_throws() {
    using aura::shell::DockSlot;
    using aura::shell::PanelId;
    using aura::shell::PanelMoveRequest;

    const auto state = aura::shell::build_default_dock_state();
    // Right has 2 panels (ProcessPanel, TopProcesses).
    // Valid to_index values are 0, 1, 2 (append). Index 10 is out of range.
    bool threw = false;
    try {
        static_cast<void>(aura::shell::move_panel(
            state,
            PanelMoveRequest{
                .panel_id = PanelId::TelemetryOverview,
                .to_slot = DockSlot::Right,
                .to_index = 10U,  // Far out of range
            }
        ));
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    assert(threw);
}

// ---------------------------------------------------------------------------
// set_active_tab with tab_index=0 on empty slot is allowed
// ---------------------------------------------------------------------------

void test_set_active_tab_zero_on_empty_slot_allowed() {
    using aura::shell::DockSlot;
    using aura::shell::PanelId;
    using aura::shell::PanelMoveRequest;

    // Create a state where Center has no panels (move both RenderSurface and DvrTimeline out)
    const auto state = aura::shell::build_default_dock_state();
    auto intermediate = aura::shell::move_panel(
        state,
        PanelMoveRequest{
            .panel_id = PanelId::RenderSurface,
            .to_slot = DockSlot::Right,
            .to_index = std::nullopt,
        }
    );
    const auto empty_state = aura::shell::move_panel(
        intermediate,
        PanelMoveRequest{
            .panel_id = PanelId::DvrTimeline,
            .to_slot = DockSlot::Right,
            .to_index = std::nullopt,
        }
    );
    assert(empty_state.slot_tabs[slot_index(DockSlot::Center)].empty());

    // Setting active_tab=0 on an empty slot must not throw
    bool threw = false;
    try {
        static_cast<void>(aura::shell::set_active_tab(empty_state, DockSlot::Center, 0U));
    } catch (...) {
        threw = true;
    }
    assert(!threw);

    // active_panel on empty slot should return nullopt
    assert(!aura::shell::active_panel(empty_state, DockSlot::Center).has_value());
}

// ---------------------------------------------------------------------------
// Append semantics: move with nullopt to_index appends to end
// ---------------------------------------------------------------------------

void test_move_nullopt_index_appends_to_end() {
    using aura::shell::DockSlot;
    using aura::shell::PanelId;
    using aura::shell::PanelMoveRequest;

    // Default: Right={ProcessPanel[0], TopProcesses[1]}
    const auto state = aura::shell::build_default_dock_state();

    // Move TelemetryOverview to Right with nullopt -> should be appended at index 2
    const auto moved = aura::shell::move_panel(
        state,
        PanelMoveRequest{
            .panel_id = PanelId::TelemetryOverview,
            .to_slot = DockSlot::Right,
            .to_index = std::nullopt,
        }
    );

    // Right now has 3 panels: {ProcessPanel, TopProcesses, TelemetryOverview}
    assert(moved.slot_tabs[slot_index(DockSlot::Right)].size() == 3U);
    assert(moved.slot_tabs[slot_index(DockSlot::Right)][0] == PanelId::ProcessPanel);
    assert(moved.slot_tabs[slot_index(DockSlot::Right)][1] == PanelId::TopProcesses);
    assert(moved.slot_tabs[slot_index(DockSlot::Right)][2] == PanelId::TelemetryOverview);

    // Destination active_tab should point to the newly inserted panel at index 2
    assert(moved.active_tab[slot_index(DockSlot::Right)] == 2U);
    const auto right_active = aura::shell::active_panel(moved, DockSlot::Right);
    assert(right_active.has_value());
    assert(*right_active == PanelId::TelemetryOverview);

    // Left is now empty
    assert(moved.slot_tabs[slot_index(DockSlot::Left)].empty());

    assert_single_instance_per_panel(moved);
}

// ---------------------------------------------------------------------------
// Full shuffle: move every panel to a different slot
// ---------------------------------------------------------------------------

// Perform a complete 4-move shuffle so every panel ends up in a different slot
// than it started in, and verify the invariant at every step.
void test_full_panel_shuffle_preserves_uniqueness() {
    using aura::shell::DockSlot;
    using aura::shell::PanelId;
    using aura::shell::PanelMoveRequest;

    // Default:
    //   Left = { TelemetryOverview }
    //   Center = { RenderSurface, DvrTimeline }
    //   Right = { ProcessPanel, TopProcesses }

    auto state = aura::shell::build_default_dock_state();
    assert_single_instance_per_panel(state);

    // Step 1: TelemetryOverview -> Center
    state = aura::shell::move_panel(
        state, PanelMoveRequest{PanelId::TelemetryOverview, DockSlot::Center, std::nullopt}
    );
    assert_single_instance_per_panel(state);

    // Step 2: RenderSurface -> Left
    state = aura::shell::move_panel(
        state, PanelMoveRequest{PanelId::RenderSurface, DockSlot::Left, std::nullopt}
    );
    assert_single_instance_per_panel(state);

    // Step 3: TopProcesses -> Left
    state = aura::shell::move_panel(
        state, PanelMoveRequest{PanelId::TopProcesses, DockSlot::Left, std::nullopt}
    );
    assert_single_instance_per_panel(state);

    // Step 4: DvrTimeline -> Center
    state = aura::shell::move_panel(
        state, PanelMoveRequest{PanelId::DvrTimeline, DockSlot::Center, std::nullopt}
    );
    assert_single_instance_per_panel(state);

    // Step 5: ProcessPanel -> Center
    state = aura::shell::move_panel(
        state, PanelMoveRequest{PanelId::ProcessPanel, DockSlot::Center, std::nullopt}
    );
    assert_single_instance_per_panel(state);

    // End state:
    //   Left = { RenderSurface, TopProcesses }
    //   Center = { TelemetryOverview, DvrTimeline, ProcessPanel }
    //   Right = {} (empty)
    assert(state.slot_tabs[slot_index(DockSlot::Left)].size() == 2U);
    assert(state.slot_tabs[slot_index(DockSlot::Center)].size() == 3U);
    assert(state.slot_tabs[slot_index(DockSlot::Right)].empty());

    assert(state.slot_tabs[slot_index(DockSlot::Left)][0] == PanelId::RenderSurface);
    assert(state.slot_tabs[slot_index(DockSlot::Left)][1] == PanelId::TopProcesses);

    assert(state.slot_tabs[slot_index(DockSlot::Center)][0] == PanelId::TelemetryOverview);
    assert(state.slot_tabs[slot_index(DockSlot::Center)][1] == PanelId::DvrTimeline);
    assert(state.slot_tabs[slot_index(DockSlot::Center)][2] == PanelId::ProcessPanel);

    // active_panel for Right must be nullopt
    assert(!aura::shell::active_panel(state, DockSlot::Right).has_value());
}
