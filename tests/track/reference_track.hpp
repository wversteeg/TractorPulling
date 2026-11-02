#pragma once

#include "tp/core/track/track.hpp"

#include "../tire/reference_tire.hpp"

namespace tp::track::testing {

// The reference track: one moist, rolled clay and sand soil throughout.
inline Params reference_params() {
    return Params{.soil = tire::testing::reference_soil(),
                  .moisture_along_m = std::nullopt,
                  .compaction_along_m = std::nullopt};
}

}  // namespace tp::track::testing
