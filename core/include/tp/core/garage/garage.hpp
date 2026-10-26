#pragma once

#include "tp/core/event/event.hpp"
#include "tp/core/rules/class_rules.hpp"
#include "tp/core/vehicle/vehicle.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace tp::garage {

inline constexpr std::size_t kMaxUpgrades = 8;
inline constexpr std::size_t kMaxLevels = 6;

/// What an upgrade changes on the tractor. Each level sets the part's value
/// outright, so the levels of different parts never interact:
enum class Part : std::uint8_t {
    /// The full-load torque, as a scale on the tractor's own curve (the
    /// injection pump and turbo set to the maker's figures, up to the
    /// class's PTO power).
    kEngineTorque,
    /// The rear tires' traction limit (tire::Traction::limit): worn tires,
    /// new ones, a bigger size.
    kRearTraction,
    /// The ballast's mass [kg] (chassis::with_ballast_mass): weights bought
    /// up to the class's mass.
    kBallastMass,
    /// The drawbar's height [m]: higher transfers more weight to the rear
    /// tires, up to the class's limit.
    kHitchHeight,
    /// How many gears the gearbox shifts down under load (a whole number;
    /// driveline::Params::load_downshift_gears), once a pull unless the
    /// tractor says otherwise.
    kLoadDownshift,
};

struct Level {
    /// The part's value at this level, in the part's unit (see Part).
    double value = 0.0;
    /// What this level costs, bought from the one below; level 0 is what
    /// the tractor starts with, and costs nothing [EUR].
    std::int64_t cost_eur = 0;
};

struct Upgrade {
    Part part = Part::kEngineTorque;
    std::array<Level, kMaxLevels> levels{};
    std::size_t level_count = 0;
};

/// The garage's offer for one class: the parts the player can upgrade, each
/// from the level the tractor starts at, and the money a career starts with.
struct Catalogue {
    std::array<Upgrade, kMaxUpgrades> upgrades{};
    std::size_t count = 0;
    std::int64_t start_money_eur = 0;

    [[nodiscard]] std::span<const Upgrade> offer() const noexcept {
        return {upgrades.data(), count};
    }
};

/// The player's money and how far each upgrade is bought (by the
/// catalogue's order). Plain data: it is saved and carries from season to
/// season.
struct Career {
    std::int64_t money_eur = 0;
    std::array<int, kMaxUpgrades> levels{};
};

enum class ParamError : std::uint8_t {
    kTooMany,
    kNoLevels,
    kNotFinite,
    kCostNegative,
    kFirstLevelNotFree,
    kStartMoneyNegative,
    kPartTwice,
    /// A level gives a tractor that is not valid (e.g. a gear count the
    /// gearbox does not have, a scale that breaks the torque curve).
    kValueInvalid,
    /// A level gives a tractor the class does not allow.
    kOutsideTheClass,
};

/// Whether every level of every upgrade, together with every other
/// upgrade's highest level, gives a valid tractor within the class (the
/// parts are independent, so that covers every combination), and the money
/// adds up.
[[nodiscard]] std::optional<ParamError> validate(const Catalogue& catalogue,
                                                 const vehicle::Params& tractor,
                                                 const rules::ClassRules& class_rules) noexcept;

/// A career at its start: the catalogue's start money, every part at level 0.
[[nodiscard]] Career new_career(const Catalogue& catalogue) noexcept;

/// `tractor` with the career's level of every upgrade (each held to the
/// levels there are); nothing if the result is not a valid tractor.
[[nodiscard]] std::optional<vehicle::Params> equipped(const vehicle::Params& tractor,
                                                      const Catalogue& catalogue,
                                                      const Career& career) noexcept;

/// Whether every upgrade is at its highest level.
[[nodiscard]] bool fully_upgraded(const Catalogue& catalogue, const Career& career) noexcept;

enum class BuyError : std::uint8_t {
    kNoSuchUpgrade,
    kTopLevel,
    kNotEnoughMoney,
};

/// Buys the next level of upgrade `upgrade`, paying its cost; the career is
/// unchanged if that cannot be done.
std::optional<BuyError> buy(Career& career, const Catalogue& catalogue,
                            std::size_t upgrade) noexcept;

/// What an event pays for `standing`: `by_place[place - 1]` [EUR], nothing
/// past the list or when disqualified.
[[nodiscard]] std::int64_t prize_eur(std::span<const std::int64_t> by_place,
                                     const event::Standing& standing) noexcept;

}  // namespace tp::garage
