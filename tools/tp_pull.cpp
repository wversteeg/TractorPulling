// Headless simulation runner. Commands:
//
//   tp_pull engine-dyno <engine.toml> [-o <out.csv>]
//   tp_pull launch <engine.toml> <clutch.toml> <driveline.toml>
//                  [--gear N] [--resistance-kn F] [--dump] [-o <out.csv>]
//
// engine-dyno runs the engine alone on a virtual dynamometer: idle, rev up,
// then a slowly rising load until the engine stalls.
//
// launch selects a gear with the clutch pressed, revs the engine, then slips
// the clutch in over 3 s or, with --dump, drops it, and pulls at full
// throttle. The vehicle is a stand-in until the tires arrive (Milestone 1,
// step 4): its mass reflected to the axle through a fixed tire radius, plus
// a constant resistance force.
//
// Both write one CSV row every 10 ms so curves can be plotted and checked
// for plausibility. A scripted full pull follows in Milestone 1, step 7.

#include "tp/core/clutch/clutch.hpp"
#include "tp/core/driveline/driveline.hpp"
#include "tp/core/engine/engine.hpp"
#include "tp/core/units/conversions.hpp"
#include "tp/data/clutch/clutch_loader.hpp"
#include "tp/data/driveline/driveline_loader.hpp"
#include "tp/data/engine/engine_loader.hpp"

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {

using tp::units::rad_s_to_rpm;

constexpr double kDt_s = 0.001;
constexpr int kStepsPerRow = 10;
constexpr double kWattsPerKilowatt = 1000.0;
constexpr double kJoulesPerKilojoule = 1000.0;
constexpr double kNewtonsPerKilonewton = 1000.0;

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

// --- launch -----------------------------------------------------------------

// Stand-in vehicle until the tires arrive: no tire slip, so the vehicle's
// mass acts at the axle as m r².
constexpr double kVehicleMass_kg = 3500.0;
constexpr double kTireRadius_m = 0.8;
constexpr double kMetersPerSecondToKmh = 3.6;

constexpr double kLaunchSelectGear_s = 0.5;
constexpr double kLaunchRevUpEnd_s = 3.0;
constexpr double kLaunchSlipIn_s = 3.0;
constexpr double kLaunchDuration_s = 20.0;

struct LaunchOptions {
    int gear = 4;
    double resistance_n = 15000.0;
    bool dump = false;
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

int launch(const tp::engine::Params& engine_params, const tp::clutch::Params& clutch_params,
           tp::driveline::Params driveline_params, const LaunchOptions& options,
           std::ostream& csv) {
    csv << "time_s,pedal,gear,engine_rpm,axle_rpm,speed_kmh,distance_m,clutch_torque_nm,"
           "slip_power_kw,locked,boost\n";

    driveline_params.output_inertia_kg_m2 += kVehicleMass_kg * kTireRadius_m * kTireRadius_m;
    const double axle_load_nm = options.resistance_n * kTireRadius_m;

    tp::engine::State engine = tp::engine::initial_state(engine_params);
    tp::clutch::State clutch;
    tp::driveline::State driveline;
    double distance_m = 0.0;
    double dissipated_j = 0.0;
    double t_s = 0.0;
    for (int i = 0; t_s < kLaunchDuration_s; ++i) {
        t_s = i * kDt_s;
        const double pedal = launch_pedal(t_s, options.dump);
        const double throttle = t_s < kLaunchSelectGear_s ? 0.0 : 1.0;
        if (t_s >= kLaunchSelectGear_s && driveline.gear != options.gear) {
            const bool disengaged = tp::clutch::capacity_nm(clutch_params, pedal) == 0.0;
            if (!tp::driveline::shift(driveline, driveline_params, options.gear, disengaged)) {
                std::cerr << "cannot select gear " << options.gear << '\n';
                return EXIT_FAILURE;
            }
        }

        const tp::engine::Prediction e =
            tp::engine::predict(engine, engine_params, throttle, kDt_s);
        const tp::driveline::Prediction d =
            tp::driveline::predict(driveline, driveline_params, axle_load_nm, kDt_s);
        const tp::clutch::Output out =
            tp::clutch::step(clutch, clutch_params, {.pedal = pedal},
                             tp::engine::shaft_prediction(e), tp::driveline::input_shaft(d));
        tp::engine::advance(engine, engine_params, e, out.torque_nm);
        tp::driveline::advance(driveline, d, out.torque_nm);

        const double speed_m_s = driveline.output_omega_rad_s * kTireRadius_m;
        distance_m += speed_m_s * kDt_s;
        dissipated_j += out.slip_power_w * kDt_s;
        if (i % kStepsPerRow == 0) {
            csv << t_s << ',' << pedal << ',' << driveline.gear << ','
                << rad_s_to_rpm(engine.omega_rad_s) << ','
                << rad_s_to_rpm(driveline.output_omega_rad_s) << ','
                << speed_m_s * kMetersPerSecondToKmh << ',' << distance_m << ',' << out.torque_nm
                << ',' << out.slip_power_w / kWattsPerKilowatt << ',' << (clutch.locked ? 1 : 0)
                << ',' << engine.boost << '\n';
        }
        if (!engine.running) {
            break;
        }
    }

    std::cerr << "gear " << options.gear << ", " << options.resistance_n / kNewtonsPerKilonewton
              << " kN, " << (options.dump ? "dumped" : "slipped in") << ": ";
    if (engine.running) {
        std::cerr << "pulling at "
                  << driveline.output_omega_rad_s * kTireRadius_m * kMetersPerSecondToKmh
                  << " km/h, " << rad_s_to_rpm(engine.omega_rad_s) << " rpm, " << distance_m
                  << " m after " << t_s << " s\n";
    } else {
        std::cerr << "engine stalled at " << t_s << " s after " << distance_m << " m\n";
    }
    std::cerr << "heat into the clutch " << dissipated_j / kJoulesPerKilojoule << " kJ\n";
    return EXIT_SUCCESS;
}

// --- command line -----------------------------------------------------------

struct Options {
    std::vector<std::string_view> positional;
    std::optional<std::string_view> output_path;
    LaunchOptions launch;
};

template <typename T>
std::optional<T> parse_number(std::string_view text) {
    T value{};
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size()) {
        return std::nullopt;
    }
    return value;
}

std::optional<Options> parse_options(std::span<char* const> args) {
    Options options;
    for (std::size_t i = 0; i < args.size(); ++i) {
        const std::string_view arg{args[i]};
        const bool has_value = i + 1 < args.size();
        if (arg == "-o" && has_value) {
            options.output_path = args[++i];
        } else if (arg == "--dump") {
            options.launch.dump = true;
        } else if (arg == "--gear" && has_value) {
            const auto gear = parse_number<int>(args[++i]);
            if (!gear) {
                return std::nullopt;
            }
            options.launch.gear = *gear;
        } else if (arg == "--resistance-kn" && has_value) {
            const auto kilonewtons = parse_number<double>(args[++i]);
            if (!kilonewtons || *kilonewtons < 0.0) {
                return std::nullopt;
            }
            options.launch.resistance_n = *kilonewtons * kNewtonsPerKilonewton;
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
                 "       tp_pull launch <engine.toml> <clutch.toml> <driveline.toml>\n"
                 "                      [--gear N] [--resistance-kn F] [--dump] [-o <out.csv>]\n";
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

    if (command == "engine-dyno" && positional.size() == 1) {
        const auto engine = load_or_report(tp::data::load_engine(positional[0]));
        if (!engine) {
            return EXIT_FAILURE;
        }
        return with_output(*options, [&](std::ostream& csv) { return engine_dyno(*engine, csv); });
    }
    if (command == "launch" && positional.size() == 3) {
        const auto engine = load_or_report(tp::data::load_engine(positional[0]));
        const auto clutch = load_or_report(tp::data::load_clutch(positional[1]));
        const auto driveline = load_or_report(tp::data::load_driveline(positional[2]));
        if (!engine || !clutch || !driveline) {
            return EXIT_FAILURE;
        }
        return with_output(*options, [&](std::ostream& csv) {
            return launch(*engine, *clutch, *driveline, options->launch, csv);
        });
    }
    return usage();
}
