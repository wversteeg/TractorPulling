#include "tp/core/driver/driver.hpp"
#include "tp/core/field/field.hpp"
#include "tp/core/field/play.hpp"
#include "tp/core/pull/pull.hpp"
#include "tp/core/run/run.hpp"
#include "tp/core/session/session.hpp"
#include "tp/data/event/event_loader.hpp"
#include "tp/data/session/run_setup.hpp"
#include "tp/data/sled/sled_loader.hpp"
#include "tp/data/track/track_loader.hpp"
#include "tp/data/vehicle/tractor_loader.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace tp::session {
namespace {

// The README's driver: neutral, 6th at 0.5 s, the clutch slipped in from
// 3 s over 3 s at full throttle.
constexpr double kSelectGear_s = 0.5;
constexpr double kLaunch_s = 3.0;
constexpr double kSlipIn_s = 3.0;
constexpr int kGear = 6;
constexpr double kDt_s = 0.001;
constexpr std::int64_t kStep_ns = 1'000'000;
// Past the launch, where the engine dips anyway: from here on the driver
// watches the rev counter [m].
constexpr double kWatchFrom_m = 40.0;
// A sled lighter than shipped: two knives, the pull-off setting at the
// shipped event (its field pulls on one, where the shift pays more). Past
// the push-down its drag levels out between what 6th holds before the
// engine stalls and what 5th's grip holds, which is where a shift down
// pays; on the heavier sled as shipped neither gear holds it.
constexpr int kPullOffKnives = 2;

std::filesystem::path data(const std::string& file) {
    return std::filesystem::path{TP_DATA_DIR} / file;
}

template <typename T>
T loaded(data::Result<T> result) {
    if (!result) {
        throw std::runtime_error(result.error().message);
    }
    return std::move(result).value();
}

// How far the shipped tractor pulls the class sled on ideal clay-sand in
// 6th, shifting one gear down with the clutch in (the gearbox's powershift
// step) once the engine falls below `lugging_share` of its rated speed;
// never with nothing.
double reach_m(std::optional<double> lugging_share) {
    const vehicle::Params tractor =
        loaded(data::load_tractor(data("tractors/farm_stock_light.toml")));
    sled::Params sled = loaded(data::load_sled(data("sleds/light_class.toml")));
    sled.knives.count = kPullOffKnives;
    const data::Track track = loaded(data::load_track(data("tracks/ideal_clay_sand.toml")));
    std::optional<Session> session = Session::create(data::run_setup(tractor, sled, track));
    if (!session) {
        throw std::runtime_error("the run was refused");
    }
    Controls controls;
    bool shifted = false;
    for (std::int64_t step = 0; !session->result(); ++step) {
        const double t_s = static_cast<double>(step) * kDt_s;
        const Snapshot& now = session->current();
        controls.throttle = t_s < kSelectGear_s ? 0.0 : 1.0;
        controls.clutch_pedal = std::clamp(1.0 - (t_s - kLaunch_s) / kSlipIn_s, 0.0, 1.0);
        controls.gear = t_s < kSelectGear_s ? driveline::kNeutral : kGear;
        if (lugging_share && !shifted && now.sled_x_m > kWatchFrom_m &&
            now.engine_omega_rad_s < *lugging_share * tractor.engine.rated_omega_rad_s) {
            shifted = true;
        }
        if (shifted) {
            controls.gear = kGear - 1;
        }
        session->advance(controls, kStep_ns);
    }
    return session->result().value_or(run::Result{}).distance_m;
}

// As the README says: shifting down as the engine is pulled down pulls
// further than staying in 6th until it stalls.
TEST(LoadDownshiftRuns, ShiftingDownLatePullsFurther) {
    const double held_m = reach_m(std::nullopt);
    const double shifted_m = reach_m(0.9);
    EXPECT_GT(shifted_m, held_m + 1.0) << "held " << held_m << " m, shifted " << shifted_m << " m";
}

// A scripted rival of the shipped event, the reference puller in `gear`
// shifting down at `share` of the rated speed, on its sled and ideal
// clay-sand, as field::run_pull pulls it: how far, and the shifts under
// load it made.
struct RivalPull {
    double reached_m = 0.0;
    int load_shifts = 0;
};

RivalPull rival_pull(int gear, double share) {
    const data::EventDefinition event = loaded(data::load_event(data("events/club_pull.toml")));
    const data::Track track = loaded(data::load_track(data("tracks/ideal_clay_sand.toml")));
    sled::Params sled = event.sled;
    sled.knives.count = kPullOffKnives;
    const field::Venue venue = data::venue(sled, track);
    const run::Rules rules = field::pull_rules(event.rules, venue, event.rules.full_pull_m);
    field::Puller puller = event.reference;
    puller.driver.gear = gear;
    puller.driver.downshift_share = share;
    const std::optional<pull::Model> model =
        pull::Model::create({.tractor = puller.tractor, .sled = venue.sled, .track = venue.track});
    if (!model) {
        throw std::runtime_error("the model was refused");
    }
    run::State state = run::start(*model);
    driver::State driver;
    while (!state.result) {
        const double t_s = static_cast<double>(state.steps) * kDt_s;
        run::step(state, *model, rules,
                  driver::input(puller.driver, driver, t_s, state.pull.tractor,
                                model->tractor().params()),
                  kDt_s);
    }
    const RivalPull out{.reached_m = state.result.value_or(run::Result{}).distance_m,
                        .load_shifts = state.pull.tractor.driveline.load_shifts};
    // The same pull as the field's own.
    EXPECT_EQ(
        field::run_pull(puller, venue.sled, venue.track, rules).value_or(run::Result{}).distance_m,
        out.reached_m);
    return out;
}

// As the README says: a rival in 6th watching the rev counter shifts once,
// as the sled pulls the engine down, and pulls about as far as 5th; in 4th
// and 5th the tires end the pull before the engine is pulled down, and the
// rival never shifts.
TEST(LoadDownshiftRuns, RivalsShiftDownLateIn6th) {
    const RivalPull held = rival_pull(6, 0.0);
    const RivalPull shifted = rival_pull(6, 0.9);
    EXPECT_EQ(held.load_shifts, 0);
    EXPECT_EQ(shifted.load_shifts, 1);
    EXPECT_GT(shifted.reached_m, held.reached_m + 1.0)
        << "held " << held.reached_m << " m, shifted " << shifted.reached_m << " m";
    const RivalPull fifth = rival_pull(5, 0.0);
    EXPECT_NEAR(shifted.reached_m, fifth.reached_m, 1.0);
    for (const int gear : {4, 5}) {
        const RivalPull watching = rival_pull(gear, 0.9);
        EXPECT_EQ(watching.load_shifts, 0) << gear;
        EXPECT_EQ(watching.reached_m, rival_pull(gear, 0.0).reached_m) << gear;
    }
}

}  // namespace
}  // namespace tp::session
