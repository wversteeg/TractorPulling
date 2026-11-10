#pragma once

#include "tp/core/vehicle/vehicle.hpp"

namespace tp::vehicle {

/// What the puller sets on the tractor before a pull: where the ballast
/// sits (chassis::Ballast, 0 rearmost to 1 foremost), the rear tires'
/// inflation pressure (tire::Pressure) [bar] and the hitch's height on an
/// adjustable pulling hitch (chassis::Params::hitch_min_height_m) [m]; 0
/// leaves the hitch in its top hole. Parameters only: the simulation runs
/// the tuned tractor as any other.
///
/// A higher hitch puts more of the sled's pull on the rear tires (more
/// grip) but lifts the front sooner.
struct Tuning {
    double ballast_position = 0.0;
    double rear_tire_pressure_bar = 0.0;
    double hitch_height_m = 0.0;
};

/// The tractor as built: its ballast where it sits, its rear tires at their
/// reference pressure, its hitch in the top hole.
[[nodiscard]] Tuning as_built(const Params& params) noexcept;

/// What Tuning::hitch_height_m keeps for a hitch the puller sets at
/// `wanted_m`: 0 (the top hole) at or above `params`' top hole, so that a
/// higher drawbar bought later raises it; below, the height itself.
[[nodiscard]] double hitch_setting(const Params& params, double wanted_m) noexcept;

/// The tractor with `tuning` applied, each setting held to its range. A
/// tractor without ballast, a set tire pressure or an adjustable hitch
/// keeps that part as it is;
/// as_built() gives `params` unchanged, bit for bit.
[[nodiscard]] Params tuned(const Params& params, const Tuning& tuning) noexcept;

}  // namespace tp::vehicle
