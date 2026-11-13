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

#include <cstdint>
#include <filesystem>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace tp::session {
namespace {

constexpr double kDt_s = 0.001;

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

// A scripted rival of the Sport 4.5 t top event, the reference puller in
// `gear` shifting down at `share` of the rated speed (0 never), on the
// class's sled `steps` pull-off steps heavier and ideal clay-sand, to the
// pull-off's line, as field::run_pull pulls it: how far, and the shifts
// under load it made.
struct RivalPull {
    double reached_m = 0.0;
    int load_shift_gears = 0;
};

RivalPull rival_pull(int gear, double share, int steps) {
    const data::EventDefinition event =
        loaded(data::load_event(data("events/sport_4500_top.toml")));
    const data::Track track = loaded(data::load_track(data("tracks/ideal_clay_sand.toml")));
    const field::Venue venue = data::venue(sled::heavier(event.sled, steps), track);
    const run::Rules rules =
        field::trial_pull_rules(event.rules, venue, event.rules.pull_off_full_pull_m);
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
                        .load_shift_gears = state.pull.tractor.driveline.load_shift_gears};
    // The same pull as the field's own.
    EXPECT_EQ(
        field::run_pull(puller, venue.sled, venue.track, rules).value_or(run::Result{}).distance_m,
        out.reached_m);
    return out;
}

// The class's own sled: past the push-down its drag levels out (the pan
// carries half the sled at most) between what 8th holds before the engine
// stalls and what 7th's grip holds, which is where a shift down pays. On a
// heavier pull-off sled 8th stalls before the engine falls to the shift.
constexpr int kPullOffSteps = 0;

// As the README says: shifting down as the engine is pulled down pulls
// further than staying in 8th until it stalls, about as far as 7th (within
// a couple of metres).
TEST(LoadDownshiftRuns, ShiftingDownLatePullsFurther) {
    const RivalPull held = rival_pull(8, 0.0, kPullOffSteps);
    const RivalPull shifted = rival_pull(8, 0.9, kPullOffSteps);
    EXPECT_EQ(held.load_shift_gears, 0);
    EXPECT_EQ(shifted.load_shift_gears, 1);
    EXPECT_GT(shifted.reached_m, held.reached_m + 1.0)
        << "held " << held.reached_m << " m, shifted " << shifted.reached_m << " m";
    EXPECT_NEAR(shifted.reached_m, rival_pull(7, 0.0, kPullOffSteps).reached_m, 2.5);
}

// In 5th and 6th the tires, not the engine, end the pull: the engine is
// never pulled down, and a rival watching the rev counter never shifts.
TEST(LoadDownshiftRuns, InALowGearTheTiresEndThePull) {
    for (const int gear : {5, 6}) {
        const RivalPull watching = rival_pull(gear, 0.9, kPullOffSteps);
        EXPECT_EQ(watching.load_shift_gears, 0) << gear;
        EXPECT_EQ(watching.reached_m, rival_pull(gear, 0.0, kPullOffSteps).reached_m) << gear;
    }
}

}  // namespace
}  // namespace tp::session
