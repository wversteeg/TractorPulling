#pragma once

#include "tp/core/rules/class_rules.hpp"
#include "tp/data/result.hpp"

#include <filesystem>
#include <string_view>

namespace tp::data {

/// Loads a pulling class's rules (see data/classes/). Power is given in
/// metric hp and engine speed in rpm in the file, converted to SI.
[[nodiscard]] Result<rules::ClassRules> load_class_rules(const std::filesystem::path& path);

/// As load_class_rules, from TOML text. `source_name` prefixes error messages.
[[nodiscard]] Result<rules::ClassRules> parse_class_rules(std::string_view toml_text,
                                                          std::string_view source_name);

}  // namespace tp::data
