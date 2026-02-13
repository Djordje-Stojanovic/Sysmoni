#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

namespace aura::shell {

enum class DockSlot : std::size_t {
    Left = 0,
    Center = 1,
    Right = 2,
};

enum class PanelId : std::size_t {
    TelemetryOverview = 0,
    TopProcesses = 1,
    DvrTimeline = 2,
    RenderSurface = 3,
};

struct DockState {
    std::array<std::vector<PanelId>, 3> slot_tabs{};
    std::array<std::size_t, 3> active_tab{};
};

struct PanelMoveRequest {
    PanelId panel_id;
    DockSlot to_slot;
    std::optional<std::size_t> to_index;
};

std::array<PanelId, 4> all_panel_ids();
std::array<DockSlot, 3> all_dock_slots();

DockState build_default_dock_state();
DockState move_panel(const DockState& state, const PanelMoveRequest& request);
DockState set_active_tab(const DockState& state, DockSlot slot, std::size_t tab_index);
std::optional<PanelId> active_panel(const DockState& state, DockSlot slot);

std::string_view to_string(DockSlot slot);
std::string_view to_string(PanelId panel_id);

}  // namespace aura::shell

