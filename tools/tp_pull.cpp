// Headless pull runner. Placeholder until Milestone 1 provides a simulation:
// it will load a tractor and sled from data/, run a scripted pull and write
// RPM, slip, sled load, distance and pitch as CSV.

#include "tp/core/fixed_step_clock.hpp"

#include <cstdlib>
#include <iostream>

int main() {
    const auto clock = tp::FixedStepClock::create({});
    if (!clock) {
        return EXIT_FAILURE;
    }
    std::cout << "tp_pull: no simulation yet (step " << clock->step_s() << " s)\n";
    return EXIT_SUCCESS;
}
