#include "aura_platform.h"
#include "c_api_helpers.hpp"
#include "health_engine.hpp"

#include <exception>
#include <memory>
#include <vector>

using aura::platform::EmaSmoother;
using aura::platform::HealthScore;
using aura::platform::HealthWeights;
using aura::platform::Snapshot;
using aura::platform::TrendResult;

namespace {

struct AuraSmoother {
    explicit AuraSmoother(std::unique_ptr<EmaSmoother> smoother_in)
        : smoother(std::move(smoother_in)) {}

    std::unique_ptr<EmaSmoother> smoother;
};

void CopyHealthScore(const HealthScore& src, aura_health_score_t& dst) {
    dst.overall = src.overall;
    dst.cpu_score = src.cpu_score;
    dst.memory_score = src.memory_score;
    dst.disk_score = src.disk_score;
    dst.network_score = src.network_score;
}

HealthWeights ToInternalWeights(const aura_health_weights_t& abi) {
    HealthWeights w;
    w.cpu = abi.cpu;
    w.memory = abi.memory;
    w.disk = abi.disk;
    w.network = abi.network;
    return w;
}

void CopyTrendResult(const TrendResult& src, aura_trend_result_t& dst) {
    dst.direction = static_cast<int>(src.direction);
    dst.slope = src.slope;
    dst.r_squared = src.r_squared;
    dst.intercept = src.intercept;
}

} // namespace

extern "C" {

// ---------------------------------------------------------------------------
// Health Score
// ---------------------------------------------------------------------------

AURA_PLATFORM_EXPORT int aura_health_score_compute(
    const aura_snapshot_t* snapshot,
    aura_health_score_t* out_score,
    aura_error_t* out_error
) {
    if (snapshot == nullptr) {
        SetError(out_error, AURA_ERR_INVALID_ARGUMENT, "snapshot must not be null.");
        return AURA_ERR_INVALID_ARGUMENT;
    }
    if (out_score == nullptr) {
        SetError(out_error, AURA_ERR_INVALID_ARGUMENT, "out_score must not be null.");
        return AURA_ERR_INVALID_ARGUMENT;
    }

    try {
        const Snapshot internal = ToInternalSnapshot(*snapshot);
        const HealthScore score = aura::platform::ComputeHealthScore(internal);
        CopyHealthScore(score, *out_score);
        ClearError(out_error);
        return AURA_OK;
    } catch (const std::exception& exc) {
        return HandleException(exc, out_error);
    }
}

AURA_PLATFORM_EXPORT int aura_health_score_compute_weighted(
    const aura_snapshot_t* snapshot,
    const aura_health_weights_t* weights,
    aura_health_score_t* out_score,
    aura_error_t* out_error
) {
    if (snapshot == nullptr) {
        SetError(out_error, AURA_ERR_INVALID_ARGUMENT, "snapshot must not be null.");
        return AURA_ERR_INVALID_ARGUMENT;
    }
    if (weights == nullptr) {
        SetError(out_error, AURA_ERR_INVALID_ARGUMENT, "weights must not be null.");
        return AURA_ERR_INVALID_ARGUMENT;
    }
    if (out_score == nullptr) {
        SetError(out_error, AURA_ERR_INVALID_ARGUMENT, "out_score must not be null.");
        return AURA_ERR_INVALID_ARGUMENT;
    }

    try {
        const Snapshot internal = ToInternalSnapshot(*snapshot);
        const HealthWeights internal_weights = ToInternalWeights(*weights);
        const HealthScore score = aura::platform::ComputeHealthScoreWeighted(internal, internal_weights);
        CopyHealthScore(score, *out_score);
        ClearError(out_error);
        return AURA_OK;
    } catch (const std::exception& exc) {
        return HandleException(exc, out_error);
    }
}

// ---------------------------------------------------------------------------
// Trend Detection
// ---------------------------------------------------------------------------

AURA_PLATFORM_EXPORT int aura_trend_detect(
    const aura_snapshot_t* snapshots,
    const int count,
    const int metric,
    const double sensitivity,
    aura_trend_result_t* out_trend,
    aura_error_t* out_error
) {
    if (count < 0) {
        SetError(out_error, AURA_ERR_INVALID_ARGUMENT, "count must be >= 0.");
        return AURA_ERR_INVALID_ARGUMENT;
    }
    if (count > 0 && snapshots == nullptr) {
        SetError(out_error, AURA_ERR_INVALID_ARGUMENT, "snapshots must not be null when count > 0.");
        return AURA_ERR_INVALID_ARGUMENT;
    }
    if (out_trend == nullptr) {
        SetError(out_error, AURA_ERR_INVALID_ARGUMENT, "out_trend must not be null.");
        return AURA_ERR_INVALID_ARGUMENT;
    }

    try {
        std::vector<Snapshot> internal;
        internal.reserve(static_cast<std::size_t>(count));
        for (int i = 0; i < count; ++i) {
            internal.push_back(ToInternalSnapshot(snapshots[i]));
        }

        const TrendResult result = aura::platform::DetectTrend(internal, metric, sensitivity);
        CopyTrendResult(result, *out_trend);
        ClearError(out_error);
        return AURA_OK;
    } catch (const std::invalid_argument& exc) {
        SetError(out_error, AURA_ERR_INVALID_ARGUMENT, exc.what());
        return AURA_ERR_INVALID_ARGUMENT;
    } catch (const std::exception& exc) {
        return HandleException(exc, out_error);
    }
}

// ---------------------------------------------------------------------------
// EMA Smoother
// ---------------------------------------------------------------------------

AURA_PLATFORM_EXPORT int aura_smoother_create(
    const double alpha,
    aura_smoother_t** out_smoother,
    aura_error_t* out_error
) {
    if (out_smoother == nullptr) {
        SetError(out_error, AURA_ERR_INVALID_ARGUMENT, "out_smoother must not be null.");
        return AURA_ERR_INVALID_ARGUMENT;
    }

    try {
        auto wrapped = std::make_unique<AuraSmoother>(
            std::make_unique<EmaSmoother>(alpha)
        );
        *out_smoother = reinterpret_cast<aura_smoother_t*>(wrapped.release());
        ClearError(out_error);
        return AURA_OK;
    } catch (const std::invalid_argument& exc) {
        SetError(out_error, AURA_ERR_INVALID_ARGUMENT, exc.what());
        return AURA_ERR_INVALID_ARGUMENT;
    } catch (const std::exception& exc) {
        return HandleException(exc, out_error);
    }
}

AURA_PLATFORM_EXPORT int aura_smoother_update(
    aura_smoother_t* smoother,
    const aura_snapshot_t* raw_snapshot,
    aura_snapshot_t* out_smoothed,
    aura_error_t* out_error
) {
    if (smoother == nullptr) {
        SetError(out_error, AURA_ERR_INVALID_ARGUMENT, "smoother must not be null.");
        return AURA_ERR_INVALID_ARGUMENT;
    }
    if (raw_snapshot == nullptr) {
        SetError(out_error, AURA_ERR_INVALID_ARGUMENT, "raw_snapshot must not be null.");
        return AURA_ERR_INVALID_ARGUMENT;
    }
    if (out_smoothed == nullptr) {
        SetError(out_error, AURA_ERR_INVALID_ARGUMENT, "out_smoothed must not be null.");
        return AURA_ERR_INVALID_ARGUMENT;
    }

    try {
        auto* typed = reinterpret_cast<AuraSmoother*>(smoother);
        const Snapshot raw = ToInternalSnapshot(*raw_snapshot);
        const Snapshot smoothed = typed->smoother->Update(raw);
        *out_smoothed = ToAbiSnapshot(smoothed);
        ClearError(out_error);
        return AURA_OK;
    } catch (const std::exception& exc) {
        return HandleException(exc, out_error);
    }
}

AURA_PLATFORM_EXPORT int aura_smoother_reset(
    aura_smoother_t* smoother,
    aura_error_t* out_error
) {
    if (smoother == nullptr) {
        SetError(out_error, AURA_ERR_INVALID_ARGUMENT, "smoother must not be null.");
        return AURA_ERR_INVALID_ARGUMENT;
    }

    try {
        auto* typed = reinterpret_cast<AuraSmoother*>(smoother);
        typed->smoother->Reset();
        ClearError(out_error);
        return AURA_OK;
    } catch (const std::exception& exc) {
        return HandleException(exc, out_error);
    }
}

AURA_PLATFORM_EXPORT int aura_smoother_destroy(aura_smoother_t* smoother) {
    if (smoother == nullptr) {
        return AURA_OK;
    }

    auto* typed = reinterpret_cast<AuraSmoother*>(smoother);
    delete typed;
    return AURA_OK;
}

} // extern "C"
