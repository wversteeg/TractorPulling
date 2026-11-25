#include "tp/data/career/career_sim.hpp"

#include "tp/core/field/field.hpp"
#include "tp/core/field/good_buy.hpp"
#include "tp/core/field/live.hpp"
#include "tp/core/field/live_season.hpp"
#include "tp/data/session/run_setup.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <memory>

namespace tp::data {

namespace {

// Advances the rivals in slices this many steps long between the player's
// turns.
constexpr std::int64_t kSlice = 1'000'000;
// More turns than any event takes: a guard against an event that never
// ends.
constexpr int kMostTurns = 100'000;

struct Fitting {
    garage::Career career;
    field::GearTrial trial;
};

// Combinations that full-pull the day's sled are told apart, and their gear
// chosen, on one this many pull-off steps heavier: the sled the event's
// operator soon sets for a strong field.
constexpr int kTieBreakSteps = 4;

// Of the alternatives `career` owns, the combination that takes the tractor
// furthest on `sled` and `track` in its best gear and hand throttle among
// `lowest` to `highest`: what the player fits for the day.
// How far `trying`'s tractor pulls `sled` in its best gear; a full pull
// also counts what it pulls on the sled kTieBreakSteps heavier, so full
// pulls are told apart. Nothing if refused.
std::optional<field::GearTrial> day_trial(const SeasonDefinition& d, const garage::Career& trying,
                                          const field::Puller& player, const sled::Params& sled,
                                          const track::Params& track, const run::Rules& rules,
                                          int lowest, int highest) {
    field::Puller puller = player;
    const auto tractor = garage::equipped(d.event.reference.tractor, d.garage.catalogue, trying);
    if (!tractor) {
        return std::nullopt;
    }
    puller.tractor = *tractor;
    auto trial = field::best_gear_trial(puller, sled, track, lowest, highest, rules);
    if (!trial || trial->distance_m < rules.full_pull_line_m) {
        return trial;
    }
    auto heavier = field::best_gear_trial(puller, sled::heavier(sled, kTieBreakSteps), track,
                                          lowest, highest, rules);
    if (!heavier) {
        return trial;
    }
    heavier->distance_m += trial->distance_m;
    return heavier;
}

std::optional<Fitting> fit_for_the_day(const SeasonDefinition& d, const garage::Career& career,
                                       const field::Puller& player, const sled::Params& sled,
                                       const track::Params& track, const run::Rules& rules,
                                       int lowest, int highest) {
    const garage::Catalogue& catalogue = d.garage.catalogue;
    std::optional<Fitting> best;
    garage::Career trying = career;
    trying.fitted.fill(0);
    for (bool more = true; more;) {
        bool owned = true;
        for (std::size_t c = 0; c < catalogue.choice_count; ++c) {
            owned = owned && garage::owns(career, c, static_cast<std::size_t>(trying.fitted.at(c)));
        }
        const auto trial = owned ? day_trial(d, trying, player, sled, track, rules, lowest, highest)
                                 : std::nullopt;
        if (trial && (!best || trial->distance_m > best->trial.distance_m)) {
            best = Fitting{.career = trying, .trial = *trial};
        }
        // The next combination, the first choice counting fastest.
        more = false;
        for (std::size_t c = 0; c < catalogue.choice_count && !more; ++c) {
            if (static_cast<std::size_t>(++trying.fitted.at(c)) <
                catalogue.choices.at(c).alternative_count) {
                more = true;
            } else {
                trying.fitted.at(c) = 0;
            }
        }
    }
    return best;
}

// The cheapest next upgrade level or alternative the player could buy, and
// what it costs; nothing when everything is bought.
struct Purchase {
    bool alternative = false;
    std::size_t index = 0;
    std::size_t choice = 0;
    std::int64_t cost_eur = 0;
};

std::optional<Purchase> cheapest(const garage::Catalogue& catalogue, const garage::Career& career) {
    std::optional<Purchase> best;
    const auto consider = [&](const Purchase& p) {
        if (!best || p.cost_eur < best->cost_eur) {
            best = p;
        }
    };
    for (std::size_t i = 0; i < catalogue.count; ++i) {
        const garage::Upgrade& u = catalogue.upgrades.at(i);
        const auto next = static_cast<std::size_t>(career.levels.at(i)) + 1;
        if (next < u.level_count && (u.choice == garage::kNoChoice ||
                                     garage::owns(career, static_cast<std::size_t>(u.choice),
                                                  static_cast<std::size_t>(u.alternative)))) {
            consider({.index = i, .cost_eur = u.levels.at(next).cost_eur});
        }
    }
    for (std::size_t c = 0; c < catalogue.choice_count; ++c) {
        const garage::Choice& choice = catalogue.choices.at(c);
        for (std::size_t a = 0; a < choice.alternative_count; ++a) {
            if (!garage::owns(career, c, a)) {
                consider({.alternative = true,
                          .index = a,
                          .choice = c,
                          .cost_eur = choice.alternatives.at(a).cost_eur});
            }
        }
    }
    return best;
}

// Spends the money on the cheapest next purchase while there is enough.
void shop(const garage::Catalogue& catalogue, garage::Career& career) {
    while (const auto p = cheapest(catalogue, career)) {
        if (p->cost_eur > career.money_eur) {
            return;
        }
        const auto error = p->alternative
                               ? garage::buy_alternative(career, catalogue, p->choice, p->index)
                               : garage::buy(career, catalogue, p->index);
        if (error) {
            return;
        }
    }
}

// Spends the money as the crew chief advises for `round` (field::good_buy):
// each good buy while it can be afforded, saving when the advice is to save;
// when nothing gains on that track any more, the cheapest next purchase.
void shop_for(const SeasonDefinition& d, garage::Career& career, const SeasonRound& round,
              bool plan_ahead) {
    const EventDefinition& e = d.event;
    const garage::Catalogue& catalogue = d.garage.catalogue;
    const field::Venue venue = data::venue(e.sled, round.track);
    const run::Rules rules = field::trial_pull_rules(e.rules, venue, e.rules.full_pull_m);
    const int gears = static_cast<int>(e.reference.tractor.driveline.gear_count);
    const int lowest = e.variation.lowest_gear;
    const int highest = std::min(e.variation.highest_gear + 1, gears);
    for (;;) {
        const auto advice = field::good_buy(e.reference, catalogue, career, venue.sled, venue.track,
                                            lowest, highest, rules, plan_ahead);
        if (!advice) {
            shop(catalogue, career);
            return;
        }
        if (advice->save_for) {
            return;
        }
        const garage::Career bought = field::with_buy(catalogue, career, advice->buy);
        if (bought.money_eur == career.money_eur && advice->buy.cost_eur > 0) {
            return;  // could not be made
        }
        career = bought;
    }
}

// The player's place in `event`'s standings, 0 if not there.
int player_place(field::LiveEvent& event) {
    for (const event::Standing& s : event.standings()) {
        if (s.puller == field::LiveSeason::kPlayer) {
            return s.place;
        }
    }
    return 0;
}

// Plays the event to its end, the player pulling as fitted for the day.
std::optional<int> play_event(const SeasonDefinition& d, garage::Career& career,
                              field::LiveSeason& season, const SeasonRound& round) {
    const EventDefinition& e = d.event;
    const field::Venue venue = data::venue(e.sled, round.track);
    field::Puller player = season.player();
    const int gears = static_cast<int>(e.reference.tractor.driveline.gear_count);
    const int lowest = e.variation.lowest_gear;
    const int highest = std::min(e.variation.highest_gear + 1, gears);
    // Fitted on the event's own sled before the event, as the crew does.
    const auto fitting = fit_for_the_day(
        d, career, player, venue.sled, venue.track,
        field::trial_pull_rules(e.rules, venue, e.rules.full_pull_m), lowest, highest);
    if (!fitting) {
        return std::nullopt;
    }
    career.fitted = fitting->career.fitted;
    const auto tractor = garage::equipped(e.reference.tractor, d.garage.catalogue, career);
    if (!tractor || !season.start_event(venue, *tractor)) {
        return std::nullopt;
    }
    player.tractor = *tractor;
    field::LiveEvent& event = *season.event();
    for (int turn = 0; turn < kMostTurns; ++turn) {
        switch (event.turn()) {
            case field::LiveEvent::Turn::kRivals:
                event.advance(kSlice);
                break;
            case field::LiveEvent::Turn::kPlayerPull: {
                // The gear and hand throttle chosen before the event, as the
                // rivals choose theirs: no trial on the sled of each pull.
                const track::Params& track = event.venue().track;
                player.driver.gear = fitting->trial.gear;
                player.driver.throttle = fitting->trial.throttle;
                event.record_player(field::run_pull(player, event.sled(), track, event.pull_rules())
                                        .value_or(run::Result{}),
                                    player.driver.gear);
                break;
            }
            case field::LiveEvent::Turn::kPlayerDecision:
                event.decide_player(event.suggested_keep());
                break;
            case field::LiveEvent::Turn::kFinished: {
                const int place = player_place(event);
                if (!season.score_event(d.prize_money_eur, career)) {
                    return std::nullopt;
                }
                return place;
            }
            case field::LiveEvent::Turn::kFailed:
                return std::nullopt;
        }
    }
    return std::nullopt;
}

// Buys the dealer's cheapest model if the career has no tractor yet.
bool buy_tractor(const garage::Catalogue& catalogue, garage::Career& career) {
    if (!garage::needs_tractor(catalogue, career)) {
        return true;
    }
    std::size_t cheapest_model = 0;
    for (std::size_t m = 1; m < catalogue.dealer.model_count; ++m) {
        if (garage::model_price_eur(catalogue, m, career.money_eur) <
            garage::model_price_eur(catalogue, cheapest_model, career.money_eur)) {
            cheapest_model = m;
        }
    }
    return !garage::buy_model(career, catalogue, cheapest_model, 0);
}

}  // namespace

double garage_share(const garage::Catalogue& catalogue, const garage::Career& career) noexcept {
    std::int64_t total_eur = 0;
    std::int64_t spent_eur = 0;
    for (std::size_t i = 0; i < catalogue.count; ++i) {
        const garage::Upgrade& u = catalogue.upgrades.at(i);
        for (std::size_t l = 1; l < u.level_count; ++l) {
            total_eur += u.levels.at(l).cost_eur;
            spent_eur += static_cast<int>(l) <= career.levels.at(i) ? u.levels.at(l).cost_eur : 0;
        }
    }
    for (std::size_t c = 0; c < catalogue.choice_count; ++c) {
        const garage::Choice& choice = catalogue.choices.at(c);
        for (std::size_t a = 1; a < choice.alternative_count; ++a) {
            total_eur += choice.alternatives.at(a).cost_eur;
            spent_eur += garage::owns(career, c, a) ? choice.alternatives.at(a).cost_eur : 0;
        }
    }
    return total_eur > 0 ? static_cast<double>(spent_eur) / static_cast<double>(total_eur) : 1.0;
}

std::optional<std::vector<SimSeason>> simulate_level(const SeasonDefinition& d,
                                                     garage::Career& career, int seasons,
                                                     std::uint64_t seed, bool plan_ahead) {
    const EventDefinition& e = d.event;
    const garage::Catalogue& catalogue = d.garage.catalogue;
    if (!buy_tractor(catalogue, career)) {
        return std::nullopt;
    }
    std::vector<SimSeason> out;
    for (int s = 0; s < seasons; ++s) {
        const auto created = field::LiveSeason::create(
            e.rules, e.reference, e.variation, e.class_rules, e.pull_to_pull, d.points,
            e.game.pullers, static_cast<int>(d.rounds.size()), seed + static_cast<std::uint64_t>(s),
            e.game.season_guests());
        if (!created) {
            return std::nullopt;
        }
        // A season is large: on the heap, as the game holds it.
        const auto season = std::make_unique<field::LiveSeason>(*created);
        SimSeason result;
        for (std::size_t r = 0; r < d.rounds.size(); ++r) {
            const SeasonRound& round = d.rounds[r];
            const auto place = play_event(d, career, *season, round);
            if (!place || result.events >= season::kMaxEvents) {
                return std::nullopt;
            }
            result.places.at(static_cast<std::size_t>(result.events++)) = *place;
            result.wins += *place == 1 ? 1 : 0;
            // Bought for the next event's track, as the crew chief advises.
            shop_for(d, career, d.rounds[(r + 1) % d.rounds.size()], plan_ahead);
        }
        for (const season::Standing& st : season->standings()) {
            if (st.puller == field::LiveSeason::kPlayer) {
                result.championship_place = st.place;
            }
        }
        result.money_eur = career.money_eur;
        result.garage_share = garage::build_share(catalogue, career);
        out.push_back(result);
    }
    return out;
}

}  // namespace tp::data
