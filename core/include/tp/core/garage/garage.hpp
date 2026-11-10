#pragma once

#include "tp/core/driveline/driveline.hpp"
#include "tp/core/engine/engine.hpp"
#include "tp/core/event/event.hpp"
#include "tp/core/rules/class_rules.hpp"
#include "tp/core/tire/tire.hpp"
#include "tp/core/vehicle/vehicle.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace tp::garage {

inline constexpr std::size_t kMaxUpgrades = 16;
inline constexpr std::size_t kMaxLevels = 12;
inline constexpr std::size_t kMaxChoices = 4;
inline constexpr std::size_t kMaxAlternatives = 4;
/// An upgrade that belongs to no choice's alternative: it always counts.
inline constexpr int kNoChoice = -1;

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
    /// driveline::Params::load_downshift_gears), as often a pull as the
    /// tractor's driveline allows (load_shifts_per_pull).
    kLoadDownshift,
    /// The governor's rated speed [rad/s] (engine::with_rated_speed): full
    /// fuel up to a higher speed, the high idle moved with it, up to the
    /// class's unloaded engine speed.
    kRatedSpeed,
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
    /// The choice and its alternative this upgrade belongs to (an engine's
    /// own build): it counts only while that alternative is fitted, and its
    /// levels stay bought while it is not. kNoChoice: it always counts.
    int choice = kNoChoice;
    int alternative = 0;
};

/// What a choice swaps on the tractor: a whole component, for one of the
/// same kind. The tractor's other parts stay as they are.
enum class Slot : std::uint8_t {
    /// engine::Params: another engine, or the same one with another turbo.
    kEngine,
    /// tire::Params, the rear tires: of the tractor's radius (the rear axle's
    /// height is the tire's), with another grip or size of section.
    kRearTire,
    /// driveline::Params: another gearbox or final drive.
    kDriveline,
};

/// One of a choice's alternatives: the component for the choice's slot (the
/// others are not read; the slot's must be there) and what it costs to buy.
struct Alternative {
    std::optional<engine::Params> engine;
    std::optional<tire::Params> rear_tire;
    std::optional<driveline::Params> driveline;
    /// Nothing for the first alternative, the one the tractor comes with
    /// [EUR].
    std::int64_t cost_eur = 0;
};

/// A part the player chooses rather than builds up: alternatives that are
/// each better somewhere (a turbo that boosts early or one with more top
/// power). Bought once, an alternative is owned for good and fitted or
/// taken off again as often as the player likes.
struct Choice {
    Slot slot = Slot::kEngine;
    std::array<Alternative, kMaxAlternatives> alternatives{};
    std::size_t alternative_count = 0;
};

/// The garage's offer for one class: the parts the player can upgrade, each
/// from the level the tractor starts at, and the money a career starts with.
struct Catalogue {
    std::array<Upgrade, kMaxUpgrades> upgrades{};
    std::size_t count = 0;
    std::int64_t start_money_eur = 0;
    std::array<Choice, kMaxChoices> choices{};
    std::size_t choice_count = 0;

    [[nodiscard]] std::span<const Upgrade> offer() const noexcept {
        return {upgrades.data(), count};
    }
    [[nodiscard]] std::span<const Choice> choice_offer() const noexcept {
        return {choices.data(), choice_count};
    }
};

/// The player's money and how far each upgrade is bought (by the
/// catalogue's order). Plain data: it is saved and carries from season to
/// season.
struct Career {
    std::int64_t money_eur = 0;
    std::array<int, kMaxUpgrades> levels{};
    /// Each choice's fitted alternative (by the catalogue's order); one not
    /// owned counts as the first.
    std::array<int, kMaxChoices> fitted{};
    /// Each choice's alternatives bought, bit i for alternative i. The first
    /// is always owned, whatever its bit says.
    std::array<std::uint8_t, kMaxChoices> owned{};
};

enum class ParamError : std::uint8_t {
    /// More than kMaxUpgrades upgrades or kMaxChoices choices.
    kTooMany,
    kNoLevels,
    kNotFinite,
    kCostNegative,
    kFirstLevelNotFree,
    kStartMoneyNegative,
    /// Two upgrades of one part that could count at once: the same part
    /// is allowed twice only for different alternatives of one choice.
    kPartTwice,
    /// A choice without alternatives, or more than kMaxAlternatives.
    kNoAlternatives,
    /// An upgrade belongs to a choice or alternative there is not.
    kNoSuchAlternative,
    /// A level gives a tractor that is not valid (e.g. a gear count the
    /// gearbox does not have, a scale that breaks the torque curve).
    kValueInvalid,
    /// A level gives a tractor the class does not allow.
    kOutsideTheClass,
};

/// Whether, with every combination of the choices' alternatives fitted,
/// every level of every upgrade, together with every other upgrade's
/// highest level, gives a valid tractor within the class, and the money
/// adds up. That covers every combination because each of the class's
/// limits (rules::check) reads one part alone, or rises with each of its
/// parts: the mass the ballast, the PTO power the torque and the rated
/// speed (more of either never gives less power), the engine speed the
/// rated speed, the hitch the drawbar. A rule coupling parts any other way
/// (power to weight) would need every combination checked. The
/// alternatives are few (at most kMaxAlternatives^kMaxChoices
/// combinations), so each is tried.
[[nodiscard]] std::optional<ParamError> validate(const Catalogue& catalogue,
                                                 const vehicle::Params& tractor,
                                                 const rules::ClassRules& class_rules) noexcept;

/// A career at its start: the catalogue's start money, every part at level 0,
/// each choice's first alternative fitted.
[[nodiscard]] Career new_career(const Catalogue& catalogue) noexcept;

/// `tractor` with each choice's fitted alternative put in, then the career's
/// level of every upgrade that counts (each held to the levels there are);
/// nothing if the result is not a valid tractor.
[[nodiscard]] std::optional<vehicle::Params> equipped(const vehicle::Params& tractor,
                                                      const Catalogue& catalogue,
                                                      const Career& career) noexcept;

/// Whether every upgrade is at its highest level and every alternative is
/// owned.
[[nodiscard]] bool fully_upgraded(const Catalogue& catalogue, const Career& career) noexcept;

enum class BuyError : std::uint8_t {
    kNoSuchUpgrade,
    kTopLevel,
    kNotEnoughMoney,
    /// The alternative is owned already.
    kOwned,
};

/// Whether alternative `alternative` of choice `choice` is owned (the first
/// always is).
[[nodiscard]] bool owns(const Career& career, std::size_t choice, std::size_t alternative) noexcept;

/// The alternative of choice `choice` the tractor has fitted: the career's,
/// or the first if that is not owned or not there.
[[nodiscard]] std::size_t fitted(const Catalogue& catalogue, const Career& career,
                                 std::size_t choice) noexcept;

/// Whether upgrade `upgrade` counts on the tractor as fitted: it belongs to
/// no choice, or its alternative is fitted.
[[nodiscard]] bool counts(const Catalogue& catalogue, const Career& career,
                          std::size_t upgrade) noexcept;

/// Buys the next level of upgrade `upgrade`, paying its cost; the career is
/// unchanged if that cannot be done.
std::optional<BuyError> buy(Career& career, const Catalogue& catalogue,
                            std::size_t upgrade) noexcept;

/// Buys alternative `alternative` of choice `choice`, paying its cost, and
/// fits it; the career is unchanged if that cannot be done
/// (kNoSuchUpgrade: no such choice or alternative).
std::optional<BuyError> buy_alternative(Career& career, const Catalogue& catalogue,
                                        std::size_t choice, std::size_t alternative) noexcept;

enum class FitError : std::uint8_t {
    kNoSuchAlternative,
    kNotOwned,
};

/// Fits alternative `alternative` of choice `choice`, which must be owned;
/// free, and as often as the player likes. The alternative taken off keeps
/// its upgrades' levels.
std::optional<FitError> fit(Career& career, const Catalogue& catalogue, std::size_t choice,
                            std::size_t alternative) noexcept;

/// What an event pays for `standing`: `by_place[place - 1]` [EUR], nothing
/// past the list or when disqualified.
[[nodiscard]] std::int64_t prize_eur(std::span<const std::int64_t> by_place,
                                     const event::Standing& standing) noexcept;

}  // namespace tp::garage
