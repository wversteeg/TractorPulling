#include "tp/core/session/session.hpp"
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
    const sled::Params sled = loaded(data::load_sled(data("sleds/light_class.toml")));
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

}  // namespace
}  // namespace tp::session
