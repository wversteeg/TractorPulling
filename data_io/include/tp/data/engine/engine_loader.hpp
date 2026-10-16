#pragma once

#include "tp/core/engine/engine.hpp"
#include "tp/data/result.hpp"

#include <filesystem>
#include <string_view>

namespace tp::data {

/// Loads and validates an engine definition (see data/engines/). Speeds are
/// given in rpm in the file and converted to rad/s.
[[nodiscard]] Result<engine::Params> load_engine(const std::filesystem::path& path);

/// As load_engine, from TOML text. `source_name` prefixes error messages.
[[nodiscard]] Result<engine::Params> parse_engine(std::string_view toml_text,
                                                  std::string_view source_name);

}  // namespace tp::data
