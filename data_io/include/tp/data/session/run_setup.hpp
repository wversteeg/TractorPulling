#pragma once

#include "tp/core/session/session.hpp"
#include "tp/core/sled/sled.hpp"
#include "tp/core/vehicle/vehicle.hpp"
#include "tp/data/track/track_loader.hpp"

namespace tp::data {

/// A run of `tractor` on `track` with the class's sled as the sled operator
/// sets it there: the track's conditions (the day's push-down), made
/// heavier one step at a time for each of `pull_offs` (sled::heavier; none
/// if it is 0 or less), on the track's ground and by its rules. The pro
/// layout; a front end sets the easy layout itself. The one place a
/// session's run is built from, so the game and tp_pull pull the same run.
[[nodiscard]] session::Setup run_setup(const vehicle::Params& tractor,
                                       const sled::Params& class_sled, const Track& track,
                                       int pull_offs = 0);

}  // namespace tp::data
