#pragma once

#include "tp/core/tire/tire.hpp"
#include "tp/data/files.hpp"
#include "tp/data/result.hpp"

#include <filesystem>
#include <string_view>

namespace tp::data {

/// Loads and validates a tire definition (see data/tires/).
[[nodiscard]] Result<tire::Params> load_tire(const std::filesystem::path& path);

/// As load_tire, reading through `read` (see files.hpp).
[[nodiscard]] Result<tire::Params> load_tire(std::string_view path, const ReadFile& read);

/// As load_tire, from TOML text. `source_name` prefixes error messages.
[[nodiscard]] Result<tire::Params> parse_tire(std::string_view toml_text,
                                              std::string_view source_name);

}  // namespace tp::data
