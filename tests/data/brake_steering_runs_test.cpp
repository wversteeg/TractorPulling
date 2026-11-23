#include "tp/core/controls/controls.hpp"
#include "tp/core/run/run.hpp"
#include "tp/core/units/conversions.hpp"
#include "tp/data/controls/controls_loader.hpp"
#include "tp/data/driveline/driveline_loader.hpp"
#include "tp/data/session/run_setup.hpp"
#include "tp/data/sled/sled_loader.hpp"
#include "tp/data/tire/tire_loader.hpp"
#include "tp/data/track/track_loader.hpp"
#include "tp/data/vehicle/tractor_loader.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace tp::data {
namespace {

constexpr double kDt_s = 0.001;
constexpr double kKmhPerM_s = 3.6;

template <typename T>
T loaded(Result<T> result) {
    if (!result.ok()) {
        throw std::runtime_error(result.error().message);
    }
    return std::move(result).value();
}

std::filesystem::path data(const std::string& file) {
    return std::filesystem::path{TP_DATA_DIR} / file;
}

// The Supersport on `tire_file` and the final drive `driveline_file` that
// suits it, in the easy layout, in 3rd at full throttle
// on the clay-sand with its sled. Once it is past `from_m_s`, the right
// brake is pressed fully for kBrake_s with the nose up; returns the
// heading kAfter_s after it is let go [rad, positive turned left]. With
// `limit` false the differential's coupling holds as if it had no limit.
constexpr double kBrake_s = 2.0;
constexpr double kAfter_s = 2.0;

double heading_after_a_brake(const std::string& tire_file, const std::string& driveline_file,
                             double from_m_s, bool limit = true) {
    vehicle::Params tractor = loaded(load_tractor(data("tractors/supersport_3600.toml")));
    const tire::Params tire = loaded(load_tire(data("tires/" + tire_file)));
    tractor = vehicle::with_rear_tire(tractor, tire).value_or(tractor);
    tractor.driveline = loaded(load_driveline(data("drivelines/" + driveline_file)));
    if (!limit) {
        tractor.differential.coupling_limit_nm = 0.0;
    }
    const sled::Params sled = loaded(load_sled(data("sleds/supersport_3600.toml")));
    const Track track = loaded(load_track(data("tracks/ideal_clay_sand.toml")));
    const controls::EasyParams easy = loaded(load_easy_controls(data("controls/easy.toml")));
    const session::Setup setup = run_setup(tractor, sled, track, 0);
    const std::optional<pull::Model> model =
        pull::Model::create({.tractor = setup.tractor, .sled = setup.sled, .track = setup.ground});
    if (!model) {
        throw std::runtime_error("the pull was refused");
    }
    run::State state = run::start(*model);
    controls::AssistState assist;
    double braked_at_s = -1.0;
    for (int i = 0; !state.result; ++i) {
        const double t_s = i * kDt_s;
        const controls::EasyInput in{.throttle = 1.0, .gear = 3};
        vehicle::Input input =
            controls::easy(in, easy, assist, setup.tractor, state.pull.tractor, kDt_s);
        if (braked_at_s < 0.0 && state.pull.tractor.chassis.velocity_m_s >= from_m_s) {
            braked_at_s = t_s;
        }
        if (braked_at_s >= 0.0 && t_s < braked_at_s + kBrake_s) {
            input.brake_right = 1.0;
        }
        run::step(state, *model, setup.rules, input, kDt_s);
        if (braked_at_s >= 0.0 && t_s >= braked_at_s + kBrake_s + kAfter_s) {
            return state.pull.tractor.chassis.heading_rad;
        }
    }
    throw std::runtime_error("the pull ended before the brake was let go");
}

// Steering by brake with the nose up ("optoeren" done, the pull on): the
// Supersport's coupled differential gives way to a hard brake
// (vehicle::Differential::coupling_limit_nm), so a full right brake for 2 s
// turns it right by a few degrees, on its puller tires and on the farm
// tires it starts on, each on the final drive that suits it; held without
// a limit the coupling drives the braked side on, and the brake turns it
// less.
TEST(BrakeSteeringRuns, TheSupersportTurnsToTheBrakedSide) {
    for (const auto& [tire, drive] :
         {std::pair{"supersport_puller_rear.toml", "supersport_3600_tall.toml"},
          std::pair{"supersport_farm_rear.toml", "supersport_3600_short.toml"}}) {
        for (const double kmh : {12.0, 18.0}) {
            const double heading_deg =
                units::rad_to_deg(heading_after_a_brake(tire, drive, kmh / kKmhPerM_s));
            EXPECT_LT(heading_deg, -1.0) << tire << " at " << kmh << " km/h";
            EXPECT_GT(heading_deg, -8.0) << tire << " at " << kmh << " km/h";
        }
    }
    const double limited_deg = units::rad_to_deg(heading_after_a_brake(
        "supersport_puller_rear.toml", "supersport_3600_tall.toml", 18.0 / kKmhPerM_s));
    const double held_deg = units::rad_to_deg(heading_after_a_brake(
        "supersport_puller_rear.toml", "supersport_3600_tall.toml", 18.0 / kKmhPerM_s, false));
    EXPECT_GT(held_deg, limited_deg + 0.5) << "without the limit the brake turns it less";
}

}  // namespace
}  // namespace tp::data
