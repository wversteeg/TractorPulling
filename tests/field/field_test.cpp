#include "tp/core/field/field.hpp"

#include "tp/core/field/play.hpp"
#include "tp/core/rules/class_rules.hpp"
#include "tp/core/units/conversions.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <set>
#include <span>

#include "../rules/reference_class.hpp"
#include "../sled/reference_sled.hpp"
#include "../track/reference_track.hpp"
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
                               .clutch_slip_in_s = {.low = 2.0, .high = 4.0},
                               .track_reading = {.low = 0.5, .high = 0.95},
                               .downshift_share = {.low = 0.85, .high = 0.95},
                               .hitch_height_m = {},
                               .fewest_load_downshift_gears = -1,
                               .most_load_downshift_gears = -1,
                               .rated_omega_rad_s = {}};

Field field_of(int count, std::uint64_t seed, const Variation& variation = kVariation) {
    const auto field = generate(reference(), variation, ntto_standard_3500(), count, seed);
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
    v = kVariation;
    v.hitch_height_m = {.low = -0.1, .high = 0.5};
    EXPECT_EQ(validate(reference(), v, ntto_standard_3500(), 15), ParamError::kRangeOutOfOrder);
    v.hitch_height_m = {.low = 0.0, .high = 0.5};
    EXPECT_EQ(validate(reference(), v, ntto_standard_3500(), 15), ParamError::kRangeOutOfOrder)
        << "a drawbar on the ground";
    v = kVariation;
    v.rated_omega_rad_s = {.low = 0.0, .high = 250.0};
    EXPECT_EQ(validate(reference(), v, ntto_standard_3500(), 15), ParamError::kRangeOutOfOrder)
        << "a governor at standstill";
    v = kVariation;
    v.fewest_load_downshift_gears = 1;
    v.most_load_downshift_gears = 0;
    EXPECT_EQ(validate(reference(), v, ntto_standard_3500(), 15),
              ParamError::kLoadDownshiftsOutOfRange);
    v.fewest_load_downshift_gears = -1;
    v.most_load_downshift_gears = 1;
    EXPECT_EQ(validate(reference(), v, ntto_standard_3500(), 15),
              ParamError::kLoadDownshiftsOutOfRange)
        << "both kept or both drawn";
    v.fewest_load_downshift_gears = 0;
    v.most_load_downshift_gears = static_cast<int>(reference().tractor.driveline.gear_count);
    EXPECT_EQ(validate(reference(), v, ntto_standard_3500(), 15),
              ParamError::kLoadDownshiftsOutOfRange);
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
        EXPECT_GE(p.track_reading, 0.5);
        EXPECT_LT(p.track_reading, 0.95);
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
    std::set<double> readings;
    for (int i = 0; i < field.count; ++i) {
        masses.insert(field.pullers[static_cast<std::size_t>(i)].tractor.chassis.mass_kg);
        readings.insert(field.pullers[static_cast<std::size_t>(i)].track_reading);
    }
    EXPECT_GT(masses.size(), 10U);
    EXPECT_EQ(readings.size(), static_cast<std::size_t>(field.count));

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

// Left out, the drawbar and the powershift are the reference's; given,
// each puller draws them, the drawbar held to the class's limit.
TEST(Field, DrawbarAndPowershiftVary) {
    const Field kept = field_of(10, 5);
    const vehicle::Params& base = reference().tractor;
    for (int i = 0; i < kept.count; ++i) {
        const vehicle::Params& t = kept.pullers[static_cast<std::size_t>(i)].tractor;
        EXPECT_EQ(t.chassis.hitch_height_m, base.chassis.hitch_height_m);
        EXPECT_EQ(t.driveline.load_downshift_gears, base.driveline.load_downshift_gears);
    }
    Variation v = kVariation;
    v.hitch_height_m = {.low = 0.36, .high = 0.6};
    v.fewest_load_downshift_gears = 0;
    v.most_load_downshift_gears = 1;
    const Field field = field_of(event::kMaxPullers, 5, v);
    std::set<int> counts;
    bool at_the_limit = false;
    for (int i = 0; i < field.count; ++i) {
        const vehicle::Params& t = field.pullers[static_cast<std::size_t>(i)].tractor;
        EXPECT_GE(t.chassis.hitch_height_m, 0.36) << i;
        EXPECT_LE(t.chassis.hitch_height_m, ntto_standard_3500().max_hitch_height_m) << i;
        at_the_limit =
            at_the_limit || t.chassis.hitch_height_m == ntto_standard_3500().max_hitch_height_m;
        counts.insert(t.driveline.load_downshift_gears);
        EXPECT_TRUE(rules::check(ntto_standard_3500(), t).ok()) << i;
    }
    EXPECT_TRUE(at_the_limit) << "a draw past the class's drawbar limit is held to it";
    EXPECT_EQ(counts, (std::set<int>{0, 1}));
}

// Given, each puller draws a governor, held to the class's unloaded engine
// speed; left out, every one has the reference's.
TEST(Field, GovernorsVary) {
    const vehicle::Params& base = reference().tractor;
    const Field kept = field_of(10, 5);
    for (int i = 0; i < kept.count; ++i) {
        EXPECT_EQ(kept.pullers[static_cast<std::size_t>(i)].tractor.engine.rated_omega_rad_s,
                  base.engine.rated_omega_rad_s);
    }
    Variation v = kVariation;
    v.rated_omega_rad_s = {.low = base.engine.rated_omega_rad_s,
                           .high = units::rpm_to_rad_s(2700.0)};
    const Field field = field_of(event::kMaxPullers, 5, v);
    bool raised = false;
    bool at_the_limit = false;
    for (int i = 0; i < field.count; ++i) {
        const engine::Params& e = field.pullers[static_cast<std::size_t>(i)].tractor.engine;
        EXPECT_GE(e.rated_omega_rad_s, base.engine.rated_omega_rad_s) << i;
        EXPECT_LE(e.high_idle_omega_rad_s, ntto_standard_3500().max_engine_speed_rad_s + 1e-9) << i;
        raised = raised || e.rated_omega_rad_s > base.engine.rated_omega_rad_s;
        at_the_limit = at_the_limit || std::abs(e.high_idle_omega_rad_s -
                                                ntto_standard_3500().max_engine_speed_rad_s) < 1e-9;
        EXPECT_TRUE(
            rules::check(ntto_standard_3500(), field.pullers[static_cast<std::size_t>(i)].tractor)
                .ok())
            << i;
    }
    EXPECT_TRUE(raised);
    EXPECT_TRUE(at_the_limit) << "a draw past the class's engine speed is held to it";
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

// An event's pull: on to 130 m, the chalk lines counting to 100 m.
constexpr run::Rules kRules{.track_length_m = 130.0,
                            .max_duration_s = 120.0,
                            .lane_width_m = 12.0,
                            .full_pull_line_m = 100.0};

// The best gear is the one that pulls furthest: no other gear in the range
// gets further.
TEST(Driver, BestGearPullsFurthest) {
    const Puller puller = reference();
    const sled::Params sled = sled::testing::reference_params();
    const track::Params track = track::testing::reference_params();
    const std::optional<int> best = best_gear(puller, sled, track, 4, 6, kRules);
    ASSERT_TRUE(best.has_value());
    Puller in_best = puller;
    in_best.driver.gear = best.value_or(0);
    const double best_m = run_pull(in_best, sled, track, kRules).value_or(run::Result{}).distance_m;
    for (int gear = 4; gear <= 6; ++gear) {
        Puller other = puller;
        other.driver.gear = gear;
        EXPECT_LE(run_pull(other, sled, track, kRules).value_or(run::Result{}).distance_m, best_m)
            << "gear " << gear;
    }
}

// A driver who always reads the track right picks the best gear; one who
// never does is one gear off, within the range.
TEST(Driver, GuessesAreTheBestGearOrOneOff) {
    Puller sharp = reference();
    sharp.track_reading = 1.0;
    Puller lost = reference();
    lost.track_reading = 0.0;
    math::SplitMix64 random{9};
    for (int i = 0; i < 100; ++i) {
        EXPECT_EQ(guess_gear(sharp, 5, 4, 6, random), 5);
        const int guess = guess_gear(lost, 5, 4, 6, random);
        EXPECT_TRUE(guess == 4 || guess == 6) << guess;
        EXPECT_EQ(guess_gear(lost, 4, 4, 6, random), 5);
        EXPECT_EQ(guess_gear(lost, 6, 4, 6, random), 5);
    }
}

// No two pulls are quite the same: grip and clutch timing vary within the
// set bounds, and not at all with zero bounds.
TEST(Driver, EveryPullDiffersALittle) {
    const Puller puller = reference();
    const PullToPull variation{.grip_share = 0.01, .clutch_slip_in_s = 0.3};
    math::SplitMix64 random{4};
    std::set<double> grips;
    for (int i = 0; i < 50; ++i) {
        const Puller today = on_this_pull(puller, variation, random);
        const double share =
            today.tractor.rear_tire.traction.limit / puller.tractor.rear_tire.traction.limit - 1.0;
        EXPECT_LE(std::abs(share), 0.01 + 1e-12);
        EXPECT_LE(std::abs(today.driver.clutch_slip_in_s - puller.driver.clutch_slip_in_s),
                  0.3 + 1e-12);
        grips.insert(today.tractor.rear_tire.traction.limit);
    }
    EXPECT_GT(grips.size(), 40U);
    const Puller same = on_this_pull(puller, {}, random);
    EXPECT_EQ(same.tractor.rear_tire.traction.limit, puller.tractor.rear_tire.traction.limit);
}

// The event's hash is the exact bits of every entry: a pull one ulp
// further, another decision or one entry fewer is another hash.
TEST(LogHash, IsTheExactBitsOfEveryEntry) {
    std::array<LogEntry, 2> log{};
    log[0].pull = {.puller = 4, .kind = event::PullKind::kTest, .pull_off_round = 0};
    log[0].reached_m = 92.5;
    log[0].full_pull_m = 100.0;
    log[1].deciding = 4;
    log[1].keep = true;
    const std::uint64_t base = log_hash(log);
    EXPECT_EQ(base, log_hash(log));

    std::array<LogEntry, 2> further = log;
    further[0].reached_m = std::nextafter(92.5, 100.0);
    EXPECT_NE(log_hash(further), base);
    std::array<LogEntry, 2> repull = log;
    repull[1].keep = false;
    EXPECT_NE(log_hash(repull), base);
    EXPECT_NE(log_hash(std::span{log}.first(1)), base);
}

}  // namespace
}  // namespace tp::field
