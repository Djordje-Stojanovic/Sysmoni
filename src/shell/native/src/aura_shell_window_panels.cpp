#include "aura_shell/aura_shell_window.hpp"

#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QQmlContext>
#include <QQuickWidget>
#include <QScrollArea>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QTabBar>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <stdexcept>

namespace aura::shell {

QVBoxLayout* AuraShellWindow::create_panel_page(const PanelId panel_id, const QString& title) {
    auto* page = new QWidget(this);
    panel_pages_[panel_index(panel_id)] = page;

    auto* page_layout = new QVBoxLayout(page);
    page_layout->setContentsMargins(12, 10, 12, 8);
    page_layout->setSpacing(0);

    // --- Header row ---
    auto* header_layout = new QHBoxLayout();
    header_layout->setContentsMargins(0, 0, 0, 0);
    header_layout->setSpacing(6);

    auto* title_label = new QLabel(title.toUpper(), page);
    title_label->setObjectName("panelTitle");
    panel_title_labels_[panel_index(panel_id)] = title_label;
    header_layout->addWidget(title_label);
    header_layout->addStretch(1);

    auto* move_to_label = new QLabel("MOVE:", page);
    move_to_label->setObjectName("moveToLabel");
    header_layout->addWidget(move_to_label);

    for (const auto slot : all_dock_slots()) {
        auto* move_button = new QPushButton(slot_short_label(slot), page);
        move_button->setObjectName("moveBtn");
        move_button->setFocusPolicy(Qt::NoFocus);
        connect(move_button, &QPushButton::clicked, this, [this, panel_id, slot]() {
            move_panel_to_slot(panel_id, slot);
        });
        panel_move_buttons_[panel_index(panel_id)][slot_index(slot)] = move_button;
        header_layout->addWidget(move_button);
    }

    page_layout->addLayout(header_layout);
    page_layout->addSpacing(6);

    // Accent separator line below header
    auto* sep_line = new QFrame(page);
    sep_line->setObjectName("panelHeaderLine");
    sep_line->setFrameShape(QFrame::HLine);
    sep_line->setFixedHeight(1);
    page_layout->addWidget(sep_line);
    page_layout->addSpacing(10);

    // Content area
    auto* content_layout = new QVBoxLayout();
    content_layout->setContentsMargins(0, 0, 0, 0);
    content_layout->setSpacing(4);
    page_layout->addLayout(content_layout, 1);

    return content_layout;
}

void AuraShellWindow::build_panel_pages() {
    // --- Telemetry Overview ---
    {
        auto* telemetry_layout =
            create_panel_page(PanelId::TelemetryOverview, QStringLiteral("Telemetry Overview"));

        QWidget* parent_page = panel_pages_[panel_index(PanelId::TelemetryOverview)];

        // CPU metric block
        auto* cpu_block = new QWidget(parent_page);
        auto* cpu_block_layout = new QVBoxLayout(cpu_block);
        cpu_block_layout->setContentsMargins(0, 0, 0, 8);
        cpu_block_layout->setSpacing(1);

        cpu_key_ = new QLabel("CPU LOAD", parent_page);
        cpu_key_->setObjectName("metricKey");
        telemetry_cpu_ = new QLabel("CPU --", parent_page);
        telemetry_cpu_->setObjectName("metricValue");

        cpu_block_layout->addWidget(cpu_key_);
        cpu_block_layout->addWidget(telemetry_cpu_);
        telemetry_layout->addWidget(cpu_block);

        // Memory metric block
        auto* mem_block = new QWidget(parent_page);
        auto* mem_block_layout = new QVBoxLayout(mem_block);
        mem_block_layout->setContentsMargins(0, 0, 0, 8);
        mem_block_layout->setSpacing(1);

        mem_key_ = new QLabel("MEMORY USE", parent_page);
        mem_key_->setObjectName("metricKey");
        telemetry_memory_ = new QLabel("Memory --", parent_page);
        telemetry_memory_->setObjectName("metricValue");

        mem_block_layout->addWidget(mem_key_);
        mem_block_layout->addWidget(telemetry_memory_);
        telemetry_layout->addWidget(mem_block);

        // GPU metric block
        auto* gpu_block = new QWidget(parent_page);
        auto* gpu_block_layout = new QVBoxLayout(gpu_block);
        gpu_block_layout->setContentsMargins(0, 0, 0, 8);
        gpu_block_layout->setSpacing(1);

        gpu_key_ = new QLabel("GPU", parent_page);
        gpu_key_->setObjectName("metricKey");
        gpu_value_ = new QLabel("--", parent_page);
        gpu_value_->setObjectName("metricValue");

        gpu_block_layout->addWidget(gpu_key_);
        gpu_block_layout->addWidget(gpu_value_);
        telemetry_layout->addWidget(gpu_block);

        // Disk metric block
        auto* disk_block = new QWidget(parent_page);
        auto* disk_block_layout = new QVBoxLayout(disk_block);
        disk_block_layout->setContentsMargins(0, 0, 0, 8);
        disk_block_layout->setSpacing(1);

        disk_key_ = new QLabel("DISK ACTIVITY", parent_page);
        disk_key_->setObjectName("metricKey");
        disk_value_ = new QLabel("--", parent_page);
        disk_value_->setObjectName("metricValue");

        disk_block_layout->addWidget(disk_key_);
        disk_block_layout->addWidget(disk_value_);
        telemetry_layout->addWidget(disk_block);

        // Network metric block
        auto* net_block = new QWidget(parent_page);
        auto* net_block_layout = new QVBoxLayout(net_block);
        net_block_layout->setContentsMargins(0, 0, 0, 8);
        net_block_layout->setSpacing(1);

        net_key_ = new QLabel("NETWORK", parent_page);
        net_key_->setObjectName("metricKey");
        net_value_ = new QLabel("--", parent_page);
        net_value_->setObjectName("metricValue");

        net_block_layout->addWidget(net_key_);
        net_block_layout->addWidget(net_value_);
        telemetry_layout->addWidget(net_block);

        // Thermal metric block
        auto* thermal_block = new QWidget(parent_page);
        auto* thermal_block_layout = new QVBoxLayout(thermal_block);
        thermal_block_layout->setContentsMargins(0, 0, 0, 8);
        thermal_block_layout->setSpacing(1);

        thermal_key_ = new QLabel("TEMPERATURE", parent_page);
        thermal_key_->setObjectName("metricKey");
        thermal_value_ = new QLabel("--", parent_page);
        thermal_value_->setObjectName("metricValue");

        thermal_block_layout->addWidget(thermal_key_);
        thermal_block_layout->addWidget(thermal_value_);
        telemetry_layout->addWidget(thermal_block);

        // Timestamp and status
        telemetry_timestamp_ = new QLabel("Timestamp --", parent_page);
        telemetry_timestamp_->setObjectName("telemetryTimestamp");
        telemetry_layout->addWidget(telemetry_timestamp_);

        telemetry_status_ = new QLabel("Awaiting telemetry snapshot...", parent_page);
        telemetry_status_->setObjectName("telemetryStatus");
        telemetry_status_->setWordWrap(true);
        telemetry_layout->addWidget(telemetry_status_);

        telemetry_layout->addStretch(1);
    }

    // --- Top Processes ---
    {
        auto* processes_layout =
            create_panel_page(PanelId::TopProcesses, QStringLiteral("Top Processes"));

        QWidget* parent_page = panel_pages_[panel_index(PanelId::TopProcesses)];

        process_status_ = new QLabel("Waiting for process samples...", parent_page);
        process_status_->setObjectName("processStatus");
        process_status_->setWordWrap(true);
        processes_layout->addWidget(process_status_);

        for (std::size_t i = 0; i < process_labels_.size(); ++i) {
            process_labels_[i] = new QLabel("-", parent_page);
            // Alternate row styling for readability
            process_labels_[i]->setObjectName((i % 2 == 0) ? "processRow" : "processRowAlt");
            process_labels_[i]->setTextInteractionFlags(Qt::TextSelectableByMouse);
            process_labels_[i]->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            processes_layout->addWidget(process_labels_[i]);
        }
        processes_layout->addStretch(1);
    }

    // --- DVR Timeline ---
    {
        auto* timeline_layout =
            create_panel_page(PanelId::DvrTimeline, QStringLiteral("DVR Timeline"));

        QWidget* parent_page = panel_pages_[panel_index(PanelId::DvrTimeline)];

        timeline_quick_ = new QQuickWidget(parent_page);
        timeline_quick_->setResizeMode(QQuickWidget::SizeRootObjectToView);
        timeline_quick_->setSource(
            QUrl::fromLocalFile(QStringLiteral(AURA_SHELL_TIMELINE_QML_PATH)));
        timeline_quick_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

        timeline_status_ = new QLabel("Awaiting timeline samples...", parent_page);
        timeline_status_->setObjectName("timelineStatus");
        timeline_status_->setWordWrap(true);

        timeline_layout->addWidget(timeline_quick_, 1);
        timeline_layout->addWidget(timeline_status_);
    }

    // --- Render Surface ---
    {
        auto* render_layout =
            create_panel_page(PanelId::RenderSurface, QStringLiteral("Render Surface"));

        QWidget* parent_page = panel_pages_[panel_index(PanelId::RenderSurface)];

        quick_ = new QQuickWidget(parent_page);
        quick_->setResizeMode(QQuickWidget::SizeRootObjectToView);
        quick_->setSource(QUrl::fromLocalFile(QStringLiteral(AURA_SHELL_QML_PATH)));
        quick_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

        render_status_ = new QLabel("Cockpit scene online", parent_page);
        render_status_->setObjectName("renderStatus");
        render_status_->setWordWrap(true);

        render_layout->addWidget(quick_, 1);
        render_layout->addWidget(render_status_);
    }

    // --- Process Panel (QML) ---
    {
        auto* process_layout =
            create_panel_page(PanelId::ProcessPanel, QStringLiteral("Process Manager"));

        QWidget* parent_page = panel_pages_[panel_index(PanelId::ProcessPanel)];

        process_model_ = new ProcessListModel(this);
        process_model_->setBridge(telemetry_bridge_raw_);

        process_quick_ = new QQuickWidget(parent_page);
        process_quick_->setResizeMode(QQuickWidget::SizeRootObjectToView);
        process_quick_->rootContext()->setContextProperty(
            QStringLiteral("processModel"), process_model_);
        process_quick_->setSource(
            QUrl::fromLocalFile(QStringLiteral(AURA_SHELL_PROCESS_QML_PATH)));
        process_quick_->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

        process_layout->addWidget(process_quick_, 1);
    }
}

std::optional<DockSlot> AuraShellWindow::panel_slot(const PanelId panel_id) const {
    for (const auto slot : all_dock_slots()) {
        const auto& tabs = dock_state_.slot_tabs[slot_index(slot)];
        if (std::find(tabs.begin(), tabs.end(), panel_id) != tabs.end()) {
            return slot;
        }
    }
    return std::nullopt;
}

QString AuraShellWindow::themed_tab_title(const PanelId panel_id) const {
    if (is_pink_) {
        switch (panel_id) {
            case PanelId::TelemetryOverview: return QStringLiteral("Status");
            case PanelId::TopProcesses:      return QStringLiteral("Apps");
            case PanelId::DvrTimeline:        return QStringLiteral("History");
            case PanelId::RenderSurface:      return QStringLiteral("Cockpit");
            case PanelId::ProcessPanel:       return QStringLiteral("Manager");
        }
    }
    return panel_title(panel_id);
}

void AuraShellWindow::rebuild_dock_slots() {
    syncing_tabs_ = true;
    for (const auto slot : all_dock_slots()) {
        const std::size_t slot_idx = slot_index(slot);
        auto& sw = slot_widgets_[slot_idx];

        while (sw.tab_bar->count() > 0) {
            sw.tab_bar->removeTab(0);
        }
        while (sw.stack->count() > 0) {
            sw.stack->removeWidget(sw.stack->widget(0));
        }
    }

    for (const auto slot : all_dock_slots()) {
        const std::size_t slot_idx = slot_index(slot);
        auto& sw = slot_widgets_[slot_idx];
        const auto& tabs = dock_state_.slot_tabs[slot_idx];

        for (const auto panel_id : tabs) {
            sw.tab_bar->addTab(themed_tab_title(panel_id));
            QWidget* page = panel_pages_[panel_index(panel_id)];
            if (page != nullptr) {
                sw.stack->addWidget(page);
            }
        }

        if (tabs.empty()) {
            sw.tab_bar->setCurrentIndex(-1);
            sw.stack->setCurrentIndex(-1);
            continue;
        }

        const int active_tab = static_cast<int>(std::min(
            dock_state_.active_tab[slot_idx],
            tabs.size() - 1U
        ));
        sw.tab_bar->setCurrentIndex(active_tab);
        sw.stack->setCurrentIndex(active_tab);
    }
    syncing_tabs_ = false;
    update_move_button_states();
}

void AuraShellWindow::on_tab_changed(const DockSlot slot, const int tab_index) {
    if (syncing_tabs_ || tab_index < 0) {
        return;
    }

    const std::size_t slot_idx = slot_index(slot);
    const auto tab_count = dock_state_.slot_tabs[slot_idx].size();
    if (tab_count == 0U || static_cast<std::size_t>(tab_index) >= tab_count) {
        return;
    }

    try {
        dock_state_ = set_active_tab(dock_state_, slot, static_cast<std::size_t>(tab_index));
    } catch (const std::invalid_argument&) {
        return;
    }

    syncing_tabs_ = true;
    slot_widgets_[slot_idx].stack->setCurrentIndex(tab_index);
    syncing_tabs_ = false;
    update_move_button_states();
}

void AuraShellWindow::move_panel_to_slot(
    const PanelId panel_id,
    const DockSlot target_slot
) {
    try {
        dock_state_ = move_panel(
            dock_state_,
            PanelMoveRequest{
                panel_id,
                target_slot,
                std::nullopt,
            }
        );
    } catch (const std::invalid_argument&) {
        return;
    }
    rebuild_dock_slots();
}

void AuraShellWindow::update_move_button_states() {
    for (const auto panel_id : all_panel_ids()) {
        const std::optional<DockSlot> source_slot = panel_slot(panel_id);
        for (const auto target_slot : all_dock_slots()) {
            QPushButton* button = panel_move_buttons_[panel_index(panel_id)][slot_index(target_slot)];
            if (button == nullptr) {
                continue;
            }
            const bool disable = source_slot.has_value() && *source_slot == target_slot;
            button->setEnabled(!disable);
        }
    }
}

}  // namespace aura::shell
