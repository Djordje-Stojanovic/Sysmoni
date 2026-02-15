#include <iostream>
#include <vector>

// --- System snapshot tests ---
int test_system_snapshot_success();
int test_system_error_message_clears_on_success_path();

// --- Process tests ---
int test_process_sort_and_limit();
int test_process_error_message_clears_on_success_path();
int test_process_tie_break_is_deterministic();
int test_process_empty_collection_returns_empty();
int test_process_empty_name_falls_back_to_pid();
int test_native_process_collector_returns_ranked_top_k();

// --- Disk tests ---
int test_disk_rate_computation();
int test_disk_non_increasing_timestamp_keeps_baseline();
int test_disk_unavailable_degrades_gracefully();
int test_disk_error_still_fails();
int test_disk_error_message_clears_on_graceful_paths();

// --- Network tests ---
int test_network_rate_computation();
int test_network_non_increasing_timestamp_keeps_baseline();
int test_network_unavailable_degrades_gracefully();
int test_network_error_still_fails();
int test_network_error_message_clears_on_graceful_paths();

// --- Thermal tests ---
int test_thermal_degrades_gracefully_when_unavailable();
int test_thermal_success();
int test_thermal_error_message_clears_on_graceful_paths();

// --- Per-core CPU tests ---
int test_per_core_cpu_success();
int test_per_core_cpu_unavailable_degrades_gracefully();
int test_per_core_cpu_null_collector_degrades_gracefully();

// --- GPU tests ---
int test_gpu_stub_returns_unavailable_gracefully();
int test_gpu_success_when_available();
int test_gpu_null_collector_degrades_gracefully();

// --- Process management tests ---
int test_get_process_by_pid_success();
int test_collect_process_details_with_filter();
int test_build_process_tree();
int test_terminate_process_success();
int test_terminate_process_with_exit_code();
int test_terminate_process_invalid_pid_fails();
int test_terminate_process_error_status_fails();
int test_terminate_process_null_collector_degrades_gracefully();
int test_set_process_priority_success();
int test_set_process_priority_invalid_pid_fails();
int test_set_process_priority_all_priority_levels();
int test_set_process_priority_error_status_fails();
int test_set_process_priority_null_collector_degrades_gracefully();
int test_get_process_children_success();
int test_get_process_children_invalid_pid_succeeds();
int test_get_process_children_null_collector_degrades_gracefully();

int main() {
    using TestCase = int (*)();
    const std::vector<TestCase> tests = {
        test_system_snapshot_success,
        test_system_error_message_clears_on_success_path,
        test_process_sort_and_limit,
        test_process_error_message_clears_on_success_path,
        test_process_tie_break_is_deterministic,
        test_process_empty_collection_returns_empty,
        test_process_empty_name_falls_back_to_pid,
        test_native_process_collector_returns_ranked_top_k,
        test_disk_rate_computation,
        test_disk_non_increasing_timestamp_keeps_baseline,
        test_disk_unavailable_degrades_gracefully,
        test_disk_error_still_fails,
        test_disk_error_message_clears_on_graceful_paths,
        test_network_rate_computation,
        test_network_non_increasing_timestamp_keeps_baseline,
        test_network_unavailable_degrades_gracefully,
        test_network_error_still_fails,
        test_network_error_message_clears_on_graceful_paths,
        test_thermal_degrades_gracefully_when_unavailable,
        test_thermal_success,
        test_thermal_error_message_clears_on_graceful_paths,
        test_per_core_cpu_success,
        test_per_core_cpu_unavailable_degrades_gracefully,
        test_per_core_cpu_null_collector_degrades_gracefully,
        test_gpu_stub_returns_unavailable_gracefully,
        test_gpu_success_when_available,
        test_gpu_null_collector_degrades_gracefully,
        test_get_process_by_pid_success,
        test_collect_process_details_with_filter,
        test_build_process_tree,
        test_terminate_process_success,
        test_terminate_process_with_exit_code,
        test_terminate_process_invalid_pid_fails,
        test_terminate_process_error_status_fails,
        test_terminate_process_null_collector_degrades_gracefully,
        test_set_process_priority_success,
        test_set_process_priority_invalid_pid_fails,
        test_set_process_priority_all_priority_levels,
        test_set_process_priority_error_status_fails,
        test_set_process_priority_null_collector_degrades_gracefully,
        test_get_process_children_success,
        test_get_process_children_invalid_pid_succeeds,
        test_get_process_children_null_collector_degrades_gracefully,
    };

    int failures = 0;
    for (const TestCase test : tests) {
        failures += test();
    }

    if (failures == 0) {
        std::cout << "PASS: telemetry native tests\n";
        return 0;
    }

    std::cerr << "FAIL: telemetry native tests (" << failures << " failing cases)\n";
    return 1;
}
