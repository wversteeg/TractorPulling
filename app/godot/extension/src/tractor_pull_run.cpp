#include "tractor_pull_run.hpp"

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
        case session::SetupError::kMaxStepsPerFrame:
            return "invalid steps per frame";
    }
    return "invalid setup";
}

}  // namespace

godot::String TractorPullRun::setup(const godot::String& tractor_path,
                                    const godot::String& sled_path, const godot::String& track_path,
                                    const godot::String& easy_controls_path) {
    session_.reset();
    dropped_ns_ = 0;
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
    session::Setup setup = data::run_setup(tractor.value(), sled.value(), track.value());
    if (!easy_controls_path.is_empty()) {
        const auto easy = data::load_easy_controls(to_std(easy_controls_path), read);
        if (!easy) {
            return to_godot(easy.error().message);
        }
        setup.layout = session::Layout::kEasy;
        setup.easy = easy.value();
    }
    if (const auto error = session::validate(setup)) {
        return describe(*error);
    }
    session_ = session::Session::create(setup);
    return session_ ? godot::String{} : godot::String{"inconsistent tractor, sled or track"};
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
    out["engine_omega_rad_s"] = s.engine_omega_rad_s;
    out["engine_running"] = s.engine_running;
    out["gear"] = s.gear;
    out["throttle"] = s.throttle;
    out["clutch_pedal"] = s.clutch_pedal;
    out["sled_x_m"] = s.sled_x_m;
    out["sled_y_m"] = s.sled_y_m;
    out["sled_heading_rad"] = s.sled_heading_rad;
    out["sled_speed_m_s"] = s.sled_speed_m_s;
    out["box_ahead_of_wheels_m"] = s.box_ahead_of_wheels_m;
    out["chain_tension_n"] = s.chain_tension_n;
    return out;
}

bool TractorPullRun::is_finished() const {
    return session_ && session_->result().has_value();
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
        D_METHOD("setup", "tractor_path", "sled_path", "track_path", "easy_controls_path"),
        &TractorPullRun::setup);
    ClassDB::bind_method(D_METHOD("set_controls", "throttle", "clutch_pedal", "gear", "steering",
                                  "brake_left", "brake_right"),
                         &TractorPullRun::set_controls);
    ClassDB::bind_method(D_METHOD("advance", "elapsed_ns"), &TractorPullRun::advance);
    ClassDB::bind_method(D_METHOD("get_dropped_ns"), &TractorPullRun::get_dropped_ns);
    ClassDB::bind_method(D_METHOD("get_snapshot"), &TractorPullRun::get_snapshot);
    ClassDB::bind_method(D_METHOD("is_finished"), &TractorPullRun::is_finished);
    ClassDB::bind_method(D_METHOD("get_result"), &TractorPullRun::get_result);
    ClassDB::bind_method(D_METHOD("get_state_hash"), &TractorPullRun::get_state_hash);
}

}  // namespace tp::godot_binding
