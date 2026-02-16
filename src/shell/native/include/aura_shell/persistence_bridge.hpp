#pragma once

#include <memory>
#include <string>

namespace aura::shell {

class IPersistenceBridge {
public:
    virtual ~IPersistenceBridge() = default;
    virtual bool available() const = 0;
    virtual bool open_store(const std::string& db_path, double retention_seconds, std::string& error) = 0;
    virtual bool append_snapshot(double ts, double cpu, double mem, double disk_r, double disk_w, std::string& error) = 0;
    virtual void close_store() = 0;
};

class PersistenceBridge final : public IPersistenceBridge {
public:
    PersistenceBridge();
    ~PersistenceBridge() override;

    PersistenceBridge(const PersistenceBridge&) = delete;
    PersistenceBridge& operator=(const PersistenceBridge&) = delete;

    bool available() const override;
    bool open_store(const std::string& db_path, double retention_seconds, std::string& error) override;
    bool append_snapshot(double ts, double cpu, double mem, double disk_r, double disk_w, std::string& error) override;
    void close_store() override;

    std::string loaded_path() const;
    std::string load_error() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace aura::shell
