#include "tp/core/field/field.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <set>

#include "../rules/reference_class.hpp"
#include "../vehicle/reference_vehicle.hpp"

namespace tp::field {
namespace {

using rules::testing::ntto_standard_3500;

Puller reference() {
    return {.tractor = vehicle::testing::reference_params(),
            .driver = {.gear = 6, .select_gear_s = 0.5, .launch_s = 3.0, .clutch_slip_in_s = 3.0}};
}

constexpr Variation kVariation{.torque_scale = {.low = 0.95, .high = 1.03},
                               .mass_kg = {.low = 3300.0, .high = 3550.0},
                               .traction_limit = {.low = 0.90, .high = 0.95},
                               .lowest_gear = 5,
                               .highest_gear = 6,
                               .clutch_slip_in_s = {.low = 2.0, .high = 4.0}};

Field field_of(int count, std::uint64_t seed) {
    const auto field = generate(reference(), kVariation, ntto_standard_3500(), count, seed);
    if (!field) {
        std::abort();
    }
    return *field;
}

TEST(FieldParams, RejectsNonsense) {
    EXPECT_EQ(validate(reference(), kVariation, ntto_standard_3500(), 15), std::nullopt);
    Variation v = kVariation;
    v.mass_kg = {.low = 3500.0, .high = 3300.0};
    EXPECT_EQ(validate(reference(), v, ntto_standard_3500(), 15), ParamError::kRangeOutOfOrder);
    v = kVariation;
    v.highest_gear = 9;
    EXPECT_EQ(validate(reference(), v, ntto_standard_3500(), 15), ParamError::kGearsOutOfRange);
    EXPECT_EQ(validate(reference(), kVariation, ntto_standard_3500(), 0),
              ParamError::kCountOutOfRange);
    EXPECT_EQ(validate(reference(), kVariation, ntto_standard_3500(), event::kMaxPullers + 1),
              ParamError::kCountOutOfRange);
    Puller bad = reference();
    bad.driver.gear = 0;
    EXPECT_EQ(validate(bad, kVariation, ntto_standard_3500(), 15), ParamError::kReference);
    EXPECT_FALSE(generate(bad, kVariation, ntto_standard_3500(), 15, 1).has_value());
}

// Every puller may compete in the class, even where the ranges reach past
// its limits: power and weight are clamped to them.
TEST(Field, EveryPullerMayCompete) {
    const Field field = field_of(event::kMaxPullers, 7);
    EXPECT_EQ(field.count, event::kMaxPullers);
    double most_power_w = 0.0;
    for (int i = 0; i < field.count; ++i) {
        const Puller& p = field.pullers[static_cast<std::size_t>(i)];
        most_power_w = std::max(most_power_w, rules::pto_power_w(p.tractor));
        EXPECT_TRUE(rules::check(ntto_standard_3500(), p.tractor).ok()) << i;
        EXPECT_EQ(vehicle::validate(p.tractor), std::nullopt) << i;
        EXPECT_GE(p.driver.gear, 5);
        EXPECT_LE(p.driver.gear, 6);
        EXPECT_GE(p.driver.clutch_slip_in_s, 2.0);
        EXPECT_LT(p.driver.clutch_slip_in_s, 4.0);
        EXPECT_GE(p.tractor.rear_tire.traction.limit, 0.90);
        EXPECT_LT(p.tractor.rear_tire.traction.limit, 0.95);
    }
    // Some drew more torque than the class allows and were tuned down to
    // its limit.
    EXPECT_NEAR(most_power_w, ntto_standard_3500().max_pto_power_w,
                1e-6 * ntto_standard_3500().max_pto_power_w);
}

// The pullers differ, and the same seed gives the same field.
TEST(Field, PullersDifferAndTheSeedRepeats) {
    const Field field = field_of(15, 3);
    std::set<double> masses;
    std::set<int> gears;
    for (int i = 0; i < field.count; ++i) {
        masses.insert(field.pullers[static_cast<std::size_t>(i)].tractor.chassis.mass_kg);
        gears.insert(field.pullers[static_cast<std::size_t>(i)].driver.gear);
    }
    EXPECT_GT(masses.size(), 10U);
    EXPECT_EQ(gears.size(), 2U);

    const Field again = field_of(15, 3);
    const Field other = field_of(15, 4);
    for (int i = 0; i < field.count; ++i) {
        const auto& a = field.pullers[static_cast<std::size_t>(i)];
        const auto& b = again.pullers[static_cast<std::size_t>(i)];
        EXPECT_EQ(a.tractor.chassis.mass_kg, b.tractor.chassis.mass_kg);
        EXPECT_EQ(a.driver.clutch_slip_in_s, b.driver.clutch_slip_in_s);
    }
    EXPECT_NE(field.pullers[0].tractor.chassis.mass_kg, other.pullers[0].tractor.chassis.mass_kg);
}

// A heavier tractor keeps its balance: the pitch inertia scales with the
// mass and the centre of gravity stays.
TEST(Field, MassKeepsTheBalance) {
    const Field field = field_of(5, 11);
    const vehicle::Params& base = reference().tractor;
    for (int i = 0; i < field.count; ++i) {
        const chassis::Params& c = field.pullers[static_cast<std::size_t>(i)].tractor.chassis;
        EXPECT_NEAR(c.pitch_inertia_kg_m2 / c.mass_kg,
                    base.chassis.pitch_inertia_kg_m2 / base.chassis.mass_kg, 1e-12);
        EXPECT_EQ(c.cg_ahead_of_rear_axle_m, base.chassis.cg_ahead_of_rear_axle_m);
    }
}

}  // namespace
}  // namespace tp::field
