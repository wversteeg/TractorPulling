#include "tp/core/field/field.hpp"
#include "tp/core/garage/garage.hpp"
#include "tp/data/season/season_loader.hpp"
#include "tp/data/session/run_setup.hpp"
#include <gtest/gtest.h>
#include <filesystem>
#include <iostream>
namespace tp::field {
namespace {
TEST(ZProbe, NewTractorsEverywhere) {
    for (const char* file : {"standard_3500.toml", "sport_2500_promo.toml"}) {
        const auto d = data::load_season(std::filesystem::path{TP_DATA_DIR} / "seasons" / file).value();
        const data::EventDefinition& e = d.event;
        const vehicle::Params fresh = *garage::equipped(e.reference.tractor, d.garage.catalogue, garage::new_career(d.garage.catalogue));
        for (std::size_t i : {0u, 3u}) {
            const auto& round = d.rounds[i];
            const Venue venue = data::venue(e.sled, round.track);
            const run::Rules rules = trial_pull_rules(e.rules, venue, e.rules.full_pull_m);
            Puller p = e.reference; p.tractor = fresh;
            const auto best = best_gear_trial(p, venue.sled, venue.track, e.variation.lowest_gear, e.variation.highest_gear, rules);
            std::cout << file << " " << round.name << ": g" << best->gear << " " << best->distance_m << "\n";
        }
    }
}
}  // namespace
}  // namespace tp::field
