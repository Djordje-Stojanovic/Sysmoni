#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>
#include <cmath>
#include <unordered_set>

#ifdef _WIN32
// Forward declare Windows types
typedef struct _FILETIME FILETIME;
typedef void* HANDLE;
#endif

constexpr size_t kProcessNameBytes = 260;
constexpr size_t kThermalLabelBytes = 128;

void write_error(char* error_buffer, size_t error_buffer_len, const char* message);

#ifdef _WIN32
uint64_t filetime_to_uint64(const FILETIME& ft);
uint64_t now_100ns();
double clamp_percent(double value);
std::string utf8_from_utf16(const wchar_t* input);
void write_utf8_name(char* destination, size_t destination_size, const std::string& value);
int logical_cpu_count();
bool compute_process_cpu_percent(uint32_t pid, HANDLE process_handle, uint64_t sampled_at_100ns, int cpu_count, double* out_percent);
void prune_process_cpu_state(const std::unordered_set<uint32_t>& seen_pids);
#endif
