#include "tp/core/field/live_season.hpp"
#include "tp/core/garage/garage.hpp"
#include "tp/data/season/season_loader.hpp"
#include "tp/data/session/run_setup.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <stdexcept>

namespace tp::field {
namespace {

// The game's event (ten pullers) at the season's first round, with the
// drivers choosing between 5th and 6th only, so the debug build plays it
// quickly; a few seeds, each a different field.
constexpr std::array<std::uint64_t, 3> kSeeds{1, 2, 3};
constexpr int kGamePullers = 10;
constexpr int kLowestGear = 5;
constexpr std::int64_t kSlice = 1'000'000;

data::SeasonDefinition club_season() {
    auto season =
        data::load_season(std::filesystem::path{TP_DATA_DIR} / "seasons" / "club_season.toml");
    if (!season) {
        throw std::runtime_error(season.error().message);
    }
    data::SeasonDefinition definition = season.value();
    definition.event.variation.lowest_gear = kLowestGear;
    return definition;
}

// Where the player ends at the first event with this career's tractor,
// driving as the reference driver does and always in their best gear.
int place(const data::SeasonDefinition& d, const garage::Career& career, std::uint64_t seed) {
    const data::EventDefinition& e = d.event;
    const std::optional<LiveSeason> created =
        LiveSeason::create(e.rules, e.reference, e.variation, e.class_rules, e.pull_to_pull,
                           d.points, kGamePullers, static_cast<int>(d.rounds.size()), seed);
    if (!created) {
        throw std::runtime_error("the season was refused");
    }
    const auto season = std::make_unique<LiveSeason>(*created);
    Puller player = season->player();
    const std::optional<vehicle::Params> tractor =
        garage::equipped(e.reference.tractor, d.garage.catalogue, career);
    if (!tractor) {
        throw std::runtime_error("the career's tractor was refused");
    }
    player.tractor = *tractor;
    if (!season->start_event(data::venue(e.sled, d.rounds.front().track))) {
        throw std::runtime_error("the event was refused");
    }
    LiveEvent& event = *season->event();
    for (int guard = 0; guard < 10'000; ++guard) {
        switch (event.turn()) {
            case LiveEvent::Turn::kRivals:
                event.advance(kSlice);
                break;
            case LiveEvent::Turn::kPlayerPull: {
                const track::Params& track = event.venue().track;
                player.driver.gear = best_gear(player, event.sled(), track, kLowestGear,
                                               e.variation.highest_gear, event.pull_rules())
                                         .value_or(e.reference.driver.gear);
                event.record_player(run_pull(player, event.sled(), track, event.pull_rules())
                                        .value_or(run::Result{}),
                                    player.driver.gear);
                break;
            }
            case LiveEvent::Turn::kPlayerDecision:
                event.decide_player(event.suggested_keep());
                break;
            case LiveEvent::Turn::kFinished:
                for (const event::Standing& s : event.standings()) {
                    if (s.puller == LiveSeason::kPlayer) {
                        return s.place;
                    }
                }
                throw std::runtime_error("the player has no place");
            case LiveEvent::Turn::kFailed:
                throw std::runtime_error("the event failed");
        }
    }
    throw std::runtime_error("the event did not finish");
}

garage::Career fully_upgraded(const garage::Catalogue& catalogue) {
    garage::Career career = garage::new_career(catalogue);
    for (std::size_t i = 0; i < catalogue.count; ++i) {
        career.levels[i] = static_cast<int>(catalogue.upgrades[i].level_count) - 1;
    }
    return career;
}

// The field spans the garage: its weakest rivals about a new career's
// tractor, its strongest about a fully upgraded one. So a new tractor
// starts near the back, beating a rival or two, and a fully upgraded one
// wins.
TEST(RivalDifficulty, ANewTractorStartsNearTheBackAndAFullyUpgradedOneWins) {
    const data::SeasonDefinition d = club_season();
    const garage::Catalogue& catalogue = d.garage.catalogue;
    const garage::Career full = fully_upgraded(catalogue);
    ASSERT_TRUE(garage::fully_upgraded(catalogue, full));
    bool beat_a_rival = false;
    for (const std::uint64_t seed : kSeeds) {
        const int start = place(d, garage::new_career(catalogue), seed);
        EXPECT_GE(start, kGamePullers - 3) << "seed " << seed;
        beat_a_rival = beat_a_rival || start < kGamePullers;
        EXPECT_EQ(place(d, full, seed), 1) << "seed " << seed;
    }
    EXPECT_TRUE(beat_a_rival) << "a new tractor is not last at every event";
}

}  // namespace
}  // namespace tp::field
