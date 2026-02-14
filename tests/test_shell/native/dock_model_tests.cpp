#include "aura_shell/dock_model.hpp"

#include <cassert>
#include <cstddef>
#include <optional>
#include <stdexcept>

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

}  // namespace

int main() {
    test_default_layout();
    test_repeated_moves_preserve_uniqueness();
    test_active_tab_clamps_after_panel_removal();
    test_destination_active_tab_tracks_inserted_panel();
    test_set_active_tab_out_of_range_throws();
    test_empty_slot_behavior();
    return 0;
}
