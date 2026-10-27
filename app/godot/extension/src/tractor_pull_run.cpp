#include "tractor_pull_run.hpp"

#include "tp/core/chassis/chassis.hpp"
#include "tp/data/controls/controls_loader.hpp"
#include "tp/data/files.hpp"
#include "tp/data/result.hpp"
#include "tp/data/session/run_setup.hpp"
#include "tp/data/sled/sled_loader.hpp"
#include "tp/data/track/track_loader.hpp"
#include "tp/data/vehicle/tractor_loader.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>

#include "godot_files.hpp"
#include "tractor_pull_event.hpp"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/char_string.hpp>

namespace tp::godot_binding {

namespace {

std::string to_std(const godot::String& text) {
    const godot::CharString utf8 = text.utf8();
    return {utf8.get_data()};
}

godot::String to_godot(const std::string& text) {
    return godot::String::utf8(text.c_str());
}

// GDScript's integers are 64-bit: held to the int range before they narrow,
// so an out-of-range gear is held to the gearbox by the session, not wrapped
// into another gear.
constexpr std::int64_t kMinGear = std::numeric_limits<int>::min();
constexpr std::int64_t kMaxGear = std::numeric_limits<int>::max();

const char* ending_name(run::Ending ending) {
    switch (ending) {
        case run::Ending::kFullPull:
            return "full_pull";
        case run::Ending::kStalled:
            return "stalled";
        case run::Ending::kSpunOut:
            return "spun_out";
        case run::Ending::kStopped:
            return "stopped";
        case run::Ending::kTimedOut:
            return "timed_out";
        case run::Ending::kOutOfBounds:
            return "out_of_bounds";
    }
    return "unknown";
}

const char* describe(session::SetupError error) {
    switch (error) {
        case session::SetupError::kTractor:
            return "invalid tractor";
        case session::SetupError::kSled:
            return "invalid sled";
        case session::SetupError::kTrack:
            return "invalid track";
        case session::SetupError::kRules:
            return "invalid run rules";
        case session::SetupError::kEasyControls:
            return "invalid easy controls";
        case session::SetupError::kProControls:
            return "invalid pro controls";
        case session::SetupError::kParking:
            return "the parking must be beside the track, out of the lane";
        case session::SetupError::kMaxStepsPerFrame:
            return "invalid steps per frame";
    }
    return "invalid setup";
}

const char* stage_name(turn::Stage stage) {
    switch (stage) {
        case turn::Stage::kToTheSled:
            return "to_the_sled";
        case turn::Stage::kTakingUpSlack:
            return "taking_up_slack";
        case turn::Stage::kPulling:
            return "pulling";
        case turn::Stage::kReleasing:
            return "releasing";
        case turn::Stage::kToTheParking:
            return "to_the_parking";
        case turn::Stage::kParked:
            return "parked";
    }
    return "unknown";
}

}  // namespace

godot::String TractorPullRun::setup(const godot::String& tractor_path,
                                    const godot::String& sled_path, const godot::String& track_path,
                                    const godot::String& controls_path) {
    reset();
    const data::ReadFile read = read_godot_file;
    const auto tractor = data::load_tractor(to_std(tractor_path), read);
    if (!tractor) {
        return to_godot(tractor.error().message);
    }
    const auto sled = data::load_sled(to_std(sled_path), read);
    if (!sled) {
        return to_godot(sled.error().message);
    }
    const auto track = data::load_track(to_std(track_path), read);
    if (!track) {
        return to_godot(track.error().message);
    }
    return start(data::run_setup(tractor.value(), sled.value(), track.value()), controls_path,
                 read);
}

godot::String TractorPullRun::setup_event_pull(const godot::Ref<TractorPullEvent>& event,
                                               const godot::String& controls_path) {
    reset();
    if (event.is_null()) {
        return "no event";
    }
    const std::optional<session::Setup> setup = event->player_setup();
    if (!setup) {
        return "no pull of the player's is due";
    }
    godot::String error = start(*setup, controls_path, read_godot_file);
    if (error.is_empty()) {
        event_pull_ = event->pull_key();
    }
    return error;
}

godot::String TractorPullRun::setup_practice(const godot::Ref<TractorPullEvent>& event,
                                             const godot::String& controls_path) {
    reset();
    if (event.is_null()) {
        return "no event";
    }
    const std::optional<session::Setup> setup = event->practice_setup();
    if (!setup) {
        return "no event to practise at";
    }
    return start(*setup, controls_path, read_godot_file);
}

void TractorPullRun::reset() {
    session_.reset();
    setup_.reset();
    event_pull_.reset();
    dropped_ns_ = 0;
}

godot::String TractorPullRun::start(session::Setup setup, const godot::String& controls_path,
                                    const data::ReadFile& read) {
    setup.parking = parking_;
    if (!controls_path.is_empty()) {
        const auto controls = data::load_control_layout(to_std(controls_path), read);
        if (!controls) {
            return to_godot(controls.error().message);
        }
        setup.layout = controls.value().layout;
        setup.easy = controls.value().easy;
        setup.pro = controls.value().pro;
    }
    if (const auto error = session::validate(setup)) {
        return describe(*error);
    }
    session_ = session::Session::create(setup);
    if (session_) {
        setup_ = setup;
    }
    return session_ ? godot::String{} : godot::String{"inconsistent tractor, sled or track"};
}

void TractorPullRun::set_parking(double x_m, double y_m) {
    parking_ = turn::Parking{.x_m = x_m, .y_m = y_m};
}

void TractorPullRun::clear_parking() {
    parking_.reset();
}

void TractorPullRun::set_controls(double throttle, double clutch_pedal, std::int64_t gear,
                                  double steering, double brake_left, double brake_right) {
    controls_ = {.throttle = throttle,
                 .clutch_pedal = clutch_pedal,
                 .gear = static_cast<int>(std::clamp<std::int64_t>(gear, kMinGear, kMaxGear)),
                 .steering = steering,
                 .brake_left = brake_left,
                 .brake_right = brake_right};
}

std::int64_t TractorPullRun::advance(std::int64_t elapsed_ns) {
    if (!session_) {
        return 0;
    }
    const session::Frame frame = session_->advance(controls_, elapsed_ns);
    dropped_ns_ = frame.dropped_ns;
    return frame.steps;
}

std::int64_t TractorPullRun::get_dropped_ns() const {
    return dropped_ns_;
}

godot::Dictionary TractorPullRun::get_snapshot() const {
    godot::Dictionary out;
    if (!session_) {
        return out;
    }
    const session::Snapshot s =
        session::blend(session_->previous(), session_->current(), session_->alpha());
    out["time_s"] = s.time_s;
    out["tractor_x_m"] = s.tractor_x_m;
    out["tractor_y_m"] = s.tractor_y_m;
    out["tractor_heading_rad"] = s.tractor_heading_rad;
    out["tractor_pitch_rad"] = s.tractor_pitch_rad;
    out["tractor_speed_m_s"] = s.tractor_speed_m_s;
    out["steering_rad"] = s.steering_rad;
    out["rear_left_omega_rad_s"] = s.rear_left_omega_rad_s;
    out["rear_right_omega_rad_s"] = s.rear_right_omega_rad_s;
    out["slip_ratio"] = s.slip_ratio;
    out["front_axle_load_n"] = s.front_axle_load_n;
    out["engine_omega_rad_s"] = s.engine_omega_rad_s;
    out["engine_running"] = s.engine_running;
    out["gear"] = s.gear;
    out["lowest_gear"] = s.lowest_gear;
    out["throttle"] = s.throttle;
    out["clutch_pedal"] = s.clutch_pedal;
    out["sled_x_m"] = s.sled_x_m;
    out["sled_y_m"] = s.sled_y_m;
    out["sled_heading_rad"] = s.sled_heading_rad;
    out["sled_speed_m_s"] = s.sled_speed_m_s;
    out["box_ahead_of_wheels_m"] = s.box_ahead_of_wheels_m;
    out["pushdown"] = s.pushdown;
    out["berm_m3"] = s.berm_m3;
    out["chain_tension_n"] = s.chain_tension_n;
    out["stage"] = stage_name(s.stage);
    out["chain_hooked"] = s.chain_hooked;
    out["hitch_to_hook_m"] = s.hitch_to_hook_m;
    out["hitch_along_m"] = s.hitch_along_m;
    out["hitch_across_m"] = s.hitch_across_m;
    out["flag_progress"] = s.flag_progress;
    out["sled_back"] = s.sled_back;
    return out;
}

godot::Dictionary TractorPullRun::get_dimensions() const {
    godot::Dictionary out;
    if (!setup_) {
        return out;
    }
    const vehicle::Params& tractor = setup_->tractor;
    const chassis::Params& chassis = tractor.chassis;
    out["wheelbase_m"] = chassis.wheelbase_m;
    out["rear_axle_height_m"] = chassis.rear_axle_height_m;
    out["rear_track_m"] = chassis.rear_track_m;
    out["front_track_m"] = chassis.front_track_m;
    out["rear_tire_radius_m"] = tractor.rear_tire.size.radius_m;
    out["rear_tire_width_m"] = tractor.rear_tire.size.section_width_m;
    out["front_tire_radius_m"] = tractor.front_tire.radius_m;
    out["front_tire_width_m"] = tractor.front_tire.section_width_m;
    out["hitch_behind_rear_axle_m"] = chassis.hitch_behind_rear_axle_m;
    out["hitch_height_m"] = chassis.hitch_height_m;
    out["wheelie_bar_behind_rear_axle_m"] = chassis.wheelie_bar.behind_rear_axle_m;
    out["wheelie_bar_tip_height_m"] = chassis.wheelie_bar.tip_height_m;
    // The weights as the puller set them, for drawing: how much, and where
    // between their rearmost and foremost places (0 to 1).
    out["ballast_mass_kg"] = chassis.ballast.mass_kg;
    out["ballast_position"] = chassis.ballast.position;
    // What the front axle carries at rest, as the tractor is tuned.
    out["front_axle_rest_load_n"] = chassis::axle_loads_at_rest(chassis).front_n;
    out["gear_count"] = static_cast<std::int64_t>(tractor.driveline.gear_count);
    out["has_reverse"] = tractor.driveline.reverse_ratio > 0.0;
    const sled::Params& sled = setup_->sled;
    out["sled_wheel_radius_m"] = sled.wheel.radius_m;
    out["sled_wheel_width_m"] = sled.wheel.section_width_m;
    out["hook_ahead_of_wheels_m"] = sled.hook_ahead_of_wheels_m;
    out["hook_height_m"] = sled.hook_height_m;
    out["pan_ahead_of_wheels_m"] = sled.pan_ahead_of_wheels_m;
    out["box_start_ahead_of_wheels_m"] = sled.box_start_ahead_of_wheels_m;
    out["box_end_ahead_of_wheels_m"] = sled.box_end_ahead_of_wheels_m;
    // Where the push-down starts and has the wheels fully off, as the sled
    // is set for this pull [m along the track].
    out["pushdown_start_m"] = sled.pushdown_start_m;
    out["pushdown_full_m"] = sled.pushdown_start_m + sled.pushdown_ramp_m;
    out["chain_length_m"] = sled.chain.length_m;
    const run::Rules& rules = setup_->rules;
    out["track_length_m"] = rules.track_length_m;
    out["lane_width_m"] = rules.lane_width_m;
    out["full_pull_line_m"] = rules.full_pull_line_m;
    const engine::Params& engine = tractor.engine;
    out["stall_omega_rad_s"] = engine.stall_omega_rad_s;
    out["rated_omega_rad_s"] = engine.rated_omega_rad_s;
    out["high_idle_omega_rad_s"] = engine.high_idle_omega_rad_s;
    const clutch::EngagementRange clutch = clutch::engagement_range(tractor.clutch);
    out["clutch_full_grip_pedal"] = clutch.full_grip_pedal;
    out["clutch_bite_pedal"] = clutch.bite_pedal;
    return out;
}

godot::String TractorPullRun::get_layout() const {
    if (!setup_) {
        return {};
    }
    return setup_->layout == session::Layout::kEasy ? "easy" : "pro";
}

bool TractorPullRun::is_finished() const {
    return session_ && session_->result().has_value();
}

bool TractorPullRun::is_over() const {
    return session_ && session_->over();
}

godot::Dictionary TractorPullRun::get_result() const {
    godot::Dictionary out;
    if (!session_) {
        return out;
    }
    const std::optional<run::Result>& result = session_->result();
    if (!result) {
        return out;
    }
    out["ending"] = ending_name(result->ending);
    out["distance_m"] = result->distance_m;
    out["time_s"] = result->time_s;
    out["top_speed_m_s"] = result->top_speed_m_s;
    return out;
}

godot::String TractorPullRun::get_state_hash() const {
    if (!session_) {
        return {};
    }
    constexpr int kHexDigits = 16;
    constexpr int kBitsPerDigit = 4;
    constexpr std::uint64_t kDigitMask = 0xfU;
    constexpr std::array<char, kHexDigits> kDigits = {'0', '1', '2', '3', '4', '5', '6', '7',
                                                      '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
    const std::uint64_t hash = session_->hash();
    std::string hex(kHexDigits, '0');
    for (int i = 0; i < kHexDigits; ++i) {
        const auto shift = static_cast<unsigned>((kHexDigits - 1 - i) * kBitsPerDigit);
        hex[static_cast<std::size_t>(i)] = kDigits.at((hash >> shift) & kDigitMask);
    }
    return to_godot(hex);
}

void TractorPullRun::_bind_methods() {
    using godot::ClassDB;
    using godot::D_METHOD;
    ClassDB::bind_method(
        D_METHOD("setup", "tractor_path", "sled_path", "track_path", "controls_path"),
        &TractorPullRun::setup);
    ClassDB::bind_method(D_METHOD("setup_event_pull", "event", "controls_path"),
                         &TractorPullRun::setup_event_pull);
    ClassDB::bind_method(D_METHOD("setup_practice", "event", "controls_path"),
                         &TractorPullRun::setup_practice);
    ClassDB::bind_method(D_METHOD("set_parking", "x_m", "y_m"), &TractorPullRun::set_parking);
    ClassDB::bind_method(D_METHOD("clear_parking"), &TractorPullRun::clear_parking);
    ClassDB::bind_method(D_METHOD("set_controls", "throttle", "clutch_pedal", "gear", "steering",
                                  "brake_left", "brake_right"),
                         &TractorPullRun::set_controls);
    ClassDB::bind_method(D_METHOD("advance", "elapsed_ns"), &TractorPullRun::advance);
    ClassDB::bind_method(D_METHOD("get_dropped_ns"), &TractorPullRun::get_dropped_ns);
    ClassDB::bind_method(D_METHOD("get_snapshot"), &TractorPullRun::get_snapshot);
    ClassDB::bind_method(D_METHOD("get_dimensions"), &TractorPullRun::get_dimensions);
    ClassDB::bind_method(D_METHOD("get_layout"), &TractorPullRun::get_layout);
    ClassDB::bind_method(D_METHOD("is_finished"), &TractorPullRun::is_finished);
    ClassDB::bind_method(D_METHOD("is_over"), &TractorPullRun::is_over);
    ClassDB::bind_method(D_METHOD("get_result"), &TractorPullRun::get_result);
    ClassDB::bind_method(D_METHOD("get_state_hash"), &TractorPullRun::get_state_hash);
}

}  // namespace tp::godot_binding
