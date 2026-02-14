#pragma once

#include <string>

namespace aura::render_native {

struct QtRenderCallbacks {
    void (*begin_frame)(void* user_data){nullptr};
    void (*set_accent_rgba)(
        void* user_data,
        double red,
        double green,
        double blue,
        double alpha
    ){nullptr};
    void (*set_panel_frost)(void* user_data, double frost_intensity, double tint_strength){nullptr};
    void (*set_ring_style)(void* user_data, double line_width, double glow_strength){nullptr};
    void (*set_timeline_emphasis)(void* user_data, double cpu_alpha, double memory_alpha){nullptr};
    void (*commit_frame)(void* user_data){nullptr};
};

struct QtRenderFrameInput {
    double cpu_percent{0.0};
    double memory_percent{0.0};
    double elapsed_since_last_frame{0.0};
    double pulse_hz{0.5};
    int target_fps{60};
    int max_catchup_frames{4};
};

struct QtRenderStyleTokens {
    double phase{0.0};
    double next_delay_seconds{0.0};
    double accent_intensity{0.0};
    double accent_red{0.0};
    double accent_green{0.0};
    double accent_blue{0.0};
    double accent_alpha{0.0};
    double frost_intensity{0.0};
    double tint_strength{0.0};
    double ring_line_width{0.0};
    double ring_glow_strength{0.0};
    double cpu_alpha{0.0};
    double memory_alpha{0.0};
};

struct QtRenderBackendCaps {
    bool available{false};
    bool supports_callbacks{true};
    int preferred_fps{60};
};

bool qt_callbacks_complete(const QtRenderCallbacks& callbacks);
QtRenderBackendCaps qt_backend_caps();
QtRenderStyleTokens
compute_qt_style_tokens(double previous_phase, const QtRenderFrameInput& input);

class QtRenderHooks {
  public:
    QtRenderHooks(QtRenderCallbacks callbacks, void* user_data);

    bool render_frame(const QtRenderFrameInput& input);
    [[nodiscard]] const std::string& last_error() const;

  private:
    QtRenderCallbacks callbacks_{};
    void* user_data_{nullptr};
    double phase_{0.0};
    std::string last_error_{};

    void set_error(std::string message);
};

}  // namespace aura::render_native
