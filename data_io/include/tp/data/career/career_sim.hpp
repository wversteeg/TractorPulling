#pragma once

#include "tp/core/garage/garage.hpp"
#include "tp/core/season/season.hpp"
#include "tp/data/season/season_loader.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace tp::data {

/// One season of a simulated career at one level.
struct SimSeason {
    int events = 0;
    /// The player's place at each event, 1 the winner.
    std::array<int, season::kMaxEvents> places{};
    int wins = 0;
    /// The player's place in the championship at the season's end.
    int championship_place = 0;
    /// The money left, and the share of the class garage's money spent on
    /// it (upgrades and alternatives) at the season's end.
    std::int64_t money_eur = 0;
    double garage_share = 0.0;
};

/// `seasons` seasons at the level of `definition` as a sensible player
/// pulls them, from `career` (which they change: the money and the garage
/// carry on to the next level). The player buys the class's cheapest model
/// if they have no tractor yet. Before each event they fit, of the
/// alternatives they own, those that take the tractor furthest on the day's
/// sled and track, and choose their gear and hand throttle there, as the
/// rivals do, for every pull of the event; after it they
/// spend the prize money on the cheapest next upgrade or alternative while
/// they can afford one. Season `s` draws its field from `seed` + `s`.
/// Nothing if a season, an event or the tractor is refused. Runs whole
/// seasons of pulls: for tools and tests, not a frame.
[[nodiscard]] std::optional<std::vector<SimSeason>> simulate_level(
    const SeasonDefinition& definition, garage::Career& career, int seasons, std::uint64_t seed);

/// The share of the class garage's money (`catalogue`: every level of every
/// upgrade and every alternative) that `career` has spent, 0 to 1.
[[nodiscard]] double garage_share(const garage::Catalogue& catalogue,
                                  const garage::Career& career) noexcept;

}  // namespace tp::data
