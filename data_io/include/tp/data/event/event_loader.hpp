#pragma once

#include "tp/core/event/event.hpp"
#include "tp/core/field/field.hpp"
#include "tp/core/rules/class_rules.hpp"
#include "tp/core/sled/sled.hpp"
#include "tp/data/files.hpp"
#include "tp/data/result.hpp"
#include "tp/data/track/track_loader.hpp"

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace tp::data {

/// The event as the game plays it: the player and scripted rivals.
struct GameField {
    /// Pullers, the player among them.
    int pullers = 0;
    /// The rivals' names, one for each puller but the player, in the
    /// field's order with the player left out.
    std::vector<std::string> rival_names;
};

/// Everything an event file describes (see data/events/): the class and
/// its competition rules, the field (a reference tractor and driver, how
/// much the pullers vary, how many and the seed), and the sled and track.
struct EventDefinition {
    rules::ClassRules class_rules;
    event::Rules rules;
    field::Puller reference;
    field::Variation variation;
    field::PullToPull pull_to_pull;
    int pullers = 0;
    std::uint64_t seed = 0;
    /// The class's sled, before the track's conditions.
    sled::Params sled;
    Track track;
    GameField game;
};

/// Loads and validates an event definition; component paths are relative
/// to the event file.
[[nodiscard]] Result<EventDefinition> load_event(const std::filesystem::path& path);

/// As load_event, reading this file and the files it names through `read`
/// (see files.hpp).
[[nodiscard]] Result<EventDefinition> load_event(std::string_view path, const ReadFile& read);

/// As load_event, from TOML text.
[[nodiscard]] Result<EventDefinition> parse_event(std::string_view toml_text,
                                                  std::string_view source_name,
                                                  const std::filesystem::path& base_directory);

/// As parse_event, reading the files it names through `read`, relative to
/// `base_directory` ('/'-separated, see files.hpp).
[[nodiscard]] Result<EventDefinition> parse_event(std::string_view toml_text,
                                                  std::string_view source_name,
                                                  std::string_view base_directory,
                                                  const ReadFile& read);

}  // namespace tp::data
