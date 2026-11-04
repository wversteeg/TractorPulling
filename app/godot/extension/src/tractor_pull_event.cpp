#include "tractor_pull_event.hpp"

#include "tp/data/session/run_setup.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "godot_files.hpp"
#include "tractor_pull_run.hpp"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/variant/char_string.hpp>
#include <godot_cpp/variant/packed_float64_array.hpp>
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
        case event::SledChange::kTroubleSuspected:
            return "trouble";
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

godot::String TractorPullEvent::load_career(const godot::String& career_path) {
    auto loaded = data::load_career(to_std(career_path), read_godot_file);
    if (!loaded) {
        return to_godot(loaded.error().message);
    }
    career_definition_ = std::move(loaded).value();
    progress_ = {};
    return {};
}

std::string TractorPullEvent::class_season() const {
    if (!career_definition_) {
        return {};
    }
    return career_definition_->rungs.at(static_cast<std::size_t>(progress_.rung)).season;
}

bool TractorPullEvent::between_seasons() const {
    if (!season_) {
        return true;
    }
    return season_->over() ||
           (season_->events_scored() == 0 && season_->player_may_change_tractor());
}

godot::Dictionary TractorPullEvent::get_career() const {
    godot::Dictionary out;
    if (!career_definition_) {
        return out;
    }
    const std::vector<data::CareerRung>& rungs = career_definition_->rungs;
    godot::PackedStringArray names;
    for (const data::CareerRung& r : rungs) {
        names.push_back(to_godot(r.class_name + " · " + r.level_name));
    }
    const data::CareerRung& here = rungs.at(static_cast<std::size_t>(progress_.rung));
    const std::size_t above = static_cast<std::size_t>(progress_.rung) + 1;
    const bool top = above >= rungs.size();
    out["name"] = to_godot(career_definition_->name);
    out["rungs"] = names;
    out["rung"] = progress_.rung;
    out["class_name"] = to_godot(here.class_name);
    out["level_name"] = to_godot(here.level_name);
    out["unlocked"] = progress_.unlocked;
    out["next_class"] = top ? godot::String{} : to_godot(rungs[above].class_name);
    out["next_level"] = top ? godot::String{} : to_godot(rungs[above].level_name);
    out["next_is_new_class"] = !top && rungs[above].class_name != here.class_name;
    out["can_move_up"] = career::may_move_up(progress_) && between_seasons();
    out["seasons"] = progress_.seasons;
    out["min_seasons"] = career_definition_->ladder.min_seasons;
    out["title"] = progress_.title;
    return out;
}

godot::String TractorPullEvent::new_career(std::int64_t seed) {
    return new_career_at(0, seed);
}

godot::String TractorPullEvent::new_career_at(std::int64_t rung, std::int64_t seed) {
    if (!career_definition_) {
        return "no career";
    }
    const std::optional<career::Progress> start = career::starting_at(
        career_definition_->ladder,
        static_cast<int>(std::clamp<std::int64_t>(rung, -1, career::kMaxRungs)));
    if (!start) {
        return "no such rung";
    }
    const career::Progress kept = progress_;
    progress_ = *start;
    godot::String error = start_career(to_godot(class_season()), seed);
    if (!error.is_empty()) {
        progress_ = kept;
    }
    return error;
}

godot::String TractorPullEvent::new_season(std::int64_t seed) {
    if (!career_definition_) {
        return "no career";
    }
    return start_season(to_godot(class_season()), seed);
}

godot::String TractorPullEvent::resume_career(const godot::String& save) {
    if (!career_definition_) {
        return "no career";
    }
    const data::Result<data::SeasonSave> parsed =
        data::parse_season_save(to_std(save), "the saved season");
    if (!parsed) {
        season_.reset();
        return to_godot(parsed.error().message);
    }
    // By name; a class the career no longer has is its first rung, a level
    // its class's first (or a save from before the levels), so the save's
    // money and upgrades still carry into a new season there.
    const auto index_of = [&](const std::string& class_name, const std::string& level_name) {
        const std::vector<data::CareerRung>& rungs = career_definition_->rungs;
        int first_of_class = -1;
        for (std::size_t i = 0; i < rungs.size(); ++i) {
            if (rungs[i].class_name != class_name) {
                continue;
            }
            if (first_of_class < 0) {
                first_of_class = static_cast<int>(i);
            }
            if (rungs[i].level_name == level_name) {
                return static_cast<int>(i);
            }
        }
        return std::max(first_of_class, 0);
    };
    progress_ = {};
    if (const std::optional<data::SavedCareer>& career = parsed.value().career) {
        const career::Progress saved{
            .rung = index_of(career->class_name, career->level_name),
            .unlocked = index_of(career->unlocked_class, career->unlocked_level),
            .seasons = career->seasons_on_level,
            .title = career->title_on_level};
        if (career::valid(saved, career_definition_->ladder)) {
            progress_ = saved;
        } else {
            progress_ = {.rung = saved.rung, .unlocked = saved.rung, .seasons = 0, .title = false};
        }
    }
    return resume_season(to_godot(class_season()), save);
}

godot::String TractorPullEvent::move_up(std::int64_t seed) {
    if (!career_definition_) {
        return "no career";
    }
    if (!career::may_move_up(progress_)) {
        return "nothing above is open";
    }
    if (!between_seasons()) {
        return "not during a season";
    }
    const career::Progress kept = progress_;
    career::move_up(progress_);
    godot::String error = new_season(seed);
    if (!error.is_empty()) {
        progress_ = kept;
    }
    return error;
}

godot::String TractorPullEvent::start_career(const godot::String& season_path, std::int64_t seed) {
    // The career so far stays if the season cannot be started.
    const std::optional<garage::Career> kept = career_;
    career_.reset();
    godot::String error = start_season(season_path, seed);
    if (!error.is_empty()) {
        career_ = kept;
    }
    return error;
}

godot::String TractorPullEvent::start_season(const godot::String& season_path, std::int64_t seed) {
    // The upgrades' names, to carry the career's levels into another
    // class's garage.
    std::vector<data::UpgradeText> bought_from;
    if (definition_) {
        bought_from = definition_->garage.texts;
    }
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
    if (!career_) {
        career_ = garage::new_career(d.garage.catalogue);
    } else if (!bought_from.empty()) {
        // By name, as a save is read: what the new garage does not offer is
        // left behind, and what it offers that was not bought starts at 0.
        garage::Career carried{.money_eur = career_->money_eur, .levels = {}};
        for (std::size_t from = 0; from < bought_from.size(); ++from) {
            for (std::size_t to = 0; to < d.garage.texts.size(); ++to) {
                if (d.garage.texts[to].name == bought_from[from].name) {
                    carried.levels.at(to) = career_->levels.at(from);
                }
            }
        }
        career_ = carried;
    }
    definition_ = std::move(loaded).value();
    tuning_ = vehicle::as_built(player_tractor());
    names_.emplace_back(kPlayerName);
    names_.insert(names_.end(), definition_->event.game.rival_names.begin(),
                  definition_->event.game.rival_names.end());
    season_path_ = to_std(season_path);
    seed_ = static_cast<std::uint64_t>(seed);
    return {};
}

godot::String TractorPullEvent::get_save() const {
    if (!season_) {
        return {};
    }
    data::SeasonSave save{
        .season = season_path_, .seed = seed_, .tuning = tuning_, .career = {}, .events = {}};
    if (career_ && definition_) {
        data::SavedCareer career{.money_eur = career_->money_eur,
                                 .upgrades = {},
                                 .class_name = {},
                                 .level_name = {},
                                 .unlocked_class = {},
                                 .unlocked_level = {},
                                 .seasons_on_level = 0,
                                 .title_on_level = false};
        if (career_definition_) {
            const std::vector<data::CareerRung>& rungs = career_definition_->rungs;
            const data::CareerRung& here = rungs.at(static_cast<std::size_t>(progress_.rung));
            const data::CareerRung& open = rungs.at(static_cast<std::size_t>(progress_.unlocked));
            career.class_name = here.class_name;
            career.level_name = here.level_name;
            career.unlocked_class = open.class_name;
            career.unlocked_level = open.level_name;
            career.seasons_on_level = progress_.seasons;
            career.title_on_level = progress_.title;
        }
        const std::vector<data::UpgradeText>& texts = definition_->garage.texts;
        for (std::size_t i = 0; i < texts.size(); ++i) {
            career.upgrades.push_back({.name = texts[i].name, .level = career_->levels.at(i)});
        }
        save.career = career;
    }
    for (int i = 0; i < season_->events_scored(); ++i) {
        const std::span<const event::Standing> standings = season_->event_standings(i);
        save.events.emplace_back(standings.begin(), standings.end());
    }
    return to_godot(data::write_season_save(save));
}

godot::String TractorPullEvent::resume_season(const godot::String& season_path,
                                              const godot::String& save) {
    const data::Result<data::SeasonSave> parsed =
        data::parse_season_save(to_std(save), "the saved season");
    if (!parsed) {
        season_.reset();
        return to_godot(parsed.error().message);
    }
    const data::SeasonSave& saved = parsed.value();
    if (godot::String error = start_season(season_path, static_cast<std::int64_t>(saved.seed));
        !error.is_empty()) {
        return error;
    }
    // The career first: it outlives seasons, so a season the save no longer
    // fits (another season's, or events the data has changed under) gives
    // way to a new season with the career kept, not to a new career.
    if (saved.career && definition_) {
        // By name: an upgrade the garage no longer offers is dropped, and a
        // level past its top is held to it (garage::equipped).
        garage::Career career{.money_eur = saved.career->money_eur, .levels = {}};
        const std::vector<data::UpgradeText>& texts = definition_->garage.texts;
        for (const data::SavedUpgrade& upgrade : saved.career->upgrades) {
            for (std::size_t i = 0; i < texts.size(); ++i) {
                if (texts[i].name == upgrade.name) {
                    career.levels.at(i) = upgrade.level;
                }
            }
        }
        career_ = career;
    }
    if (saved.season != to_std(season_path)) {
        season_.reset();
        return godot::vformat("the saved season is %s's", to_godot(saved.season));
    }
    for (const std::vector<event::Standing>& standings : saved.events) {
        if (!season_->add_pulled_event(standings)) {
            season_.reset();
            return "the saved events do not fit the season";
        }
    }
    set_tuning(saved.tuning.ballast_position, saved.tuning.rear_tire_pressure_bar);
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
    // The sled operator's trials see the player's own tractor, tuned.
    if (!season_->start_event(data::venue(definition_->event.sled, round.track),
                              vehicle::tuned(player_tractor(), tuning_))) {
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
    out["standard_full_pull_m"] = first_line_m();
    out["turn"] = turn_name(event.turn());
    if (const std::optional<event::Pull> pull = event.pending_pull()) {
        out["pull_kind"] = kind_name(pull->kind);
        out["pull_off_round"] = pull->pull_off_round;
    }
    // A pull stopped short of the restart line may move this many places
    // later (move_back()), if it may at all.
    out["move_back_places"] = event.may_defer_player() ? event::kDeferPlaces : 0;
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

double TractorPullEvent::first_line_m() const {
    return definition_ ? definition_->event.rules.full_pull_m : 0.0;
}

godot::Dictionary TractorPullEvent::get_tuning() const {
    godot::Dictionary out;
    if (!season_) {
        return out;
    }
    const vehicle::Params tractor = player_tractor();
    const vehicle::Params set = vehicle::tuned(tractor, tuning_);
    const tire::Pressure& pressure = tractor.rear_tire.pressure;
    out["ballast"] = tractor.chassis.ballast.mass_kg > 0.0;
    out["ballast_position"] = set.chassis.ballast.position;
    out["rear_share"] = chassis::rear_axle_share(set.chassis);
    out["pressure"] = pressure.reference_bar > 0.0;
    out["pressure_bar"] = tuning_.rear_tire_pressure_bar;
    out["min_bar"] = pressure.min_bar;
    out["max_bar"] = pressure.max_bar;
    return out;
}

void TractorPullEvent::set_tuning(double ballast_position, double pressure_bar) {
    // A setting that is not a number (a 0/0 in GDScript) leaves the tuning
    // as it is: std::clamp would keep it, and the save would refuse it.
    if (!season_ || !std::isfinite(ballast_position) || !std::isfinite(pressure_bar)) {
        return;
    }
    const tire::Pressure pressure = player_tractor().rear_tire.pressure;
    tuning_.ballast_position = std::clamp(ballast_position, 0.0, 1.0);
    if (pressure.reference_bar > 0.0) {
        tuning_.rear_tire_pressure_bar =
            std::clamp(pressure_bar, pressure.min_bar, pressure.max_bar);
    }
}

PullKey TractorPullEvent::pull_key() const {
    return {.season_id = get_instance_id(),
            .event = season_ ? season_->events_started() : 0,
            .log_size = get_log_size()};
}

godot::Dictionary TractorPullEvent::entry(const field::LogEntry& e, double line_before_m) const {
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
    // A test pull between the line and the extended line moved the line
    // instead: it counts its distance, not a full pull.
    out["full_pull"] = !e.disqualified && e.reached_m >= line_before_m &&
                       e.outcome.change != event::SledChange::kLineExtended;
    out["gear"] = e.gear;
    out["change"] = change_name(e.outcome.change);
    out["steps"] = e.outcome.steps;
    out["full_pull_m"] = e.full_pull_m;
    out["moved_back"] = e.moved_back;
    return out;
}

godot::Array TractorPullEvent::get_log(std::int64_t from) const {
    godot::Array out;
    if (!season_ || season_->event() == nullptr) {
        return out;
    }
    // The line before each pull is the line after the one before it.
    double line_m = first_line_m();
    const auto log = season_->event()->log();
    for (std::size_t i = 0; i < log.size(); ++i) {
        if (static_cast<std::int64_t>(i) >= from) {
            out.push_back(entry(log[i], line_m));
        }
        if (log[i].deciding < 0) {
            line_m = log[i].full_pull_m;
        }
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
        godot::PackedFloat64Array rounds;
        for (int r = 0; r < s.pull_off_rounds; ++r) {
            rounds.push_back(s.pull_off_distances_m.at(static_cast<std::size_t>(r)));
        }
        line["pull_off_distances_m"] = rounds;
        out.push_back(line);
    }
    return out;
}

std::optional<session::Setup> TractorPullEvent::player_setup() const {
    if (!season_ || season_->event() == nullptr ||
        season_->event()->turn() != field::LiveEvent::Turn::kPlayerPull) {
        return std::nullopt;
    }
    return data::run_setup(vehicle::tuned(player_tractor(), tuning_), *season_->event());
}

std::optional<session::Setup> TractorPullEvent::practice_setup() const {
    if (!season_ || season_->event() == nullptr) {
        return std::nullopt;
    }
    return data::run_setup(vehicle::tuned(player_tractor(), tuning_), *season_->event());
}

godot::Dictionary TractorPullEvent::record_pull(const godot::Ref<TractorPullRun>& run) {
    if (!season_ || season_->event() == nullptr || run.is_null()) {
        return {};
    }
    field::LiveEvent& event = *season_->event();
    const session::Session* session = run->session();
    if (event.turn() != field::LiveEvent::Turn::kPlayerPull || session == nullptr ||
        run->event_pull() != std::optional<PullKey>{pull_key()}) {
        return {};
    }
    const std::optional<run::Result>& result = session->result();
    if (!result.has_value()) {
        return {};
    }
    const double line_before_m = event.full_pull_m();
    event.record_player(*result, session->current().gear);
    return entry(event.log().back(), line_before_m);
}

godot::Dictionary TractorPullEvent::move_back() {
    if (!season_ || season_->event() == nullptr) {
        return {};
    }
    field::LiveEvent& event = *season_->event();
    const double line_m = event.full_pull_m();
    if (event.defer_player() == 0) {
        return {};
    }
    return entry(event.log().back(), line_m);
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
    if (!season_ || !definition_ || !career_) {
        return false;
    }
    if (!season_->score_event(definition_->prize_money_eur, *career_)) {
        return false;
    }
    // A season in the career's class counts, once: its last event is
    // scored once.
    if (season_->over() && career_definition_ && season_path_ == class_season()) {
        career::finish_season(progress_, career_definition_->ladder, season_->standings(),
                              field::LiveSeason::kPlayer);
    }
    return true;
}

std::int64_t TractorPullEvent::get_event_prize() const {
    if (!season_ || !definition_) {
        return 0;
    }
    const std::optional<event::Standing> standing = season_->player_standing();
    return standing ? garage::prize_eur(definition_->prize_money_eur, *standing) : 0;
}

vehicle::Params TractorPullEvent::player_tractor() const {
    const vehicle::Params& reference = season_->player().tractor;
    if (!career_ || !definition_) {
        return reference;
    }
    const garage::Catalogue& catalogue = definition_->garage.catalogue;
    // The season loader checked every level against the class, so this
    // holds; were it ever refused, the tractor the career starts with is the
    // honest fallback, not the rivals' reference.
    const std::optional<vehicle::Params> equipped =
        garage::equipped(reference, catalogue, *career_);
    if (equipped) {
        return *equipped;
    }
    return garage::equipped(reference, catalogue, garage::new_career(catalogue))
        .value_or(reference);
}

godot::Dictionary TractorPullEvent::get_garage() const {
    godot::Dictionary out;
    if (!season_ || !definition_ || !career_) {
        return out;
    }
    const data::GarageDefinition& garage = definition_->garage;
    out["money_eur"] = career_->money_eur;
    out["can_buy"] = season_->player_may_change_tractor();
    out["fully_upgraded"] = garage::fully_upgraded(garage.catalogue, *career_);
    godot::Array upgrades;
    for (std::size_t i = 0; i < garage.catalogue.count; ++i) {
        const garage::Upgrade& upgrade = garage.catalogue.upgrades.at(i);
        const data::UpgradeText& text = garage.texts.at(i);
        const auto top = static_cast<int>(upgrade.level_count) - 1;
        const int level = std::clamp(career_->levels.at(i), 0, top);
        godot::Dictionary line;
        line["name"] = to_godot(text.name);
        line["level"] = level;
        line["levels"] = static_cast<int>(upgrade.level_count);
        line["label"] = to_godot(text.level_labels.at(static_cast<std::size_t>(level)));
        const bool at_top = level >= top;
        const auto next = static_cast<std::size_t>(at_top ? level : level + 1);
        line["next_label"] = at_top ? godot::String{} : to_godot(text.level_labels.at(next));
        line["next_cost_eur"] = at_top ? std::int64_t{0} : upgrade.levels.at(next).cost_eur;
        upgrades.push_back(line);
    }
    out["upgrades"] = upgrades;
    return out;
}

godot::String TractorPullEvent::buy_upgrade(std::int64_t index) {
    if (!season_ || !definition_ || !career_ || index < 0) {
        return "no such upgrade";
    }
    if (!season_->player_may_change_tractor()) {
        return "not during an event";
    }
    const std::optional<garage::BuyError> error =
        garage::buy(*career_, definition_->garage.catalogue, static_cast<std::size_t>(index));
    if (error) {
        switch (*error) {
            case garage::BuyError::kNoSuchUpgrade:
                return "no such upgrade";
            case garage::BuyError::kTopLevel:
                return "top level";
            case garage::BuyError::kNotEnoughMoney:
                return "not enough money";
        }
    }
    // The tuning keeps its settings, held to the new tractor's ranges.
    set_tuning(tuning_.ballast_position, tuning_.rear_tire_pressure_bar);
    return {};
}

bool TractorPullEvent::add_money(std::int64_t amount_eur) {
    if (!career_) {
        return false;
    }
    career_->money_eur = std::max<std::int64_t>(career_->money_eur + amount_eur, 0);
    return true;
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
    ClassDB::bind_method(D_METHOD("load_career", "career_path"), &TractorPullEvent::load_career);
    ClassDB::bind_method(D_METHOD("get_career"), &TractorPullEvent::get_career);
    ClassDB::bind_method(D_METHOD("new_career", "seed"), &TractorPullEvent::new_career);
    ClassDB::bind_method(D_METHOD("new_career_at", "rung", "seed"),
                         &TractorPullEvent::new_career_at);
    ClassDB::bind_method(D_METHOD("new_season", "seed"), &TractorPullEvent::new_season);
    ClassDB::bind_method(D_METHOD("resume_career", "save"), &TractorPullEvent::resume_career);
    ClassDB::bind_method(D_METHOD("move_up", "seed"), &TractorPullEvent::move_up);
    ClassDB::bind_method(D_METHOD("start_career", "season_path", "seed"),
                         &TractorPullEvent::start_career);
    ClassDB::bind_method(D_METHOD("get_save"), &TractorPullEvent::get_save);
    ClassDB::bind_method(D_METHOD("resume_season", "season_path", "save"),
                         &TractorPullEvent::resume_season);
    ClassDB::bind_method(D_METHOD("get_season"), &TractorPullEvent::get_season);
    ClassDB::bind_method(D_METHOD("start_event"), &TractorPullEvent::start_event);
    ClassDB::bind_method(D_METHOD("get_event"), &TractorPullEvent::get_event);
    ClassDB::bind_method(D_METHOD("advance", "max_steps"), &TractorPullEvent::advance);
    ClassDB::bind_method(D_METHOD("get_log", "from"), &TractorPullEvent::get_log);
    ClassDB::bind_method(D_METHOD("get_log_size"), &TractorPullEvent::get_log_size);
    ClassDB::bind_method(D_METHOD("get_standings"), &TractorPullEvent::get_standings);
    ClassDB::bind_method(D_METHOD("record_pull", "run"), &TractorPullEvent::record_pull);
    ClassDB::bind_method(D_METHOD("move_back"), &TractorPullEvent::move_back);
    ClassDB::bind_method(D_METHOD("get_suggested_keep"), &TractorPullEvent::get_suggested_keep);
    ClassDB::bind_method(D_METHOD("decide", "keep"), &TractorPullEvent::decide);
    ClassDB::bind_method(D_METHOD("score_event"), &TractorPullEvent::score_event);
    ClassDB::bind_method(D_METHOD("get_season_standings"), &TractorPullEvent::get_season_standings);
    ClassDB::bind_method(D_METHOD("get_event_prize"), &TractorPullEvent::get_event_prize);
    ClassDB::bind_method(D_METHOD("get_garage"), &TractorPullEvent::get_garage);
    ClassDB::bind_method(D_METHOD("buy_upgrade", "index"), &TractorPullEvent::buy_upgrade);
    ClassDB::bind_method(D_METHOD("add_money", "amount_eur"), &TractorPullEvent::add_money);
    ClassDB::bind_method(D_METHOD("get_tuning"), &TractorPullEvent::get_tuning);
    ClassDB::bind_method(D_METHOD("set_tuning", "ballast_position", "pressure_bar"),
                         &TractorPullEvent::set_tuning);
}

}  // namespace tp::godot_binding
