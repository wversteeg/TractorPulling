#pragma once

#include "tp/core/vehicle/vehicle.hpp"

namespace tp::vehicle {

/// What the puller sets on the tractor before a pull: where the ballast
/// sits (chassis::Ballast, 0 rearmost to 1 foremost) and the rear tires'
/// inflation pressure (tire::Pressure) [bar]. Parameters only: the
/// simulation runs the tuned tractor as any other.
struct Tuning {
    double ballast_position = 0.0;
    double rear_tire_pressure_bar = 0.0;
};

/// The tractor as built: its ballast where it sits, its rear tires at their
/// reference pressure.
[[nodiscard]] Tuning as_built(const Params& params) noexcept;

/// The tractor with `tuning` applied, each setting held to its range. A
/// tractor without ballast or a set tire pressure keeps that part as it is;
/// as_built() gives `params` unchanged, bit for bit.
[[nodiscard]] Params tuned(const Params& params, const Tuning& tuning) noexcept;

}  // namespace tp::vehicle
