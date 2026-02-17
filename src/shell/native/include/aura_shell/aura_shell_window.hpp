#pragma once

#include <QApplication>
#include <QDrag>
#include <QFrame>
#include <QLabel>
#include <QMainWindow>
#include <QMimeData>
#include <QMouseEvent>
#include <QPushButton>
#include <QQuickWidget>
#include <QSettings>
#include <QSplitter>
#include <QStackedWidget>
#include <QTabBar>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <array>
#include <memory>
#include <optional>

#include "aura_shell/cockpit_controller.hpp"
#include "aura_shell/dock_model.hpp"
#include "aura_shell/process_list_model.hpp"
#include "aura_shell/shell_utils.hpp"
#include "aura_shell/size_metrics.hpp"
#include "aura_shell/ui_theme.hpp"

namespace aura::shell {

// Forward declaration
class AuraShellWindow;

// ── DragTabBar: tab bar that supports cross-zone drag-and-drop ──
// Starts a QDrag with MIME "application/x-aura-panel-id" when a tab is
// dragged beyond a small threshold. Destination slot frames accept the
// drop and call move_panel_to_slot().
class DragTabBar final : public QTabBar {
    Q_OBJECT
public:
    explicit DragTabBar(DockSlot slot, AuraShellWindow* window, QWidget* parent = nullptr);
protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
private:
    DockSlot slot_;
    AuraShellWindow* window_;
    QPoint drag_start_;
    int drag_tab_index_{-1};
};

struct SlotWidgets {
    QFrame* frame;
    QTabBar* tab_bar;
    QStackedWidget* stack;
};

class AuraShellWindow final : public QMainWindow {
    Q_OBJECT
    friend class DragTabBar;
public:
    explicit AuraShellWindow(const LaunchConfig& config, QWidget* parent = nullptr);

protected:
#ifdef _WIN32
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;
#endif
    bool eventFilter(QObject* watched, QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    // --- Panel management (aura_shell_window_panels.cpp) ---
    QVBoxLayout* create_panel_page(PanelId panel_id, const QString& title);
    void build_panel_pages();
    std::optional<DockSlot> panel_slot(PanelId panel_id) const;
    QString themed_tab_title(PanelId panel_id) const;
    void rebuild_dock_slots();
    void on_tab_changed(DockSlot slot, int tab_index);
    void move_panel_to_slot(PanelId panel_id, DockSlot target_slot);
    void update_move_button_states();

    // --- Refresh (aura_shell_window_refresh.cpp) ---
    void refresh_cockpit();

    // --- Core methods (aura_shell_window.cpp) ---
    SlotWidgets build_slot(DockSlot slot, QWidget* parent);
    void sync_theme_to_qml();
    void apply_theme(UiThemeMode mode, bool persist);

    // --- Constants ---
    static constexpr const char* k_theme_mode_setting_key = "ui/theme_mode";

    // --- Member data ---
    LaunchConfig config_{};
    QSettings settings_{QStringLiteral("Aura"), QStringLiteral("AuraShell")};
    UiThemeMode current_theme_mode_{UiThemeMode::DarkBlue};
    SizeCategory current_size_category_{SizeCategory::Regular};
    SizeMetrics current_metrics_{metrics_for_category(SizeCategory::Regular)};
    QWidget* body_{nullptr};
    QSplitter* splitter_{nullptr};
    double current_interval_seconds_{1.0};
    std::unique_ptr<CockpitController> controller_;
    DockState dock_state_{build_default_dock_state()};
    std::array<SlotWidgets, 3> slot_widgets_{};
    std::array<QWidget*, 5> panel_pages_{};
    std::array<std::array<QPushButton*, 3>, 5> panel_move_buttons_{};
    bool syncing_tabs_{false};
    QFrame* titlebar_{nullptr};
    QPushButton* max_btn_{nullptr};
    QPushButton* theme_toggle_btn_{nullptr};
    QQuickWidget* quick_{nullptr};
    QQuickWidget* timeline_quick_{nullptr};
    QQuickWidget* process_quick_{nullptr};
    ProcessListModel* process_model_{nullptr};
    ITelemetryBridge* telemetry_bridge_raw_{nullptr};
    QTimer* update_timer_{nullptr};
    QLabel* telemetry_cpu_{nullptr};
    QLabel* telemetry_memory_{nullptr};
    QLabel* telemetry_timestamp_{nullptr};
    QLabel* telemetry_status_{nullptr};
    QLabel* process_status_{nullptr};
    QLabel* timeline_status_{nullptr};
    QLabel* render_status_{nullptr};
    QLabel* footer_status_{nullptr};
    std::array<QLabel*, 5> process_labels_{};
    bool theme_dirty_{true};
    bool is_pink_{false};
    QLabel* cpu_key_{nullptr};
    QLabel* mem_key_{nullptr};
    QLabel* gpu_key_{nullptr};
    QLabel* gpu_value_{nullptr};
    QLabel* disk_key_{nullptr};
    QLabel* disk_value_{nullptr};
    QLabel* net_key_{nullptr};
    QLabel* net_value_{nullptr};
    QLabel* thermal_key_{nullptr};
    QLabel* thermal_value_{nullptr};
    std::array<QLabel*, 5> panel_title_labels_{};
    bool dragging_{false};
    QPoint drag_origin_{};
};

}  // namespace aura::shell
