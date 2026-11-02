#include "tp/core/math/curve.hpp"

#include <algorithm>
#include <cmath>

namespace tp::math {

std::optional<CurveError> Curve::validate(std::span<const CurvePoint> points) noexcept {
    if (points.size() < kMinPoints) {
        return CurveError::kTooFewPoints;
    }
    if (points.size() > kMaxPoints) {
        return CurveError::kTooManyPoints;
    }
    for (std::size_t i = 0; i < points.size(); ++i) {
        if (!std::isfinite(points[i].x) || !std::isfinite(points[i].y)) {
            return CurveError::kNotFinite;
        }
        if (i > 0 && !(points[i].x > points[i - 1].x)) {
            return CurveError::kNotStrictlyIncreasing;
        }
    }
    return std::nullopt;
}

std::optional<Curve> Curve::create(std::span<const CurvePoint> points) noexcept {
    if (validate(points).has_value()) {
        return std::nullopt;
    }
    Curve curve;
    std::ranges::copy(points, curve.points_.begin());
    curve.size_ = points.size();
    return curve;
}

double Curve::max_y() const noexcept {
    return std::ranges::max(points(), {}, &CurvePoint::y).y;
}

}  // namespace tp::math
