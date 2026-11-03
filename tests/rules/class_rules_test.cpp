#include "tp/core/rules/class_rules.hpp"

#include "tp/core/engine/engine.hpp"
#include "tp/core/units/conversions.hpp"

#include <gtest/gtest.h>

#include <cmath>

#include "../vehicle/reference_vehicle.hpp"
#include "reference_class.hpp"

namespace tp::rules {
namespace {

using testing::ntto_standard_3500;
using vehicle::testing::reference_params;

TEST(ClassRulesParams, ReferenceIsValid) {
    EXPECT_EQ(validate(ntto_standard_3500()), std::nullopt);
}

TEST(ClassRulesParams, RejectsNonsense) {
    ClassRules rules = ntto_standard_3500();
    rules.max_mass_kg = 0.0;
    EXPECT_EQ(validate(rules), ParamError::kLimitNotPositive);
    rules = ntto_standard_3500();
    rules.max_hitch_height_m = -0.5;
    EXPECT_EQ(validate(rules), ParamError::kLimitNotPositive);
    rules = ntto_standard_3500();
    rules.max_pto_power_w = std::nan("");
    EXPECT_EQ(validate(rules), ParamError::kNotFinite);
    rules.max_pto_power_w = 0.0;
    EXPECT_EQ(validate(rules), ParamError::kLimitNotPositive);
}

// A class that limits the engine's build (Sport and up) sets no PTO power
// limit: any power passes, the other limits still hold.
TEST(ClassRules, NoPtoPowerLimitPassesAnyPower) {
    ClassRules rules = ntto_standard_3500();
    rules.max_pto_power_w.reset();
    EXPECT_FALSE(validate(rules));
    vehicle::Params tractor = reference_params();
    tractor.pto_efficiency = 1.0;
    tractor.engine = engine::with_torque_scale(tractor.engine, 3.0).value_or(tractor.engine);
    EXPECT_FALSE(check(rules, tractor).too_much_pto_power);
    EXPECT_TRUE(check(ntto_standard_3500(), tractor).too_much_pto_power);
}

// The reference light farm-stock tractor is built for this class.
TEST(ClassRules, ReferenceTractorCompetesInTheNttoStandardClass) {
    const Compliance compliance = check(ntto_standard_3500(), reference_params());
    EXPECT_TRUE(compliance.ok());
    EXPECT_LT(units::w_to_metric_hp(pto_power_w(reference_params())), 85.0);
}

TEST(ClassRules, PtoPowerIsEnginePeakAfterPtoLosses) {
    const vehicle::Params tractor = reference_params();
    EXPECT_DOUBLE_EQ(pto_power_w(tractor),
                     engine::max_net_power_w(tractor.engine) * tractor.pto_efficiency);
}

// Each rule catches its own violation, and only that one.
TEST(ClassRules, FlagsEachViolation) {
    const ClassRules rules = ntto_standard_3500();
    const auto check_with = [&](auto mutate) {
        vehicle::Params tractor = reference_params();
        mutate(tractor);
        return check(rules, tractor);
    };

    const Compliance heavy = check_with([](vehicle::Params& t) { t.chassis.mass_kg = 3600.0; });
    EXPECT_TRUE(heavy.too_heavy);
    EXPECT_FALSE(heavy.too_much_pto_power);

    const Compliance powerful = check_with([](vehicle::Params& t) { t.pto_efficiency = 1.0; });
    EXPECT_TRUE(powerful.too_much_pto_power) << "about 93 hp at a lossless PTO";
    EXPECT_FALSE(powerful.too_heavy);

    const Compliance revving = check_with(
        [](vehicle::Params& t) { t.engine.high_idle_omega_rad_s = units::rpm_to_rad_s(2800.0); });
    EXPECT_TRUE(revving.engine_speed_too_high);

    const Compliance high_hitch =
        check_with([](vehicle::Params& t) { t.chassis.hitch_height_m = 0.6; });
    EXPECT_TRUE(high_hitch.hitch_too_high);
    EXPECT_FALSE(high_hitch.hitch_above_rear_axle);

    // On small rear tires a 0.5 m hitch sits above the axle.
    const Compliance small_tires = check_with([](vehicle::Params& t) {
        t.rear_tire.size.radius_m = 0.45;
        t.chassis.rear_axle_height_m = 0.45;
    });
    EXPECT_TRUE(small_tires.hitch_above_rear_axle);
    EXPECT_FALSE(small_tires.hitch_too_high);
    EXPECT_FALSE(small_tires.ok());
}

// At the limit is allowed: the rules are maxima.
TEST(ClassRules, LimitsAreInclusive) {
    const vehicle::Params tractor = reference_params();
    const ClassRules rules{.max_mass_kg = tractor.chassis.mass_kg,
                           .max_pto_power_w = pto_power_w(tractor),
                           .max_engine_speed_rad_s = tractor.engine.high_idle_omega_rad_s,
                           .max_hitch_height_m = tractor.chassis.hitch_height_m};
    EXPECT_TRUE(check(rules, tractor).ok());
}

}  // namespace
}  // namespace tp::rules
