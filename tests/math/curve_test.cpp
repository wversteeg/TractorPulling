#include "tp/core/math/curve.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <vector>

namespace tp::math {
namespace {

constexpr std::array<CurvePoint, 3> kPoints = {{{1.0, 10.0}, {2.0, 30.0}, {4.0, 20.0}}};

Curve make_curve() {
    const auto curve = Curve::create(kPoints);
    if (!curve) {
        ADD_FAILURE() << "invalid test curve";
        std::abort();
    }
    return *curve;
}

TEST(Curve, PassesThroughItsPoints) {
    const Curve curve = make_curve();
    for (const CurvePoint& p : kPoints) {
        EXPECT_DOUBLE_EQ(curve.evaluate(p.x), p.y);
    }
}

TEST(Curve, InterpolatesLinearlyBetweenPoints) {
    const Curve curve = make_curve();
    EXPECT_DOUBLE_EQ(curve.evaluate(1.5), 20.0);
    EXPECT_DOUBLE_EQ(curve.evaluate(3.0), 25.0);
}

TEST(Curve, ClampsOutsideItsRange) {
    const Curve curve = make_curve();
    EXPECT_DOUBLE_EQ(curve.evaluate(-100.0), 10.0);
    EXPECT_DOUBLE_EQ(curve.evaluate(100.0), 20.0);
}

TEST(Curve, SlopeIsThatOfTheSegmentAndFlatOutside) {
    const Curve curve = make_curve();
    EXPECT_DOUBLE_EQ(curve.slope(0.5), 0.0);
    EXPECT_DOUBLE_EQ(curve.slope(1.0), 20.0) << "right-hand slope at the first point";
    EXPECT_DOUBLE_EQ(curve.slope(1.5), 20.0);
    EXPECT_DOUBLE_EQ(curve.slope(2.0), -5.0);
    EXPECT_DOUBLE_EQ(curve.slope(3.9), -5.0);
    EXPECT_DOUBLE_EQ(curve.slope(4.0), 0.0);
    EXPECT_DOUBLE_EQ(curve.slope(10.0), 0.0);
}

TEST(Curve, ReportsRangeAndMaximum) {
    const Curve curve = make_curve();
    EXPECT_DOUBLE_EQ(curve.min_x(), 1.0);
    EXPECT_DOUBLE_EQ(curve.max_x(), 4.0);
    EXPECT_DOUBLE_EQ(curve.max_y(), 30.0);
    EXPECT_EQ(curve.points().size(), kPoints.size());
}

TEST(Curve, RejectsTooFewPoints) {
    const std::array<CurvePoint, 1> one = {{{1.0, 1.0}}};
    EXPECT_EQ(Curve::validate(one), CurveError::kTooFewPoints);
    EXPECT_FALSE(Curve::create(one).has_value());
}

TEST(Curve, RejectsTooManyPoints) {
    std::vector<CurvePoint> points;
    for (std::size_t i = 0; i <= Curve::kMaxPoints; ++i) {
        points.push_back({static_cast<double>(i), 0.0});
    }
    EXPECT_EQ(Curve::validate(points), CurveError::kTooManyPoints);
}

TEST(Curve, RejectsNonIncreasingX) {
    const std::array<CurvePoint, 3> repeated = {{{1.0, 1.0}, {2.0, 1.0}, {2.0, 3.0}}};
    const std::array<CurvePoint, 2> decreasing = {{{2.0, 1.0}, {1.0, 1.0}}};
    EXPECT_EQ(Curve::validate(repeated), CurveError::kNotStrictlyIncreasing);
    EXPECT_EQ(Curve::validate(decreasing), CurveError::kNotStrictlyIncreasing);
}

TEST(Curve, RejectsNonFiniteValues) {
    const std::array<CurvePoint, 2> nan_y = {{{1.0, std::nan("")}, {2.0, 1.0}}};
    const std::array<CurvePoint, 2> inf_x = {
        {{1.0, 1.0}, {std::numeric_limits<double>::infinity(), 1.0}}};
    EXPECT_EQ(Curve::validate(nan_y), CurveError::kNotFinite);
    EXPECT_EQ(Curve::validate(inf_x), CurveError::kNotFinite);
}

}  // namespace
}  // namespace tp::math
