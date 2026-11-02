#include "tp/core/track/track.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstdlib>
#include <span>

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

}  // namespace
}  // namespace tp::track
