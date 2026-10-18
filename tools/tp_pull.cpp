// Headless simulation runner. Commands:
//
//   tp_pull engine-dyno <engine.toml> [-o <out.csv>]
//   tp_pull pull <tractor.toml> [--gear N] [--resistance-kn F] [--dump]
//                [-o <out.csv>]
//   tp_pull run <tractor.toml> <sled.toml> <track.toml> [--gear N] [--dump]
//               [-o <out.csv>]
//   tp_pull compare-tracks <tractor.toml> <sled.toml> <track.toml>...
//                          [--gear N] [--dump]
//   tp_pull event <event.toml> [--seed N]
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
// for plausibility.
//
// compare-tracks runs the same tractor, sled and driving on several tracks
// and prints one line per track: its surface at the start line, the day's
// push-down and how far the run got.
//
// event runs one class at an event: a seeded field and running order, test
// pulls and the sled operator's changes, test pullers keeping or re-pulling,
// qualifying and pull-offs. It prints every pull and the standings.
//
// check compares a tractor with a pulling class's rules and exits non-zero
// if it may not compete.

#include "tp/core/driveline/driveline.hpp"
#include "tp/core/engine/engine.hpp"
#include "tp/core/event/event.hpp"
#include "tp/core/field/field.hpp"
#include "tp/core/pull/pull.hpp"
#include "tp/core/rules/class_rules.hpp"
#include "tp/core/run/run.hpp"
#include "tp/core/sled/sled.hpp"
#include "tp/core/soil/soil.hpp"
#include "tp/core/units/conversions.hpp"
#include "tp/core/vehicle/vehicle.hpp"
#include "tp/data/engine/engine_loader.hpp"
#include "tp/data/event/event_loader.hpp"
#include "tp/data/rules/class_rules_loader.hpp"
#include "tp/data/sled/sled_loader.hpp"
#include "tp/data/track/track_loader.hpp"
#include "tp/data/vehicle/tractor_loader.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
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
// The constant-force pull runs on the reference track's soil: moist, rolled
// clay and sand (data/tracks/ideal_clay_sand.toml).
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
int run(const tp::vehicle::Params& tractor, const tp::sled::Params& class_sled,
        const tp::data::Track& track, const PullOptions& options, std::ostream& csv) {
    csv << "round,time_s,pedal,gear,engine_rpm,speed_kmh,sled_distance_m,slip_ratio,rear_load_kn,"
           "front_load_kn,pitch_deg,chain_tension_kn,chain_angle_deg,box_m,pushdown,"
           "sled_pan_load_kn,sled_resistance_kn\n";
    // The class's sled with the day's push-down.
    tp::sled::Params sled = tp::sled::with_conditions(class_sled, track.sled);
    for (int round = 0; round <= kMaxPullOffs; ++round) {
        const auto model =
            tp::pull::Model::create({.tractor = tractor, .sled = sled, .track = track.ground});
        if (!model) {
            std::cerr << "inconsistent tractor, sled or track\n";
            return EXIT_FAILURE;
        }
        const auto result = one_run(*model, track.rules, options, round, csv);
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

// --- compare-tracks ---------------------------------------------------------

constexpr double kPascalsPerKilopascal = 1000.0;

// Runs the tractor once on each track (no pull-offs) and prints a table.
int compare_tracks(const tp::vehicle::Params& tractor, const tp::sled::Params& class_sled,
                   std::span<const std::string_view> track_paths, const PullOptions& options) {
    std::cout << "track                         cone index   grip  fall-off  push-down  result\n";
    // The runs' CSV rows go nowhere.
    std::ostream discard{nullptr};
    for (const std::string_view path : track_paths) {
        const auto track = tp::data::load_track(path);
        if (!track) {
            std::cerr << track.error().message << '\n';
            return EXIT_FAILURE;
        }
        const tp::sled::Params sled = tp::sled::with_conditions(class_sled, track.value().sled);
        const auto model = tp::pull::Model::create(
            {.tractor = tractor, .sled = sled, .track = track.value().ground});
        if (!model) {
            std::cerr << path << ": inconsistent tractor, sled or track\n";
            return EXIT_FAILURE;
        }
        const auto result = one_run(*model, track.value().rules, options, 0, discard);
        if (!result) {
            return EXIT_FAILURE;
        }
        const tp::soil::Surface start = model->track().surface_at(0.0);
        const std::string name = std::filesystem::path{path}.stem().string();
        std::cout << std::left << std::setw(28) << name << std::right << std::fixed
                  << std::setprecision(0) << std::setw(9)
                  << start.cone_index_pa / kPascalsPerKilopascal << " kPa" << std::setprecision(2)
                  << std::setw(7) << start.grip_factor << std::setw(10) << start.spin_fall_off
                  << std::setprecision(1) << std::setw(9) << sled.pushdown_start_m << " m  "
                  << std::setprecision(2) << result->distance_m << " m, "
                  << describe(result->ending) << '\n';
        std::cout.unsetf(std::ios::fixed);
    }
    return EXIT_SUCCESS;
}

// --- event ------------------------------------------------------------------

std::string_view describe(tp::event::PullKind kind) {
    switch (kind) {
        case tp::event::PullKind::kTest:
            return "test pull";
        case tp::event::PullKind::kCompetition:
            return "";
        case tp::event::PullKind::kRepull:
            return "re-pull";
        case tp::event::PullKind::kPullOff:
            return "pull-off";
    }
    return "?";
}

void report_change(const tp::event::Outcome& outcome, const tp::event::Event& event) {
    switch (outcome.change) {
        case tp::event::SledChange::kNone:
            return;
        case tp::event::SledChange::kLineExtended:
            std::cout << "      operator: full pull at " << event.full_pull_m() << " m\n";
            return;
        case tp::event::SledChange::kLighter:
        case tp::event::SledChange::kHeavier:
            std::cout << "      operator: "
                      << (outcome.change == tp::event::SledChange::kLighter ? "lighter" : "heavier")
                      << " by " << outcome.steps << " step(s): " << event.sled().knives.count
                      << " knives, push-down at " << event.sled().pushdown_start_m
                      << " m; the next puller tests it\n";
            return;
    }
}

void report_standings(tp::event::Event& event) {
    std::cout << "\nstandings\n";
    for (const tp::event::Standing& s : event.standings()) {
        std::cout << std::setw(4) << s.place << ".  #" << std::setw(2) << s.puller + 1 << "  "
                  << std::setw(7) << s.distance_m << " m" << (s.full_pull ? " full pull" : "");
        if (s.pull_off_rounds > 0) {
            std::cout << ", pull-off " << s.pull_off_rounds << ": " << s.pull_off_distance_m
                      << " m";
        }
        std::cout << '\n';
    }
}

int run_event(const tp::data::EventDefinition& definition, std::uint64_t seed) {
    const auto field = tp::field::generate(definition.reference, definition.variation,
                                           definition.class_rules, definition.pullers, seed);
    // The draw for the running order is separate from the field's.
    const auto order = tp::event::running_order(definition.pullers, seed + 1);
    const std::span<const int> running{order.data(), static_cast<std::size_t>(definition.pullers)};
    auto event = tp::event::Event::create(
        definition.rules, tp::sled::with_conditions(definition.sled, definition.track.sled),
        running);
    if (!field || !event) {
        std::cerr << "inconsistent event\n";
        return EXIT_FAILURE;
    }
    std::cout << std::fixed << std::setprecision(2) << definition.pullers << " pullers, seed "
              << seed << "; running order:";
    for (const int puller : running) {
        std::cout << " #" << puller + 1;
    }
    std::cout << "\n\n";
    int pull_number = 0;
    while (!event->finished()) {
        if (const std::optional<int> deciding = event->decision_due()) {
            const bool keep = event->suggested_keep();
            std::cout << "      #" << *deciding + 1
                      << (keep ? " keeps the test pull\n" : " will re-pull after the 6th pull\n");
            event->decide(keep);
            continue;
        }
        const std::optional<tp::event::Pull> pull = event->next();
        if (!pull) {
            break;
        }
        const tp::field::Puller& puller = field->pullers[static_cast<std::size_t>(pull->puller)];
        const std::optional<double> reached = tp::field::pull_distance(
            puller, event->sled(), definition.track.ground, definition.rules.max_distance_m,
            definition.track.rules.max_duration_s);
        if (!reached) {
            std::cerr << "inconsistent puller or sled\n";
            return EXIT_FAILURE;
        }
        const double line_m = event->full_pull_m();
        std::cout << std::setw(4) << ++pull_number << ".  #" << std::setw(2) << pull->puller + 1
                  << "  gear " << puller.driver.gear << "  " << std::setw(7) << *reached << " m"
                  << (*reached >= line_m ? " full pull" : "          ") << "  "
                  << describe(pull->kind);
        if (pull->kind == tp::event::PullKind::kPullOff) {
            std::cout << ' ' << pull->pull_off_round << " (" << event->sled().knives.count
                      << " knives)";
        }
        std::cout << '\n';
        report_change(event->record(*reached), *event);
    }
    report_standings(*event);
    std::cout.unsetf(std::ios::fixed);
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
    std::optional<std::uint64_t> seed;
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
        } else if (arg == "--seed" && has_value) {
            options.seed = parse_number<std::uint64_t>(args[++i]);
            if (!options.seed) {
                return std::nullopt;
            }
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
                 "       tp_pull compare-tracks <tractor.toml> <sled.toml> <track.toml>...\n"
                 "                              [--gear N] [--dump]\n"
                 "       tp_pull event <event.toml> [--seed N]\n"
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

// Each command loads its files and runs; positional arguments are checked
// against the command's range before.
int engine_dyno_command(const Options& options) {
    const auto engine = load_or_report(tp::data::load_engine(options.positional[0]));
    if (!engine) {
        return EXIT_FAILURE;
    }
    return with_output(options, [&](std::ostream& csv) { return engine_dyno(*engine, csv); });
}

int pull_command(const Options& options) {
    const auto params = load_or_report(tp::data::load_tractor(options.positional[0]));
    if (!params) {
        return EXIT_FAILURE;
    }
    const auto model = tp::vehicle::Model::create(*params);
    if (!model) {
        std::cerr << options.positional[0] << ": inconsistent tractor\n";
        return EXIT_FAILURE;
    }
    return with_output(options, [&](std::ostream& csv) { return pull(*model, options.pull, csv); });
}

int run_command(const Options& options) {
    const auto tractor = load_or_report(tp::data::load_tractor(options.positional[0]));
    const auto sled = load_or_report(tp::data::load_sled(options.positional[1]));
    const auto track = load_or_report(tp::data::load_track(options.positional[2]));
    if (!tractor || !sled || !track) {
        return EXIT_FAILURE;
    }
    return with_output(options, [&](std::ostream& csv) {
        return run(*tractor, *sled, *track, options.pull, csv);
    });
}

int compare_tracks_command(const Options& options) {
    const auto tractor = load_or_report(tp::data::load_tractor(options.positional[0]));
    const auto sled = load_or_report(tp::data::load_sled(options.positional[1]));
    if (!tractor || !sled) {
        return EXIT_FAILURE;
    }
    return compare_tracks(*tractor, *sled, std::span{options.positional}.subspan(2), options.pull);
}

int event_command(const Options& options) {
    const auto definition = load_or_report(tp::data::load_event(options.positional[0]));
    if (!definition) {
        return EXIT_FAILURE;
    }
    return run_event(*definition, options.seed.value_or(definition->seed));
}

int check_command(const Options& options) {
    const auto tractor = load_or_report(tp::data::load_tractor(options.positional[0]));
    const auto rules = load_or_report(tp::data::load_class_rules(options.positional[1]));
    if (!tractor || !rules) {
        return EXIT_FAILURE;
    }
    return check(*tractor, *rules);
}

struct Command {
    std::string_view name;
    std::size_t min_files;
    std::size_t max_files;
    int (*run)(const Options&);
};

constexpr std::size_t kAnyNumber = 1000;

constexpr std::array kCommands = {
    Command{"engine-dyno", 1, 1, &engine_dyno_command},
    Command{"pull", 1, 1, &pull_command},
    Command{"run", 3, 3, &run_command},
    Command{"compare-tracks", 3, kAnyNumber, &compare_tracks_command},
    Command{"event", 1, 1, &event_command},
    Command{"check", 2, 2, &check_command},
};

}  // namespace

int main(int argc, char** argv) {
    const std::span<char* const> args{argv, static_cast<std::size_t>(argc)};
    if (args.size() < 2) {
        return usage();
    }
    const std::string_view name{args[1]};
    const std::optional<Options> options = parse_options(args.subspan(2));
    if (!options) {
        return usage();
    }
    const std::size_t files = options->positional.size();
    for (const Command& command : kCommands) {
        if (command.name == name && files >= command.min_files && files <= command.max_files) {
            return command.run(*options);
        }
    }
    return usage();
}
