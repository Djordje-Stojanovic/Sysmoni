#include "render_native/widgets_models.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

#include "render_native/math.hpp"

namespace aura::render_native {

namespace {

double clamp_ratio(double value) {
    if (value < 0.0) {
        return 0.0;
    }
    if (value > 1.0) {
        return 1.0;
    }
    return value;
}

}  // namespace

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

}  // namespace aura::render_native
