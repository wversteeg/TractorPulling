// Headless simulation runner. Commands:
//
//   tp_pull engine-dyno <engine.toml> [-o <out.csv>]
//   tp_pull pull <tractor.toml> [--gear N] [--resistance-kn F] [--dump]
//                [-o <out.csv>]
//
// engine-dyno runs the engine alone on a virtual dynamometer: idle, rev up,
// then a slowly rising load until the engine stalls.
//
// pull drives the whole tractor: idle in neutral, select the gear with the
// clutch pressed, rev up, slip the clutch in over 3 s or, with --dump, drop
// it, and pull at full throttle until 100 m, a stall or 30 s. The sled is a
// constant drawbar force until Milestone 1, step 6.
//
// Both write one CSV row every 10 ms so curves can be plotted and checked
// for plausibility. A scripted full pull follows in Milestone 1, step 7.

#include "tp/core/driveline/driveline.hpp"
#include "tp/core/engine/engine.hpp"
#include "tp/core/units/conversions.hpp"
#include "tp/core/vehicle/vehicle.hpp"
#include "tp/data/engine/engine_loader.hpp"
#include "tp/data/vehicle/tractor_loader.hpp"

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
using tp::units::rad_to_deg;

constexpr double kDt_s = 0.001;
constexpr int kStepsPerRow = 10;
constexpr double kWattsPerKilowatt = 1000.0;
constexpr double kJoulesPerKilojoule = 1000.0;
constexpr double kNewtonsPerKilonewton = 1000.0;
constexpr double kPercent = 100.0;

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

// --- pull -------------------------------------------------------------------

constexpr double kMetersPerSecondToKmh = 3.6;
constexpr double kTrackLength_m = 100.0;

constexpr double kPullSelectGear_s = 0.5;
constexpr double kPullRevUpEnd_s = 3.0;
constexpr double kPullSlipIn_s = 3.0;
constexpr double kPullMaxDuration_s = 30.0;

struct PullOptions {
    int gear = 4;
    double resistance_n = 8000.0;
    bool dump = false;
};

tp::vehicle::Input pull_input(double t_s, const PullOptions& options) {
    tp::vehicle::Input input{.throttle = 1.0, .clutch_pedal = 1.0, .gear = options.gear};
    if (t_s < kPullSelectGear_s) {
        return {.throttle = 0.0, .clutch_pedal = 1.0, .gear = tp::driveline::kNeutral};
    }
    if (t_s >= kPullRevUpEnd_s) {
        input.clutch_pedal =
            options.dump ? 0.0
                         : std::clamp(1.0 - (t_s - kPullRevUpEnd_s) / kPullSlipIn_s, 0.0, 1.0);
    }
    return input;
}

int pull(const tp::vehicle::Model& model, const PullOptions& options, std::ostream& csv) {
    csv << "time_s,pedal,gear,engine_rpm,axle_rpm,speed_kmh,distance_m,slip_ratio,"
           "tire_force_kn,rear_load_kn,front_load_kn,wheelie_bar_load_kn,pitch_deg,"
           "clutch_torque_nm,clutch_slip_power_kw,locked,boost\n";

    tp::vehicle::State state = tp::vehicle::initial_state(model);
    double max_slip = 0.0;
    double dissipated_j = 0.0;
    bool front_lifted = false;
    bool on_wheelie_bar = false;
    double max_pitch_rad = 0.0;
    double t_s = 0.0;
    for (int i = 0; t_s < kPullMaxDuration_s; ++i) {
        t_s = i * kDt_s;
        const tp::vehicle::Input input = pull_input(t_s, options);
        const tp::vehicle::Output out =
            tp::vehicle::step(state, model, input, options.resistance_n, kDt_s);
        if (out.shift_refused) {
            std::cerr << "cannot select gear " << options.gear << '\n';
            return EXIT_FAILURE;
        }
        if (t_s >= kPullRevUpEnd_s) {
            max_slip = std::max(max_slip, out.slip_ratio);
        }
        front_lifted = front_lifted || out.axle_loads.front_lifted;
        on_wheelie_bar = on_wheelie_bar || out.axle_loads.wheelie_bar_n > 0.0;
        max_pitch_rad = std::max(max_pitch_rad, state.chassis.pitch_rad);
        dissipated_j += out.clutch_slip_power_w * kDt_s;

        if (i % kStepsPerRow == 0) {
            csv << t_s << ',' << input.clutch_pedal << ',' << state.driveline.gear << ','
                << rad_s_to_rpm(state.engine.omega_rad_s) << ','
                << rad_s_to_rpm(state.driveline.output_omega_rad_s) << ','
                << state.chassis.velocity_m_s * kMetersPerSecondToKmh << ','
                << state.chassis.position_m << ',' << out.slip_ratio << ','
                << out.tire_force_n / kNewtonsPerKilonewton << ','
                << out.axle_loads.rear_n / kNewtonsPerKilonewton << ','
                << out.axle_loads.front_n / kNewtonsPerKilonewton << ','
                << out.axle_loads.wheelie_bar_n / kNewtonsPerKilonewton << ','
                << rad_to_deg(state.chassis.pitch_rad) << ',' << out.clutch_torque_nm << ','
                << out.clutch_slip_power_w / kWattsPerKilowatt << ','
                << (state.clutch.locked ? 1 : 0) << ',' << state.engine.boost << '\n';
        }
        if (!state.engine.running || state.chassis.position_m >= kTrackLength_m) {
            break;
        }
    }

    std::cerr << "gear " << options.gear << ", " << options.resistance_n / kNewtonsPerKilonewton
              << " kN, " << (options.dump ? "dumped" : "slipped in") << ": ";
    if (state.chassis.position_m >= kTrackLength_m) {
        std::cerr << "full pull, " << kTrackLength_m << " m in " << t_s << " s";
    } else if (!state.engine.running) {
        std::cerr << "engine stalled after " << state.chassis.position_m << " m";
    } else {
        std::cerr << state.chassis.position_m << " m after " << t_s << " s at "
                  << state.chassis.velocity_m_s * kMetersPerSecondToKmh << " km/h";
    }
    std::cerr << "; peak wheel slip " << max_slip * kPercent << " %; peak pitch "
              << rad_to_deg(max_pitch_rad) << "°" << (front_lifted ? ", front lifted" : "")
              << (on_wheelie_bar ? ", on the wheelie bar" : "") << "; heat into the clutch "
              << dissipated_j / kJoulesPerKilojoule << " kJ\n";
    return EXIT_SUCCESS;
}

// --- command line -----------------------------------------------------------

struct Options {
    std::vector<std::string_view> positional;
    std::optional<std::string_view> output_path;
    PullOptions pull;
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
            options.pull.dump = true;
        } else if (arg == "--gear" && has_value) {
            const auto gear = parse_number<int>(args[++i]);
            if (!gear) {
                return std::nullopt;
            }
            options.pull.gear = *gear;
        } else if (arg == "--resistance-kn" && has_value) {
            const auto kilonewtons = parse_number<double>(args[++i]);
            if (!kilonewtons || *kilonewtons < 0.0) {
                return std::nullopt;
            }
            options.pull.resistance_n = *kilonewtons * kNewtonsPerKilonewton;
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
                 "       tp_pull pull <tractor.toml> [--gear N] [--resistance-kn F] [--dump]\n"
                 "                    [-o <out.csv>]\n";
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
    if (command == "pull" && positional.size() == 1) {
        const auto params = load_or_report(tp::data::load_tractor(positional[0]));
        if (!params) {
            return EXIT_FAILURE;
        }
        const auto model = tp::vehicle::Model::create(*params);
        if (!model) {
            std::cerr << positional[0] << ": inconsistent tractor\n";
            return EXIT_FAILURE;
        }
        return with_output(*options,
                           [&](std::ostream& csv) { return pull(*model, options->pull, csv); });
    }
    return usage();
}
