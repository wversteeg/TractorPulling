#include "tp/core/track/track.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <span>
#include <vector>

#include "reference_track.hpp"

namespace tp::track {
namespace {

using testing::reference_params;

math::Curve curve(std::span<const math::CurvePoint> points) {
    const auto result = math::Curve::create(points);
    if (!result) {
        std::abort();
    }
    return *result;
}

// Clay soil, watered between 40 and 60 m: a wet patch in a dry track.
Params wet_patch() {
    constexpr std::array<math::CurvePoint, 4> kMoisture = {
        {{35.0, 0.12}, {40.0, 0.24}, {60.0, 0.24}, {65.0, 0.12}}};
    Params params = reference_params();
    params.soil.clay_fraction = 0.7;
    params.moisture_along_m = curve(kMoisture);
    return params;
}

Model model_of(const Params& params) {
    const auto model = Model::create(params);
    if (!model) {
        std::abort();
    }
    return *model;
}

void expect_same(const soil::Surface& a, const soil::Surface& b) {
    EXPECT_EQ(a.cone_index_pa, b.cone_index_pa);
    EXPECT_EQ(a.grip_factor, b.grip_factor);
    EXPECT_EQ(a.spin_fall_off, b.spin_fall_off);
}

TEST(TrackParams, ReferenceIsValid) {
    EXPECT_EQ(validate(reference_params()), std::nullopt);
    EXPECT_EQ(validate(wet_patch()), std::nullopt);
}

TEST(TrackParams, RejectsNonsense) {
    Params bad_soil = reference_params();
    bad_soil.soil.clay_fraction = 1.5;
    EXPECT_EQ(validate(bad_soil), ParamError::kSoil);
    EXPECT_FALSE(Model::create(bad_soil).has_value());

    constexpr std::array<math::CurvePoint, 2> kMud = {{{0.0, 0.1}, {50.0, 0.9}}};
    Params mud = reference_params();
    mud.moisture_along_m = curve(kMud);
    EXPECT_EQ(validate(mud), ParamError::kMoistureProfile);

    constexpr std::array<math::CurvePoint, 2> kOverRolled = {{{0.0, 0.5}, {50.0, 1.2}}};
    Params rolled = reference_params();
    rolled.compaction_along_m = curve(kOverRolled);
    EXPECT_EQ(validate(rolled), ParamError::kCompactionProfile);

    for (const double grip : {0.4, 1.6, std::numeric_limits<double>::quiet_NaN()}) {
        Params prepared = reference_params();
        prepared.grip_scale = grip;
        EXPECT_EQ(validate(prepared), ParamError::kGripScale) << grip;
    }
}

// How the track was prepared scales the soil's grip everywhere, and
// nothing else about it: a grippier track bites harder, its strength and
// wheelspin the same.
TEST(Track, GripScaleScalesTheSurfaceGripOnly) {
    Params grippy = wet_patch();
    grippy.grip_scale = 1.2;
    const Model plain = model_of(wet_patch());
    const Model scaled = model_of(grippy);
    for (const double x_m : {0.0, 37.5, 50.0, 100.0}) {
        const soil::Surface a = plain.surface_at(x_m);
        const soil::Surface b = scaled.surface_at(x_m);
        EXPECT_DOUBLE_EQ(b.grip_factor, 1.2 * a.grip_factor) << x_m;
        EXPECT_EQ(b.cone_index_pa, a.cone_index_pa);
        EXPECT_EQ(b.spin_fall_off, a.spin_fall_off);
    }
}

// Without a profile the track is its soil everywhere, exactly.
TEST(Track, UniformTrackIsItsSoilEverywhere) {
    const Params params = reference_params();
    const Model model = model_of(params);
    const soil::Surface expected = soil::surface(params.soil);
    for (const double x_m : {-10.0, 0.0, 33.3, 100.0, 1e6}) {
        expect_same(model.surface_at(x_m), expected);
    }
}

// The profile sets the composition along the track and holds its end
// values beyond it.
TEST(Track, ProfileSetsTheSoilAlongTheTrack) {
    const Params params = wet_patch();
    EXPECT_EQ(composition_at(params, 0.0).moisture, 0.12);
    EXPECT_EQ(composition_at(params, 50.0).moisture, 0.24);
    EXPECT_NEAR(composition_at(params, 37.5).moisture, 0.18, 1e-12);
    EXPECT_EQ(composition_at(params, 500.0).moisture, 0.12);
    EXPECT_EQ(composition_at(params, 50.0).clay_fraction, 0.7);
    EXPECT_EQ(composition_at(params, 50.0).compaction, params.soil.compaction);
}

// The sampled surface is the soil model's at every sample, and close to it
// in between. Grip and fall-off have kinks (where clay passes its plastic
// limit); between two samples half a metre apart, a steep patch edge can
// put the kink mid-sample, which costs up to about 0.01.
TEST(Track, SamplesFollowTheSoilModel) {
    const Params params = wet_patch();
    const Model model = model_of(params);
    for (int i = 0; i <= 120; ++i) {
        const double x_m = 30.0 + 0.25 * i;
        const soil::Surface exact = soil::surface(composition_at(params, x_m));
        const soil::Surface sampled = model.surface_at(x_m);
        ASSERT_NEAR(sampled.cone_index_pa, exact.cone_index_pa, 0.01 * exact.cone_index_pa)
            << x_m << " m";
        ASSERT_NEAR(sampled.grip_factor, exact.grip_factor, 0.015) << x_m << " m";
        ASSERT_NEAR(sampled.spin_fall_off, exact.spin_fall_off, 0.015) << x_m << " m";
    }
    expect_same(model.surface_at(50.0), soil::surface(composition_at(params, 50.0)));
    expect_same(model.surface_at(0.0), soil::surface(composition_at(params, 0.0)));
}

// The wet clay is weaker, slicker and smears more than the dry track around
// it, and the surface changes smoothly into and out of the patch.
TEST(Track, WetPatchIsSofterAndSlicker) {
    const Model model = model_of(wet_patch());
    const soil::Surface dry = model.surface_at(20.0);
    const soil::Surface wet = model.surface_at(50.0);
    EXPECT_LT(wet.cone_index_pa, 0.6 * dry.cone_index_pa);
    EXPECT_LT(wet.grip_factor, dry.grip_factor);
    EXPECT_GT(wet.spin_fall_off, dry.spin_fall_off);
    for (int cm = 3000; cm < 7000; ++cm) {
        const double x_m = cm / 100.0;
        ASSERT_LT(std::abs(model.surface_at(x_m + 0.01).cone_index_pa -
                           model.surface_at(x_m).cone_index_pa),
                  0.01 * dry.cone_index_pa)
            << x_m << " m";
    }
}

// A profile longer than the table holds is sampled more coarsely, but
// still covers its whole length.
TEST(Track, LongProfilesAreCoveredToTheirEnd) {
    constexpr std::array<math::CurvePoint, 2> kDrying = {{{0.0, 0.2}, {2000.0, 0.1}}};
    Params params = reference_params();
    params.moisture_along_m = curve(kDrying);
    const Model model = model_of(params);
    expect_same(model.surface_at(2000.0), soil::surface(composition_at(params, 2000.0)));
    expect_same(model.surface_at(5000.0), soil::surface(composition_at(params, 2000.0)));
    const soil::Surface mid = model.surface_at(1000.0);
    const soil::Surface exact = soil::surface(composition_at(params, 1000.0));
    EXPECT_NEAR(mid.cone_index_pa, exact.cone_index_pa, 0.01 * exact.cone_index_pa);
}

// The drag scale along the first 100 m, every 0.25 m.
std::vector<double> drag_profile(const Model& model) {
    std::vector<double> scales;
    for (int i = 0; i <= 400; ++i) {
        scales.push_back(model.surface_at(0.25 * i).drag_scale);
    }
    return scales;
}

Params with_hard_spots(std::uint32_t seed) {
    Params params = reference_params();
    params.hard_spots =
        HardSpots{.every_m = 8.0, .length_m = 1.5, .width_m = 12.0, .extra = 0.5, .seed = seed};
    return params;
}

// Without hard spots the sled drags through the soil as it is.
TEST(Track, WithoutHardSpotsTheDragIsEven) {
    for (const double scale : drag_profile(model_of(reference_params()))) {
        EXPECT_DOUBLE_EQ(scale, 1.0);
    }
}

// Hard spots come and go along the track: along each one's middle the
// drag jumps up to its peak (60 to 100 % of the extra) and back to 1
// between them, about once every every_m.
TEST(Track, HardSpotsMakeTheDragJump) {
    const Model model = model_of(with_hard_spots(7));
    int spots = 0;
    for (const Spot& spot : model.hard_spots()) {
        if (spot.start_m >= 100.0) {
            break;
        }
        ++spots;
        EXPECT_GE(spot.peak, 0.6 * 0.5);
        EXPECT_LE(spot.peak, 0.5);
        double highest = 1.0;
        for (int i = 0; i <= 40; ++i) {
            const double x_m = spot.start_m + spot.length_m * i / 40.0;
            const double scale = model.surface_at(x_m, spot.centre_y_m).drag_scale;
            EXPECT_GE(scale, 1.0);
            highest = std::max(highest, scale);
        }
        EXPECT_DOUBLE_EQ(highest, 1.0 + spot.peak);
        // Just before and after it, at any line across, even.
        for (const double y_m : {-5.0, 0.0, spot.centre_y_m, 5.0}) {
            EXPECT_DOUBLE_EQ(model.surface_at(spot.start_m - 0.01, y_m).drag_scale, 1.0);
            EXPECT_DOUBLE_EQ(model.surface_at(spot.start_m + spot.length_m + 0.01, y_m).drag_scale,
                             1.0);
        }
    }
    // 100 m with a spot every 8 m on average (each 1.5 m long, gaps of 4 to
    // 12 m): from about 6 to 18.
    EXPECT_GE(spots, 6);
    EXPECT_LE(spots, 18);
}

// Even the shortest spot reaches its full peak along its middle (they are
// evaluated, not sampled), and past where the spots are laid the drag is
// even again.
TEST(Track, EveryHardSpotReachesItsPeak) {
    Params params = reference_params();
    params.hard_spots = HardSpots{
        .every_m = 4.0, .length_m = kMinHardSpotLength_m, .width_m = 12.0, .extra = 1.0, .seed = 3};
    const Model model = model_of(params);
    ASSERT_GT(model.hard_spots().size(), 40U);
    for (const Spot& spot : model.hard_spots()) {
        double peak = 1.0;
        for (int i = 0; i <= 100; ++i) {
            const double x_m = spot.start_m + 0.005 * i;
            peak = std::max(peak, model.surface_at(x_m, spot.centre_y_m).drag_scale);
        }
        EXPECT_GE(peak, 1.0 + 0.6 - 0.02) << "spot at " << spot.start_m;
        EXPECT_LE(peak, 2.0);
    }
    for (const double y_m : {-5.0, 0.0, 5.0}) {
        EXPECT_DOUBLE_EQ(model.surface_at(Model::kHardSpotsUntil_m + 5.0, y_m).drag_scale, 1.0);
        EXPECT_DOUBLE_EQ(model.surface_at(1000.0, y_m).drag_scale, 1.0);
    }
}

// The same seed lays the same spots; another seed others.
TEST(Track, HardSpotsFollowTheirSeed) {
    EXPECT_EQ(drag_profile(model_of(with_hard_spots(7))),
              drag_profile(model_of(with_hard_spots(7))));
    EXPECT_NE(drag_profile(model_of(with_hard_spots(7))),
              drag_profile(model_of(with_hard_spots(8))));
}

TEST(Track, RejectsNonsenseHardSpots) {
    for (const HardSpots spots :
         {HardSpots{.every_m = 1.0, .length_m = 1.0, .width_m = 12.0, .extra = 0.5, .seed = 1},
          HardSpots{.every_m = 8.0, .length_m = 0.1, .width_m = 12.0, .extra = 0.5, .seed = 1},
          HardSpots{.every_m = 8.0, .length_m = 20.0, .width_m = 12.0, .extra = 0.5, .seed = 1},
          HardSpots{.every_m = 8.0, .length_m = 1.0, .width_m = 12.0, .extra = -0.1, .seed = 1},
          HardSpots{.every_m = 8.0, .length_m = 1.0, .width_m = 12.0, .extra = 3.0, .seed = 1},
          HardSpots{.every_m = 8.0, .length_m = 1.0, .width_m = 12.0, .extra = 0.5, .seed = 0},
          HardSpots{.every_m = 8.0, .length_m = 1.0, .width_m = 0.2, .extra = 0.5, .seed = 1},
          HardSpots{.every_m = 8.0, .length_m = 1.0, .width_m = 13.0, .extra = 0.5, .seed = 1},
          HardSpots{.every_m = std::numeric_limits<double>::quiet_NaN(),
                    .length_m = 1.0,
                    .width_m = 12.0,
                    .extra = 0.5,
                    .seed = 1}}) {
        Params params = reference_params();
        params.hard_spots = spots;
        EXPECT_EQ(validate(params), ParamError::kHardSpots);
    }
}

Params with_weak_spots(std::uint32_t seed) {
    Params params = reference_params();
    params.weak_spots = WeakSpots{
        .every_m = 10.0, .length_m = 2.0, .width_m = 12.0, .grip_loss = 0.2, .seed = seed};
    return params;
}

// Weak spots take a share of the tires' grip, at most their grip loss, and
// nothing between them; the cone index and the drag stay as they were.
TEST(Track, WeakSpotsTakeSomeGrip) {
    const Model even = model_of(reference_params());
    const Model weak = model_of(with_weak_spots(5));
    int spots = 0;
    bool in_spot = false;
    double least = 1.0;
    for (int i = 0; i <= 4000; ++i) {
        const double x_m = 0.025 * i;
        const soil::Surface plain = even.surface_at(x_m);
        const soil::Surface spot = weak.surface_at(x_m);
        const double share = spot.grip_factor / plain.grip_factor;
        EXPECT_LE(share, 1.0);
        EXPECT_GE(share, 1.0 - 0.2 - 1e-12);
        EXPECT_DOUBLE_EQ(spot.cone_index_pa, plain.cone_index_pa);
        EXPECT_DOUBLE_EQ(spot.drag_scale, 1.0);
        least = std::min(least, share);
        if (share < 1.0 && !in_spot) {
            ++spots;
        }
        in_spot = share < 1.0;
    }
    EXPECT_LE(least, 1.0 - 0.6 * 0.2);
    // 100 m with one every 10 m on average (gaps of 5 to 15 m), the middle
    // line crossing about half of them (as wide as the lane, any point of
    // it as likely to lie in one): 2 to 15.
    EXPECT_GE(spots, 2);
    EXPECT_LE(spots, 15);
}

// The same seed lays the same weak spots; another seed, others.
TEST(Track, WeakSpotsFollowTheirSeed) {
    const Model first = model_of(with_weak_spots(5));
    const Model again = model_of(with_weak_spots(5));
    const Model other = model_of(with_weak_spots(6));
    bool differs = false;
    for (int i = 0; i <= 4000; ++i) {
        const double x_m = 0.025 * i;
        EXPECT_EQ(first.surface_at(x_m).grip_factor, again.surface_at(x_m).grip_factor);
        differs = differs || first.surface_at(x_m).grip_factor != other.surface_at(x_m).grip_factor;
    }
    EXPECT_TRUE(differs);
}

TEST(Track, RejectsNonsenseWeakSpots) {
    for (const WeakSpots spots :
         {WeakSpots{.every_m = 1.0, .length_m = 1.0, .width_m = 12.0, .grip_loss = 0.1, .seed = 1},
          WeakSpots{.every_m = 10.0, .length_m = 0.1, .width_m = 12.0, .grip_loss = 0.1, .seed = 1},
          WeakSpots{
              .every_m = 10.0, .length_m = 1.0, .width_m = 12.0, .grip_loss = -0.1, .seed = 1},
          WeakSpots{.every_m = 10.0, .length_m = 1.0, .width_m = 12.0, .grip_loss = 0.6, .seed = 1},
          WeakSpots{.every_m = 10.0, .length_m = 1.0, .width_m = 12.0, .grip_loss = 0.1, .seed = 0},
          WeakSpots{.every_m = 10.0, .length_m = 1.0, .width_m = 0.2, .grip_loss = 0.1, .seed = 1},
          WeakSpots{
              .every_m = 10.0, .length_m = 1.0, .width_m = 13.0, .grip_loss = 0.1, .seed = 1}}) {
        Params params = reference_params();
        params.weak_spots = spots;
        EXPECT_EQ(validate(params), ParamError::kWeakSpots);
    }
}

// A spot lies somewhere across the lane, at least partly in it, any point
// of the lane as likely to be in one: at full strength over the middle
// half of its width, fading to nothing at its edges, and nothing beside it.
TEST(Track, SpotsLieAcrossTheLane) {
    Params params = reference_params();
    params.weak_spots =
        WeakSpots{.every_m = 6.0, .length_m = 2.0, .width_m = 3.0, .grip_loss = 0.2, .seed = 9};
    const Model model = model_of(params);
    const double even = model_of(reference_params()).surface_at(10.0).grip_factor;
    ASSERT_GT(model.weak_spots().size(), 20U);
    double lowest_y_m = 0.0;
    double highest_y_m = 0.0;
    for (const Spot& spot : model.weak_spots()) {
        EXPECT_GE(spot.centre_y_m + spot.width_m / 2.0, -6.0);
        EXPECT_LE(spot.centre_y_m - spot.width_m / 2.0, 6.0);
        lowest_y_m = std::min(lowest_y_m, spot.centre_y_m);
        highest_y_m = std::max(highest_y_m, spot.centre_y_m);
        const double middle_m = spot.start_m + spot.length_m / 2.0;
        const double y_m = spot.centre_y_m;
        EXPECT_DOUBLE_EQ(model.surface_at(middle_m, y_m).grip_factor, even * (1.0 - spot.peak));
        EXPECT_DOUBLE_EQ(model.surface_at(middle_m, y_m + 0.7).grip_factor,
                         even * (1.0 - spot.peak));
        const double fading = model.surface_at(middle_m, y_m + 1.2).grip_factor;
        EXPECT_GT(fading, even * (1.0 - spot.peak));
        EXPECT_LT(fading, even);
        EXPECT_DOUBLE_EQ(model.surface_at(middle_m, y_m + 1.5).grip_factor, even);
        EXPECT_DOUBLE_EQ(model.surface_at(middle_m, y_m - 2.0).grip_factor, even);
    }
    // Spread over the lane, out to its edges.
    EXPECT_LT(lowest_y_m, -4.0);
    EXPECT_GT(highest_y_m, 4.0);
}

// Any line down the lane, the middle or near a chalk line, crosses about
// as many spots: none is the safe side by the way they are laid.
TEST(Track, EveryLineIsAsLikelyToCrossASpot) {
    Params params = reference_params();
    params.weak_spots =
        WeakSpots{.every_m = 2.0, .length_m = 0.5, .width_m = 3.0, .grip_loss = 0.2, .seed = 4};
    const Model model = model_of(params);
    const std::size_t laid = model.weak_spots().size();
    ASSERT_GT(laid, 70U);
    for (const double y_m : {-5.5, -3.0, 0.0, 3.0, 5.5}) {
        const auto crossed = std::ranges::count_if(model.weak_spots(), [&](const Spot& spot) {
            return std::abs(spot.centre_y_m - y_m) < spot.width_m / 2.0;
        });
        // A share of width / (lane + width) = 0.2 of them, give or take.
        const double share = static_cast<double>(crossed) / static_cast<double>(laid);
        EXPECT_GT(share, 0.1) << y_m;
        EXPECT_LT(share, 0.3) << y_m;
    }
}

}  // namespace
}  // namespace tp::track
