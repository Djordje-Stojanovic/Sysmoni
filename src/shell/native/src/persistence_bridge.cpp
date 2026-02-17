#include "aura_shell/persistence_bridge.hpp"

#include "platform_dll_helpers.hpp"

#include <filesystem>
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "aura_platform.h"
#endif

namespace aura::shell {

namespace {
constexpr int kStatusOk = 0;
}  // namespace

struct PersistenceBridge::Impl {
#ifdef _WIN32
    using StoreOpenFn = int (*)(const char*, double, aura_store_t**, aura_error_t*);
    using StoreAppendFn = int (*)(aura_store_t*, const aura_snapshot_t*, aura_error_t*);
    using StoreCloseFn = int (*)(aura_store_t*);

    HMODULE module_handle{nullptr};
    StoreOpenFn store_open_fn{nullptr};
    StoreAppendFn store_append_fn{nullptr};
    StoreCloseFn store_close_fn{nullptr};
    aura_store_t* open_store{nullptr};
#endif
    bool loaded{false};
    std::string loaded_path;
    std::string load_error;
};

PersistenceBridge::PersistenceBridge() : impl_(std::make_unique<Impl>()) {
#ifdef _WIN32
    using namespace detail;

    DWORD last_error_code = 0;
    for (const auto& candidate : runtime_library_candidates()) {
        const std::wstring path = candidate.wstring();
        HMODULE module = LoadLibraryW(path.c_str());
        if (module == nullptr) {
            last_error_code = GetLastError();
            continue;
        }

        auto* store_open = reinterpret_cast<Impl::StoreOpenFn>(GetProcAddress(module, "aura_store_open"));
        auto* store_append = reinterpret_cast<Impl::StoreAppendFn>(GetProcAddress(module, "aura_store_append"));
        auto* store_close = reinterpret_cast<Impl::StoreCloseFn>(GetProcAddress(module, "aura_store_close"));
        if (store_open == nullptr || store_append == nullptr || store_close == nullptr) {
            last_error_code = GetLastError();
            FreeLibrary(module);
            continue;
        }

        impl_->module_handle = module;
        impl_->store_open_fn = store_open;
        impl_->store_append_fn = store_append;
        impl_->store_close_fn = store_close;
        impl_->loaded = true;
        impl_->loaded_path = narrow_from_wide(path);
        impl_->load_error.clear();
        return;
    }

    impl_->load_error = "Unable to load aura_platform.dll";
    const std::string suffix = format_windows_error(last_error_code);
    if (!suffix.empty()) {
        impl_->load_error += ": " + suffix;
    }
#else
    impl_->load_error = "Persistence bridge is only supported on Windows.";
#endif
}

PersistenceBridge::~PersistenceBridge() {
    close_store();
#ifdef _WIN32
    if (impl_ != nullptr && impl_->module_handle != nullptr) {
        FreeLibrary(impl_->module_handle);
        impl_->module_handle = nullptr;
    }
#endif
}

bool PersistenceBridge::available() const {
    return impl_ != nullptr && impl_->loaded;
}

bool PersistenceBridge::open_store(const std::string& db_path, double retention_seconds, std::string& error) {
    error.clear();
    if (!available()) {
        error = impl_ != nullptr ? impl_->load_error : "Persistence bridge unavailable.";
        return false;
    }
    if (db_path.empty()) {
        error = "db_path cannot be empty.";
        return false;
    }

#ifdef _WIN32
    using namespace detail;

    std::error_code ec;
    const auto parent = std::filesystem::path(db_path).parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, ec);
    }

    aura_store_t* store = nullptr;
    aura_error_t open_error{};
    const int status = impl_->store_open_fn(db_path.c_str(), retention_seconds, &store, &open_error);
    if (status != kStatusOk || store == nullptr) {
        error = aura_error_message(open_error, "Failed to open persistence store.");
        return false;
    }
    impl_->open_store = store;
    return true;
#else
    error = "Persistence bridge is only supported on Windows.";
    return false;
#endif
}

bool PersistenceBridge::append_snapshot(
    const double ts,
    const double cpu,
    const double mem,
    const double disk_r,
    const double disk_w,
    std::string& error
) {
    error.clear();
    if (!available()) {
        error = "Persistence bridge unavailable.";
        return false;
    }

#ifdef _WIN32
    using namespace detail;

    if (impl_->open_store == nullptr) {
        error = "Store not open.";
        return false;
    }

    aura_snapshot_t snapshot{};
    snapshot.timestamp = ts;
    snapshot.cpu_percent = cpu;
    snapshot.memory_percent = mem;
    snapshot.disk_read_bps = disk_r;
    snapshot.disk_write_bps = disk_w;

    aura_error_t append_error{};
    const int status = impl_->store_append_fn(impl_->open_store, &snapshot, &append_error);
    if (status != kStatusOk) {
        error = aura_error_message(append_error, "Failed to append snapshot.");
        return false;
    }
    return true;
#else
    error = "Persistence bridge is only supported on Windows.";
    return false;
#endif
}

void PersistenceBridge::close_store() {
#ifdef _WIN32
    if (impl_ != nullptr && impl_->open_store != nullptr && impl_->store_close_fn != nullptr) {
        impl_->store_close_fn(impl_->open_store);
        impl_->open_store = nullptr;
    }
#endif
}

void* PersistenceBridge::store_handle() const {
#ifdef _WIN32
    return impl_ != nullptr ? static_cast<void*>(impl_->open_store) : nullptr;
#else
    return nullptr;
#endif
}

std::string PersistenceBridge::loaded_path() const {
    return impl_ != nullptr ? impl_->loaded_path : std::string{};
}

std::string PersistenceBridge::load_error() const {
    return impl_ != nullptr ? impl_->load_error : std::string{};
}

}  // namespace aura::shell
