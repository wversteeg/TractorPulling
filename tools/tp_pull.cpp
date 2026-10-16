// Headless simulation runner. Commands:
//
//   tp_pull engine-dyno <engine.toml> [-o <out.csv>]
//
// engine-dyno runs the engine alone on a virtual dynamometer: idle, rev up,
// then a slowly rising load until the engine stalls. It writes one CSV row
// every 10 ms, so curves can be plotted and checked for plausibility.
//
// A scripted full pull (tractor and sled) follows in Milestone 1, step 7.

#include "tp/core/engine/engine.hpp"
#include "tp/core/units/conversions.hpp"
#include "tp/data/engine/engine_loader.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <span>
#include <string_view>

namespace {

constexpr double kDt_s = 0.001;
constexpr int kStepsPerRow = 10;

constexpr double kIdleEnd_s = 1.0;
constexpr double kThrottleRampEnd_s = 2.0;
constexpr double kLoadStart_s = 5.0;
constexpr double kLoadRate_nm_per_s = 25.0;
constexpr double kMaxDuration_s = 30.0;
constexpr double kWattsPerKilowatt = 1000.0;

tp::engine::Input dyno_input(double t_s) {
    const double throttle =
        std::clamp((t_s - kIdleEnd_s) / (kThrottleRampEnd_s - kIdleEnd_s), 0.0, 1.0);
    const double load_nm = std::max(0.0, (t_s - kLoadStart_s) * kLoadRate_nm_per_s);
    return {.throttle = throttle, .load_torque_nm = load_nm};
}

int engine_dyno(const tp::engine::Params& params, std::ostream& csv) {
    csv << "time_s,throttle,load_nm,rpm,fuel,boost,combustion_torque_nm,shaft_power_kw\n";

    tp::engine::State state = tp::engine::initial_state(params);
    double peak_power_kw = 0.0;
    double peak_power_rpm = 0.0;
    double stall_load_nm = 0.0;
    double t_s = 0.0;
    for (int i = 0; t_s < kMaxDuration_s; ++i) {
        t_s = i * kDt_s;
        const tp::engine::Input input = dyno_input(t_s);
        const tp::engine::Output out = tp::engine::step(state, params, input, kDt_s);

        const double rpm = tp::units::rad_s_to_rpm(state.omega_rad_s);
        const double power_kw = input.load_torque_nm * state.omega_rad_s / kWattsPerKilowatt;
        if (power_kw > peak_power_kw) {
            peak_power_kw = power_kw;
            peak_power_rpm = rpm;
        }
        if (i % kStepsPerRow == 0) {
            csv << t_s << ',' << input.throttle << ',' << input.load_torque_nm << ',' << rpm << ','
                << out.fuel << ',' << state.boost << ',' << out.combustion_torque_nm << ','
                << power_kw << '\n';
        }
        if (!state.running) {
            stall_load_nm = input.load_torque_nm;
            break;
        }
    }

    std::cerr << "peak shaft power " << peak_power_kw << " kW at " << peak_power_rpm << " rpm\n";
    if (state.running) {
        std::cerr << "no stall within " << kMaxDuration_s << " s\n";
    } else {
        std::cerr << "stalled at " << t_s << " s under " << stall_load_nm << " N·m\n";
    }
    return EXIT_SUCCESS;
}

int usage() {
    std::cerr << "usage: tp_pull engine-dyno <engine.toml> [-o <out.csv>]\n";
    return EXIT_FAILURE;
}

}  // namespace

int main(int argc, char** argv) {
    const std::span args{argv, static_cast<std::size_t>(argc)};
    if (args.size() < 3 || std::string_view{args[1]} != "engine-dyno") {
        return usage();
    }
    const char* output_path = nullptr;
    if (args.size() == 5 && std::string_view{args[3]} == "-o") {
        output_path = args[4];
    } else if (args.size() != 3) {
        return usage();
    }

    const auto params = tp::data::load_engine(args[2]);
    if (!params) {
        std::cerr << params.error().message << '\n';
        return EXIT_FAILURE;
    }

    if (output_path == nullptr) {
        return engine_dyno(params.value(), std::cout);
    }
    std::ofstream file{output_path};
    if (!file) {
        std::cerr << output_path << ": cannot open for writing\n";
        return EXIT_FAILURE;
    }
    return engine_dyno(params.value(), file);
}
