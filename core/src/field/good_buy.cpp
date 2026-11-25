#include "tp/core/field/good_buy.hpp"

#include <algorithm>
#include <limits>

namespace tp::field {

namespace {

// How far `career`'s tractor pulls at `throttle` in the best of the gears
// `lowest` to `highest`: nothing if refused.
std::optional<double> pulled_m(const Puller& reference, const garage::Catalogue& catalogue,
                               const garage::Career& career, const sled::Params& sled,
                               const track::Params& track, int lowest, int highest, double throttle,
                               const run::Rules& rules) noexcept {
    const std::optional<vehicle::Params> tractor =
        garage::equipped(reference.tractor, catalogue, career);
    if (!tractor) {
        return std::nullopt;
    }
    Puller puller = reference;
    puller.tractor = *tractor;
    puller.driver.throttle = throttle;
    double furthest_m = 0.0;
    for (int gear = lowest; gear <= highest; ++gear) {
        puller.driver.gear = gear;
        const std::optional<run::Result> result = run_pull(puller, sled, track, rules);
        if (!result) {
            return std::nullopt;
        }
        furthest_m = std::max(furthest_m, result->distance_m);
    }
    return furthest_m;
}

// Fits, of the alternatives `career` owns, those made for what is fitted
// with alternative `alternative` of choice `choice`, and the one that is
// made for where `alternative` is made for another choice's.
void fit_what_suits(const garage::Catalogue& catalogue, garage::Career& career, std::size_t choice,
                    std::size_t alternative) noexcept {
    const garage::Alternative& bought = catalogue.choices.at(choice).alternatives.at(alternative);
    if (bought.suits_choice != garage::kNoChoice) {
        const auto other = static_cast<std::size_t>(bought.suits_choice);
        const auto wanted = static_cast<std::size_t>(bought.suits_alternative);
        if (garage::owns(career, other, wanted)) {
            (void)garage::fit(career, catalogue, other, wanted);
        }
    }
    for (std::size_t c = 0; c < catalogue.choice_count; ++c) {
        if (c == choice) {
            continue;
        }
        const garage::Choice& other = catalogue.choices.at(c);
        for (std::size_t a = 0; a < other.alternative_count; ++a) {
            const garage::Alternative& made = other.alternatives.at(a);
            if (made.suits_choice == static_cast<int>(choice) &&
                made.suits_alternative == static_cast<int>(alternative) &&
                garage::owns(career, c, a)) {
                (void)garage::fit(career, catalogue, c, a);
                break;
            }
        }
    }
}

// Every buy there is for `career`: the next level of each upgrade that
// counts as fitted, and every alternative not owned.
struct Candidates {
    std::array<Buy, garage::kMaxUpgrades + (garage::kMaxChoices * garage::kMaxAlternatives)> buys{};
    std::size_t count = 0;
};

Candidates candidates(const garage::Catalogue& catalogue, const garage::Career& career) noexcept {
    Candidates out;
    for (std::size_t i = 0; i < catalogue.count; ++i) {
        const garage::Upgrade& upgrade = catalogue.upgrades.at(i);
        const int next = career.levels.at(i) + 1;
        if (next < static_cast<int>(upgrade.level_count) && garage::counts(catalogue, career, i)) {
            out.buys.at(out.count++) =
                Buy{.kind = BuyKind::kUpgrade,
                    .index = i,
                    .cost_eur = upgrade.levels.at(static_cast<std::size_t>(next)).cost_eur};
        }
    }
    for (std::size_t c = 0; c < catalogue.choice_count; ++c) {
        const garage::Choice& choice = catalogue.choices.at(c);
        for (std::size_t a = 0; a < choice.alternative_count; ++a) {
            if (!garage::owns(career, c, a)) {
                out.buys.at(out.count++) = Buy{.kind = BuyKind::kAlternative,
                                               .index = c,
                                               .alternative = a,
                                               .cost_eur = choice.alternatives.at(a).cost_eur};
            }
        }
    }
    return out;
}

// Whether upgrade `upgrade` is alternative `alternative` of choice
// `choice`'s own (its build): it counts only while that is fitted.
bool own_upgrade(const garage::Upgrade& upgrade, std::size_t choice,
                 std::size_t alternative) noexcept {
    return upgrade.choice == static_cast<int>(choice) &&
           upgrade.alternative == static_cast<int>(alternative);
}

// Alternative `alternative` of choice `choice` fully built: bought if the
// career does not own it, fitted, its own upgrades (its build) and those
// every alternative shares at their top; and its first piece to buy (the alternative, or the cheapest next
// level of its build), none when it is all bought.
struct Route {
    garage::Career career;
    std::optional<Buy> first;
};

Route fully_built(const garage::Catalogue& catalogue, const garage::Career& career,
                  std::size_t choice, std::size_t alternative) noexcept {
    Route out;
    out.career = career;
    out.career.money_eur = std::numeric_limits<std::int64_t>::max() / 2;
    if (!garage::owns(career, choice, alternative)) {
        out.first =
            Buy{.kind = BuyKind::kAlternative,
                .index = choice,
                .alternative = alternative,
                .cost_eur = catalogue.choices.at(choice).alternatives.at(alternative).cost_eur};
        (void)garage::buy_alternative(out.career, catalogue, choice, alternative);
    }
    for (std::size_t i = 0; i < catalogue.count; ++i) {
        const garage::Upgrade& upgrade = catalogue.upgrades.at(i);
        const int top = static_cast<int>(upgrade.level_count) - 1;
        if (upgrade.choice == garage::kNoChoice) {
            // Shared by every engine (the turbo, the hitch): at its top for
            // both, so that the engines' full potentials are compared.
            out.career.levels.at(i) = std::max(top, 0);
            continue;
        }
        if (!own_upgrade(upgrade, choice, alternative)) {
            continue;
        }
        const int next = career.levels.at(i) + 1;
        if (next <= top && out.first.value_or(Buy{}).kind != BuyKind::kAlternative) {
            const std::int64_t cost_eur =
                upgrade.levels.at(static_cast<std::size_t>(next)).cost_eur;
            if (!out.first || cost_eur < out.first->cost_eur) {
                out.first = Buy{.kind = BuyKind::kUpgrade, .index = i, .cost_eur = cost_eur};
            }
        }
        out.career.levels.at(i) = std::max(top, 0);
    }
    (void)garage::fit(out.career, catalogue, choice, alternative);
    fit_what_suits(catalogue, out.career, choice, alternative);
    out.career.money_eur = career.money_eur;
    return out;
}

// Whether alternative `alternative` of choice `choice` has upgrades of its
// own: an engine and its build.
bool has_own_upgrades(const garage::Catalogue& catalogue, std::size_t choice,
                      std::size_t alternative) noexcept {
    for (std::size_t i = 0; i < catalogue.count; ++i) {
        if (own_upgrade(catalogue.upgrades.at(i), choice, alternative)) {
            return true;
        }
    }
    return false;
}

// How far `career`'s tractor pulls `sled` made heavier until `career`
// stops short of the line, and that sled; in its best gear.
std::optional<double> furthest_m(const Puller& reference, const garage::Catalogue& catalogue,
                                 const garage::Career& career, const sled::Params& sled,
                                 const track::Params& track, int lowest, int highest,
                                 const run::Rules& rules) noexcept {
    Puller puller = reference;
    if (const auto tractor = garage::equipped(reference.tractor, catalogue, career)) {
        puller.tractor = *tractor;
    } else {
        return std::nullopt;
    }
    const std::optional<GearTrial> trial =
        best_gear_trial(puller, sled, track, lowest, highest, rules);
    if (!trial) {
        return std::nullopt;
    }
    return trial->distance_m;
}

// The switch a crew plans for a new level: an alternative with a build of
// its own (an engine) that, fully built, pulls further than the fitted one
// fully built, though it may be weaker as it comes. Its first piece, the
// gain between the two fully built; nothing when there is no such switch.
std::optional<GoodBuy> switch_ahead(const Puller& reference, const garage::Catalogue& catalogue,
                                    const garage::Career& career, const sled::Params& sled,
                                    const track::Params& track, int lowest, int highest,
                                    const run::Rules& rules) noexcept {
    std::optional<GoodBuy> best;
    for (std::size_t c = 0; c < catalogue.choice_count; ++c) {
        const std::size_t fitted = garage::fitted(catalogue, career, c);
        const Route now = fully_built(catalogue, career, c, fitted);
        // Weighed on a sled the fitted one fully built stops short on.
        int steps = kGoodBuySteps;
        sled::Params weighing = sled::heavier(sled, steps);
        std::optional<double> now_m =
            furthest_m(reference, catalogue, now.career, weighing, track, lowest, highest, rules);
        while (now_m && *now_m >= rules.full_pull_line_m && steps < kMostGoodBuySteps) {
            steps += kGoodBuySteps;
            weighing = sled::heavier(sled, steps);
            now_m = furthest_m(reference, catalogue, now.career, weighing, track, lowest, highest,
                               rules);
        }
        if (!now_m) {
            return std::nullopt;
        }
        for (std::size_t a = 0; a < catalogue.choices.at(c).alternative_count; ++a) {
            if (a == fitted || !has_own_upgrades(catalogue, c, a)) {
                continue;
            }
            const Route ahead = fully_built(catalogue, career, c, a);
            if (!ahead.first) {
                continue;  // all bought: fitted on the day when it pulls further
            }
            const std::optional<double> ahead_m = furthest_m(
                reference, catalogue, ahead.career, weighing, track, lowest, highest, rules);
            if (!ahead_m) {
                return std::nullopt;
            }
            const double gain_m = *ahead_m - *now_m;
            if (gain_m >= kLeastGain_m && (!best || gain_m > best->gain_m)) {
                best = GoodBuy{.buy = *ahead.first,
                               .gain_m = gain_m,
                               .save_for = ahead.first->cost_eur > career.money_eur,
                               .steps = steps,
                               .route_choice = static_cast<int>(c),
                               .route_alternative = static_cast<int>(a)};
            }
        }
    }
    return best;
}

}  // namespace

garage::Career with_buy(const garage::Catalogue& catalogue, const garage::Career& career,
                        const Buy& buy) noexcept {
    garage::Career out = career;
    // Weighed whether or not it can be afforded: the money only pays here.
    out.money_eur = std::numeric_limits<std::int64_t>::max() / 2;
    const bool made = buy.kind == BuyKind::kUpgrade
                          ? !garage::buy(out, catalogue, buy.index)
                          : !garage::buy_alternative(out, catalogue, buy.index, buy.alternative);
    if (!made) {
        return career;
    }
    if (buy.kind == BuyKind::kAlternative) {
        fit_what_suits(catalogue, out, buy.index, buy.alternative);
    }
    out.money_eur = career.money_eur - buy.cost_eur;
    return out;
}

std::optional<GoodBuy> good_buy(const Puller& reference, const garage::Catalogue& catalogue,
                                const garage::Career& career, const sled::Params& sled,
                                const track::Params& track, int lowest, int highest,
                                const run::Rules& rules, bool plan_ahead) noexcept {
    // The tractor as it is in its best gear and throttle; each buy is tried
    // at that throttle in that gear and the ones either side of it: a part
    // moves the best gear by one at most, and the trial stays quick enough
    // for the garage.
    Puller now = reference;
    if (const auto tractor = garage::equipped(reference.tractor, catalogue, career)) {
        now.tractor = *tractor;
    } else {
        return std::nullopt;
    }
    int steps = kGoodBuySteps;
    sled::Params weighing = sled::heavier(sled, steps);
    std::optional<GearTrial> trial = best_gear_trial(now, weighing, track, lowest, highest, rules);
    while (trial && trial->distance_m >= rules.full_pull_line_m && steps < kMostGoodBuySteps) {
        steps += kGoodBuySteps;
        weighing = sled::heavier(sled, steps);
        trial = best_gear_trial(now, weighing, track, lowest, highest, rules);
    }
    if (!trial) {
        return std::nullopt;
    }
    const int from = std::max(trial->gear - 1, lowest);
    const int to = std::min(trial->gear + 1, highest);
    const std::optional<double> now_m =
        pulled_m(reference, catalogue, career, weighing, track, from, to, trial->throttle, rules);
    if (!now_m) {
        return std::nullopt;
    }
    if (plan_ahead) {
        if (const std::optional<GoodBuy> ahead =
                switch_ahead(reference, catalogue, career, sled, track, lowest, highest, rules)) {
            return ahead;
        }
    }
    std::optional<GoodBuy> best_affordable;
    std::optional<GoodBuy> best_of_all;
    double affordable_per_eur = 0.0;
    double all_per_eur = 0.0;
    // Weighs `candidate` (its buy made, the tractor as `with`) by the metres
    // it gains per euro of `cost_eur`; false if a pull was refused.
    const auto weigh = [&](GoodBuy candidate, const garage::Career& with,
                           std::int64_t cost_eur) noexcept {
        const std::optional<double> with_m =
            pulled_m(reference, catalogue, with, weighing, track, from, to, trial->throttle, rules);
        if (!with_m) {
            return false;
        }
        candidate.gain_m = *with_m - *now_m;
        if (candidate.gain_m < kLeastGain_m) {
            return true;
        }
        // A free buy gains the most per euro.
        const double per_eur =
            candidate.gain_m / static_cast<double>(std::max<std::int64_t>(cost_eur, 1));
        if (per_eur > all_per_eur) {
            all_per_eur = per_eur;
            best_of_all = candidate;
        }
        if (candidate.buy.cost_eur <= career.money_eur && per_eur > affordable_per_eur) {
            affordable_per_eur = per_eur;
            best_affordable = candidate;
        }
        return true;
    };
    const Candidates offer = candidates(catalogue, career);
    for (std::size_t i = 0; i < offer.count; ++i) {
        const Buy& buy = offer.buys.at(i);
        if (!weigh(GoodBuy{.buy = buy, .steps = steps}, with_buy(catalogue, career, buy),
                   buy.cost_eur)) {
            return std::nullopt;
        }
    }
    if (!best_of_all) {
        return std::nullopt;
    }
    if (!best_affordable || all_per_eur >= kSaveForGainRatio * affordable_per_eur) {
        if (best_of_all->buy.cost_eur > career.money_eur) {
            best_of_all->save_for = true;
            return best_of_all;
        }
    }
    return best_affordable ? best_affordable : best_of_all;
}

}  // namespace tp::field
