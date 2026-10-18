// Headless simulation runner. Commands:
//
//   tp_pull engine-dyno <engine.toml> [-o <out.csv>]
//   tp_pull pull <tractor.toml> [--gear N] [--resistance-kn F] [--dump]
//                [-o <out.csv>]
//   tp_pull run <tractor.toml> <sled.toml> <track.toml> [--gear N] [--dump]
//               [-o <out.csv>]
//   tp_pull check <tractor.toml> <class.toml>
//
// engine-dyno runs the engine alone on a virtual dynamometer: idle, rev up,
// then a slowly rising load until the engine stalls.
//
// pull drives the whole tractor: idle in neutral, select the gear with the
// clutch pressed, rev up, slip the clutch in over 3 s or, with --dump, drop
// it, and pull at full throttle until 100 m, a stall or 30 s, against a
// constant drawbar force: a test stand for the tractor.
//
// run drives the same way against a sled on its chain, by the track's rules:
// the run ends at the sled's first stop or with a full pull, and every full
// pull is followed by a pull-off on a heavier sled. It prints each result.
//
// Both write one CSV row every 10 ms so curves can be plotted and checked
// for plausibility. A scripted full pull follows in Milestone 1, step 7.
//
// check compares a tractor with a pulling class's rules and exits non-zero
// if it may not compete.

#include "tp/core/driveline/driveline.hpp"
#include "tp/core/engine/engine.hpp"
#include "tp/core/pull/pull.hpp"
#include "tp/core/rules/class_rules.hpp"
#include "tp/core/run/run.hpp"
#include "tp/core/sled/sled.hpp"
#include "tp/core/soil/soil.hpp"
#include "tp/core/units/conversions.hpp"
#include "tp/core/vehicle/vehicle.hpp"
#include "tp/data/engine/engine_loader.hpp"
#include "tp/data/rules/class_rules_loader.hpp"
#include "tp/data/run/track_loader.hpp"
#include "tp/data/sled/sled_loader.hpp"
#include "tp/data/vehicle/tractor_loader.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iomanip>
#include <ios>
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
// The reference track, moist and rolled clay and sand, until track files
// describe their soil.
constexpr tp::soil::Composition kReferenceSoil{
    .clay_fraction = 0.5, .moisture = 0.14, .compaction = 0.8};

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
    const tp::soil::Surface surface = tp::soil::surface(kReferenceSoil);
    const tp::vehicle::Ground ground{.rear = surface, .front = surface};
    double t_s = 0.0;
    for (int i = 0; t_s < kPullMaxDuration_s; ++i) {
        t_s = i * kDt_s;
        const tp::vehicle::Input input = pull_input(t_s, options);
        const tp::vehicle::Output out =
            tp::vehicle::step(state, model, input, ground, {.pull_n = options.resistance_n}, kDt_s);
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

// --- run --------------------------------------------------------------------

// Pull-offs after full pulls before the tool gives up making the sled heavier.
constexpr int kMaxPullOffs = 10;

std::string_view describe(tp::run::Ending ending) {
    switch (ending) {
        case tp::run::Ending::kFullPull:
            return "full pull";
        case tp::run::Ending::kStalled:
            return "engine stalled";
        case tp::run::Ending::kSpunOut:
            return "wheels spun out";
        case tp::run::Ending::kStopped:
            return "stopped";
        case tp::run::Ending::kTimedOut:
            return "timed out";
    }
    return "?";
}

// One run on `model`, writing CSV rows tagged with the pull-off `round`.
std::optional<tp::run::Result> one_run(const tp::pull::Model& model, const tp::run::Rules& rules,
                                       const PullOptions& options, int round, std::ostream& csv) {
    tp::run::State state = tp::run::start(model);
    for (int i = 0; !state.result; ++i) {
        const double t_s = i * kDt_s;
        const tp::vehicle::Input input = pull_input(t_s, options);
        const tp::pull::Output out = tp::run::step(state, model, rules, input, kDt_s);
        if (out.tractor.shift_refused) {
            std::cerr << "cannot select gear " << options.gear << '\n';
            return std::nullopt;
        }
        if (i % kStepsPerRow == 0) {
            const tp::pull::State& pull = state.pull;
            // Output only: atan2 may differ in the last bit between platforms.
            const double chain_angle_rad = std::atan2(out.chain.direction_z, out.chain.direction_x);
            csv << round << ',' << t_s << ',' << input.clutch_pedal << ','
                << pull.tractor.driveline.gear << ','
                << rad_s_to_rpm(pull.tractor.engine.omega_rad_s) << ','
                << pull.sled.velocity_m_s * kMetersPerSecondToKmh << ',' << pull.sled.distance_m
                << ',' << out.tractor.slip_ratio << ','
                << out.tractor.axle_loads.rear_n / kNewtonsPerKilonewton << ','
                << out.tractor.axle_loads.front_n / kNewtonsPerKilonewton << ','
                << rad_to_deg(pull.tractor.chassis.pitch_rad) << ','
                << out.chain.tension_n / kNewtonsPerKilonewton << ',' << rad_to_deg(chain_angle_rad)
                << ',' << out.sled.box_ahead_of_wheels_m << ',' << out.sled.pushdown << ','
                << out.sled.pan_n / kNewtonsPerKilonewton << ','
                << out.sled.resistance_n / kNewtonsPerKilonewton << '\n';
        }
    }
    return state.result;
}

// A run, and after every full pull a pull-off on a heavier sled.
int run(const tp::vehicle::Params& tractor, const tp::sled::Params& first_sled,
        const tp::run::Rules& rules, const PullOptions& options, std::ostream& csv) {
    csv << "round,time_s,pedal,gear,engine_rpm,speed_kmh,sled_distance_m,slip_ratio,rear_load_kn,"
           "front_load_kn,pitch_deg,chain_tension_kn,chain_angle_deg,box_m,pushdown,"
           "sled_pan_load_kn,sled_resistance_kn\n";
    tp::sled::Params sled = first_sled;
    for (int round = 0; round <= kMaxPullOffs; ++round) {
        const auto model = tp::pull::Model::create({.tractor = tractor,
                                                    .sled = sled,
                                                    .track = {.soil = kReferenceSoil,
                                                              .moisture_along_m = std::nullopt,
                                                              .compaction_along_m = std::nullopt}});
        if (!model) {
            std::cerr << "inconsistent tractor or sled\n";
            return EXIT_FAILURE;
        }
        const auto result = one_run(*model, rules, options, round, csv);
        if (!result) {
            return EXIT_FAILURE;
        }
        std::cout << (round == 0 ? std::string{"run"} : "pull-off " + std::to_string(round)) << ": "
                  << std::fixed << std::setprecision(2) << result->distance_m << " m, "
                  << describe(result->ending) << " (" << std::setprecision(1) << result->time_s
                  << " s, top " << result->top_speed_m_s * kMetersPerSecondToKmh << " km/h)\n";
        std::cout.unsetf(std::ios::fixed);
        if (result->ending != tp::run::Ending::kFullPull) {
            return EXIT_SUCCESS;
        }
        sled = tp::sled::heavier(sled);
    }
    std::cerr << "still full pulls after " << kMaxPullOffs << " pull-offs\n";
    return EXIT_SUCCESS;
}

// --- check ------------------------------------------------------------------

void report_rule(std::string_view rule, double value, double limit, std::string_view unit,
                 bool broken) {
    std::cout << (broken ? "FAIL  " : "ok    ") << rule << ": " << value << ' ' << unit
              << " (limit " << limit << ' ' << unit << ")\n";
}

int check(const tp::vehicle::Params& tractor, const tp::rules::ClassRules& rules) {
    const tp::rules::Compliance result = tp::rules::check(rules, tractor);
    const tp::chassis::Params& chassis = tractor.chassis;
    report_rule("mass", chassis.mass_kg, rules.max_mass_kg, "kg", result.too_heavy);
    report_rule("PTO power", tp::units::w_to_metric_hp(tp::rules::pto_power_w(tractor)),
                tp::units::w_to_metric_hp(rules.max_pto_power_w), "hp", result.too_much_pto_power);
    report_rule("unloaded engine speed", rad_s_to_rpm(tractor.engine.high_idle_omega_rad_s),
                rad_s_to_rpm(rules.max_engine_speed_rad_s), "rpm", result.engine_speed_too_high);
    report_rule("hitch height", chassis.hitch_height_m, rules.max_hitch_height_m, "m",
                result.hitch_too_high);
    report_rule("hitch below rear axle centre", chassis.hitch_height_m, chassis.rear_axle_height_m,
                "m", result.hitch_above_rear_axle);
    std::cout << (result.ok() ? "may compete\n" : "may not compete\n");
    return result.ok() ? EXIT_SUCCESS : EXIT_FAILURE;
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
                 "                    [-o <out.csv>]\n"
                 "       tp_pull run <tractor.toml> <sled.toml> <track.toml> [--gear N]\n"
                 "                   [--dump] [-o <out.csv>]\n"
                 "       tp_pull check <tractor.toml> <class.toml>\n";
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
    if (command == "run" && positional.size() == 3) {
        const auto tractor = load_or_report(tp::data::load_tractor(positional[0]));
        const auto sled = load_or_report(tp::data::load_sled(positional[1]));
        const auto rules = load_or_report(tp::data::load_track(positional[2]));
        if (!tractor || !sled || !rules) {
            return EXIT_FAILURE;
        }
        return with_output(*options, [&](std::ostream& csv) {
            return run(*tractor, *sled, *rules, options->pull, csv);
        });
    }
    if (command == "check" && positional.size() == 2) {
        const auto tractor = load_or_report(tp::data::load_tractor(positional[0]));
        const auto rules = load_or_report(tp::data::load_class_rules(positional[1]));
        if (!tractor || !rules) {
            return EXIT_FAILURE;
        }
        return check(*tractor, *rules);
    }
    return usage();
}
