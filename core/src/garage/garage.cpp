#include "tp/core/garage/garage.hpp"

#include "tp/core/chassis/chassis.hpp"
#include "tp/core/engine/engine.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace tp::garage {

namespace {

// `tractor` with `part` set to `value`; nothing if it cannot be.
std::optional<vehicle::Params> with_part(const vehicle::Params& tractor, Part part,
                                         double value) noexcept {
    if (!std::isfinite(value)) {
        return std::nullopt;
    }
    vehicle::Params out = tractor;
    switch (part) {
        case Part::kEngineTorque: {
            const std::optional<engine::Params> engine =
                engine::with_torque_scale(tractor.engine, value);
            if (!engine || value <= 0.0) {
                return std::nullopt;
            }
            out.engine = *engine;
            break;
        }
        case Part::kRearTraction:
            out.rear_tire.traction.limit = value;
            break;
        case Part::kBallastMass:
            if (value < 0.0) {
                return std::nullopt;
            }
            out.chassis = chassis::with_ballast_mass(tractor.chassis, value);
            break;
        case Part::kHitchHeight:
            out.chassis.hitch_height_m = value;
            break;
        case Part::kLoadDownshift:
            if (value != std::floor(value) || value < 0.0 ||
                value > static_cast<double>(driveline::kMaxGears)) {
                return std::nullopt;
            }
            out.driveline.load_downshift_gears = static_cast<int>(value);
            break;
        case Part::kRatedSpeed: {
            const std::optional<engine::Params> engine =
                engine::with_rated_speed(tractor.engine, value);
            if (!engine) {
                return std::nullopt;
            }
            out.engine = *engine;
            break;
        }
    }
    return out;
}

// `tractor` with `alternative` in `slot`; nothing if it has no component
// for the slot.
std::optional<vehicle::Params> with_alternative(const vehicle::Params& tractor, Slot slot,
                                                const Alternative& alternative) noexcept {
    vehicle::Params out = tractor;
    switch (slot) {
        case Slot::kEngine:
            if (!alternative.engine) {
                return std::nullopt;
            }
            out.engine = *alternative.engine;
            break;
        case Slot::kRearTire:
            if (!alternative.rear_tire) {
                return std::nullopt;
            }
            out.rear_tire = *alternative.rear_tire;
            break;
        case Slot::kDriveline:
            if (!alternative.driveline) {
                return std::nullopt;
            }
            out.driveline = *alternative.driveline;
            break;
    }
    return out;
}

bool alternatives_valid(const Choice& choice) noexcept {
    return choice.alternative_count >= 1 && choice.alternative_count <= kMaxAlternatives;
}

int level_of(const Upgrade& upgrade, const Career& career, std::size_t index) noexcept {
    const int top = static_cast<int>(upgrade.level_count) - 1;
    return std::clamp(career.levels[index], 0, std::max(top, 0));
}

bool levels_valid(const Upgrade& upgrade) noexcept {
    return upgrade.level_count >= 1 && upgrade.level_count <= kMaxLevels;
}

// Whether upgrades `a` and `b` can both count at once: unless they belong to
// different alternatives of one choice.
bool can_count_together(const Upgrade& a, const Upgrade& b) noexcept {
    return a.choice == kNoChoice || a.choice != b.choice || a.alternative == b.alternative;
}

// Whether upgrade `upgrade`'s choice and alternative are in the catalogue.
bool owner_valid(const Catalogue& catalogue, const Upgrade& upgrade) noexcept {
    if (upgrade.choice == kNoChoice) {
        return true;
    }
    if (upgrade.choice < 0 || static_cast<std::size_t>(upgrade.choice) >= catalogue.choice_count) {
        return false;
    }
    const Choice& choice = catalogue.choices[static_cast<std::size_t>(upgrade.choice)];
    return upgrade.alternative >= 0 &&
           static_cast<std::size_t>(upgrade.alternative) < choice.alternative_count;
}

// What is wrong with upgrade `i` alone, or with it beside the ones before.
std::optional<ParamError> upgrade_error(const Catalogue& catalogue, std::size_t i) noexcept {
    const std::span<const Upgrade> offer = catalogue.offer();
    const Upgrade& upgrade = offer[i];
    if (!owner_valid(catalogue, upgrade)) {
        return ParamError::kNoSuchAlternative;
    }
    if (!levels_valid(upgrade)) {
        return ParamError::kNoLevels;
    }
    const std::span<const Level> levels{upgrade.levels.data(), upgrade.level_count};
    if (!std::ranges::all_of(levels, [](const Level& l) { return std::isfinite(l.value); })) {
        return ParamError::kNotFinite;
    }
    if (std::ranges::any_of(levels, [](const Level& l) { return l.cost_eur < 0; })) {
        return ParamError::kCostNegative;
    }
    if (levels.front().cost_eur != 0) {
        return ParamError::kFirstLevelNotFree;
    }
    const auto before = offer.first(i);
    if (std::ranges::any_of(before, [&](const Upgrade& u) {
            return u.part == upgrade.part && can_count_together(u, upgrade);
        })) {
        return ParamError::kPartTwice;
    }
    return std::nullopt;
}

// What is wrong with choice `choice`'s alternatives, apart from the tractor
// they give.
std::optional<ParamError> choice_error(const Choice& choice) noexcept {
    if (!alternatives_valid(choice)) {
        return ParamError::kNoAlternatives;
    }
    const std::span<const Alternative> alternatives{choice.alternatives.data(),
                                                    choice.alternative_count};
    if (std::ranges::any_of(alternatives, [](const Alternative& a) { return a.cost_eur < 0; })) {
        return ParamError::kCostNegative;
    }
    if (alternatives.front().cost_eur != 0) {
        return ParamError::kFirstLevelNotFree;
    }
    return std::nullopt;
}

// Fits the next combination of alternatives into `career` (the first choice
// counting fastest); false once every combination has been.
bool next_fitting(const Catalogue& catalogue, Career& career) noexcept {
    for (std::size_t c = 0; c < catalogue.choice_count; ++c) {
        ++career.fitted[c];
        if (static_cast<std::size_t>(career.fitted[c]) < catalogue.choices[c].alternative_count) {
            return true;
        }
        career.fitted[c] = 0;
    }
    return false;
}

// Whether, with `fitting`'s alternatives fitted, each upgrade's every level
// beside every other's highest gives a valid tractor within the class.
std::optional<ParamError> fitting_error(const Catalogue& catalogue, const Career& fitting,
                                        const vehicle::Params& tractor,
                                        const rules::ClassRules& class_rules) noexcept {
    const std::span<const Upgrade> offer = catalogue.offer();
    Career top = fitting;
    for (std::size_t i = 0; i < offer.size(); ++i) {
        top.levels[i] = static_cast<int>(offer[i].level_count) - 1;
    }
    // The tractor with nothing but the choices, then each level in turn.
    for (std::size_t i = 0; i <= offer.size(); ++i) {
        const std::size_t level_count = i < offer.size() ? offer[i].level_count : 1;
        for (std::size_t l = 0; l < level_count; ++l) {
            Career career = top;
            if (i < offer.size()) {
                career.levels[i] = static_cast<int>(l);
            }
            const std::optional<vehicle::Params> built = equipped(tractor, catalogue, career);
            if (!built) {
                return ParamError::kValueInvalid;
            }
            if (!rules::check(class_rules, *built).ok()) {
                return ParamError::kOutsideTheClass;
            }
        }
    }
    return std::nullopt;
}

}  // namespace

std::optional<ParamError> validate(const Catalogue& catalogue, const vehicle::Params& tractor,
                                   const rules::ClassRules& class_rules) noexcept {
    if (catalogue.count > kMaxUpgrades || catalogue.choice_count > kMaxChoices) {
        return ParamError::kTooMany;
    }
    if (catalogue.start_money_eur < 0) {
        return ParamError::kStartMoneyNegative;
    }
    for (const Choice& choice : catalogue.choice_offer()) {
        if (const std::optional<ParamError> error = choice_error(choice)) {
            return error;
        }
    }
    for (std::size_t i = 0; i < catalogue.count; ++i) {
        if (const std::optional<ParamError> error = upgrade_error(catalogue, i)) {
            return error;
        }
    }
    // Every combination of alternatives, each owned so that it is fitted.
    Career fitting;
    fitting.owned.fill(std::numeric_limits<std::uint8_t>::max());
    for (bool more = true; more; more = next_fitting(catalogue, fitting)) {
        if (const std::optional<ParamError> error =
                fitting_error(catalogue, fitting, tractor, class_rules)) {
            return error;
        }
    }
    return std::nullopt;
}

Career new_career(const Catalogue& catalogue) noexcept {
    return {.money_eur = catalogue.start_money_eur, .levels = {}, .fitted = {}, .owned = {}};
}

std::optional<vehicle::Params> equipped(const vehicle::Params& tractor, const Catalogue& catalogue,
                                        const Career& career) noexcept {
    const std::span<const Upgrade> offer = catalogue.offer();
    if (offer.size() > kMaxUpgrades || catalogue.choice_count > kMaxChoices) {
        return std::nullopt;
    }
    vehicle::Params out = tractor;
    for (std::size_t c = 0; c < catalogue.choice_count; ++c) {
        const Choice& choice = catalogue.choices[c];
        if (!alternatives_valid(choice)) {
            return std::nullopt;
        }
        const std::optional<vehicle::Params> next =
            with_alternative(out, choice.slot, choice.alternatives[fitted(catalogue, career, c)]);
        if (!next) {
            return std::nullopt;
        }
        out = *next;
    }
    for (std::size_t i = 0; i < offer.size(); ++i) {
        const Upgrade& upgrade = offer[i];
        if (!levels_valid(upgrade) || !owner_valid(catalogue, upgrade)) {
            return std::nullopt;
        }
        if (!counts(catalogue, career, i)) {
            continue;
        }
        const Level& level = upgrade.levels[static_cast<std::size_t>(level_of(upgrade, career, i))];
        const std::optional<vehicle::Params> next = with_part(out, upgrade.part, level.value);
        if (!next) {
            return std::nullopt;
        }
        out = *next;
    }
    if (vehicle::validate(out)) {
        return std::nullopt;
    }
    return out;
}

bool fully_upgraded(const Catalogue& catalogue, const Career& career) noexcept {
    const std::span<const Upgrade> offer = catalogue.offer();
    for (std::size_t i = 0; i < offer.size(); ++i) {
        if (career.levels[i] < static_cast<int>(offer[i].level_count) - 1) {
            return false;
        }
    }
    for (std::size_t c = 0; c < catalogue.choice_count; ++c) {
        for (std::size_t a = 0; a < catalogue.choices[c].alternative_count; ++a) {
            if (!owns(career, c, a)) {
                return false;
            }
        }
    }
    return true;
}

bool owns(const Career& career, std::size_t choice, std::size_t alternative) noexcept {
    if (alternative == 0) {
        return true;
    }
    if (choice >= kMaxChoices || alternative >= kMaxAlternatives) {
        return false;
    }
    return (career.owned[choice] & (1U << alternative)) != 0U;
}

std::size_t fitted(const Catalogue& catalogue, const Career& career, std::size_t choice) noexcept {
    if (choice >= catalogue.choice_count || choice >= kMaxChoices) {
        return 0;
    }
    const int wanted = career.fitted[choice];
    if (wanted < 0 ||
        static_cast<std::size_t>(wanted) >= catalogue.choices[choice].alternative_count ||
        !owns(career, choice, static_cast<std::size_t>(wanted))) {
        return 0;
    }
    return static_cast<std::size_t>(wanted);
}

bool counts(const Catalogue& catalogue, const Career& career, std::size_t upgrade) noexcept {
    if (upgrade >= catalogue.count || upgrade >= kMaxUpgrades) {
        return false;
    }
    const Upgrade& u = catalogue.upgrades[upgrade];
    return u.choice == kNoChoice ||
           (u.choice >= 0 && fitted(catalogue, career, static_cast<std::size_t>(u.choice)) ==
                                 static_cast<std::size_t>(u.alternative));
}

std::optional<BuyError> buy(Career& career, const Catalogue& catalogue,
                            std::size_t upgrade) noexcept {
    if (upgrade >= catalogue.count || upgrade >= kMaxUpgrades ||
        !levels_valid(catalogue.upgrades[upgrade])) {
        return BuyError::kNoSuchUpgrade;
    }
    const Upgrade& offered = catalogue.upgrades[upgrade];
    const int next = level_of(offered, career, upgrade) + 1;
    if (next >= static_cast<int>(offered.level_count)) {
        return BuyError::kTopLevel;
    }
    const std::int64_t cost_eur = offered.levels[static_cast<std::size_t>(next)].cost_eur;
    if (cost_eur > career.money_eur) {
        return BuyError::kNotEnoughMoney;
    }
    career.money_eur -= cost_eur;
    career.levels[upgrade] = next;
    return std::nullopt;
}

std::optional<BuyError> buy_alternative(Career& career, const Catalogue& catalogue,
                                        std::size_t choice, std::size_t alternative) noexcept {
    if (choice >= catalogue.choice_count || choice >= kMaxChoices ||
        alternative >= catalogue.choices[choice].alternative_count ||
        alternative >= kMaxAlternatives) {
        return BuyError::kNoSuchUpgrade;
    }
    if (owns(career, choice, alternative)) {
        return BuyError::kOwned;
    }
    const std::int64_t cost_eur = catalogue.choices[choice].alternatives[alternative].cost_eur;
    if (cost_eur > career.money_eur) {
        return BuyError::kNotEnoughMoney;
    }
    career.money_eur -= cost_eur;
    career.owned[choice] = static_cast<std::uint8_t>(career.owned[choice] | (1U << alternative));
    career.fitted[choice] = static_cast<int>(alternative);
    return std::nullopt;
}

std::optional<FitError> fit(Career& career, const Catalogue& catalogue, std::size_t choice,
                            std::size_t alternative) noexcept {
    if (choice >= catalogue.choice_count || choice >= kMaxChoices ||
        alternative >= catalogue.choices[choice].alternative_count) {
        return FitError::kNoSuchAlternative;
    }
    if (!owns(career, choice, alternative)) {
        return FitError::kNotOwned;
    }
    career.fitted[choice] = static_cast<int>(alternative);
    return std::nullopt;
}

std::int64_t prize_eur(std::span<const std::int64_t> by_place,
                       const event::Standing& standing) noexcept {
    if (standing.disqualified || standing.place < 1 ||
        static_cast<std::size_t>(standing.place) > by_place.size()) {
        return 0;
    }
    return by_place[static_cast<std::size_t>(standing.place) - 1];
}

}  // namespace tp::garage
