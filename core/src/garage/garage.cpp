#include "tp/core/garage/garage.hpp"

#include "tp/core/chassis/chassis.hpp"
#include "tp/core/engine/engine.hpp"

#include <algorithm>
#include <cmath>

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

int level_of(const Upgrade& upgrade, const Career& career, std::size_t index) noexcept {
    const int top = static_cast<int>(upgrade.level_count) - 1;
    return std::clamp(career.levels[index], 0, std::max(top, 0));
}

bool levels_valid(const Upgrade& upgrade) noexcept {
    return upgrade.level_count >= 1 && upgrade.level_count <= kMaxLevels;
}

// What is wrong with upgrade `i` alone, or with it beside the ones before.
std::optional<ParamError> upgrade_error(std::span<const Upgrade> offer, std::size_t i) noexcept {
    const Upgrade& upgrade = offer[i];
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
    if (std::ranges::any_of(before, [&](const Upgrade& u) { return u.part == upgrade.part; })) {
        return ParamError::kPartTwice;
    }
    return std::nullopt;
}

}  // namespace

std::optional<ParamError> validate(const Catalogue& catalogue, const vehicle::Params& tractor,
                                   const rules::ClassRules& class_rules) noexcept {
    if (catalogue.count > kMaxUpgrades) {
        return ParamError::kTooMany;
    }
    if (catalogue.start_money_eur < 0) {
        return ParamError::kStartMoneyNegative;
    }
    const std::span<const Upgrade> offer = catalogue.offer();
    for (std::size_t i = 0; i < offer.size(); ++i) {
        if (const std::optional<ParamError> error = upgrade_error(offer, i)) {
            return error;
        }
    }
    // Each level with every other part at its highest: each class limit
    // reads one part alone (see the header), so this covers every
    // combination the class judges.
    Career top;
    for (std::size_t i = 0; i < offer.size(); ++i) {
        top.levels[i] = static_cast<int>(offer[i].level_count) - 1;
    }
    for (std::size_t i = 0; i < offer.size(); ++i) {
        for (std::size_t l = 0; l < offer[i].level_count; ++l) {
            Career career = top;
            career.levels[i] = static_cast<int>(l);
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

Career new_career(const Catalogue& catalogue) noexcept {
    return {.money_eur = catalogue.start_money_eur, .levels = {}};
}

std::optional<vehicle::Params> equipped(const vehicle::Params& tractor, const Catalogue& catalogue,
                                        const Career& career) noexcept {
    const std::span<const Upgrade> offer = catalogue.offer();
    if (offer.size() > kMaxUpgrades) {
        return std::nullopt;
    }
    vehicle::Params out = tractor;
    for (std::size_t i = 0; i < offer.size(); ++i) {
        const Upgrade& upgrade = offer[i];
        if (!levels_valid(upgrade)) {
            return std::nullopt;
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
    return true;
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

std::int64_t prize_eur(std::span<const std::int64_t> by_place,
                       const event::Standing& standing) noexcept {
    if (standing.disqualified || standing.place < 1 ||
        static_cast<std::size_t>(standing.place) > by_place.size()) {
        return 0;
    }
    return by_place[static_cast<std::size_t>(standing.place) - 1];
}

}  // namespace tp::garage
