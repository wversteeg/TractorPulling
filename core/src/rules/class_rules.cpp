#include "tp/core/rules/class_rules.hpp"

#include "tp/core/engine/engine.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace tp::rules {

std::optional<ParamError> validate(const ClassRules& rules) noexcept {
    const std::array limits = {rules.max_mass_kg, rules.max_pto_power_w,
                               rules.max_engine_speed_rad_s, rules.max_hitch_height_m};
    if (!std::ranges::all_of(limits, [](double v) { return std::isfinite(v); })) {
        return ParamError::kNotFinite;
    }
    if (std::ranges::any_of(limits, [](double v) { return v <= 0.0; })) {
        return ParamError::kLimitNotPositive;
    }
    return std::nullopt;
}

double pto_power_w(const vehicle::Params& tractor) noexcept {
    return engine::max_net_power_w(tractor.engine) * tractor.pto_efficiency;
}

Compliance check(const ClassRules& rules, const vehicle::Params& tractor) noexcept {
    const chassis::Params& chassis = tractor.chassis;
    return {
        .too_heavy = chassis.mass_kg > rules.max_mass_kg,
        .too_much_pto_power = pto_power_w(tractor) > rules.max_pto_power_w,
        .engine_speed_too_high =
            tractor.engine.high_idle_omega_rad_s > rules.max_engine_speed_rad_s,
        .hitch_too_high = chassis.hitch_height_m > rules.max_hitch_height_m,
        .hitch_above_rear_axle = chassis.hitch_height_m > chassis.rear_axle_height_m,
    };
}

}  // namespace tp::rules
