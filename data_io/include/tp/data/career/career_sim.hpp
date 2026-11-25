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
    /// The money left at the season's end.
    std::int64_t money_eur = 0;
    /// How far the tractor as fitted is built at the season's end
    /// (garage::build_share): of its own route, not of every part the
    /// garage sells.
    double garage_share = 0.0;
};

/// `seasons` seasons at the level of `definition` as a sensible player
/// pulls them, from `career` (which they change: the money and the garage
/// carry on to the next level). The player buys the class's cheapest model
/// if they have no tractor yet. Before each event they fit, of the
/// alternatives they own, those that take the tractor furthest on the day's
/// sled and track, and choose their gear and hand throttle there, as the
/// rivals do, for every pull of the event; after it they buy as the crew
/// chief advises (field::good_buy) for the next event's track, saving
/// where it says so; with `plan_ahead`, planning ahead before each
/// season's first event. Season `s` draws its field from `seed` + `s`.
/// Nothing if a season, an event or the tractor is refused. Runs whole
/// seasons of pulls: for tools and tests, not a frame.
[[nodiscard]] std::optional<std::vector<SimSeason>> simulate_level(
    const SeasonDefinition& definition, garage::Career& career, int seasons, std::uint64_t seed,
    bool plan_ahead = false);

}  // namespace tp::data
