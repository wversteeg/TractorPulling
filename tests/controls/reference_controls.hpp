#pragma once

#include "tp/core/controls/controls.hpp"

namespace tp::controls::testing {

// Mirrors data/controls/easy.toml.
inline constexpr EasyParams kReferenceEasy{.steer_share = 0.6,
                                           .steer_dead_zone = 0.03,
                                           .steer_rate_per_s = 1.5,
                                           .brake_off_the_gas = 1.0,
                                           .clutch_assist = {.engage_time_s = 2.0,
                                                             .release_time_s = 0.3,
                                                             .guard_speed_share = 0.6,
                                                             .guard_band_share = 0.1,
                                                             .min_throttle = 0.05}};

// Mirrors data/controls/pro.toml.
inline constexpr ProParams kReferencePro{.steer_dead_zone = 0.03, .steer_rate_per_s = 1.5};

}  // namespace tp::controls::testing
