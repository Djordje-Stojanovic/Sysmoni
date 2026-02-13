#pragma once

#include <deque>
#include <string>
#include <vector>

namespace aura::render_native {

struct RadialGaugeConfig {
    double sweep_degrees{270.0};
    double start_angle_degrees{225.0};
    int arc_width{10};
    bool show_label{true};
    std::string label_format{"{:.0f}%"};
    int min_size{120};
};

struct SparkLineConfig {
    int buffer_size{120};
    double line_width{1.5};
    int gradient_alpha_top{80};
    int gradient_alpha_bottom{0};
    bool show_latest_value{true};
    std::string label_format{"{:.1f}%"};
    int min_height{60};
};

struct TimelineConfig {
    double line_width{1.5};
    int gradient_alpha_top{60};
    int gradient_alpha_bottom{0};
    double scrubber_width{2.0};
    double scrubber_handle_radius{5.0};
    int axis_height{20};
    int min_height{80};
    int min_width{200};
    bool show_memory{false};
};

struct GlassPanelConfig {
    double frost_intensity{0.08};
    double frost_scale{3.5};
    double base_alpha{0.72};
    double accent_tint_strength{0.18};
    bool animate_frost{true};
    int min_width{100};
    int min_height{60};
};

struct TimelinePoint {
    double timestamp{0.0};
    double cpu_percent{0.0};
    double memory_percent{0.0};
};

class RadialGaugeModel {
  public:
    explicit RadialGaugeModel(RadialGaugeConfig config = {});
    [[nodiscard]] double value() const;
    [[nodiscard]] double accent_intensity() const;
    void set_value(double value);
    void set_accent_intensity(double accent_intensity);

  private:
    RadialGaugeConfig config_;
    double value_{0.0};
    double accent_intensity_{0.0};
};

class SparkLineModel {
  public:
    explicit SparkLineModel(SparkLineConfig config = {});
    [[nodiscard]] int buffer_len() const;
    [[nodiscard]] double latest() const;
    [[nodiscard]] bool has_data() const;
    void push(double value);
    void push_many(const std::vector<double>& values);

  private:
    SparkLineConfig config_;
    std::deque<double> buffer_;
};

class TimelineModel {
  public:
    explicit TimelineModel(TimelineConfig config = {});
    [[nodiscard]] int snapshot_count() const;
    [[nodiscard]] double scrub_ratio() const;
    [[nodiscard]] double scrub_timestamp() const;
    [[nodiscard]] bool has_scrub_timestamp() const;
    void set_data(const std::vector<TimelinePoint>& snapshots);
    void set_scrub_ratio(double ratio);

  private:
    TimelineConfig config_;
    std::vector<TimelinePoint> snapshots_;
    double scrub_ratio_{1.0};
};

}  // namespace aura::render_native
