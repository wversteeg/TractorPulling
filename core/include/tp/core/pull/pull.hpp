#pragma once

#include "tp/core/chassis/chassis.hpp"
#include "tp/core/hash/fnv1a.hpp"
#include "tp/core/sled/sled.hpp"
#include "tp/core/track/track.hpp"
#include "tp/core/vehicle/vehicle.hpp"

#include <cstdint>
#include <optional>

namespace tp::pull {

/// A pull: the tractor, the sled and the chain between them, on a track.
struct Params {
    vehicle::Params tractor;
    sled::Params sled;
    track::Params track;
};

enum class ParamError : std::uint8_t {
    kTractor,
    kSled,
    kTrack,
};

[[nodiscard]] std::optional<ParamError> validate(const Params& params) noexcept;

class Model {
public:
    [[nodiscard]] static std::optional<Model> create(const Params& params) noexcept;

    [[nodiscard]] const vehicle::Model& tractor() const noexcept { return tractor_; }
    [[nodiscard]] const sled::Params& sled() const noexcept { return sled_; }
    [[nodiscard]] const track::Model& track() const noexcept { return track_; }
    /// Where the start line is in the pull's coordinates (the tractor's rear
    /// axle starts at 0): the sled's rear wheels at the start [m].
    [[nodiscard]] double start_line_m() const noexcept { return start_line_m_; }

private:
    Model(const vehicle::Model& tractor, const sled::Params& sled,
          const track::Model& track) noexcept;

    vehicle::Model tractor_;
    sled::Params sled_;
    track::Model track_;
    double start_line_m_ = 0.0;
};

/// What the sled does: pulled on the chain, held where it stands (before
/// the flag, while the tractor takes up the slack), or driving itself back
/// to the start line after the pull (sled::drive_back).
enum class SledMode : std::uint8_t {
    kOnTheChain,
    kHeld,
    kDrivingBack,
};

struct State {
    vehicle::State tractor;
    sled::State sled;
    /// The chain between the hitch and the sled's hook: off, it pulls
    /// neither.
    bool hooked = true;
    SledMode sled_mode = SledMode::kOnTheChain;
};

/// The chain's tension and the direction it runs in, from the hook to the
/// hitch (a unit vector in the world; y positive when the hitch is further
/// left, z when it is higher).
struct ChainForce {
    double tension_n = 0.0;
    double direction_x = 1.0;
    double direction_y = 0.0;
    double direction_z = 0.0;
};

/// A point in the world and its velocity, for the chain's ends.
struct ChainEnd {
    double x_m = 0.0;
    double y_m = 0.0;
    double z_m = 0.0;
    double velocity_x_m_s = 0.0;
    double velocity_y_m_s = 0.0;
    double velocity_z_m_s = 0.0;
};

/// The chain pulls only when stretched beyond its length: spring plus damper
/// along its line, never pushing.
[[nodiscard]] ChainForce chain_force(const sled::Chain& chain, const ChainEnd& hitch,
                                     const ChainEnd& hook) noexcept;

struct Output {
    vehicle::Output tractor;
    sled::Loads sled;
    ChainForce chain;
};

/// Tractor idling in neutral at the start line, the sled behind it with the
/// chain laid out with a millimetre of slack, hooked on, the sled on it.
[[nodiscard]] State initial_state(const Model& model) noexcept;

/// One step: the chain force from where the hitch and hook are, the ground
/// under each of the tractor's axles and under the sled's pan and wheels,
/// then the tractor and the sled each advance under the chain. The chain is explicit within
/// a step; at 1 kHz its oscillation (tens of rad/s) is well resolved.
///
/// The chain runs between the hitch and the hook wherever they are: off
/// the tractor's line it pulls the tractor's hitch and the sled's hook
/// sideways, and the sled swings behind the tractor as a trailer does (see
/// sled::Params). Unhooked it pulls neither; a held sled stands still
/// whatever the chain does; one driving back goes its own way.
Output step(State& state, const Model& model, const vehicle::Input& input, double dt_s) noexcept;

/// Where the chain's hitch end and the sled's hook are now.
[[nodiscard]] ChainEnd hitch_end(const Model& model, const State& state) noexcept;
[[nodiscard]] ChainEnd hook_end(const Model& model, const State& state) noexcept;
constexpr void hash_append(hash::Fnv1a& hash, const State& state) noexcept {
    vehicle::hash_append(hash, state.tractor);
    sled::hash_append(hash, state.sled);
    hash.add(state.hooked);
    hash.add(static_cast<std::uint64_t>(state.sled_mode));
}

}  // namespace tp::pull
