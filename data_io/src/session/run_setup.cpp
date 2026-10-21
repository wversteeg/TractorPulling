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

}  // namespace tp::data
