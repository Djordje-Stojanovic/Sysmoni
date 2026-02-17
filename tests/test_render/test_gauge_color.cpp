#include "render_test_helpers.h"
#include "render_native/theme.hpp"
#include <cstdio>
#include <limits>
#include <string>

using aura::render_native::RgbColor;
using aura::render_native::AuraPalette;
using aura::render_native::parse_hex_color;
using aura::render_native::interpolate_gauge_color;
using aura::render_native::interpolate_gauge_color_hex;
using aura::render_native::relative_luminance;
using aura::render_native::contrast_ratio;

namespace {

void assert_rgb_equal(const RgbColor& actual, int r, int g, int b) {
    assert(actual.red == r);
    assert(actual.green == g);
    assert(actual.blue == b);
}

void assert_rgb_close(const RgbColor& a, const RgbColor& b, int tolerance) {
    const int dr = a.red > b.red ? a.red - b.red : b.red - a.red;
    const int dg = a.green > b.green ? a.green - b.green : b.green - a.green;
    const int db = a.blue > b.blue ? a.blue - b.blue : b.blue - a.blue;
    assert(dr <= tolerance);
    assert(dg <= tolerance);
    assert(db <= tolerance);
}

}  // namespace

// ---------------------------------------------------------------------------
// Gauge color: flat blue segment [0, 40]
// ---------------------------------------------------------------------------

void test_gauge_color_flat_blue_segment() {
    const RgbColor blue = {0x3b, 0x82, 0xf6};

    assert_rgb_equal(interpolate_gauge_color(0.0), blue.red, blue.green, blue.blue);
    assert_rgb_equal(interpolate_gauge_color(20.0), blue.red, blue.green, blue.blue);
    assert_rgb_equal(interpolate_gauge_color(40.0), blue.red, blue.green, blue.blue);
}

// ---------------------------------------------------------------------------
// Gauge color: blue → cyan blend (40, 70]
// ---------------------------------------------------------------------------

void test_gauge_color_blue_to_cyan_blend() {
    const RgbColor blue = {0x3b, 0x82, 0xf6};
    const RgbColor cyan = {0x06, 0xb6, 0xd4};

    // At 55%: midpoint of [40,70], t = 0.5
    const RgbColor mid = interpolate_gauge_color(55.0);
    assert(mid.red < blue.red);      // trending toward cyan (lower red)
    assert(mid.green > blue.green);  // trending toward cyan (higher green)

    // At 70%: boundary → exactly cyan (t = 1.0)
    assert_rgb_equal(interpolate_gauge_color(70.0), cyan.red, cyan.green, cyan.blue);
}

// ---------------------------------------------------------------------------
// Gauge color: cyan → amber blend (70, 85]
// ---------------------------------------------------------------------------

void test_gauge_color_cyan_to_amber_blend() {
    const RgbColor cyan = {0x06, 0xb6, 0xd4};
    const RgbColor amber = {0xf5, 0x9e, 0x0b};

    // At 77.5%: midpoint of [70,85], t = 0.5
    const RgbColor mid = interpolate_gauge_color(77.5);
    assert(mid.red > cyan.red);      // trending toward amber (higher red)
    assert(mid.green < cyan.green);  // trending toward amber (lower green)

    // At 85%: boundary → exactly amber (t = 1.0)
    assert_rgb_equal(interpolate_gauge_color(85.0), amber.red, amber.green, amber.blue);
}

// ---------------------------------------------------------------------------
// Gauge color: amber → red blend (85, 100]
// ---------------------------------------------------------------------------

void test_gauge_color_amber_to_red_blend() {
    const RgbColor amber = {0xf5, 0x9e, 0x0b};
    const RgbColor red = {0xef, 0x44, 0x44};

    // At 92.5%: midpoint of [85,100], t = 0.5
    const RgbColor mid = interpolate_gauge_color(92.5);
    assert(mid.green < amber.green);  // trending toward red (lower green)
    assert(mid.blue > amber.blue);    // trending toward red (higher blue)

    // At 100%: boundary → exactly red (t = 1.0)
    assert_rgb_equal(interpolate_gauge_color(100.0), red.red, red.green, red.blue);
}

// ---------------------------------------------------------------------------
// Gauge color: no discontinuity at segment boundaries
// ---------------------------------------------------------------------------

void test_gauge_color_boundary_continuity() {
    // Colors on each side of a boundary should be nearly identical.
    // Tolerance of 2 RGB steps accounts for rounding.

    // 40% boundary: flat blue → blue-to-cyan blend
    assert_rgb_close(interpolate_gauge_color(39.999), interpolate_gauge_color(40.001), 2);

    // 70% boundary: blue-to-cyan → cyan-to-amber blend
    assert_rgb_close(interpolate_gauge_color(69.999), interpolate_gauge_color(70.001), 2);

    // 85% boundary: cyan-to-amber → amber-to-red blend
    assert_rgb_close(interpolate_gauge_color(84.999), interpolate_gauge_color(85.001), 2);
}

// ---------------------------------------------------------------------------
// Gauge color: NaN, Inf, and out-of-range clamping
// ---------------------------------------------------------------------------

void test_gauge_color_nan_inf_negative() {
    const RgbColor blue = {0x3b, 0x82, 0xf6};
    const RgbColor red = {0xef, 0x44, 0x44};

    // NaN → not finite → treated as 0 → blue
    assert_rgb_equal(
        interpolate_gauge_color(std::numeric_limits<double>::quiet_NaN()),
        blue.red, blue.green, blue.blue
    );

    // +Inf → not finite → treated as 0 → blue (NOT clamped to 100)
    assert_rgb_equal(
        interpolate_gauge_color(std::numeric_limits<double>::infinity()),
        blue.red, blue.green, blue.blue
    );

    // -Inf → not finite → treated as 0 → blue
    assert_rgb_equal(
        interpolate_gauge_color(-std::numeric_limits<double>::infinity()),
        blue.red, blue.green, blue.blue
    );

    // Negative finite → clamped to 0 → blue
    assert_rgb_equal(interpolate_gauge_color(-50.0), blue.red, blue.green, blue.blue);

    // Over 100 (finite) → clamped to 100 → red
    assert_rgb_equal(interpolate_gauge_color(200.0), red.red, red.green, red.blue);
}

// ---------------------------------------------------------------------------
// Gauge color: hex output matches RGB output
// ---------------------------------------------------------------------------

void test_gauge_color_hex_consistency() {
    const double values[] = {0.0, 25.0, 55.0, 77.5, 92.5, 100.0};
    for (const double p : values) {
        const RgbColor c = interpolate_gauge_color(p);
        const std::string hex = interpolate_gauge_color_hex(p);
        char expected[8];
        std::snprintf(expected, sizeof(expected), "#%02x%02x%02x", c.red, c.green, c.blue);
        assert(hex == expected);
    }
}

// ---------------------------------------------------------------------------
// WCAG relative luminance: known values
// ---------------------------------------------------------------------------

void test_luminance_known_values() {
    // Black → 0
    assert(std::fabs(relative_luminance({0, 0, 0}) - 0.0) < 1e-6);

    // White → 1
    assert(std::fabs(relative_luminance({255, 255, 255}) - 1.0) < 1e-6);

    // Pure primaries: coefficients are 0.2126, 0.7152, 0.0722
    assert(std::fabs(relative_luminance({255, 0, 0}) - 0.2126) < 0.001);
    assert(std::fabs(relative_luminance({0, 255, 0}) - 0.7152) < 0.001);
    assert(std::fabs(relative_luminance({0, 0, 255}) - 0.0722) < 0.001);
}

// ---------------------------------------------------------------------------
// Luminance increases with brightness
// ---------------------------------------------------------------------------

void test_luminance_monotonically_increases() {
    const double l_black = relative_luminance({0, 0, 0});
    const double l_gray = relative_luminance({128, 128, 128});
    const double l_white = relative_luminance({255, 255, 255});

    assert(l_black < l_gray);
    assert(l_gray < l_white);
}

// ---------------------------------------------------------------------------
// Contrast ratio: black vs white → 21:1 (maximum)
// ---------------------------------------------------------------------------

void test_contrast_ratio_black_vs_white() {
    const double ratio = contrast_ratio({0, 0, 0}, {255, 255, 255});
    assert(std::fabs(ratio - 21.0) < 0.1);
}

// ---------------------------------------------------------------------------
// Contrast ratio: identical colors → 1:1 (minimum)
// ---------------------------------------------------------------------------

void test_contrast_ratio_identical_colors() {
    const double ratio = contrast_ratio({100, 150, 200}, {100, 150, 200});
    assert(std::fabs(ratio - 1.0) < 1e-6);
}

// ---------------------------------------------------------------------------
// Contrast ratio is symmetric: ratio(A,B) == ratio(B,A)
// ---------------------------------------------------------------------------

void test_contrast_ratio_symmetric() {
    const RgbColor a = {59, 130, 246};   // accent blue
    const RgbColor b = {239, 68, 68};    // accent red
    const double ratio_ab = contrast_ratio(a, b);
    const double ratio_ba = contrast_ratio(b, a);
    assert(std::fabs(ratio_ab - ratio_ba) < 1e-9);
}

// ---------------------------------------------------------------------------
// Contrast ratio always in [1, 21]
// ---------------------------------------------------------------------------

void test_contrast_ratio_range() {
    const RgbColor colors[] = {
        {0, 0, 0}, {255, 255, 255}, {128, 0, 0},
        {0, 128, 0}, {0, 0, 128}, {59, 130, 246},
    };
    for (const auto& a : colors) {
        for (const auto& b : colors) {
            const double ratio = contrast_ratio(a, b);
            assert(ratio >= 1.0 - 1e-9);
            assert(ratio <= 21.0 + 0.1);
        }
    }
}

// ---------------------------------------------------------------------------
// Palette accessibility: WCAG AA (contrast >= 4.5) for text on backgrounds
// ---------------------------------------------------------------------------

void test_palette_accessibility() {
    const RgbColor text_primary = parse_hex_color(AuraPalette::kTextPrimary);    // #e0ecf7
    const RgbColor text_secondary = parse_hex_color(AuraPalette::kTextSecondary); // #8badc4
    const RgbColor window_bg = parse_hex_color(AuraPalette::kWindowBg);           // #060b14

    // Primary text vs window background — must pass WCAG AA
    assert(contrast_ratio(text_primary, window_bg) >= 4.5);

    // Secondary text vs window background — must pass WCAG AA
    assert(contrast_ratio(text_secondary, window_bg) >= 4.5);
}

// ---------------------------------------------------------------------------
// parse_hex_color: valid inputs
// ---------------------------------------------------------------------------

void test_parse_hex_color_cases() {
    assert_rgb_equal(parse_hex_color("#000000"), 0, 0, 0);
    assert_rgb_equal(parse_hex_color("#ffffff"), 255, 255, 255);
    assert_rgb_equal(parse_hex_color("#3b82f6"), 59, 130, 246);
    assert_rgb_equal(parse_hex_color("#FF0000"), 255, 0, 0);
    assert_rgb_equal(parse_hex_color("#0a1b2c"), 10, 27, 44);

    // Palette constants parse to their documented RGB values
    assert_rgb_equal(parse_hex_color(AuraPalette::kAccentBlue), 0x3b, 0x82, 0xf6);
    assert_rgb_equal(parse_hex_color(AuraPalette::kAccentCyan), 0x06, 0xb6, 0xd4);
    assert_rgb_equal(parse_hex_color(AuraPalette::kAccentAmber), 0xf5, 0x9e, 0x0b);
    assert_rgb_equal(parse_hex_color(AuraPalette::kAccentRed), 0xef, 0x44, 0x44);
}
