from __future__ import annotations

import math


def sanitize_percent(value: float) -> float:
    numeric = float(value)
    if not math.isfinite(numeric):
        return 0.0
    return max(0.0, min(100.0, numeric))


def sanitize_non_negative(value: float) -> float:
    numeric = float(value)
    if not math.isfinite(numeric):
        return 0.0
    return max(0.0, numeric)


__all__ = ["sanitize_non_negative", "sanitize_percent"]
