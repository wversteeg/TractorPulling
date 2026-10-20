#pragma once

#include "tp/core/controls/controls.hpp"
#include "tp/data/files.hpp"
#include "tp/data/result.hpp"

#include <filesystem>
#include <string_view>

namespace tp::data {

/// Loads and validates the easy control layout and its clutch assist (see
/// data/controls/easy.toml).
[[nodiscard]] Result<controls::EasyParams> load_easy_controls(const std::filesystem::path& path);

/// As load_easy_controls, reading through `read` (see files.hpp).
[[nodiscard]] Result<controls::EasyParams> load_easy_controls(std::string_view path,
                                                              const ReadFile& read);

/// As load_easy_controls, from TOML text. `source_name` prefixes error
/// messages.
[[nodiscard]] Result<controls::EasyParams> parse_easy_controls(std::string_view toml_text,
                                                               std::string_view source_name);

}  // namespace tp::data
