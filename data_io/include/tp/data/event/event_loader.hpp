#pragma once

#include "tp/core/event/event.hpp"
#include "tp/core/field/field.hpp"
#include "tp/core/rules/class_rules.hpp"
#include "tp/core/sled/sled.hpp"
#include "tp/data/result.hpp"
#include "tp/data/track/track_loader.hpp"

#include <cstdint>
#include <filesystem>
#include <string_view>

namespace tp::data {

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
};

/// Loads and validates an event definition; component paths are relative
/// to the event file.
[[nodiscard]] Result<EventDefinition> load_event(const std::filesystem::path& path);

/// As load_event, from TOML text.
[[nodiscard]] Result<EventDefinition> parse_event(std::string_view toml_text,
                                                  std::string_view source_name,
                                                  const std::filesystem::path& base_directory);

}  // namespace tp::data
