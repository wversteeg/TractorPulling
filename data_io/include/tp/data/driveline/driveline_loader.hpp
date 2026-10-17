#pragma once

#include "tp/core/driveline/driveline.hpp"
#include "tp/data/result.hpp"

#include <filesystem>
#include <string_view>

namespace tp::data {

/// Loads and validates a driveline definition (see data/drivelines/).
[[nodiscard]] Result<driveline::Params> load_driveline(const std::filesystem::path& path);

/// As load_driveline, from TOML text. `source_name` prefixes error messages.
[[nodiscard]] Result<driveline::Params> parse_driveline(std::string_view toml_text,
                                                        std::string_view source_name);

}  // namespace tp::data
