#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace tp::math {

struct CurvePoint {
    double x;
    double y;
};

enum class CurveError : std::uint8_t {
    kTooFewPoints,
    kTooManyPoints,
    kNotFinite,
    kNotStrictlyIncreasing,
};

/// Piecewise-linear function y(x) over a small, fixed-capacity table.
///
/// Storage is inline so curves can live in plain-data parameter structs and
/// be evaluated in the simulation step without touching the heap. Outside the
/// table the curve is clamped to its first and last value: physical tables
/// (torque, grip) should not be extrapolated silently.
class Curve {
public:
    static constexpr std::size_t kMinPoints = 2;
    static constexpr std::size_t kMaxPoints = 16;

    /// Returns the first problem with `points`, or nothing if they form a
    /// valid curve: 2 to 16 finite points with strictly increasing x.
    [[nodiscard]] static std::optional<CurveError> validate(
        std::span<const CurvePoint> points) noexcept;

    [[nodiscard]] static std::optional<Curve> create(std::span<const CurvePoint> points) noexcept;

    [[nodiscard]] double evaluate(double x) const noexcept {
        if (x <= points_[0].x) {
            return points_[0].y;
        }
        // Linear scan: tables are at most 16 points, so this beats a binary
        // search and keeps the code branch-predictable.
        for (std::size_t i = 1; i < size_; ++i) {
            const CurvePoint& hi = points_[i];
            if (x < hi.x) {
                const CurvePoint& lo = points_[i - 1];
                const double t = (x - lo.x) / (hi.x - lo.x);
                return lo.y + t * (hi.y - lo.y);
            }
        }
        return points_[size_ - 1].y;
    }

    [[nodiscard]] std::span<const CurvePoint> points() const noexcept {
        return {points_.data(), size_};
    }
    [[nodiscard]] double min_x() const noexcept { return points_[0].x; }
    [[nodiscard]] double max_x() const noexcept { return points_[size_ - 1].x; }
    [[nodiscard]] double max_y() const noexcept;

private:
    Curve() = default;

    std::array<CurvePoint, kMaxPoints> points_{};
    std::size_t size_ = 0;
};

}  // namespace tp::math
