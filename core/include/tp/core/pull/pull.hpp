#pragma once

#include "tp/core/chassis/chassis.hpp"
#include "tp/core/hash/fnv1a.hpp"
#include "tp/core/sled/sled.hpp"
#include "tp/core/vehicle/vehicle.hpp"

#include <cstdint>
#include <optional>

namespace tp::pull {

/// A pull: the tractor, the sled and the chain between them.
struct Params {
    vehicle::Params tractor;
    sled::Params sled;
};

enum class ParamError : std::uint8_t {
    kTractor,
    kSled,
};

[[nodiscard]] std::optional<ParamError> validate(const Params& params) noexcept;

class Model {
public:
    [[nodiscard]] static std::optional<Model> create(const Params& params) noexcept;

    [[nodiscard]] const vehicle::Model& tractor() const noexcept { return tractor_; }
    [[nodiscard]] const sled::Params& sled() const noexcept { return sled_; }

private:
    Model(const vehicle::Model& tractor, const sled::Params& sled) noexcept;

    vehicle::Model tractor_;
    sled::Params sled_;
};

struct State {
    vehicle::State tractor;
    sled::State sled;
};

/// The chain's tension and the direction it runs in, from the hook to the
/// hitch (a unit vector; z positive when the hitch is higher).
struct ChainForce {
    double tension_n = 0.0;
    double direction_x = 1.0;
    double direction_z = 0.0;
};

/// A point in the world and its velocity, for the chain's ends.
struct ChainEnd {
    double x_m = 0.0;
    double z_m = 0.0;
    double velocity_x_m_s = 0.0;
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
/// chain laid out with a millimetre of slack.
[[nodiscard]] State initial_state(const Model& model) noexcept;

/// One step: the chain force from where the hitch and hook are, then the
/// tractor and the sled each advance under it. The chain is explicit within
/// a step; at 1 kHz its oscillation (tens of rad/s) is well resolved.
Output step(State& state, const Model& model, const vehicle::Input& input, double dt_s) noexcept;

constexpr void hash_append(hash::Fnv1a& hash, const State& state) noexcept {
    vehicle::hash_append(hash, state.tractor);
    sled::hash_append(hash, state.sled);
}

}  // namespace tp::pull
