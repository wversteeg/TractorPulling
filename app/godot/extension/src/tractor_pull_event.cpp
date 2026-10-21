#include "tractor_pull_event.hpp"

#include "tp/data/session/run_setup.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "godot_files.hpp"
#include "tractor_pull_run.hpp"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/char_string.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>

namespace tp::godot_binding {

namespace {

// The player's name in the lists; the rivals' come from the event file.
constexpr const char* kPlayerName = "You";

std::string to_std(const godot::String& text) {
    const godot::CharString utf8 = text.utf8();
    return {utf8.get_data()};
}

godot::String to_godot(const std::string& text) {
    return godot::String::utf8(text.c_str());
}

const char* kind_name(event::PullKind kind) {
    switch (kind) {
        case event::PullKind::kTest:
            return "test";
        case event::PullKind::kCompetition:
            return "competition";
        case event::PullKind::kRepull:
            return "repull";
        case event::PullKind::kPullOff:
            return "pull_off";
    }
    return "unknown";
}

const char* change_name(event::SledChange change) {
    switch (change) {
        case event::SledChange::kNone:
            return "none";
        case event::SledChange::kLineExtended:
            return "line_extended";
        case event::SledChange::kLighter:
            return "lighter";
        case event::SledChange::kHeavier:
            return "heavier";
    }
    return "unknown";
}

const char* turn_name(field::LiveEvent::Turn turn) {
    switch (turn) {
        case field::LiveEvent::Turn::kRivals:
            return "rivals";
        case field::LiveEvent::Turn::kPlayerPull:
            return "player_pull";
        case field::LiveEvent::Turn::kPlayerDecision:
            return "player_decision";
        case field::LiveEvent::Turn::kFinished:
            return "finished";
        case field::LiveEvent::Turn::kFailed:
            return "failed";
    }
    return "unknown";
}

}  // namespace

godot::String TractorPullEvent::start_season(const godot::String& season_path, std::int64_t seed) {
    season_.reset();
    definition_.reset();
    names_.clear();
    auto loaded = data::load_season(to_std(season_path), read_godot_file);
    if (!loaded) {
        return to_godot(loaded.error().message);
    }
    const data::SeasonDefinition& d = loaded.value();
    const data::EventDefinition& e = d.event;
    std::optional<field::LiveSeason> season = field::LiveSeason::create(
        e.rules, e.reference, e.variation, e.class_rules, e.pull_to_pull, d.points, e.game.pullers,
        static_cast<int>(d.rounds.size()), static_cast<std::uint64_t>(seed));
    if (!season) {
        return "the season's field, points or rules are inconsistent";
    }
    season_ = std::make_unique<field::LiveSeason>(*season);
    names_.emplace_back(kPlayerName);
    names_.insert(names_.end(), e.game.rival_names.begin(), e.game.rival_names.end());
    definition_ = std::move(loaded).value();
    return {};
}

godot::Dictionary TractorPullEvent::get_season() const {
    godot::Dictionary out;
    if (!season_) {
        return out;
    }
    out["events"] = season_->event_count();
    out["scored"] = season_->events_scored();
    out["index"] = season_->events_started();
    out["over"] = season_->over();
    godot::PackedStringArray names;
    for (const std::string& name : names_) {
        names.push_back(to_godot(name));
    }
    out["names"] = names;
    return out;
}

godot::String TractorPullEvent::start_event() {
    if (!season_ || !definition_) {
        return "no season";
    }
    if (season_->over()) {
        return "the season is over";
    }
    const auto index = static_cast<std::size_t>(season_->events_started());
    const data::SeasonRound& round = definition_->rounds.at(index);
    if (!season_->start_event(data::venue(definition_->event.sled, round.track))) {
        return godot::vformat("cannot start %s", to_godot(round.name));
    }
    return {};
}

godot::Dictionary TractorPullEvent::get_event() const {
    godot::Dictionary out;
    if (!season_ || !definition_ || season_->event() == nullptr) {
        return out;
    }
    const field::LiveEvent& event = *season_->event();
    const int index = season_->events_started() - 1;
    out["name"] = to_godot(definition_->rounds.at(static_cast<std::size_t>(index)).name);
    out["index"] = index;
    out["count"] = season_->event_count();
    out["pullers"] = event.puller_count();
    out["full_pull_m"] = event.full_pull_m();
    out["turn"] = turn_name(event.turn());
    if (const std::optional<event::Pull> pull = event.pending_pull()) {
        out["pull_kind"] = kind_name(pull->kind);
        out["pull_off_round"] = pull->pull_off_round;
    }
    return out;
}

std::int64_t TractorPullEvent::advance(std::int64_t max_steps) {
    if (!season_ || season_->event() == nullptr) {
        return 0;
    }
    return season_->event()->advance(max_steps);
}

godot::String TractorPullEvent::name_of(int puller) const {
    if (puller < 0 || static_cast<std::size_t>(puller) >= names_.size()) {
        return godot::vformat("#%d", puller + 1);
    }
    return to_godot(names_[static_cast<std::size_t>(puller)]);
}

godot::Dictionary TractorPullEvent::entry(const field::LogEntry& e) const {
    godot::Dictionary out;
    if (e.deciding >= 0) {
        out["deciding"] = e.deciding;
        out["name"] = name_of(e.deciding);
        out["keep"] = e.keep;
        return out;
    }
    out["puller"] = e.pull.puller;
    out["name"] = name_of(e.pull.puller);
    out["kind"] = kind_name(e.pull.kind);
    out["pull_off_round"] = e.pull.pull_off_round;
    out["reached_m"] = e.reached_m;
    out["disqualified"] = e.disqualified;
    out["gear"] = e.gear;
    out["change"] = change_name(e.outcome.change);
    out["steps"] = e.outcome.steps;
    out["full_pull_m"] = e.full_pull_m;
    return out;
}

godot::Array TractorPullEvent::get_log(std::int64_t from) const {
    godot::Array out;
    if (!season_ || season_->event() == nullptr) {
        return out;
    }
    const auto log = season_->event()->log();
    for (std::size_t i = from < 0 ? 0 : static_cast<std::size_t>(from); i < log.size(); ++i) {
        out.push_back(entry(log[i]));
    }
    return out;
}

std::int64_t TractorPullEvent::get_log_size() const {
    if (!season_ || season_->event() == nullptr) {
        return 0;
    }
    return static_cast<std::int64_t>(season_->event()->log().size());
}

godot::Array TractorPullEvent::get_standings() const {
    godot::Array out;
    if (!season_ || season_->event() == nullptr) {
        return out;
    }
    for (const event::Standing& s : season_->event()->standings()) {
        godot::Dictionary line;
        line["place"] = s.place;
        line["puller"] = s.puller;
        line["name"] = name_of(s.puller);
        line["distance_m"] = s.distance_m;
        line["full_pull"] = s.full_pull;
        line["disqualified"] = s.disqualified;
        line["pull_off_rounds"] = s.pull_off_rounds;
        line["pull_off_distance_m"] = s.pull_off_distance_m;
        out.push_back(line);
    }
    return out;
}

std::optional<session::Setup> TractorPullEvent::player_setup() const {
    if (!season_ || season_->event() == nullptr ||
        season_->event()->turn() != field::LiveEvent::Turn::kPlayerPull) {
        return std::nullopt;
    }
    return data::run_setup(season_->player().tractor, *season_->event());
}

std::optional<session::Setup> TractorPullEvent::practice_setup() const {
    if (!season_ || season_->event() == nullptr) {
        return std::nullopt;
    }
    return data::run_setup(season_->player().tractor, *season_->event());
}

godot::Dictionary TractorPullEvent::record_pull(const godot::Ref<TractorPullRun>& run) {
    if (!season_ || season_->event() == nullptr || run.is_null()) {
        return {};
    }
    field::LiveEvent& event = *season_->event();
    const session::Session* session = run->session();
    if (event.turn() != field::LiveEvent::Turn::kPlayerPull || session == nullptr ||
        run->event_pull() != get_log_size()) {
        return {};
    }
    const std::optional<run::Result>& result = session->result();
    if (!result.has_value()) {
        return {};
    }
    event.record_player(*result, session->current().gear);
    return entry(event.log().back());
}

bool TractorPullEvent::get_suggested_keep() const {
    return season_ && season_->event() != nullptr && season_->event()->suggested_keep();
}

void TractorPullEvent::decide(bool keep) {
    if (season_ && season_->event() != nullptr) {
        season_->event()->decide_player(keep);
    }
}

bool TractorPullEvent::score_event() {
    return season_ && season_->score_event();
}

godot::Array TractorPullEvent::get_season_standings() const {
    godot::Array out;
    if (!season_) {
        return out;
    }
    for (const season::Standing& s : season_->standings()) {
        godot::Dictionary line;
        line["place"] = s.place;
        line["puller"] = s.puller;
        line["name"] = name_of(s.puller);
        line["points"] = s.points;
        line["wins"] = s.wins;
        out.push_back(line);
    }
    return out;
}

void TractorPullEvent::_bind_methods() {
    using godot::ClassDB;
    using godot::D_METHOD;
    ClassDB::bind_method(D_METHOD("start_season", "season_path", "seed"),
                         &TractorPullEvent::start_season);
    ClassDB::bind_method(D_METHOD("get_season"), &TractorPullEvent::get_season);
    ClassDB::bind_method(D_METHOD("start_event"), &TractorPullEvent::start_event);
    ClassDB::bind_method(D_METHOD("get_event"), &TractorPullEvent::get_event);
    ClassDB::bind_method(D_METHOD("advance", "max_steps"), &TractorPullEvent::advance);
    ClassDB::bind_method(D_METHOD("get_log", "from"), &TractorPullEvent::get_log);
    ClassDB::bind_method(D_METHOD("get_log_size"), &TractorPullEvent::get_log_size);
    ClassDB::bind_method(D_METHOD("get_standings"), &TractorPullEvent::get_standings);
    ClassDB::bind_method(D_METHOD("record_pull", "run"), &TractorPullEvent::record_pull);
    ClassDB::bind_method(D_METHOD("get_suggested_keep"), &TractorPullEvent::get_suggested_keep);
    ClassDB::bind_method(D_METHOD("decide", "keep"), &TractorPullEvent::decide);
    ClassDB::bind_method(D_METHOD("score_event"), &TractorPullEvent::score_event);
    ClassDB::bind_method(D_METHOD("get_season_standings"), &TractorPullEvent::get_season_standings);
}

}  // namespace tp::godot_binding
