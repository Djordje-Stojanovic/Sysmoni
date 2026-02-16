#include "aura_shell/process_list_model.hpp"
#include "aura_shell/telemetry_bridge.hpp"

#include <algorithm>
#include <unordered_map>
#include <unordered_set>

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
        case InstanceCountRole:
            return e.instance_count;
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
        {InstanceCountRole, "instanceCount"},
    };
}

void ProcessListModel::setBridge(ITelemetryBridge* bridge) {
    bridge_ = bridge;
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

    auto samples = bridge->collect_process_details(48, abi_col, sort_descending_, error);
    if (!error.empty() && samples.empty()) {
        return;
    }

    const double total_mem = total_memory_bytes > 0
        ? static_cast<double>(total_memory_bytes)
        : 1.0;

    // ── Aggregate by process name ──────────────────────────────────────────
    struct Group {
        std::uint32_t representative_pid{0};
        std::string name;
        double total_cpu{0.0};
        std::uint64_t total_memory{0};
        double top_cpu{-1.0};
        int count{0};
    };

    std::unordered_map<std::string, Group> groups;
    for (const auto& s : samples) {
        auto& g = groups[s.name];
        g.name = s.name;
        g.total_cpu += s.cpu_percent;
        g.total_memory += s.memory_rss_bytes;
        g.count++;
        if (s.cpu_percent > g.top_cpu) {
            g.top_cpu = s.cpu_percent;
            g.representative_pid = s.pid;
        }
    }

    // Build sorted list of grouped entries
    std::vector<Entry> new_entries;
    new_entries.reserve(groups.size());
    for (auto& [key, g] : groups) {
        Entry e;
        e.pid = g.representative_pid;
        e.name = std::move(g.name);
        e.cpu_percent = g.total_cpu;
        e.memory_bytes = g.total_memory;
        e.memory_percent = (static_cast<double>(g.total_memory) / total_mem) * 100.0;
        e.instance_count = g.count;
        new_entries.push_back(std::move(e));
    }

    // Sort
    auto sort_entries = [&](std::vector<Entry>& v) {
        switch (abi_col) {
            case 1: // name
                std::sort(v.begin(), v.end(),
                    [](const Entry& a, const Entry& b) { return a.name < b.name; });
                break;
            case 3: // memory
                std::sort(v.begin(), v.end(),
                    [](const Entry& a, const Entry& b) { return a.memory_bytes > b.memory_bytes; });
                break;
            default: // cpu
                std::sort(v.begin(), v.end(),
                    [](const Entry& a, const Entry& b) { return a.cpu_percent > b.cpu_percent; });
                break;
        }
        if (!sort_descending_) {
            std::reverse(v.begin(), v.end());
        }
    };

    sort_entries(new_entries);

    // ── Stable update: only reset when the set of names changes ────────────
    bool same_names = !force_reset_ && new_entries.size() == entries_.size();
    if (same_names) {
        std::unordered_set<std::string> current_names;
        current_names.reserve(entries_.size());
        for (const auto& e : entries_) {
            current_names.insert(e.name);
        }
        for (const auto& e : new_entries) {
            if (current_names.find(e.name) == current_names.end()) {
                same_names = false;
                break;
            }
        }
    }

    if (same_names && !entries_.empty()) {
        // Same process groups — update values in place (preserves scroll & hover)
        std::unordered_map<std::string, const Entry*> new_map;
        new_map.reserve(new_entries.size());
        for (const auto& e : new_entries) {
            new_map[e.name] = &e;
        }

        for (std::size_t i = 0; i < entries_.size(); ++i) {
            auto it = new_map.find(entries_[i].name);
            if (it != new_map.end()) {
                entries_[i].pid = it->second->pid;
                entries_[i].cpu_percent = it->second->cpu_percent;
                entries_[i].memory_bytes = it->second->memory_bytes;
                entries_[i].memory_percent = it->second->memory_percent;
                entries_[i].instance_count = it->second->instance_count;
            }
        }
        if (!entries_.empty()) {
            emit dataChanged(
                index(0), index(static_cast<int>(entries_.size()) - 1),
                {PidRole, CpuPercentRole, MemoryBytesRole, MemoryPercentRole, InstanceCountRole});
        }
        emit processCountChanged();
    } else {
        // Process set changed or sort mode changed — full reset
        force_reset_ = false;
        beginResetModel();
        entries_ = std::move(new_entries);
        endResetModel();
        emit processCountChanged();
    }
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
    int total = 0;
    for (const auto& e : entries_) {
        total += e.instance_count;
    }
    return total;
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
    force_reset_ = true;
}

}  // namespace aura::shell
