#include "aura_shell/process_list_model.hpp"
#include "aura_shell/telemetry_bridge.hpp"

#include <algorithm>

namespace aura::shell {

ProcessListModel::ProcessListModel(QObject* parent)
    : QAbstractListModel(parent) {}

int ProcessListModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return static_cast<int>(entries_.size());
}

QVariant ProcessListModel::data(const QModelIndex& index, const int role) const {
    if (!index.isValid() || index.row() < 0 ||
        index.row() >= static_cast<int>(entries_.size())) {
        return {};
    }
    const auto& e = entries_[static_cast<std::size_t>(index.row())];
    switch (role) {
        case PidRole:
            return static_cast<int>(e.pid);
        case NameRole:
            return QString::fromStdString(e.name);
        case CpuPercentRole:
            return e.cpu_percent;
        case MemoryBytesRole:
            return static_cast<double>(e.memory_bytes);
        case MemoryPercentRole:
            return e.memory_percent;
        default:
            return {};
    }
}

QHash<int, QByteArray> ProcessListModel::roleNames() const {
    return {
        {PidRole, "pid"},
        {NameRole, "name"},
        {CpuPercentRole, "cpuPercent"},
        {MemoryBytesRole, "memoryBytes"},
        {MemoryPercentRole, "memoryPercent"},
    };
}

void ProcessListModel::setBridge(ITelemetryBridge* bridge) {
    bridge_ = bridge;
}

bool ProcessListModel::pids_changed(const std::vector<ProcessSample>& samples) const {
    if (samples.size() != entries_.size()) {
        return true;
    }
    for (std::size_t i = 0; i < samples.size(); ++i) {
        if (samples[i].pid != entries_[i].pid) {
            return true;
        }
    }
    return false;
}

void ProcessListModel::refresh(ITelemetryBridge* bridge, const std::uint64_t total_memory_bytes) {
    if (bridge == nullptr) {
        return;
    }
    std::string error;
    // sort_column mapping: 0=cpu (ABI col 2), 1=memory (ABI col 3), 2=name (ABI col 1)
    std::uint8_t abi_col = 2;
    if (sort_column_ == 1) abi_col = 3;
    else if (sort_column_ == 2) abi_col = 1;

    auto samples = bridge->collect_process_details(128, abi_col, sort_descending_, error);
    if (!error.empty() && samples.empty()) {
        return;
    }

    const double total_mem = total_memory_bytes > 0
        ? static_cast<double>(total_memory_bytes)
        : 1.0;

    if (!pids_changed(samples)) {
        // Same PIDs in same order — update in place without model reset
        for (std::size_t i = 0; i < samples.size(); ++i) {
            entries_[i].cpu_percent = samples[i].cpu_percent;
            entries_[i].memory_bytes = samples[i].memory_rss_bytes;
            entries_[i].memory_percent =
                (static_cast<double>(samples[i].memory_rss_bytes) / total_mem) * 100.0;
        }
        if (!entries_.empty()) {
            emit dataChanged(
                index(0), index(static_cast<int>(entries_.size()) - 1),
                {CpuPercentRole, MemoryBytesRole, MemoryPercentRole});
        }
        return;
    }

    // PIDs changed — full model reset
    beginResetModel();
    entries_.clear();
    entries_.reserve(samples.size());
    for (const auto& s : samples) {
        Entry e;
        e.pid = s.pid;
        e.name = s.name;
        e.cpu_percent = s.cpu_percent;
        e.memory_bytes = s.memory_rss_bytes;
        e.memory_percent =
            (static_cast<double>(s.memory_rss_bytes) / total_mem) * 100.0;
        entries_.push_back(std::move(e));
    }
    endResetModel();
    emit processCountChanged();
}

int ProcessListModel::sortColumn() const { return sort_column_; }

void ProcessListModel::setSortColumn(const int column) {
    if (sort_column_ != column) {
        sort_column_ = column;
        emit sortColumnChanged();
    }
}

bool ProcessListModel::sortDescending() const { return sort_descending_; }

void ProcessListModel::setSortDescending(const bool descending) {
    if (sort_descending_ != descending) {
        sort_descending_ = descending;
        emit sortDescendingChanged();
    }
}

int ProcessListModel::processCount() const {
    return static_cast<int>(entries_.size());
}

int ProcessListModel::pendingKillPid() const {
    return static_cast<int>(pending_kill_pid_);
}

QString ProcessListModel::pendingKillName() const {
    return pending_kill_name_;
}

QString ProcessListModel::lastError() const {
    return last_error_;
}

void ProcessListModel::requestKill(const int pid) {
    pending_kill_pid_ = static_cast<std::int32_t>(pid);
    pending_kill_name_.clear();
    for (const auto& e : entries_) {
        if (static_cast<int>(e.pid) == pid) {
            pending_kill_name_ = QString::fromStdString(e.name);
            break;
        }
    }
    emit pendingKillPidChanged();
    emit pendingKillNameChanged();
    emit killConfirmationRequested(pid, pending_kill_name_);
}

void ProcessListModel::confirmKill() {
    if (pending_kill_pid_ == 0 || bridge_ == nullptr) {
        return;
    }
    std::string error;
    const bool ok = bridge_->terminate_process(
        static_cast<std::uint32_t>(pending_kill_pid_), error);
    const QString msg = ok
        ? QString("Terminated %1 (PID %2)").arg(pending_kill_name_).arg(pending_kill_pid_)
        : QString::fromStdString(error);
    last_error_ = ok ? QString() : msg;
    emit lastErrorChanged();
    emit killCompleted(ok, msg);
    pending_kill_pid_ = 0;
    pending_kill_name_.clear();
    emit pendingKillPidChanged();
    emit pendingKillNameChanged();
}

void ProcessListModel::cancelKill() {
    pending_kill_pid_ = 0;
    pending_kill_name_.clear();
    emit pendingKillPidChanged();
    emit pendingKillNameChanged();
}

void ProcessListModel::setSortMode(const int column) {
    if (column == sort_column_) {
        setSortDescending(!sort_descending_);
    } else {
        setSortColumn(column);
        setSortDescending(true);
    }
}

}  // namespace aura::shell
