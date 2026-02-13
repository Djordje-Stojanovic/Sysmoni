#include "render_native/theme.hpp"

#include "render_native/math.hpp"

#include <array>
#include <cctype>
#include <cmath>
#include <stdexcept>

namespace aura::render_native {

namespace {

int parse_hex_nibble(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return 10 + (c - 'a');
    }
    if (c >= 'A' && c <= 'F') {
        return 10 + (c - 'A');
    }
    throw std::invalid_argument("Expected #RRGGBB color.");
}

int parse_hex_byte(char hi, char lo) {
    return (parse_hex_nibble(hi) * 16) + parse_hex_nibble(lo);
}

char to_hex_char(int value) {
    static constexpr std::array<char, 16> kHex = {
        '0', '1', '2', '3', '4', '5', '6', '7',
        '8', '9', 'a', 'b', 'c', 'd', 'e', 'f',
    };
    if (value < 0 || value > 15) {
        throw std::invalid_argument("hex nibble out of range");
    }
    return kHex[static_cast<std::size_t>(value)];
}

}  // namespace

RgbColor parse_hex_color(const std::string& value) {
    if (value.size() != 7 || value.front() != '#') {
        throw std::invalid_argument("Expected #RRGGBB color.");
    }

    return RgbColor{
        parse_hex_byte(value[1], value[2]),
        parse_hex_byte(value[3], value[4]),
        parse_hex_byte(value[5], value[6]),
    };
}

std::string blend_hex_color(const std::string& start, const std::string& end, double ratio) {
    const RgbColor start_rgb = parse_hex_color(start);
    const RgbColor end_rgb = parse_hex_color(end);
    const double t = clamp_unit(ratio);

    const int red = static_cast<int>(std::lround(
        static_cast<double>(start_rgb.red) + ((end_rgb.red - start_rgb.red) * t)
    ));
    const int green = static_cast<int>(std::lround(
        static_cast<double>(start_rgb.green) + ((end_rgb.green - start_rgb.green) * t)
    ));
    const int blue = static_cast<int>(std::lround(
        static_cast<double>(start_rgb.blue) + ((end_rgb.blue - start_rgb.blue) * t)
    ));

    std::string out;
    out.reserve(7);
    out.push_back('#');
    out.push_back(to_hex_char((red >> 4) & 0x0f));
    out.push_back(to_hex_char(red & 0x0f));
    out.push_back(to_hex_char((green >> 4) & 0x0f));
    out.push_back(to_hex_char(green & 0x0f));
    out.push_back(to_hex_char((blue >> 4) & 0x0f));
    out.push_back(to_hex_char(blue & 0x0f));
    return out;
}

int quantize_accent_intensity(double accent_intensity) {
    return static_cast<int>(std::lround(clamp_unit(accent_intensity) * 100.0));
}

}  // namespace aura::render_native
