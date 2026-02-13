#pragma once

#include <string>

namespace aura::render_native {

struct RgbColor {
    int red{0};
    int green{0};
    int blue{0};
};

RgbColor parse_hex_color(const std::string& value);
std::string blend_hex_color(const std::string& start, const std::string& end, double ratio);
int quantize_accent_intensity(double accent_intensity);

}  // namespace aura::render_native
