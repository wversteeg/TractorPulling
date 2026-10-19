#pragma once

#include "tp/core/pull/pull.hpp"
#include "tp/core/vehicle/vehicle.hpp"

#include <cstdint>
#include <optional>

namespace tp::run {

/// The track a run is pulled on, and its lane.
struct Rules {
    /// The run ends when the sled reaches this far [m].
    double track_length_m = 0.0;
    /// Safety cut-off; a real pull always ends long before [s].
    double max_duration_s = 0.0;
    /// Between the chalk lines, centred on the track [m].
    double lane_width_m = 0.0;
    /// Before this line (from the start line) a tractor tire touching a
    /// chalk line disqualifies the pull; past it, it may [m]. The sled may
    /// always cross the lines.
    double full_pull_line_m = 0.0;
};

enum class ParamError : std::uint8_t {
    kNotFinite,
    kTrackLengthNotPositive,
    kMaxDurationNotPositive,
    kLaneWidthNotPositive,
    kFullPullLineNotPositive,
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
    /// A tractor tire touched a chalk line before the full-pull line: the
    /// pull is disqualified and counts 0 m.
    kOutOfBounds,
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

/// Whether a tractor tire's outer edge touches a chalk line before the
/// full-pull line. The tires are taken at the axles: the rear pair a rear
/// track apart, the front pair a front track apart, each as wide as its
/// section.
[[nodiscard]] bool touches_line(const pull::Model& model, const pull::State& state,
                                const Rules& rules) noexcept;

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
