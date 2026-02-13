#include "aura_shell/dock_model.hpp"

#include <algorithm>
#include <stdexcept>

namespace aura::shell {

namespace {

constexpr std::size_t slot_index(const DockSlot slot) {
    return static_cast<std::size_t>(slot);
}

constexpr std::size_t panel_index(const PanelId panel_id) {
    return static_cast<std::size_t>(panel_id);
}

std::size_t clamp_active_index(const std::size_t value, const std::size_t tab_count) {
    if (tab_count == 0U) {
        return 0U;
    }
    return std::min(value, tab_count - 1U);
}

}  // namespace

std::array<PanelId, 4> all_panel_ids() {
    return {
        PanelId::TelemetryOverview,
        PanelId::TopProcesses,
        PanelId::DvrTimeline,
        PanelId::RenderSurface,
    };
}

std::array<DockSlot, 3> all_dock_slots() {
    return {DockSlot::Left, DockSlot::Center, DockSlot::Right};
}

DockState build_default_dock_state() {
    DockState state;
    state.slot_tabs[slot_index(DockSlot::Left)] = {PanelId::TelemetryOverview};
    state.slot_tabs[slot_index(DockSlot::Center)] = {
        PanelId::TopProcesses,
        PanelId::DvrTimeline,
    };
    state.slot_tabs[slot_index(DockSlot::Right)] = {PanelId::RenderSurface};
    state.active_tab = {0U, 0U, 0U};
    return state;
}

DockState move_panel(const DockState& state, const PanelMoveRequest& request) {
    DockState next_state = state;

    const auto panel = request.panel_id;
    std::optional<DockSlot> source_slot;
    for (const auto slot : all_dock_slots()) {
        const auto idx = slot_index(slot);
        const auto& tabs = next_state.slot_tabs[idx];
        if (std::find(tabs.begin(), tabs.end(), panel) != tabs.end()) {
            source_slot = slot;
            break;
        }
    }

    if (!source_slot.has_value()) {
        throw std::invalid_argument("Panel is not docked.");
    }

    auto& source_tabs = next_state.slot_tabs[slot_index(*source_slot)];
    source_tabs.erase(std::remove(source_tabs.begin(), source_tabs.end(), panel), source_tabs.end());

    auto& destination_tabs = next_state.slot_tabs[slot_index(request.to_slot)];
    const std::size_t insert_index = request.to_index.value_or(destination_tabs.size());
    if (insert_index > destination_tabs.size()) {
        throw std::invalid_argument("to_index is outside the destination slot range.");
    }
    destination_tabs.insert(destination_tabs.begin() + static_cast<std::ptrdiff_t>(insert_index), panel);

    for (const auto slot : all_dock_slots()) {
        const auto idx = slot_index(slot);
        next_state.active_tab[idx] = clamp_active_index(
            next_state.active_tab[idx],
            next_state.slot_tabs[idx].size()
        );
    }
    next_state.active_tab[slot_index(request.to_slot)] = clamp_active_index(
        insert_index,
        destination_tabs.size()
    );
    return next_state;
}

DockState set_active_tab(const DockState& state, const DockSlot slot, const std::size_t tab_index) {
    DockState next_state = state;
    const auto idx = slot_index(slot);
    const auto tab_count = next_state.slot_tabs[idx].size();

    if (tab_count == 0U) {
        if (tab_index != 0U) {
            throw std::invalid_argument("tab_index must be 0 when slot is empty.");
        }
        next_state.active_tab[idx] = 0U;
        return next_state;
    }

    if (tab_index >= tab_count) {
        throw std::invalid_argument("tab_index is outside the slot range.");
    }
    next_state.active_tab[idx] = tab_index;
    return next_state;
}

std::optional<PanelId> active_panel(const DockState& state, const DockSlot slot) {
    const auto idx = slot_index(slot);
    const auto& tabs = state.slot_tabs[idx];
    if (tabs.empty()) {
        return std::nullopt;
    }
    const auto selected = clamp_active_index(state.active_tab[idx], tabs.size());
    return tabs[selected];
}

std::string_view to_string(const DockSlot slot) {
    switch (slot) {
        case DockSlot::Left:
            return "left";
        case DockSlot::Center:
            return "center";
        case DockSlot::Right:
            return "right";
    }
    return "unknown";
}

std::string_view to_string(const PanelId panel_id) {
    switch (panel_id) {
        case PanelId::TelemetryOverview:
            return "telemetry_overview";
        case PanelId::TopProcesses:
            return "top_processes";
        case PanelId::DvrTimeline:
            return "dvr_timeline";
        case PanelId::RenderSurface:
            return "render_surface";
    }
    return "unknown";
}

}  // namespace aura::shell

