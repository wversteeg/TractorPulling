#pragma once

#include "tp/core/clutch/clutch.hpp"
#include "tp/data/files.hpp"
#include "tp/data/result.hpp"

#include <filesystem>
#include <string_view>

namespace tp::data {

/// Loads and validates a clutch definition (see data/clutches/).
[[nodiscard]] Result<clutch::Params> load_clutch(const std::filesystem::path& path);

/// As load_clutch, reading through `read` (see files.hpp).
[[nodiscard]] Result<clutch::Params> load_clutch(std::string_view path, const ReadFile& read);

/// As load_clutch, from TOML text. `source_name` prefixes error messages.
[[nodiscard]] Result<clutch::Params> parse_clutch(std::string_view toml_text,
                                                  std::string_view source_name);

}  // namespace tp::data
