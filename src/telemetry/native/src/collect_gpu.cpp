#include "telemetry_abi.h"
#include "telemetry_utils.h"

#include <mutex>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <dxgi1_4.h>
#include <wrl/client.h>

namespace {

struct GpuState {
    bool initialized = false;
    bool init_failed = false;
    Microsoft::WRL::ComPtr<IDXGIAdapter3> adapter;
    LUID adapter_luid{};
};

static GpuState g_gpu_state;
static std::mutex g_gpu_mutex;

// Dynamically loaded DXGI function pointer
using CreateDXGIFactory1Fn = HRESULT(WINAPI*)(REFIID, void**);
static CreateDXGIFactory1Fn g_CreateDXGIFactory1 = nullptr;
static bool g_dxgi_resolved = false;

bool resolve_dxgi() {
    if (g_dxgi_resolved) {
        return g_CreateDXGIFactory1 != nullptr;
    }
    g_dxgi_resolved = true;
    HMODULE dxgi = LoadLibraryW(L"dxgi.dll");
    if (dxgi == nullptr) {
        return false;
    }
    g_CreateDXGIFactory1 = reinterpret_cast<CreateDXGIFactory1Fn>(
        GetProcAddress(dxgi, "CreateDXGIFactory1")
    );
    return g_CreateDXGIFactory1 != nullptr;
}

bool initialize_gpu() {
    if (g_gpu_state.initialized) {
        return !g_gpu_state.init_failed;
    }
    if (g_gpu_state.init_failed) {
        return false;
    }

    g_gpu_state.initialized = true;

    if (!resolve_dxgi()) {
        g_gpu_state.init_failed = true;
        return false;
    }

    Microsoft::WRL::ComPtr<IDXGIFactory4> factory;
    HRESULT hr = g_CreateDXGIFactory1(IID_PPV_ARGS(&factory));
    if (FAILED(hr)) {
        g_gpu_state.init_failed = true;
        return false;
    }

    Microsoft::WRL::ComPtr<IDXGIAdapter1> best_adapter;
    SIZE_T best_vram = 0;

    for (UINT i = 0; ; ++i) {
        Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
        hr = factory->EnumAdapters1(i, &adapter);
        if (hr == DXGI_ERROR_NOT_FOUND) {
            break;
        }
        if (FAILED(hr)) {
            continue;
        }

        DXGI_ADAPTER_DESC1 desc{};
        if (FAILED(adapter->GetDesc1(&desc))) {
            continue;
        }
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
            continue;
        }
        if (best_adapter == nullptr || desc.DedicatedVideoMemory > best_vram) {
            best_vram = desc.DedicatedVideoMemory;
            best_adapter = adapter;
        }
    }

    if (!best_adapter) {
        g_gpu_state.init_failed = true;
        return false;
    }

    hr = best_adapter.As(&g_gpu_state.adapter);
    if (FAILED(hr) || !g_gpu_state.adapter) {
        g_gpu_state.init_failed = true;
        return false;
    }

    DXGI_ADAPTER_DESC1 desc{};
    if (SUCCEEDED(best_adapter->GetDesc1(&desc))) {
        g_gpu_state.adapter_luid = desc.AdapterLuid;
    }

    return true;
}

bool collect_vram(uint64_t* used, uint64_t* total) {
    if (!g_gpu_state.adapter) {
        return false;
    }

    DXGI_QUERY_VIDEO_MEMORY_INFO info{};
    HRESULT hr = g_gpu_state.adapter->QueryVideoMemoryInfo(
        0,
        DXGI_MEMORY_SEGMENT_GROUP_LOCAL,
        &info
    );
    if (FAILED(hr)) {
        return false;
    }

    if (used != nullptr) {
        *used = static_cast<uint64_t>(info.CurrentUsage);
    }
    if (total != nullptr) {
        *total = static_cast<uint64_t>(info.Budget);
    }
    return true;
}

// GPU utilization % is not currently collected.
// D3DKMTQueryStatistics requires elevated privileges on some GPU driver
// configurations and crashes without them. VRAM data from DXGI is the
// primary GPU metric. GPU utilization can be added later via NVML/ADL
// vendor SDKs or when D3DKMT header issues are resolved.
//
// For now, gpu_percent stays at 0% and VRAM provides the key data.

}  // namespace

#endif  // _WIN32

extern "C" int aura_collect_gpu_utilization(
    aura_gpu_utilization* out_gpu,
    char* error_buffer,
    size_t error_buffer_len
) {
#ifndef _WIN32
    (void)out_gpu;
    write_error(error_buffer, error_buffer_len, "GPU telemetry is unavailable on this platform.");
    return AURA_STATUS_UNAVAILABLE;
#else
    if (out_gpu == nullptr) {
        write_error(error_buffer, error_buffer_len, "GPU output pointer must not be null.");
        return AURA_STATUS_ERROR;
    }

    std::lock_guard<std::mutex> lock(g_gpu_mutex);

    if (!initialize_gpu()) {
        write_error(error_buffer, error_buffer_len, "No compatible GPU adapter found.");
        return AURA_STATUS_UNAVAILABLE;
    }

    bool any_success = false;

    uint64_t vram_used = 0;
    uint64_t vram_total = 0;
    if (collect_vram(&vram_used, &vram_total)) {
        out_gpu->vram_used_bytes = vram_used;
        out_gpu->vram_total_bytes = vram_total;
        out_gpu->vram_percent = (vram_total > 0)
            ? clamp_percent((static_cast<double>(vram_used) * 100.0) / static_cast<double>(vram_total))
            : 0.0;
        any_success = true;
    } else {
        out_gpu->vram_used_bytes = 0;
        out_gpu->vram_total_bytes = 0;
        out_gpu->vram_percent = 0.0;
    }

    // GPU utilization % not yet available (see comment above collect_vram).
    // VRAM data is the primary metric; utilization can be added later.
    out_gpu->gpu_percent = 0.0;

    if (!any_success) {
        write_error(error_buffer, error_buffer_len, "Failed to collect any GPU metrics.");
        return AURA_STATUS_UNAVAILABLE;
    }

    write_error(error_buffer, error_buffer_len, "");
    return AURA_STATUS_OK;
#endif
}
