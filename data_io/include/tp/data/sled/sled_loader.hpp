#pragma once

#include "tp/core/sled/sled.hpp"
#include "tp/data/result.hpp"

#include <filesystem>
#include <string_view>

namespace tp::data {

/// Loads and validates a sled set-up (see data/sleds/): the sled, its
/// weight box, push-down and chain.
[[nodiscard]] Result<sled::Params> load_sled(const std::filesystem::path& path);

/// As load_sled, from TOML text. `source_name` prefixes error messages.
[[nodiscard]] Result<sled::Params> parse_sled(std::string_view toml_text,
                                              std::string_view source_name);

}  // namespace tp::data
