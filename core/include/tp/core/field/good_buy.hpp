#pragma once

#include "tp/core/field/field.hpp"
#include "tp/core/garage/garage.hpp"
#include "tp/core/run/run.hpp"
#include "tp/core/sled/sled.hpp"
#include "tp/core/track/track.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>

namespace tp::field {

/// What a buy is: the next level of an upgrade, or an alternative of a
/// choice (by the catalogue's order).
enum class BuyKind : std::uint8_t {
    kUpgrade,
    kAlternative,
};

struct Buy {
    BuyKind kind = BuyKind::kUpgrade;
    /// The upgrade, or the choice.
    std::size_t index = 0;
    /// The choice's alternative (kAlternative only).
    std::size_t alternative = 0;
    std::int64_t cost_eur = 0;
};

/// The crew chief's good buy for the garage as it is now.
struct GoodBuy {
    Buy buy;
    /// How much further the tractor pulls with it, on the sled and track it
    /// was weighed on [m].
    double gain_m = 0.0;
    /// The career cannot afford it yet: save for it.
    bool save_for = false;
};

/// The good buy is weighed on the day's sled made this many pull-off steps
/// heavier: on the sled as it is a strong tractor full-pulls, and a full
/// pull shows no difference between parts.
inline constexpr int kGoodBuySteps = 4;

/// A buy that gains less than this is no gain: the trial pulls' gears and
/// throttles round the distances that much [m].
inline constexpr double kLeastGain_m = 0.1;

/// The crew chief advises saving for a buy the career cannot afford yet
/// when it gains this many times as much per euro as the best it can.
inline constexpr double kSaveForGainRatio = 2.0;

/// What the crew chief advises buying next for `career`'s tractor
/// (`reference` equipped from `catalogue`): every next upgrade level and
/// every alternative not owned, each tried on `sled` made kGoodBuySteps
/// heavier and `track` at the hand throttle and in the gear (and the gears
/// either side of it, within `lowest` to `highest`) the tractor as it is
/// pulls furthest in (best_gear_trial), and weighed by the metres it gains
/// per euro. An
/// alternative is tried fitted, with the alternative made for it
/// (garage::Alternative::suits_choice) fitted too where the career owns one;
/// an upgrade of an alternative not fitted is not tried (it does not count).
/// The best the career can afford; or, to save for, one it cannot yet
/// afford that gains kSaveForGainRatio times as much per euro, or the best
/// of all when it can afford none. Nothing when nothing gains
/// kLeastGain_m, everything is bought, or the tractor, sled, track or rules
/// are refused. Runs three pulls for every candidate: for the garage and
/// tools, not inside a frame.
[[nodiscard]] std::optional<GoodBuy> good_buy(const Puller& reference,
                                              const garage::Catalogue& catalogue,
                                              const garage::Career& career,
                                              const sled::Params& sled, const track::Params& track,
                                              int lowest, int highest,
                                              const run::Rules& rules) noexcept;

/// `career` with `buy` made (paid, and an alternative fitted); the career
/// unchanged if it cannot be made.
[[nodiscard]] garage::Career with_buy(const garage::Catalogue& catalogue,
                                      const garage::Career& career, const Buy& buy) noexcept;

}  // namespace tp::field
