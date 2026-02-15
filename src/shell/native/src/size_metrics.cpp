#include "aura_shell/size_metrics.hpp"

namespace aura::shell {

SizeCategory classify_window_size(int /*w*/, int h) {
    if (h < 400)  return SizeCategory::Compact;
    if (h < 600)  return SizeCategory::Regular;
    if (h < 900)  return SizeCategory::Comfortable;
    return SizeCategory::Spacious;
}

SizeMetrics metrics_for_category(SizeCategory cat) {
    switch (cat) {
        case SizeCategory::Compact:
            return {9, 11, 8, 16, 8, 10, 9, 8, 28, 4, 10, 6, 6};
        case SizeCategory::Regular:
            return {11, 13, 10, 20, 9, 12, 11, 9, 36, 7, 16, 10, 10};
        case SizeCategory::Comfortable:
            return {12, 14, 10, 22, 9, 12, 11, 9, 40, 8, 18, 12, 10};
        case SizeCategory::Spacious:
            return {13, 15, 11, 24, 10, 13, 12, 10, 44, 9, 20, 14, 12};
    }
    return {11, 13, 10, 20, 9, 12, 11, 9, 36, 7, 16, 10, 10};
}

}  // namespace aura::shell
