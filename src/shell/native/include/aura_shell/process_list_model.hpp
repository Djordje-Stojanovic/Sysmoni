#pragma once

#include <QAbstractListModel>
#include <QString>
#include <QHash>
#include <QByteArray>

#include <cstdint>
#include <string>
#include <vector>

#include "aura_shell/cockpit_types.hpp"

namespace aura::shell {

class ITelemetryBridge;

class ProcessListModel final : public QAbstractListModel {
    Q_OBJECT

    Q_PROPERTY(int sortColumn READ sortColumn WRITE setSortColumn NOTIFY sortColumnChanged)
    Q_PROPERTY(bool sortDescending READ sortDescending WRITE setSortDescending NOTIFY sortDescendingChanged)
    Q_PROPERTY(int processCount READ processCount NOTIFY processCountChanged)
    Q_PROPERTY(int pendingKillPid READ pendingKillPid NOTIFY pendingKillPidChanged)
    Q_PROPERTY(QString pendingKillName READ pendingKillName NOTIFY pendingKillNameChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)

public:
    enum Roles {
        PidRole = Qt::UserRole + 1,
        NameRole,
        CpuPercentRole,
        MemoryBytesRole,
        MemoryPercentRole,
        InstanceCountRole,
    };

    explicit ProcessListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setBridge(ITelemetryBridge* bridge);
    void refresh(ITelemetryBridge* bridge, std::uint64_t total_memory_bytes);

    int sortColumn() const;
    void setSortColumn(int column);
    bool sortDescending() const;
    void setSortDescending(bool descending);
    int processCount() const;
    int pendingKillPid() const;
    QString pendingKillName() const;
    QString lastError() const;

    Q_INVOKABLE void requestKill(int pid);
    Q_INVOKABLE void confirmKill();
    Q_INVOKABLE void cancelKill();
    Q_INVOKABLE void setSortMode(int column);

signals:
    void sortColumnChanged();
    void sortDescendingChanged();
    void processCountChanged();
    void pendingKillPidChanged();
    void pendingKillNameChanged();
    void lastErrorChanged();
    void killConfirmationRequested(int pid, const QString& name);
    void killCompleted(bool success, const QString& message);

private:
    struct Entry {
        std::uint32_t pid{0};
        std::string name;
        double cpu_percent{0.0};
        std::uint64_t memory_bytes{0};
        double memory_percent{0.0};
        int instance_count{1};
    };

    std::vector<Entry> entries_;
    ITelemetryBridge* bridge_{nullptr};
    int sort_column_{0};
    bool sort_descending_{true};
    bool force_reset_{false};
    std::int32_t pending_kill_pid_{0};
    QString pending_kill_name_;
    QString last_error_;
};

}  // namespace aura::shell
