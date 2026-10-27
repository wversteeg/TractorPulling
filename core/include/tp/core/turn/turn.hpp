#pragma once

#include "tp/core/hash/fnv1a.hpp"
#include "tp/core/pull/pull.hpp"
#include "tp/core/run/run.hpp"
#include "tp/core/vehicle/vehicle.hpp"

#include <cstdint>
#include <optional>

namespace tp::turn {

/// One puller's turn at an event, in the order it goes: from the parking
/// beside the track to the sled, the chain hooked on and the slack taken
/// up while the sled is held, the green flag and the pull, the chain
/// released, and back to the parking while the sled drives itself back to
/// the start line.
enum class Stage : std::uint8_t {
    /// Driving from the parking and backing up to the sled, the chain off.
    kToTheSled,
    /// Hooked on: the tractor creeps forward to put some force on the
    /// chain, the sled held, until the official waves the green flag.
    kTakingUpSlack,
    /// The pull (run::step), until the sled stops or reaches the end.
    kPulling,
    /// The pull is over (the result is set): back off to slacken the chain,
    /// which comes off once it is slack and the tractor stands. A sled
    /// still rolling (a full pull, a disqualification) rolls on until it
    /// stops, and is held from then on.
    kReleasing,
    /// The chain off, back to the parking; the sled drives itself back.
    kToTheParking,
    /// The tractor stands in the parking: the turn is over.
    kParked,
};

/// Where the tractor waits for its turn and comes back to: its rear axle's
/// middle on the track, x down the track from the start line and y to the
/// left of the centre line (negative: right of it), facing down the track
/// [m].
struct Parking {
    double x_m = 0.0;
    double y_m = 0.0;
};

enum class ParamError : std::uint8_t {
    kNotFinite,
    /// The parking is (partly) in the lane: it must be beside the track.
    kInTheLane,
};

/// A parking must keep a tractor `half_width_m` wide each side of its
/// middle out of the lane.
[[nodiscard]] std::optional<ParamError> validate(const Parking& parking, const run::Rules& rules,
                                                 double half_width_m) noexcept;

/// Slower than this the tractor stands [m/s].
inline constexpr double kAtRest_m_s = 0.05;
/// The chain hooks on once the tractor stands with its hitch ahead of the
/// sled's hook, within the chain's length of it, at most this far off the
/// sled's line [m] and turned at most this far from it [rad].
inline constexpr double kHookOffLine_m = 0.4;
inline constexpr double kHookTurned_rad = 0.26;
/// "Some force on the chain": the official waves the green flag once the
/// chain has held at least this tension [N] for this long without a break
/// [s].
inline constexpr double kTakeUpTension_n = 1000.0;
inline constexpr double kFlagAfter_s = 1.0;
/// The chain comes off once it is this much shorter than its length
/// between the hitch and the hook, with the tractor standing [m].
inline constexpr double kReleaseSlack_m = 0.3;
/// Parked: standing with the rear axle this close to the parking's spot
/// [m].
inline constexpr double kParkedWithin_m = 2.0;
/// Outside the pull a stalled engine starts again once the clutch is
/// pressed this far (or the gear is in neutral): the driver turns the key.
inline constexpr double kRestartPedal = 0.95;

struct State {
    Stage stage = Stage::kPulling;
    /// The pull's run: its state holds the tractor, the sled and the chain
    /// for the whole turn; its clock and result count the pull only.
    run::State run;
    /// Started in the parking (a whole turn), or hooked at the start line
    /// (the pull alone, as the tools and the rivals run it).
    bool from_the_parking = false;
    /// Steps the chain has held kTakeUpTension_n without a break: exact,
    /// where a running sum of dt would drift.
    std::uint64_t tension_held_steps = 0;
    /// The sled stands back at the start line after the pull.
    bool sled_back = false;
};

/// The pull alone: hooked at the start line, the green flag already waved.
/// The turn ends with the pull, as run::start and run::step do.
[[nodiscard]] State at_the_line(const pull::Model& model) noexcept;

/// The whole turn: the tractor in `parking`, in neutral, its engine
/// idling, the chain off and the sled held at the start line.
[[nodiscard]] State from_the_parking(const pull::Model& model, const Parking& parking) noexcept;

/// How the tractor's hitch stands to the sled's hook, in the sled's frame:
/// how far apart they are, along and across the sled's line [m].
struct HitchToHook {
    double distance_m = 0.0;
    double along_m = 0.0;
    double across_m = 0.0;
};

[[nodiscard]] HitchToHook hitch_to_hook(const pull::Model& model,
                                        const pull::State& state) noexcept;

/// Whether the chain hooks on now (see kHookOffLine_m).
[[nodiscard]] bool hooks_on(const pull::Model& model, const pull::State& state) noexcept;

/// One step of the turn. The stage decides what `input` drives: the
/// tractor alone, the tractor on the held sled, or the pull (with
/// vehicle::Input::pull_on set accordingly); parked, the tractor idles in
/// neutral while the sled finishes driving back. At the line the turn ends
/// with the pull, and after it nothing moves.
pull::Output step(State& state, const pull::Model& model, const run::Rules& rules,
                  const Parking& parking, vehicle::Input input, double dt_s) noexcept;

constexpr void hash_append(hash::Fnv1a& hash, const State& state) noexcept {
    hash.add(static_cast<std::uint64_t>(state.stage));
    run::hash_append(hash, state.run);
    hash.add(state.from_the_parking);
    hash.add(state.tension_held_steps);
    hash.add(state.sled_back);
}

}  // namespace tp::turn
