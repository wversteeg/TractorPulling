#pragma once

#include "tp/core/pull/pull.hpp"
#include "tp/core/vehicle/vehicle.hpp"

#include <cstdint>
#include <optional>

namespace tp::run {

/// The track a run is pulled on.
struct Rules {
    /// A full pull reaches this far [m].
    double track_length_m = 0.0;
    /// Safety cut-off; a real pull always ends long before [s].
    double max_duration_s = 0.0;
};

enum class ParamError : std::uint8_t {
    kNotFinite,
    kTrackLengthNotPositive,
    kMaxDurationNotPositive,
};

[[nodiscard]] std::optional<ParamError> validate(const Rules& rules) noexcept;

/// How a run ended.
enum class Ending : std::uint8_t {
    /// The sled reached the end of the track: the tractor reruns on a
    /// heavier sled (see sled::pull_off).
    kFullPull,
    /// The sled stopped because the engine stalled.
    kStalled,
    /// The sled stopped with the wheels spinning.
    kSpunOut,
    /// The sled stopped with the engine running and the tires gripping:
    /// the driver backed off.
    kStopped,
    kTimedOut,
};

struct Result {
    Ending ending = Ending::kTimedOut;
    /// How far the sled travelled from the start line [m].
    double distance_m = 0.0;
    double time_s = 0.0;
    double top_speed_m_s = 0.0;
};

/// A run: the pull, its clock, and its result once it has ended. Before the
/// sled first moves the run is waiting for the launch; after that, the
/// first time the sled stops (or reaches the end of the track) ends it.
struct State {
    pull::State pull;
    /// Steps since the start; the run's clock is steps × dt, exact where a
    /// running sum of dt would drift.
    std::uint64_t steps = 0;
    double top_speed_m_s = 0.0;
    std::optional<Result> result;
};

/// The tractor at the start line, ready to pull.
[[nodiscard]] State start(const pull::Model& model) noexcept;

/// One step of the pull while the run is on; once it has ended, nothing
/// moves and the output is empty.
pull::Output step(State& state, const pull::Model& model, const Rules& rules,
                  const vehicle::Input& input, double dt_s) noexcept;

constexpr void hash_append(hash::Fnv1a& hash, const State& state) noexcept {
    pull::hash_append(hash, state.pull);
    hash.add(state.steps);
}

}  // namespace tp::run
