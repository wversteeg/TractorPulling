#include "tp/core/crew/crew.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <stdexcept>

#include "../vehicle/reference_vehicle.hpp"

namespace tp::crew {
namespace {

track::Params spotted_track() {
    track::Params params;
    params.soil = {.clay_fraction = 0.3, .moisture = 0.15, .compaction = 0.8};
    params.hard_spots = track::HardSpots{.every_m = 6.0,
                                         .length_m = 1.5,
                                         .min_width_m = 3.0,
                                         .max_width_m = 3.0,
                                         .extra = 0.4,
                                         .seed = 5};
    params.weak_spots = track::WeakSpots{.every_m = 8.0,
                                         .length_m = 2.0,
                                         .min_width_m = 2.0,
                                         .max_width_m = 2.0,
                                         .grip_loss = 0.2,
                                         .seed = 6};
    return params;
}

track::Model model_of(const track::Params& params) {
    const std::optional<track::Model> model = track::Model::create(params);
    if (!model) {
        throw std::invalid_argument("invalid track");
    }
    return *model;
}

constexpr double kUntil_m = 100.0;

// The spots on the track up to kUntil_m.
std::size_t laid(const track::Model& model) {
    const auto before = [](const track::Spot& spot) { return spot.start_m < kUntil_m; };
    return static_cast<std::size_t>(std::ranges::count_if(model.hard_spots(), before) +
                                    std::ranges::count_if(model.weak_spots(), before));
}

// A crew that has seen nothing misses some spots; after many pulls, and in
// the pull-off, it misses fewer.
TEST(Crew, SeesMoreWithEveryPull) {
    const track::Model model = model_of(spotted_track());
    const std::size_t all = laid(model);
    ASSERT_GT(all, 20U);
    const Report first = report(model, {.pulls_seen = 0, .pull_off = false}, kUntil_m, 1);
    const Report later = report(model, {.pulls_seen = 14, .pull_off = false}, kUntil_m, 1);
    const Report pull_off = report(model, {.pulls_seen = 14, .pull_off = true}, kUntil_m, 1);
    EXPECT_LT(first.count, all);
    EXPECT_GT(first.count, all / 4);
    EXPECT_GT(later.count, first.count);
    EXPECT_GE(pull_off.count, later.count);
    EXPECT_LE(pull_off.count, all);
    EXPECT_DOUBLE_EQ(first.uncertainty, 1.0);
    EXPECT_LT(later.uncertainty, 0.25);
    EXPECT_DOUBLE_EQ(pull_off.uncertainty, 0.5 * later.uncertainty);
}

// Each reported spot is a real one, off by no more than the errors allow
// at the crew's uncertainty.
TEST(Crew, ReportsSpotsRoughlyWhereTheyAre) {
    const track::Model model = model_of(spotted_track());
    for (const int seen : {0, 4, 20}) {
        const Report seen_report =
            report(model, {.pulls_seen = seen, .pull_off = false}, kUntil_m, 3);
        const double u = seen_report.uncertainty;
        for (const ReportedSpot& spot : seen_report.view()) {
            const auto spots =
                spot.kind == SpotKind::kHard ? model.hard_spots() : model.weak_spots();
            const bool matches = std::ranges::any_of(spots, [&](const track::Spot& real) {
                const double middle_m = real.start_m + real.length_m / 2.0;
                return std::abs(spot.at_m - middle_m) <= kAlongError_m * u + 1e-9 &&
                       std::abs(spot.centre_y_m - real.centre_y_m) <= kAcrossError_m * u + 1e-9 &&
                       std::abs(spot.strength - real.peak) <= kStrengthError * u * real.peak + 1e-9;
            });
            EXPECT_TRUE(matches) << "seen " << seen << ", at " << spot.at_m;
        }
    }
}

// The same seed, the same report; a track without spots, an empty one.
TEST(Crew, TheSameSeedTheSameReport) {
    const track::Model model = model_of(spotted_track());
    const Report a = report(model, {.pulls_seen = 3, .pull_off = false}, kUntil_m, 42);
    const Report b = report(model, {.pulls_seen = 3, .pull_off = false}, kUntil_m, 42);
    ASSERT_EQ(a.count, b.count);
    for (std::size_t i = 0; i < a.count; ++i) {
        EXPECT_EQ(a.spots[i].at_m, b.spots[i].at_m);
        EXPECT_EQ(a.spots[i].centre_y_m, b.spots[i].centre_y_m);
    }
    track::Params even = spotted_track();
    even.hard_spots.reset();
    even.weak_spots.reset();
    EXPECT_EQ(report(model_of(even), {}, kUntil_m, 42).count, 0U);
}

// The crew keeps a reported weak spot from under the rear tires: on a lane
// with one weak spot just left of the middle, it starts the tractor to its
// right, as near the middle as that allows; with nothing reported, in the
// middle.
TEST(Crew, KeepsTheWheelsOffAWeakSpot) {
    const vehicle::Params tractor = vehicle::testing::reference_params();
    const double half_track_m = tractor.chassis.rear_track_m / 2.0;
    const double half_tire_m = tractor.rear_tire.size.section_width_m / 2.0;
    Report one;
    one.spots[0] = {.kind = SpotKind::kWeak,
                    .at_m = 40.0,
                    .centre_y_m = 0.5,
                    .width_m = 1.0,
                    .length_m = 2.0,
                    .strength = 0.1};
    one.count = 1;
    const double y_m = best_line_m(one, tractor, 3.0);
    // Neither tire over the spot.
    EXPECT_GE(std::abs(y_m + half_track_m - 0.5), 0.5 + half_tire_m);
    EXPECT_GE(std::abs(y_m - half_track_m - 0.5), 0.5 + half_tire_m);
    EXPECT_LE(std::abs(y_m), 1.0);
    EXPECT_DOUBLE_EQ(best_line_m(Report{}, tractor, 3.0), 0.0);
    // No room to move: the middle.
    EXPECT_DOUBLE_EQ(best_line_m(one, tractor, 0.0), 0.0);
}

// A hard spot in the middle: the crew moves the sled's pan off it.
TEST(Crew, KeepsThePanOffAHardSpot) {
    const vehicle::Params tractor = vehicle::testing::reference_params();
    Report one;
    one.spots[0] = {.kind = SpotKind::kHard,
                    .at_m = 30.0,
                    .centre_y_m = 0.0,
                    .width_m = 2.0,
                    .length_m = 1.5,
                    .strength = 0.3};
    one.count = 1;
    EXPECT_GE(std::abs(best_line_m(one, tractor, 3.0)), 2.0 - 1e-9);
}

}  // namespace
}  // namespace tp::crew
