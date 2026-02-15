#pragma once

#include <QApplication>
#include <QFrame>
#include <QLabel>
#include <QMainWindow>
#include <QMouseEvent>
#include <QPushButton>
#include <QQuickWidget>
#include <QSettings>
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
#include "aura_shell/shell_utils.hpp"
#include "aura_shell/size_metrics.hpp"
#include "aura_shell/ui_theme.hpp"

namespace aura::shell {

struct SlotWidgets {
    QFrame* frame;
    QTabBar* tab_bar;
    QStackedWidget* stack;
};

class AuraShellWindow final : public QMainWindow {
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
    Qt::Edges hit_test_edge(const QPoint& pos) const;
    void sync_theme_to_qml();
    void apply_theme(UiThemeMode mode, bool persist);

    // --- Constants ---
    static constexpr int kResizeBorder = 14;
    static constexpr const char* k_theme_mode_setting_key = "ui/theme_mode";

    // --- Member data ---
    LaunchConfig config_{};
    QSettings settings_{QStringLiteral("Aura"), QStringLiteral("AuraShell")};
    UiThemeMode current_theme_mode_{UiThemeMode::DarkBlue};
    SizeCategory current_size_category_{SizeCategory::Regular};
    SizeMetrics current_metrics_{metrics_for_category(SizeCategory::Regular)};
    QWidget* body_{nullptr};
    double current_interval_seconds_{1.0};
    std::unique_ptr<CockpitController> controller_;
    DockState dock_state_{build_default_dock_state()};
    std::array<SlotWidgets, 3> slot_widgets_{};
    std::array<QWidget*, 4> panel_pages_{};
    std::array<std::array<QPushButton*, 3>, 4> panel_move_buttons_{};
    bool syncing_tabs_{false};
    QFrame* titlebar_{nullptr};
    QPushButton* theme_toggle_btn_{nullptr};
    QQuickWidget* quick_{nullptr};
    QQuickWidget* timeline_quick_{nullptr};
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
    std::array<QLabel*, 4> panel_title_labels_{};
    bool dragging_{false};
    QPoint drag_origin_{};
    Qt::Edges resize_edge_{};
};

}  // namespace aura::shell
