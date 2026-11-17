#include "tp/core/controls/controls.hpp"
#include "tp/core/run/run.hpp"
#include "tp/data/controls/controls_loader.hpp"
#include "tp/data/session/run_setup.hpp"
#include "tp/data/sled/sled_loader.hpp"
#include "tp/data/track/track_loader.hpp"
#include "tp/data/vehicle/tractor_loader.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>

namespace tp::data {
namespace {

constexpr double kDt_s = 0.001;

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

struct Launch {
    /// When the boost first reached the target [s]; negative if never.
    double boosted_at_s = -1.0;
    /// The tractor's fastest before then [m/s].
    double fastest_held_m_s = 0.0;
    /// The boost when the assist let the brakes go.
    double boost_at_release = 0.0;
    /// How far the pull went.
    double distance_m = 0.0;
};

// The class's tractor in the easy layout (data/controls/easy.toml) on the
// clay-sand with its sled, in `gear` at full throttle from half a second,
// on the brake hold until the turbo's boost reaches `target`; then the
// brakes come off.
Launch on_the_brakes(const std::string& tractor_file, const std::string& sled_file, int gear,
                     double target) {
    const vehicle::Params tractor = loaded(load_tractor(data("tractors/" + tractor_file)));
    const sled::Params sled = loaded(load_sled(data("sleds/" + sled_file)));
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
    Launch out;
    constexpr double kGo_s = 0.5;
    for (int i = 0; !state.result; ++i) {
        const double t_s = i * kDt_s;
        controls::EasyInput in{.hold_brakes = true, .hold_until_boost = target};
        if (t_s >= kGo_s) {
            in.throttle = 1.0;
            in.gear = gear;
        }
        const vehicle::Input input =
            controls::easy(in, easy, assist, setup.tractor, state.pull.tractor, kDt_s);
        if (input.hold_brakes) {
            out.fastest_held_m_s =
                std::max(out.fastest_held_m_s, std::abs(state.pull.tractor.chassis.velocity_m_s));
        } else if (out.boosted_at_s < 0.0) {
            // The assist let go: the boost is up.
            out.boosted_at_s = t_s;
            out.boost_at_release = state.pull.tractor.engine.boost;
        }
        run::step(state, *model, setup.rules, input, kDt_s);
    }
    out.distance_m = state.result.value_or(run::Result{}).distance_m;
    return out;
}

// "Optoeren": on the brake hold the Sport and Supersport tractors stand while
// the clutch slips against the brakes and the turbo spools, the boost up to
// three quarters of full within a few seconds; then the brakes come off and
// the tractor pulls. The brakes hold the engine's load in a pulling gear.
TEST(BrakeHoldRuns, TheTractorStandsWhileTheBoostBuilds) {
    constexpr double kTarget = 0.75;
    constexpr double kWithin_s = 5.0;
    constexpr double kStanding_m_s = 0.05;
    constexpr double kPulls_m = 50.0;
    struct Case {
        const char* tractor;
        const char* sled;
        int gear;
    };
    for (const Case& c : {Case{"sport_2500.toml", "sport_2500.toml", 6},
                          Case{"sport_4500.toml", "sport_4500.toml", 6},
                          Case{"supersport_3600.toml", "supersport_3600.toml", 5}}) {
        const Launch launch = on_the_brakes(c.tractor, c.sled, c.gear, kTarget);
        EXPECT_GT(launch.boosted_at_s, 0.0) << c.tractor;
        EXPECT_GE(launch.boost_at_release, kTarget) << c.tractor;
        EXPECT_LT(launch.boosted_at_s, kWithin_s) << c.tractor;
        EXPECT_LT(launch.fastest_held_m_s, kStanding_m_s) << c.tractor;
        EXPECT_GT(launch.distance_m, kPulls_m) << c.tractor;
    }
}

}  // namespace
}  // namespace tp::data
