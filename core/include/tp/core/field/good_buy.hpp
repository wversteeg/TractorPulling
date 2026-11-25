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
    /// The pull-off steps the sled it was weighed on is heavier than the
    /// day's.
    int steps = 0;
    /// A planned switch (garage::kNoChoice for none): the first piece of
    /// alternative `route_alternative` of choice `route_choice` with its own
    /// build, which fully built pulls `gain_m` further than the fitted one
    /// fully built.
    int route_choice = garage::kNoChoice;
    int route_alternative = 0;
};

/// The good buy is weighed on the day's sled made at least this many
/// pull-off steps heavier, and this many more at a time while the tractor
/// as it is still full-pulls it: on the sled as it is a strong tractor
/// full-pulls, and a full pull shows no difference between parts.
inline constexpr int kGoodBuySteps = 4;

/// The most steps heavier the good buy's sled is made.
inline constexpr int kMostGoodBuySteps = 40;

/// A buy that gains less than this is no gain: the trial pulls' gears and
/// throttles round the distances that much [m].
inline constexpr double kLeastGain_m = 0.1;

/// The crew chief advises saving for a buy the career cannot afford yet
/// when it gains this many times as much per euro as the best it can.
inline constexpr double kSaveForGainRatio = 2.0;

/// What the crew chief advises buying next for `career`'s tractor
/// (`reference` equipped from `catalogue`): every next upgrade level and
/// every alternative not owned, each tried on `sled` made kGoodBuySteps
/// heavier (or more, up to kMostGoodBuySteps, until the tractor as it is
/// stops short of `rules`' full-pull line) and `track` at the hand throttle and in the gear (and
/// the gears either side of it, within `lowest` to `highest`) the tractor as it is pulls furthest
/// in (best_gear_trial), and weighed by the metres it gains per euro. An alternative is tried
/// fitted, with the alternative made for it (garage::Alternative::suits_choice) fitted too where
/// the career owns one; an upgrade of an alternative not fitted is not tried alone (it does not
/// count). With `plan_ahead` (a new level, a new season), a planned switch comes first: an
/// alternative with a build of its own (an engine) that fully built pulls further than the fitted
/// one fully built, though weaker as it comes; its first piece (the alternative, or the cheapest
/// next level of its build), to save for if it cannot be afforded. Nothing when nothing gains
/// kLeastGain_m, everything is bought, or the tractor, sled, track or rules are refused. Runs
/// three pulls for every candidate: for the garage and tools, not inside a frame.
[[nodiscard]] std::optional<GoodBuy> good_buy(const Puller& reference,
                                              const garage::Catalogue& catalogue,
                                              const garage::Career& career,
                                              const sled::Params& sled, const track::Params& track,
                                              int lowest, int highest, const run::Rules& rules,
                                              bool plan_ahead = false) noexcept;

/// `career` with `buy` made (paid, and an alternative fitted); the career
/// unchanged if it cannot be made.
[[nodiscard]] garage::Career with_buy(const garage::Catalogue& catalogue,
                                      const garage::Career& career, const Buy& buy) noexcept;

}  // namespace tp::field
