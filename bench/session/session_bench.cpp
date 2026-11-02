#include "tp/core/session/session.hpp"
#include "tp/core/sled/sled.hpp"
#include "tp/data/controls/controls_loader.hpp"
#include "tp/data/sled/sled_loader.hpp"
#include "tp/data/track/track_loader.hpp"
#include "tp/data/vehicle/tractor_loader.hpp"

#include <benchmark/benchmark.h>

#include <cstdint>
#include <optional>
#include <string>

namespace {

constexpr int kPullingGear = 6;
// As BM_GameStep: the sled's push-down, the end of the track and the time
// limit are out of reach, so the pull stays steady however long it is timed.
constexpr double kOutOfReach_m = 1.0e9;
constexpr double kNeverTimesOut_s = 1.0e9;
constexpr int kFramesToSteady = 720;
// A 60 fps frame: 16 or 17 steps.
constexpr std::int64_t kFrame_ns = 16'666'667;

std::string data_file(const char* name) {
    return std::string{TP_DATA_DIR} + "/" + name;
}

// One 60 fps frame as the game runs it: the session runs its 16 or 17 steps
// of the easy layout and the run, keeping a snapshot after each, and the
// front end blends the last two to draw. The shipped tractor, sled and
// track, in a steady pull.
void BM_SessionFrame(benchmark::State& state) {
    const auto tractor = tp::data::load_tractor(data_file("tractors/farm_stock_light.toml"));
    const auto sled = tp::data::load_sled(data_file("sleds/light_class.toml"));
    const auto track = tp::data::load_track(data_file("tracks/ideal_clay_sand.toml"));
    const auto easy = tp::data::load_easy_controls(data_file("controls/easy.toml"));
    if (!tractor || !sled || !track || !easy) {
        state.SkipWithError("cannot load the shipped tractor, sled, track or easy layout");
        return;
    }
    tp::session::Setup setup{.tractor = tractor.value(),
                             .sled = tp::sled::with_conditions(sled.value(), track.value().sled),
                             .ground = track.value().ground,
                             .rules = track.value().rules,
                             .layout = tp::session::Layout::kEasy,
                             .easy = easy.value(),
                             .pro = {}};
    setup.sled.pushdown_start_m = kOutOfReach_m;
    setup.sled.box_travel_distance_m = kOutOfReach_m;
    setup.rules.track_length_m = kOutOfReach_m;
    setup.rules.full_pull_line_m = kOutOfReach_m;
    setup.rules.max_duration_s = kNeverTimesOut_s;
    std::optional<tp::session::Session> session = tp::session::Session::create(setup);
    if (!session) {
        state.SkipWithError("inconsistent tractor, sled or track");
        return;
    }

    const tp::session::Controls pulling{.throttle = 1.0, .gear = kPullingGear};
    const auto frame = [&] {
        session->advance(pulling, kFrame_ns);
        return tp::session::blend(session->previous(), session->current(), session->alpha());
    };
    for (int i = 0; i < kFramesToSteady; ++i) {
        frame();
    }
    for (auto _ : state) {
        benchmark::DoNotOptimize(frame());
        benchmark::ClobberMemory();
    }
    const tp::session::Snapshot& now = session->current();
    if (session->result() || now.clutch_pedal > 0.0 || now.sled_speed_m_s <= 0.0) {
        state.SkipWithError("left steady pulling: the benchmark would time something else");
    }
}
BENCHMARK(BM_SessionFrame);

}  // namespace
