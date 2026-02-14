#pragma once

#include <deque>
#include <string>
#include <vector>

#include "render_native/theme.hpp"

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

    // Existing API — unchanged.
    [[nodiscard]] double value() const;
    [[nodiscard]] double accent_intensity() const;
    void set_value(double value);
    void set_accent_intensity(double accent_intensity);

    // Computed properties for rendering.

    // Returns the sweep angle in degrees for the current value.
    // For the default sweep_degrees of 270 and value of 50 this returns 135.0.
    [[nodiscard]] double value_sweep_degrees() const;

    // Returns the gauge color for the current value using interpolate_gauge_color.
    [[nodiscard]] RgbColor value_color() const;

    // Returns the arc start angle in radians for Canvas arc drawing.
    // Follows the convention that 0 rad = 3-o'clock; angles increase clockwise.
    [[nodiscard]] double arc_start_radians() const;

    // Returns the arc end angle in radians that corresponds to the current value.
    [[nodiscard]] double arc_end_radians() const;

  private:
    RadialGaugeConfig config_;
    double value_{0.0};
    double accent_intensity_{0.0};
};

class SparkLineModel {
  public:
    explicit SparkLineModel(SparkLineConfig config = {});

    // Existing API — unchanged.
    [[nodiscard]] int buffer_len() const;
    [[nodiscard]] double latest() const;
    [[nodiscard]] bool has_data() const;
    void push(double value);
    void push_many(const std::vector<double>& values);

    // Computed properties for rendering.

    // Returns normalized values (0.0 to 1.0) for the entire buffer.
    // Empty buffer returns an empty vector.
    [[nodiscard]] std::vector<double> normalized_buffer() const;

    // Returns the minimum value present in the current buffer.
    // Returns 0.0 for an empty buffer.
    [[nodiscard]] double buffer_min() const;

    // Returns the maximum value present in the current buffer.
    // Returns 0.0 for an empty buffer.
    [[nodiscard]] double buffer_max() const;

    // Returns fill opacity in [0.1, 0.9] scaled to the latest value (0-100).
    // Returns 0.1 for an empty buffer.
    [[nodiscard]] double fill_opacity() const;

  private:
    SparkLineConfig config_;
    std::deque<double> buffer_;
};

class TimelineModel {
  public:
    explicit TimelineModel(TimelineConfig config = {});

    // Existing API — unchanged.
    [[nodiscard]] int snapshot_count() const;
    [[nodiscard]] double scrub_ratio() const;
    [[nodiscard]] double scrub_timestamp() const;
    [[nodiscard]] bool has_scrub_timestamp() const;
    void set_data(const std::vector<TimelinePoint>& snapshots);
    void set_scrub_ratio(double ratio);

    // Computed properties for rendering.

    // Returns normalized CPU values (0.0 to 1.0) for all snapshots.
    // Empty snapshot list returns an empty vector.
    [[nodiscard]] std::vector<double> normalized_cpu() const;

    // Returns normalized memory values (0.0 to 1.0) for all snapshots.
    // Empty snapshot list returns an empty vector.
    [[nodiscard]] std::vector<double> normalized_memory() const;

    // Returns the timestamp of the earliest (first) snapshot.
    // Returns 0.0 if there are no snapshots.
    [[nodiscard]] double earliest_timestamp() const;

    // Returns the timestamp of the latest (last) snapshot.
    // Returns 0.0 if there are no snapshots.
    [[nodiscard]] double latest_timestamp() const;

  private:
    TimelineConfig config_;
    std::vector<TimelinePoint> snapshots_;
    double scrub_ratio_{1.0};
};

}  // namespace aura::render_native
