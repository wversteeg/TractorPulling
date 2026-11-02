#pragma once

#include "tp/core/field/live.hpp"
#include "tp/core/field/play.hpp"
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

/// Where a class's event on `track` is pulled: the class sled with the
/// track's conditions, on its ground and by its rules.
[[nodiscard]] field::Venue venue(const sled::Params& class_sled, const Track& track);

/// The player's next pull at a live event, in `tractor`: the event's sled as
/// the operator has set it, on the event's ground, by its rules (the run
/// goes on past the full-pull line until the sled stops). The pro layout,
/// as run_setup above.
[[nodiscard]] session::Setup run_setup(const vehicle::Params& tractor,
                                       const field::LiveEvent& event);

}  // namespace tp::data
