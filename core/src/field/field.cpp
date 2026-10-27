#include "tp/core/field/field.hpp"

#include "tp/core/math/curve.hpp"
#include "tp/core/math/random.hpp"
#include "tp/core/pull/pull.hpp"
#include "tp/core/run/run.hpp"
#include "tp/core/time/fixed_step_clock.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace tp::field {

namespace {

bool range_valid(const Range& range) noexcept {
    return std::isfinite(range.low) && std::isfinite(range.high) && range.low <= range.high;
}

// The tractor with its engine's torque scaled by `scale`, or by the largest
// scale the class's PTO power limit allows if that is less. PTO power is net
// of the engine's friction, so it does not scale in proportion: the limit is
// found by bisection.
std::optional<vehicle::Params> with_torque_scale(const vehicle::Params& tractor, double scale,
                                                 const rules::ClassRules& class_rules) noexcept {
    const auto power_w = [&](double s) {
        vehicle::Params t = tractor;
        const auto engine = engine::with_torque_scale(tractor.engine, s);
        if (!engine) {
            return class_rules.max_pto_power_w * 2.0;
        }
        t.engine = *engine;
        return rules::pto_power_w(t);
    };
    double allowed = scale;
    if (power_w(scale) > class_rules.max_pto_power_w) {
        constexpr int kBisections = 60;
        double low = 0.0;
        double high = scale;
        for (int i = 0; i < kBisections; ++i) {
            const double mid = 0.5 * (low + high);
            (power_w(mid) <= class_rules.max_pto_power_w ? low : high) = mid;
        }
        allowed = low;
    }
    const auto engine = engine::with_torque_scale(tractor.engine, allowed);
    if (!engine) {
        return std::nullopt;
    }
    vehicle::Params out = tractor;
    out.engine = *engine;
    return out;
}

// The reference with one draw of every varied value, within the class.
std::optional<Puller> draw(const Puller& reference, const Variation& variation,
                           const rules::ClassRules& class_rules,
                           math::SplitMix64& random) noexcept {
    const vehicle::Params& base = reference.tractor;
    // Draw every value first, so that a clamp never changes the sequence.
    const double torque_scale =
        random.uniform(variation.torque_scale.low, variation.torque_scale.high);
    const double mass_kg = random.uniform(variation.mass_kg.low, variation.mass_kg.high);
    const double traction_limit =
        random.uniform(variation.traction_limit.low, variation.traction_limit.high);
    const double slip_in_s =
        random.uniform(variation.clutch_slip_in_s.low, variation.clutch_slip_in_s.high);
    const double track_reading =
        random.uniform(variation.track_reading.low, variation.track_reading.high);
    const double downshift_share =
        random.uniform(variation.downshift_share.low, variation.downshift_share.high);

    // An engine within the class's PTO power, and a tractor within its
    // weight. The pitch and yaw inertias go with the mass; the balance stays.
    const auto tractor = with_torque_scale(base, torque_scale, class_rules);
    if (!tractor) {
        return std::nullopt;
    }
    Puller puller = reference;
    puller.tractor = *tractor;
    const double mass = std::min(mass_kg, class_rules.max_mass_kg);
    puller.tractor.chassis.pitch_inertia_kg_m2 *= mass / base.chassis.mass_kg;
    puller.tractor.chassis.yaw_inertia_kg_m2 *= mass / base.chassis.mass_kg;
    puller.tractor.chassis.mass_kg = mass;
    puller.tractor.rear_tire.traction.limit = traction_limit;
    puller.driver.clutch_slip_in_s = slip_in_s;
    puller.track_reading = track_reading;
    puller.driver.downshift_share = downshift_share;
    if (vehicle::validate(puller.tractor) || !rules::check(class_rules, puller.tractor).ok()) {
        return std::nullopt;
    }
    return puller;
}

template <std::size_t... I>
std::array<Puller, sizeof...(I)> copies(const Puller& puller,
                                        std::index_sequence<I...> /*indices*/) noexcept {
    return {((void)I, puller)...};
}

}  // namespace

std::optional<ParamError> validate(const Puller& reference, const Variation& variation,
                                   const rules::ClassRules& class_rules, int count) noexcept {
    const std::array ranges = {variation.torque_scale,   variation.mass_kg,
                               variation.traction_limit, variation.clutch_slip_in_s,
                               variation.track_reading,  variation.downshift_share};
    if (!std::ranges::all_of(
            ranges, [](const Range& r) { return std::isfinite(r.low) && std::isfinite(r.high); })) {
        return ParamError::kNotFinite;
    }
    if (!std::ranges::all_of(ranges, range_valid) || variation.torque_scale.low <= 0.0 ||
        variation.mass_kg.low <= 0.0 || variation.clutch_slip_in_s.low < 0.0 ||
        variation.track_reading.low < 0.0 || variation.track_reading.high > 1.0 ||
        variation.downshift_share.low < 0.0 || variation.downshift_share.high >= 1.0) {
        return ParamError::kRangeOutOfOrder;
    }
    const auto gear_count = static_cast<int>(reference.tractor.driveline.gear_count);
    if (variation.lowest_gear < 1 || variation.lowest_gear > variation.highest_gear ||
        variation.highest_gear > gear_count) {
        return ParamError::kGearsOutOfRange;
    }
    if (count < 1 || count > event::kMaxPullers) {
        return ParamError::kCountOutOfRange;
    }
    if (vehicle::validate(reference.tractor) || rules::validate(class_rules) ||
        driver::validate(reference.driver, gear_count)) {
        return ParamError::kReference;
    }
    return std::nullopt;
}

std::optional<Field> generate(const Puller& reference, const Variation& variation,
                              const rules::ClassRules& class_rules, int count,
                              std::uint64_t seed) noexcept {
    if (validate(reference, variation, class_rules, count)) {
        return std::nullopt;
    }
    Field field{.pullers = copies(reference, std::make_index_sequence<event::kMaxPullers>{}),
                .count = 0};
    math::SplitMix64 random{seed};
    for (int i = 0; i < count; ++i) {
        const std::optional<Puller> puller = draw(reference, variation, class_rules, random);
        if (!puller) {
            return std::nullopt;
        }
        field.pullers[static_cast<std::size_t>(i)] = *puller;
    }
    field.count = count;
    return field;
}

}  // namespace tp::field

namespace tp::field {

std::optional<run::Result> run_pull(const Puller& puller, const sled::Params& sled,
                                    const track::Params& track, const run::Rules& rules) noexcept {
    constexpr double kDt_s =
        static_cast<double>(kSimulationStepNs) / static_cast<double>(kNanosecondsPerSecond);
    const auto model =
        pull::Model::create({.tractor = puller.tractor, .sled = sled, .track = track});
    if (!model || run::validate(rules)) {
        return std::nullopt;
    }
    run::State state = run::start(*model);
    driver::State driver;
    while (!state.result) {
        const double t_s = static_cast<double>(state.steps) * kDt_s;
        run::step(state, *model, rules,
                  driver::input(puller.driver, driver, t_s, state.pull.tractor,
                                model->tractor().params()),
                  kDt_s);
    }
    return state.result;
}

}  // namespace tp::field

namespace tp::field {

std::optional<int> best_gear(const Puller& puller, const sled::Params& sled,
                             const track::Params& track, int lowest, int highest,
                             const run::Rules& rules) noexcept {
    std::optional<int> best;
    double best_m = -1.0;
    for (int gear = lowest; gear <= highest; ++gear) {
        Puller trying = puller;
        trying.driver.gear = gear;
        const std::optional<run::Result> result = run_pull(trying, sled, track, rules);
        if (!result) {
            return std::nullopt;
        }
        // On a tie the higher gear: it gets there quicker.
        if (result->distance_m >= best_m) {
            best_m = result->distance_m;
            best = gear;
        }
    }
    return best;
}

int guess_gear(const Puller& puller, int best, int lowest, int highest,
               math::SplitMix64& random) noexcept {
    const bool reads_it = random.uniform() < puller.track_reading;
    const bool higher = random.uniform() < 0.5;
    if (reads_it || lowest == highest) {
        return best;
    }
    if (best == lowest) {
        return best + 1;
    }
    if (best == highest) {
        return best - 1;
    }
    return higher ? best + 1 : best - 1;
}

Puller on_this_pull(const Puller& puller, const PullToPull& variation,
                    math::SplitMix64& random) noexcept {
    Puller out = puller;
    out.tractor.rear_tire.traction.limit *=
        1.0 + random.uniform(-variation.grip_share, variation.grip_share);
    out.driver.clutch_slip_in_s =
        std::max(puller.driver.clutch_slip_in_s +
                     random.uniform(-variation.clutch_slip_in_s, variation.clutch_slip_in_s),
                 0.0);
    return out;
}

}  // namespace tp::field
