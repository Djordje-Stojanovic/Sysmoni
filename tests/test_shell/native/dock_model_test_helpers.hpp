#pragma once

#include "aura_shell/dock_model.hpp"

#include <cassert>
#include <cstddef>

inline constexpr std::size_t slot_index(const aura::shell::DockSlot slot) {
    return static_cast<std::size_t>(slot);
}

inline std::size_t panel_count(const aura::shell::DockState& state, const aura::shell::PanelId panel_id) {
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

inline void assert_single_instance_per_panel(const aura::shell::DockState& state) {
    for (const auto panel_id : aura::shell::all_panel_ids()) {
        assert(panel_count(state, panel_id) == 1U);
    }
}
