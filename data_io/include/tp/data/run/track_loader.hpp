#pragma once

#include "tp/core/run/run.hpp"
#include "tp/data/result.hpp"

#include <filesystem>
#include <string_view>

namespace tp::data {

/// Loads a track's run rules (see data/tracks/).
[[nodiscard]] Result<run::Rules> load_track(const std::filesystem::path& path);

/// As load_track, from TOML text. `source_name` prefixes error messages.
[[nodiscard]] Result<run::Rules> parse_track(std::string_view toml_text,
                                             std::string_view source_name);

}  // namespace tp::data
