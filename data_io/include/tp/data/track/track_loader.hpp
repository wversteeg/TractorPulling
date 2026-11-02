#pragma once

#include "tp/core/run/run.hpp"
#include "tp/core/sled/sled.hpp"
#include "tp/core/track/track.hpp"
#include "tp/data/files.hpp"
#include "tp/data/result.hpp"

#include <filesystem>
#include <string_view>

namespace tp::data {

/// Everything a track file describes: the run's rules, the ground, and the
/// push-down the sled operator sets for the day.
struct Track {
    run::Rules rules;
    track::Params ground;
    sled::Conditions sled;
};

/// Loads and validates a track definition (see data/tracks/).
[[nodiscard]] Result<Track> load_track(const std::filesystem::path& path);

/// As load_track, reading through `read` (see files.hpp).
[[nodiscard]] Result<Track> load_track(std::string_view path, const ReadFile& read);

/// As load_track, from TOML text. `source_name` prefixes error messages.
[[nodiscard]] Result<Track> parse_track(std::string_view toml_text, std::string_view source_name);

}  // namespace tp::data
