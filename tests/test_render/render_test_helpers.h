#pragma once

#include "aura_render.h"
#include <cassert>
#include <cmath>
#include <cstring>

constexpr double kFloatEpsilon = 1e-9;

inline void assert_last_error_contains(const char* needle) {
    const char* error = aura_last_error();
    assert(error != nullptr);
    assert(std::strstr(error, needle) != nullptr);
}

inline void assert_last_error_clear() {
    const char* error = aura_last_error();
    assert(error != nullptr);
    assert(std::strcmp(error, "") == 0);
}

void assert_style_tokens_ranges(const AuraRenderStyleTokens& tokens, int target_fps);
void assert_style_tokens_close(
    const AuraRenderStyleTokens& actual,
    const AuraRenderStyleTokens& expected,
    double epsilon
);
