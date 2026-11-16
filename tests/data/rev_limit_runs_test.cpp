#include "tp/core/driver/driver.hpp"
#include "tp/core/field/field.hpp"
#include "tp/core/field/play.hpp"
#include "tp/core/garage/garage.hpp"
#include "tp/core/run/run.hpp"
#include "tp/core/units/conversions.hpp"
#include "tp/data/season/season_loader.hpp"
#include "tp/data/session/run_setup.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>

namespace tp::data {
namespace {

constexpr double kDt_s = 0.001;

SeasonDefinition season(const std::string& file) {
    auto loaded = load_season(std::filesystem::path{TP_DATA_DIR} / "seasons" / file);
    if (!loaded) {
        throw std::runtime_error(loaded.error().message);
    }
    return std::move(loaded).value();
}

// The class's tractor with every upgrade at its top level and the last of
// each choice's alternatives fitted.
vehicle::Params fully_built(const SeasonDefinition& d) {
    const garage::Catalogue& catalogue = d.garage.catalogue;
    garage::Career full = garage::new_career(catalogue);
    for (std::size_t i = 0; i < catalogue.count; ++i) {
        full.levels.at(i) = static_cast<int>(catalogue.upgrades.at(i).level_count) - 1;
    }
    full.owned.fill(std::numeric_limits<std::uint8_t>::max());
    for (std::size_t c = 0; c < catalogue.choice_count; ++c) {
        full.fitted.at(c) = static_cast<int>(catalogue.choices.at(c).alternative_count) - 1;
    }
    const std::optional<vehicle::Params> tractor =
        garage::equipped(d.event.reference.tractor, catalogue, full);
    if (!tractor) {
        throw std::runtime_error("the built tractor was refused");
    }
    return *tractor;
}

struct Revs {
    /// The highest engine speed once the tractor moves off [rad/s].
    double highest_rad_s = 0.0;
    /// The governor's cut-off [rad/s].
    double cut_off_rad_s = 0.0;
};

// A pull of the fully built tractor of the season's class on its first
// track, with the class's sled, in the gear the driver finds best there,
// driven as the class's reference driver drives.
Revs pull(const std::string& season_file) {
    const SeasonDefinition d = season(season_file);
    const EventDefinition& e = d.event;
    const vehicle::Params tractor = fully_built(d);
    const Track& track = d.rounds.front().track;
    const field::Venue at = data::venue(e.sled, track);
    field::Puller puller = e.reference;
    puller.tractor = tractor;
    const std::optional<field::GearTrial> trial = field::best_gear_trial(
        puller, at.sled, at.track, e.variation.lowest_gear, e.variation.highest_gear,
        field::trial_pull_rules(e.rules, at, e.rules.full_pull_m));
    if (!trial) {
        throw std::runtime_error("no gear gets the sled going");
    }
    driver::Plan plan = e.reference.driver;
    plan.gear = trial->gear;

    const session::Setup setup = run_setup(tractor, e.sled, track, 0);
    const std::optional<pull::Model> model =
        pull::Model::create({.tractor = setup.tractor, .sled = setup.sled, .track = setup.ground});
    if (!model) {
        throw std::runtime_error("the pull was refused");
    }
    run::State state = run::start(*model);
    driver::State driver;
    Revs out{.cut_off_rad_s = setup.tractor.engine.high_idle_omega_rad_s};
    while (!state.result) {
        const double t_s = static_cast<double>(state.steps) * kDt_s;
        run::step(state, *model, setup.rules,
                  driver::input(plan, driver, t_s, state.pull.tractor, setup.tractor), kDt_s);
        if (state.pull.sled.position_m > model->sled_start_m()) {
            out.highest_rad_s = std::max(out.highest_rad_s, state.pull.tractor.engine.omega_rad_s);
        }
    }
    return out;
}

// Within this of the cut-off the commentator hears the engine hit the
// governor [rad/s].
const double kAtTheLimit_rad_s = units::rpm_to_rad_s(50.0);
// The governor only takes the fuel away: when the drive pushes the engine
// (a shift down under load, the tires gripping again) it may run this far
// past the cut-off [rad/s].
const double kOvershoot_rad_s = units::rpm_to_rad_s(10.0);

// A fully built tractor's governor is a pulling one: its cut-off at the
// class's 2700 rpm and full fuel to just below it, so that in a pull the
// engine runs up against the limit (the "regelaar") rather than holding
// a few hundred rpm under it; and it never runs past it by more than the
// drive can push it.
TEST(RevLimitRuns, AFullyBuiltTractorRunsUpToTheClassLimit) {
    for (const char* file : {"standard_3500.toml", "sport_2500_top.toml", "sport_4500_top.toml"}) {
        const Revs revs = pull(file);
        EXPECT_NEAR(revs.cut_off_rad_s, units::rpm_to_rad_s(2700.0), 1e-9) << file;
        EXPECT_GT(revs.highest_rad_s, revs.cut_off_rad_s - kAtTheLimit_rad_s)
            << file << ": " << units::rad_s_to_rpm(revs.highest_rad_s) << " rpm";
        EXPECT_LE(revs.highest_rad_s, revs.cut_off_rad_s + kOvershoot_rad_s) << file;
    }
}

}  // namespace
}  // namespace tp::data
