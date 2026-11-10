#include "tp/core/vehicle/tuning.hpp"

#include <gtest/gtest.h>

#include <cmath>

#include "reference_vehicle.hpp"

namespace tp::vehicle {
namespace {

using testing::reference_params;

// As built, the tuned tractor is the tractor, bit for bit: an untuned
// puller's runs keep their hashes.
TEST(Tuning, AsBuiltIsTheTractor) {
    const Params params = reference_params();
    const Tuning built = as_built(params);
    EXPECT_EQ(built.ballast_position, params.chassis.ballast.position);
    EXPECT_EQ(built.rear_tire_pressure_bar, params.rear_tire.pressure.reference_bar);
    const Params same = tuned(params, built);
    EXPECT_EQ(same.chassis.cg_ahead_of_rear_axle_m, params.chassis.cg_ahead_of_rear_axle_m);
    EXPECT_EQ(same.chassis.pitch_inertia_kg_m2, params.chassis.pitch_inertia_kg_m2);
    EXPECT_EQ(same.rear_tire.size.deflection_m, params.rear_tire.size.deflection_m);
    EXPECT_EQ(same.rear_tire.traction.limit, params.rear_tire.traction.limit);
}

// Both settings reach the tractor, and only the rear tires' pressure
// changes: the front tires keep theirs. The tuned tractor is valid.
TEST(Tuning, SetsTheBallastAndTheRearTires) {
    const Params params = reference_params();
    const Params set = tuned(
        params, {.ballast_position = 0.0, .rear_tire_pressure_bar = 0.7, .hitch_height_m = 0.0});
    EXPECT_GT(set.chassis.cg_ahead_of_rear_axle_m, 0.0);
    EXPECT_LT(set.chassis.cg_ahead_of_rear_axle_m, params.chassis.cg_ahead_of_rear_axle_m);
    EXPECT_GT(set.rear_tire.size.deflection_m, params.rear_tire.size.deflection_m);
    EXPECT_EQ(set.front_tire.deflection_m, params.front_tire.deflection_m);
    EXPECT_EQ(validate(set), std::nullopt);
    EXPECT_TRUE(Model::create(set).has_value());
}

// An adjustable hitch is set within its holes, the lowest to the top one
// the tractor is built with; 0 or nonsense keeps the top hole, and a fixed
// hitch stays where it is.
TEST(Tuning, SetsTheHitchWithinItsHoles) {
    Params params = reference_params();
    const double top_m = params.chassis.hitch_height_m;
    const auto hitch_m = [&](double wanted_m) {
        Tuning tuning = as_built(params);
        tuning.hitch_height_m = wanted_m;
        return tuned(params, tuning).chassis.hitch_height_m;
    };
    EXPECT_EQ(hitch_m(top_m - 0.05), top_m) << "a fixed hitch";
    params.chassis.hitch_min_height_m = top_m - 0.1;
    EXPECT_EQ(as_built(params).hitch_height_m, top_m);
    EXPECT_EQ(hitch_m(top_m - 0.05), top_m - 0.05);
    EXPECT_EQ(hitch_m(top_m - 0.3), top_m - 0.1);
    EXPECT_EQ(hitch_m(top_m + 0.5), top_m);
    EXPECT_EQ(hitch_m(0.0), top_m);
    EXPECT_EQ(hitch_m(std::nan("")), top_m);
    Tuning low = as_built(params);
    low.hitch_height_m = top_m - 0.1;
    EXPECT_EQ(validate(tuned(params, low)), std::nullopt);
}

// A hitch set in the top hole is kept as the top hole (0), so a higher
// drawbar bought later raises it; one set lower stays where it was set.
TEST(Tuning, AHitchInTheTopHoleFollowsAHigherDrawbar) {
    Params stock = reference_params();
    stock.chassis.hitch_min_height_m = 0.30;
    stock.chassis.hitch_height_m = 0.40;
    Params raised = stock;
    raised.chassis.hitch_height_m = 0.50;
    Tuning top = as_built(stock);
    top.hitch_height_m = hitch_setting(stock, 0.40);
    EXPECT_EQ(top.hitch_height_m, 0.0);
    EXPECT_EQ(tuned(raised, top).chassis.hitch_height_m, 0.50);
    Tuning low = as_built(stock);
    low.hitch_height_m = hitch_setting(stock, 0.34);
    EXPECT_EQ(low.hitch_height_m, 0.34);
    EXPECT_EQ(tuned(raised, low).chassis.hitch_height_m, 0.34);
    EXPECT_EQ(hitch_setting(stock, 0.0), 0.0);
    EXPECT_EQ(hitch_setting(stock, -1.0), 0.0);
    EXPECT_EQ(hitch_setting(stock, std::nan("")), 0.0);
}

}  // namespace
}  // namespace tp::vehicle
