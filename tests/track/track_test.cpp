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
    params.hard_spots = HardSpots{.every_m = 8.0, .length_m = 1.5, .extra = 0.5, .seed = seed};
    return params;
}

// Without hard spots the sled drags through the soil as it is.
TEST(Track, WithoutHardSpotsTheDragIsEven) {
    for (const double scale : drag_profile(model_of(reference_params()))) {
        EXPECT_DOUBLE_EQ(scale, 1.0);
    }
}

// Hard spots come and go along the track: the drag jumps up to at most the
// extra and back to 1 between them, about once every every_m.
TEST(Track, HardSpotsMakeTheDragJump) {
    const std::vector<double> scales = drag_profile(model_of(with_hard_spots(7)));
    int spots = 0;
    bool in_spot = false;
    for (const double scale : scales) {
        EXPECT_GE(scale, 1.0);
        EXPECT_LE(scale, 1.5);
        if (scale > 1.0 && !in_spot) {
            ++spots;
        }
        in_spot = scale > 1.0;
    }
    EXPECT_GE(*std::ranges::max_element(scales), 1.0 + 0.6 * 0.5 * 0.8);
    // 100 m with a spot every 8 m on average (each 1.5 m long, gaps of 4 to
    // 12 m): from about 6 to 18.
    EXPECT_GE(spots, 6);
    EXPECT_LE(spots, 18);
}

// Even the shortest spot reaches its full peak (they are evaluated, not
// sampled), and past where the spots are laid the drag is even again.
TEST(Track, EveryHardSpotReachesItsPeak) {
    Params params = reference_params();
    params.hard_spots =
        HardSpots{.every_m = 4.0, .length_m = kMinHardSpotLength_m, .extra = 1.0, .seed = 3};
    const Model model = model_of(params);
    double previous = 1.0;
    double peak = 1.0;
    int spots = 0;
    for (int i = 0; i <= 20000; ++i) {
        const double scale = model.surface_at(0.005 * i).drag_scale;
        if (scale > 1.0) {
            peak = std::max(peak, scale);
        } else if (previous > 1.0) {
            ++spots;
            EXPECT_GE(peak, 1.0 + 0.6 - 0.02) << "spot " << spots;
            EXPECT_LE(peak, 2.0);
            peak = 1.0;
        }
        previous = scale;
    }
    EXPECT_GT(spots, 10);
    EXPECT_DOUBLE_EQ(model.surface_at(Model::kHardSpotsUntil_m + 5.0).drag_scale, 1.0);
    EXPECT_DOUBLE_EQ(model.surface_at(1000.0).drag_scale, 1.0);
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
         {HardSpots{.every_m = 1.0, .length_m = 1.0, .extra = 0.5, .seed = 1},
          HardSpots{.every_m = 8.0, .length_m = 0.1, .extra = 0.5, .seed = 1},
          HardSpots{.every_m = 8.0, .length_m = 20.0, .extra = 0.5, .seed = 1},
          HardSpots{.every_m = 8.0, .length_m = 1.0, .extra = -0.1, .seed = 1},
          HardSpots{.every_m = 8.0, .length_m = 1.0, .extra = 3.0, .seed = 1},
          HardSpots{.every_m = 8.0, .length_m = 1.0, .extra = 0.5, .seed = 0},
          HardSpots{.every_m = std::numeric_limits<double>::quiet_NaN(),
                    .length_m = 1.0,
                    .extra = 0.5,
                    .seed = 1}}) {
        Params params = reference_params();
        params.hard_spots = spots;
        EXPECT_EQ(validate(params), ParamError::kHardSpots);
    }
}

}  // namespace
}  // namespace tp::track
