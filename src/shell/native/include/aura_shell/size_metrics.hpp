#pragma once

#include <cstddef>

namespace aura::shell {

enum class SizeCategory { Compact, Regular, Comfortable, Spacious };

struct SizeMetrics {
    int base_font;
    int title_font;
    int subtitle_font;
    int metric_value_font;
    int metric_key_font;
    int metric_unit_font;
    int tab_font;
    int small_font;
    int titlebar_height;
    int tab_v_padding;
    int tab_h_padding;
    int body_margin;
    int body_spacing;
};

SizeCategory classify_window_size(int w, int h);
SizeMetrics metrics_for_category(SizeCategory cat);

}  // namespace aura::shell
