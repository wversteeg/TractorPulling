// Headless simulation runner. Commands:
//
//   tp_pull engine-dyno <engine.toml> [-o <out.csv>]
//   tp_pull clutch-launch <engine.toml> <clutch.toml> [--dump] [-o <out.csv>]
//
// engine-dyno runs the engine alone on a virtual dynamometer: idle, rev up,
// then a slowly rising load until the engine stalls.
//
// clutch-launch revs the engine with the clutch pressed, then either slips
// the clutch in over 3 s or, with --dump, drops it instantly, against a
// heavy stand-in for the driveline (until Milestone 1, step 3).
//
// Both write one CSV row every 10 ms so curves can be plotted and checked
// for plausibility. A scripted full pull follows in Milestone 1, step 7.

#include "tp/core/clutch/clutch.hpp"
#include "tp/core/coupling/shaft_prediction.hpp"
#include "tp/core/engine/engine.hpp"
#include "tp/core/units/conversions.hpp"
#include "tp/data/clutch/clutch_loader.hpp"
#include "tp/data/engine/engine_loader.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {

using tp::units::rad_s_to_rpm;

constexpr double kDt_s = 0.001;
constexpr int kStepsPerRow = 10;
constexpr double kWattsPerKilowatt = 1000.0;
constexpr double kJoulesPerKilojoule = 1000.0;

// --- engine-dyno ------------------------------------------------------------

constexpr double kDynoIdleEnd_s = 1.0;
constexpr double kDynoThrottleRampEnd_s = 2.0;
constexpr double kDynoLoadStart_s = 5.0;
constexpr double kDynoLoadRate_nm_per_s = 25.0;
constexpr double kDynoMaxDuration_s = 30.0;

tp::engine::Input dyno_input(double t_s) {
    const double throttle =
        std::clamp((t_s - kDynoIdleEnd_s) / (kDynoThrottleRampEnd_s - kDynoIdleEnd_s), 0.0, 1.0);
    const double load_nm = std::max(0.0, (t_s - kDynoLoadStart_s) * kDynoLoadRate_nm_per_s);
    return {.throttle = throttle, .load_torque_nm = load_nm};
}

int engine_dyno(const tp::engine::Params& params, std::ostream& csv) {
    csv << "time_s,throttle,load_nm,rpm,fuel,boost,combustion_torque_nm,shaft_power_kw\n";

    tp::engine::State state = tp::engine::initial_state(params);
    double peak_power_kw = 0.0;
    double peak_power_rpm = 0.0;
    double stall_load_nm = 0.0;
    double t_s = 0.0;
    for (int i = 0; t_s < kDynoMaxDuration_s; ++i) {
        t_s = i * kDt_s;
        const tp::engine::Input input = dyno_input(t_s);
        const tp::engine::Output out = tp::engine::step(state, params, input, kDt_s);

        const double rpm = rad_s_to_rpm(state.omega_rad_s);
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
        std::cerr << "no stall within " << kDynoMaxDuration_s << " s\n";
    } else {
        std::cerr << "stalled at " << t_s << " s under " << stall_load_nm << " N·m\n";
    }
    return EXIT_SUCCESS;
}

// --- clutch-launch ----------------------------------------------------------

// Tractor and sled reflected to the clutch shaft: an inertia the engine's
// flywheel alone cannot bring up to speed, and a resistance the engine can
// carry once moving. Replaced by the real driveline in Milestone 1, step 3.
constexpr double kStandInInertia_kg_m2 = 10.0;
constexpr double kStandInResistance_nm = 100.0;

constexpr double kLaunchRevUpEnd_s = 3.0;
constexpr double kLaunchSlipIn_s = 3.0;
constexpr double kLaunchDuration_s = 14.0;

// Stand-in driveline: a flywheel whose resistance can hold it still but
// never drive it backwards.
struct StandInDriveline {
    double omega_rad_s = 0.0;

    [[nodiscard]] tp::coupling::ShaftPrediction predict() const {
        return {.free_omega_next_rad_s =
                    omega_rad_s - kDt_s / kStandInInertia_kg_m2 * kStandInResistance_nm,
                .omega_per_torque = kDt_s / kStandInInertia_kg_m2};
    }
    void advance(double clutch_torque_nm) {
        omega_rad_s += kDt_s / kStandInInertia_kg_m2 * (clutch_torque_nm - kStandInResistance_nm);
        omega_rad_s = std::max(omega_rad_s, 0.0);
    }
};

double launch_pedal(double t_s, bool dump) {
    if (t_s < kLaunchRevUpEnd_s) {
        return 1.0;
    }
    if (dump) {
        return 0.0;
    }
    return std::clamp(1.0 - (t_s - kLaunchRevUpEnd_s) / kLaunchSlipIn_s, 0.0, 1.0);
}

int clutch_launch(const tp::engine::Params& engine_params, const tp::clutch::Params& clutch_params,
                  bool dump, std::ostream& csv) {
    csv << "time_s,throttle,pedal,engine_rpm,driveline_rpm,clutch_torque_nm,capacity_nm,"
           "slip_power_kw,locked,boost\n";

    constexpr double kThrottle = 1.0;
    tp::engine::State engine = tp::engine::initial_state(engine_params);
    tp::clutch::State clutch;
    StandInDriveline driveline;
    double dissipated_j = 0.0;
    double locked_at_s = -1.0;
    double t_s = 0.0;
    for (int i = 0; t_s < kLaunchDuration_s; ++i) {
        t_s = i * kDt_s;
        const double pedal = launch_pedal(t_s, dump);
        const tp::engine::Prediction prediction =
            tp::engine::predict(engine, engine_params, kThrottle, kDt_s);
        const tp::clutch::Output out =
            tp::clutch::step(clutch, clutch_params, {.pedal = pedal},
                             tp::engine::shaft_prediction(prediction), driveline.predict());
        tp::engine::advance(engine, engine_params, prediction, out.torque_nm);
        driveline.advance(out.torque_nm);

        dissipated_j += out.slip_power_w * kDt_s;
        if (clutch.locked && locked_at_s < 0.0 && t_s >= kLaunchRevUpEnd_s) {
            locked_at_s = t_s;
        }
        if (i % kStepsPerRow == 0) {
            csv << t_s << ',' << kThrottle << ',' << pedal << ','
                << rad_s_to_rpm(engine.omega_rad_s) << ',' << rad_s_to_rpm(driveline.omega_rad_s)
                << ',' << out.torque_nm << ',' << out.capacity_nm << ','
                << out.slip_power_w / kWattsPerKilowatt << ',' << (clutch.locked ? 1 : 0) << ','
                << engine.boost << '\n';
        }
        if (!engine.running) {
            break;
        }
    }

    std::cerr << (dump ? "dumped" : "slipped in") << " the clutch: ";
    if (engine.running) {
        std::cerr << "launched, locked at " << locked_at_s << " s, now "
                  << rad_s_to_rpm(engine.omega_rad_s) << " rpm\n";
    } else {
        std::cerr << "engine stalled at " << t_s << " s\n";
    }
    std::cerr << "heat into the clutch " << dissipated_j / kJoulesPerKilojoule << " kJ\n";
    return EXIT_SUCCESS;
}

// --- command line -----------------------------------------------------------

struct Options {
    std::vector<std::string_view> positional;
    std::optional<std::string_view> output_path;
    bool dump = false;
};

std::optional<Options> parse_options(std::span<char* const> args) {
    Options options;
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string_view arg{args[i]};
        if (arg == "-o" && i + 1 < args.size()) {
            options.output_path = args[++i];
        } else if (arg == "--dump") {
            options.dump = true;
        } else if (arg.starts_with('-')) {
            return std::nullopt;
        } else {
            options.positional.push_back(arg);
        }
    }
    return options;
}

int usage() {
    std::cerr << "usage: tp_pull engine-dyno <engine.toml> [-o <out.csv>]\n"
                 "       tp_pull clutch-launch <engine.toml> <clutch.toml> [--dump] "
                 "[-o <out.csv>]\n";
    return EXIT_FAILURE;
}

template <typename T>
std::optional<T> load_or_report(const tp::data::Result<T>& result) {
    if (!result) {
        std::cerr << result.error().message << '\n';
        return std::nullopt;
    }
    return result.value();
}

// Runs `command` with CSV going to the -o file, or to stdout.
int with_output(const Options& options, const std::function<int(std::ostream&)>& command) {
    if (!options.output_path) {
        return command(std::cout);
    }
    std::ofstream file{std::string{*options.output_path}};
    if (!file) {
        std::cerr << *options.output_path << ": cannot open for writing\n";
        return EXIT_FAILURE;
    }
    return command(file);
}

}  // namespace

int main(int argc, char** argv) {
    const std::span<char* const> args{argv, static_cast<std::size_t>(argc)};
    if (args.size() < 2) {
        return usage();
    }
    const std::string_view command{args[1]};
    const std::optional<Options> options = parse_options(args.subspan(2));
    if (!options) {
        return usage();
    }
    const auto& positional = options->positional;

    if (command == "engine-dyno" && positional.size() == 1 && !options->dump) {
        const auto engine = load_or_report(tp::data::load_engine(positional[0]));
        if (!engine) {
            return EXIT_FAILURE;
        }
        return with_output(*options, [&](std::ostream& csv) { return engine_dyno(*engine, csv); });
    }
    if (command == "clutch-launch" && positional.size() == 2) {
        const auto engine = load_or_report(tp::data::load_engine(positional[0]));
        const auto clutch = load_or_report(tp::data::load_clutch(positional[1]));
        if (!engine || !clutch) {
            return EXIT_FAILURE;
        }
        return with_output(*options, [&](std::ostream& csv) {
            return clutch_launch(*engine, *clutch, options->dump, csv);
        });
    }
    return usage();
}
