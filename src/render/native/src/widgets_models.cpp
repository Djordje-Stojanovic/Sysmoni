#include "render_native/widgets_models.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

#include "render_native/math.hpp"
#include "render_native/theme.hpp"

namespace aura::render_native {

namespace {

// Clamp ratio to [0, 1].  Used by TimelineModel::set_scrub_ratio.
double clamp_ratio(double value) {
    if (value < 0.0) {
        return 0.0;
    }
    if (value > 1.0) {
        return 1.0;
    }
    return value;
}

// Degrees to radians conversion factor.
static constexpr double kDegToRad = 3.14159265358979323846 / 180.0;

}  // namespace

// ===========================================================================
// RadialGaugeModel
// ===========================================================================

RadialGaugeModel::RadialGaugeModel(RadialGaugeConfig config) : config_(std::move(config)) {
    if (config_.sweep_degrees <= 0.0 || config_.sweep_degrees > 360.0) {
        throw std::invalid_argument("sweep_degrees must be in (0, 360].");
    }
    if (config_.arc_width < 1) {
        throw std::invalid_argument("arc_width must be >= 1.");
    }
    if (config_.min_size < 1) {
        throw std::invalid_argument("min_size must be >= 1.");
    }
}

double RadialGaugeModel::value() const {
    return value_;
}

double RadialGaugeModel::accent_intensity() const {
    return accent_intensity_;
}

void RadialGaugeModel::set_value(double value) {
    value_ = sanitize_percent(value);
}

void RadialGaugeModel::set_accent_intensity(double accent_intensity) {
    accent_intensity_ = clamp_unit(accent_intensity);
}

// ---------------------------------------------------------------------------
// RadialGaugeModel — computed properties
// ---------------------------------------------------------------------------

double RadialGaugeModel::value_sweep_degrees() const {
    // value_ is already clamped to [0, 100] by sanitize_percent.
    // Map [0, 100] linearly onto [0, sweep_degrees].
    return (value_ / 100.0) * config_.sweep_degrees;
}

RgbColor RadialGaugeModel::value_color() const {
    return interpolate_gauge_color(value_);
}

double RadialGaugeModel::arc_start_radians() const {
    // start_angle_degrees follows the Qt / Canvas convention:
    //   0°   = 3-o'clock,  angles increase clockwise.
    // We convert directly: positive = clockwise from 3-o'clock.
    return config_.start_angle_degrees * kDegToRad;
}

double RadialGaugeModel::arc_end_radians() const {
    return arc_start_radians() + (value_sweep_degrees() * kDegToRad);
}

// ===========================================================================
// SparkLineModel
// ===========================================================================

SparkLineModel::SparkLineModel(SparkLineConfig config) : config_(std::move(config)) {
    if (config_.buffer_size < 2) {
        throw std::invalid_argument("buffer_size must be >= 2.");
    }
    if (config_.line_width <= 0.0) {
        throw std::invalid_argument("line_width must be > 0.");
    }
    if (config_.min_height < 1) {
        throw std::invalid_argument("min_height must be >= 1.");
    }
}

int SparkLineModel::buffer_len() const {
    return static_cast<int>(buffer_.size());
}

double SparkLineModel::latest() const {
    if (buffer_.empty()) {
        return 0.0;
    }
    return buffer_.back();
}

bool SparkLineModel::has_data() const {
    return !buffer_.empty();
}

void SparkLineModel::push(double value) {
    if (static_cast<int>(buffer_.size()) == config_.buffer_size) {
        buffer_.pop_front();
    }
    buffer_.push_back(sanitize_percent(value));
}

void SparkLineModel::push_many(const std::vector<double>& values) {
    for (const double value : values) {
        push(value);
    }
}

// ---------------------------------------------------------------------------
// SparkLineModel — computed properties
// ---------------------------------------------------------------------------

double SparkLineModel::buffer_min() const {
    if (buffer_.empty()) {
        return 0.0;
    }
    return *std::min_element(buffer_.begin(), buffer_.end());
}

double SparkLineModel::buffer_max() const {
    if (buffer_.empty()) {
        return 0.0;
    }
    return *std::max_element(buffer_.begin(), buffer_.end());
}

std::vector<double> SparkLineModel::normalized_buffer() const {
    if (buffer_.empty()) {
        return {};
    }

    const double lo = buffer_min();
    const double hi = buffer_max();
    const double range = hi - lo;

    std::vector<double> out;
    out.reserve(buffer_.size());

    if (range < 1e-9) {
        // All values are effectively equal — map them all to 0.5 so the
        // spark line still renders at mid-height rather than collapsing.
        for (std::size_t i = 0; i < buffer_.size(); ++i) {
            out.push_back(0.5);
        }
    } else {
        for (const double v : buffer_) {
            out.push_back(clamp_unit((v - lo) / range));
        }
    }
    return out;
}

double SparkLineModel::fill_opacity() const {
    // Map latest value [0, 100] linearly to opacity [0.1, 0.9].
    // Empty buffer falls back to the minimum opacity.
    static constexpr double kOpacityMin = 0.1;
    static constexpr double kOpacityMax = 0.9;
    if (buffer_.empty()) {
        return kOpacityMin;
    }
    const double normalized = clamp_unit(latest() / 100.0);
    return kOpacityMin + (normalized * (kOpacityMax - kOpacityMin));
}

// ===========================================================================
// TimelineModel
// ===========================================================================

TimelineModel::TimelineModel(TimelineConfig config) : config_(std::move(config)) {
    if (config_.line_width <= 0.0) {
        throw std::invalid_argument("line_width must be > 0.");
    }
    if (config_.gradient_alpha_top < 0 || config_.gradient_alpha_top > 255) {
        throw std::invalid_argument("gradient_alpha_top must be 0-255.");
    }
    if (config_.gradient_alpha_bottom < 0 || config_.gradient_alpha_bottom > 255) {
        throw std::invalid_argument("gradient_alpha_bottom must be 0-255.");
    }
    if (config_.scrubber_width <= 0.0) {
        throw std::invalid_argument("scrubber_width must be > 0.");
    }
    if (config_.scrubber_handle_radius <= 0.0) {
        throw std::invalid_argument("scrubber_handle_radius must be > 0.");
    }
    if (config_.axis_height < 0) {
        throw std::invalid_argument("axis_height must be >= 0.");
    }
    if (config_.min_height < 1) {
        throw std::invalid_argument("min_height must be >= 1.");
    }
    if (config_.min_width < 1) {
        throw std::invalid_argument("min_width must be >= 1.");
    }
}

int TimelineModel::snapshot_count() const {
    return static_cast<int>(snapshots_.size());
}

double TimelineModel::scrub_ratio() const {
    return scrub_ratio_;
}

double TimelineModel::scrub_timestamp() const {
    if (!has_scrub_timestamp()) {
        return 0.0;
    }
    const double t0 = snapshots_.front().timestamp;
    const double t1 = snapshots_.back().timestamp;
    return t0 + ((t1 - t0) * scrub_ratio_);
}

bool TimelineModel::has_scrub_timestamp() const {
    return snapshots_.size() >= 2;
}

void TimelineModel::set_data(const std::vector<TimelinePoint>& snapshots) {
    snapshots_ = snapshots;
}

void TimelineModel::set_scrub_ratio(double ratio) {
    scrub_ratio_ = clamp_ratio(ratio);
}

// ---------------------------------------------------------------------------
// TimelineModel — computed properties
// ---------------------------------------------------------------------------

std::vector<double> TimelineModel::normalized_cpu() const {
    if (snapshots_.empty()) {
        return {};
    }
    std::vector<double> out;
    out.reserve(snapshots_.size());
    for (const auto& pt : snapshots_) {
        // cpu_percent is stored as 0-100; normalize to [0, 1].
        out.push_back(clamp_unit(pt.cpu_percent / 100.0));
    }
    return out;
}

std::vector<double> TimelineModel::normalized_memory() const {
    if (snapshots_.empty()) {
        return {};
    }
    std::vector<double> out;
    out.reserve(snapshots_.size());
    for (const auto& pt : snapshots_) {
        out.push_back(clamp_unit(pt.memory_percent / 100.0));
    }
    return out;
}

double TimelineModel::earliest_timestamp() const {
    if (snapshots_.empty()) {
        return 0.0;
    }
    return snapshots_.front().timestamp;
}

double TimelineModel::latest_timestamp() const {
    if (snapshots_.empty()) {
        return 0.0;
    }
    return snapshots_.back().timestamp;
}

}  // namespace aura::render_native
