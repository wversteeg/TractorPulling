#include "tp/core/soil/soil.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

namespace tp::soil {
namespace {

Surface of(double clay, double moisture, double compaction) {
    return surface({.clay_fraction = clay, .moisture = moisture, .compaction = compaction});
}

TEST(SoilParams, RejectsNonsense) {
    EXPECT_EQ(validate({.clay_fraction = 0.5, .moisture = 0.15, .compaction = 0.8}), std::nullopt);
    EXPECT_EQ(validate({.clay_fraction = 1.2, .moisture = 0.15, .compaction = 0.8}),
              ParamError::kClayFractionOutOfRange);
    EXPECT_EQ(validate({.clay_fraction = 0.5, .moisture = -0.01, .compaction = 0.8}),
              ParamError::kMoistureOutOfRange);
    EXPECT_EQ(validate({.clay_fraction = 0.5, .moisture = 0.6, .compaction = 0.8}),
              ParamError::kMoistureOutOfRange);
    EXPECT_EQ(validate({.clay_fraction = 0.5, .moisture = 0.15, .compaction = 1.5}),
              ParamError::kCompactionOutOfRange);
    EXPECT_EQ(validate({.clay_fraction = std::nan(""), .moisture = 0.15, .compaction = 0.8}),
              ParamError::kNotFinite);
}

// Clay is hard when dry and goes soft as it takes up water.
TEST(Soil, ClayWeakensAsItGetsWetter) {
    double previous_pa = std::numeric_limits<double>::infinity();
    for (int percent = 4; percent <= 40; ++percent) {
        const double strength_pa = of(1.0, percent / 100.0, 0.8).cone_index_pa;
        ASSERT_LE(strength_pa, previous_pa) << percent << " %";
        previous_pa = strength_pa;
    }
    EXPECT_GT(of(1.0, 0.10, 0.8).cone_index_pa, 3.0 * of(1.0, 0.25, 0.8).cone_index_pa);
}

// Sand is strongest at a moderate moisture: dry sand runs, wet sand flows.
TEST(Soil, SandHasAnOptimumMoisture) {
    const double dry_pa = of(0.0, 0.01, 0.8).cone_index_pa;
    const double damp_pa = of(0.0, 0.08, 0.8).cone_index_pa;
    const double wet_pa = of(0.0, 0.25, 0.8).cone_index_pa;
    EXPECT_GT(damp_pa, 1.5 * dry_pa);
    EXPECT_GT(damp_pa, 1.5 * wet_pa);
}

// Rolling the track makes any mix stronger (up to the cap that very dry,
// rolled clay reaches).
TEST(Soil, CompactionStrengthens) {
    for (int clay = 0; clay <= 10; ++clay) {
        for (int percent = 4; percent <= 30; percent += 2) {
            double previous_pa = 0.0;
            for (int rolled = 0; rolled <= 10; ++rolled) {
                const double strength_pa =
                    of(clay / 10.0, percent / 100.0, rolled / 10.0).cone_index_pa;
                ASSERT_GE(strength_pa, previous_pa);
                previous_pa = strength_pa;
            }
        }
    }
}

// At a typical track moisture, sand is far weaker than clay.
TEST(Soil, SandIsWeakerThanClay) {
    EXPECT_LT(of(0.0, 0.14, 0.8).cone_index_pa, 0.5 * of(1.0, 0.14, 0.8).cone_index_pa);
}

// Clean moist track: full grip. Dry clay gets a dusty top, wet clay a slick
// one; both grip less.
TEST(Soil, SurfaceGripIsBestOnAMoistTrack) {
    EXPECT_EQ(of(0.6, 0.14, 0.8).grip_factor, 1.0);
    EXPECT_LT(of(0.6, 0.03, 0.8).grip_factor, 1.0);
    EXPECT_LT(of(0.6, 0.30, 0.8).grip_factor, of(0.6, 0.03, 0.8).grip_factor);
    EXPECT_EQ(of(0.0, 0.30, 0.8).grip_factor, 1.0) << "sand has no film";
}

// In wheelspin, tires smear wet clay and dig into sand: both lose more grip
// than on a moist clay track.
TEST(Soil, WheelspinCostsMoreOnWetClayAndSand) {
    const double moist_clay = of(1.0, 0.14, 0.8).spin_fall_off;
    EXPECT_GT(of(1.0, 0.30, 0.8).spin_fall_off, moist_clay);
    EXPECT_GT(of(0.0, 0.14, 0.8).spin_fall_off, moist_clay);
    EXPECT_LT(of(1.0, 0.40, 0.0).spin_fall_off, 1.0);
}

TEST(Soil, LooseTrackIsWeakerThanRolled) {
    for (int clay = 0; clay <= 10; ++clay) {
        EXPECT_GT(of(clay / 10.0, 0.14, 1.0).cone_index_pa,
                  1.5 * of(clay / 10.0, 0.14, 0.0).cone_index_pa);
    }
}

// Small changes in the soil give small changes in the surface.
TEST(Soil, SurfaceIsContinuous) {
    for (int clay = 0; clay <= 10; ++clay) {
        for (int step = 0; step < 400; ++step) {
            const double moisture = step / 1000.0;
            const Surface a = of(clay / 10.0, moisture, 0.7);
            const Surface b = of(clay / 10.0, moisture + 1e-6, 0.7);
            ASSERT_NEAR(a.cone_index_pa, b.cone_index_pa, 1e-3 * a.cone_index_pa) << moisture;
            ASSERT_NEAR(a.grip_factor, b.grip_factor, 1e-3);
            ASSERT_NEAR(a.spin_fall_off, b.spin_fall_off, 1e-3);
        }
    }
}

}  // namespace
}  // namespace tp::soil
