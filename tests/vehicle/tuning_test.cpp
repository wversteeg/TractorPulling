#include "tp/core/vehicle/tuning.hpp"

#include <gtest/gtest.h>

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
    const Params set = tuned(params, {.ballast_position = 0.0, .rear_tire_pressure_bar = 0.7});
    EXPECT_GT(set.chassis.cg_ahead_of_rear_axle_m, 0.0);
    EXPECT_LT(set.chassis.cg_ahead_of_rear_axle_m, params.chassis.cg_ahead_of_rear_axle_m);
    EXPECT_GT(set.rear_tire.size.deflection_m, params.rear_tire.size.deflection_m);
    EXPECT_EQ(set.front_tire.deflection_m, params.front_tire.deflection_m);
    EXPECT_EQ(validate(set), std::nullopt);
    EXPECT_TRUE(Model::create(set).has_value());
}

}  // namespace
}  // namespace tp::vehicle
