#pragma once

#include "tp/core/controls/controls.hpp"

namespace tp::controls::testing {

// Mirrors data/controls/easy.toml.
inline constexpr EasyParams kReferenceEasy{.steer_share = 0.6,
                                           .clutch_assist = {.engage_time_s = 2.0,
                                                             .release_time_s = 0.3,
                                                             .guard_speed_share = 0.6,
                                                             .guard_band_share = 0.1,
                                                             .min_throttle = 0.05}};

}  // namespace tp::controls::testing
