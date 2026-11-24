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
                                const run::Rules& rules) noexcept {
    const sled::Params weighing = sled::heavier(sled, kGoodBuySteps);
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
    const std::optional<GearTrial> trial =
        best_gear_trial(now, weighing, track, lowest, highest, rules);
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
    std::optional<GoodBuy> best_affordable;
    std::optional<GoodBuy> best_of_all;
    double affordable_per_eur = 0.0;
    double all_per_eur = 0.0;
    const Candidates offer = candidates(catalogue, career);
    for (std::size_t i = 0; i < offer.count; ++i) {
        const Buy& buy = offer.buys.at(i);
        const std::optional<double> with_m =
            pulled_m(reference, catalogue, with_buy(catalogue, career, buy), weighing, track, from,
                     to, trial->throttle, rules);
        if (!with_m) {
            return std::nullopt;
        }
        const double gain_m = *with_m - *now_m;
        if (gain_m < kLeastGain_m) {
            continue;
        }
        // A free buy gains the most per euro.
        const double per_eur =
            gain_m / static_cast<double>(std::max<std::int64_t>(buy.cost_eur, 1));
        const GoodBuy candidate{.buy = buy, .gain_m = gain_m};
        if (per_eur > all_per_eur) {
            all_per_eur = per_eur;
            best_of_all = candidate;
        }
        if (buy.cost_eur <= career.money_eur && per_eur > affordable_per_eur) {
            affordable_per_eur = per_eur;
            best_affordable = candidate;
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
