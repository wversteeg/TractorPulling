#include "tp/data/session/run_setup.hpp"

namespace tp::data {

session::Setup run_setup(const vehicle::Params& tractor, const sled::Params& class_sled,
                         const Track& track, int pull_offs) {
    sled::Params sled = sled::with_conditions(class_sled, track.sled);
    // One step at a time, as the sled is made heavier between pull-offs:
    // heavier(sled, n) adds n steps at once, which rounds differently.
    for (int i = 0; i < pull_offs; ++i) {
        sled = sled::heavier(sled);
    }
    return {.tractor = tractor,
            .sled = sled,
            .ground = track.ground,
            .rules = track.rules,
            .layout = session::Layout::kPro,
            .easy = {},
            .pro = {}};
}

field::Venue venue(const sled::Params& class_sled, const Track& track) {
    return {.track = track.ground,
            .sled = sled::with_conditions(class_sled, track.sled),
            .max_duration_s = track.rules.max_duration_s,
            .lane_width_m = track.rules.lane_width_m};
}

session::Setup run_setup(const vehicle::Params& tractor, const field::LiveEvent& event) {
    return {.tractor = tractor,
            .sled = event.sled(),
            .ground = event.venue().track,
            .rules = event.pull_rules(),
            .layout = session::Layout::kPro,
            .easy = {},
            .pro = {}};
}

}  // namespace tp::data
