#pragma once

#include "tp/core/rules/class_rules.hpp"
#include "tp/core/units/conversions.hpp"

namespace tp::rules::testing {

// Mirrors data/classes/ntto_standard_3500.toml: the NTTO standard 3.5 t class.
inline ClassRules ntto_standard_3500() {
    return ClassRules{
        .max_mass_kg = 3500.0,
        .max_pto_power_w = units::metric_hp_to_w(85.0),
        .max_engine_speed_rad_s = units::rpm_to_rad_s(2700.0),
        .max_hitch_height_m = 0.5,
    };
}

}  // namespace tp::rules::testing
