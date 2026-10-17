#pragma once

#include "tp/core/coupling/shaft_prediction.hpp"

namespace tp::clutch::testing {

// A free rotating mass with an external torque, used as either side of the
// clutch in tests. Its speed is exactly linear in the coupling torque.
struct Flywheel {
    double inertia_kg_m2 = 1.0;
    double omega_rad_s = 0.0;
    double external_torque_nm = 0.0;
    /// For a driveline whose resistance (rolling, sled) can hold it still but
    /// never drive it backwards.
    bool cannot_reverse = false;

    [[nodiscard]] coupling::ShaftPrediction predict(double dt_s) const {
        return {
            .free_omega_next_rad_s = omega_rad_s + dt_s / inertia_kg_m2 * external_torque_nm,
            .omega_per_torque = dt_s / inertia_kg_m2,
        };
    }
    void advance(double dt_s, double coupling_torque_nm) {
        omega_rad_s += dt_s / inertia_kg_m2 * (external_torque_nm + coupling_torque_nm);
        if (cannot_reverse && omega_rad_s < 0.0) {
            omega_rad_s = 0.0;
        }
    }
    [[nodiscard]] double momentum() const { return inertia_kg_m2 * omega_rad_s; }
    [[nodiscard]] double kinetic_energy_j() const {
        return 0.5 * inertia_kg_m2 * omega_rad_s * omega_rad_s;
    }
};

}  // namespace tp::clutch::testing
