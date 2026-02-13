#include "aura_shell/dock_model.hpp"

#include <cassert>

int main() {
    using aura::shell::DockSlot;
    using aura::shell::PanelId;
    using aura::shell::PanelMoveRequest;

    const auto state = aura::shell::build_default_dock_state();
    const auto left_panel = aura::shell::active_panel(state, DockSlot::Left);
    assert(left_panel.has_value());
    assert(*left_panel == PanelId::TelemetryOverview);

    const auto moved = aura::shell::move_panel(
        state,
        PanelMoveRequest{
            .panel_id = PanelId::DvrTimeline,
            .to_slot = DockSlot::Right,
            .to_index = std::nullopt,
        }
    );
    const auto right_panel = aura::shell::active_panel(moved, DockSlot::Right);
    assert(right_panel.has_value());

    const auto activated = aura::shell::set_active_tab(moved, DockSlot::Center, 0U);
    const auto center_panel = aura::shell::active_panel(activated, DockSlot::Center);
    assert(center_panel.has_value());
    assert(*center_panel == PanelId::TopProcesses);
    return 0;
}

