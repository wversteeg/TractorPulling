#pragma once

#include "tp/core/career/career.hpp"
#include "tp/data/files.hpp"
#include "tp/data/result.hpp"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace tp::data {

/// One rung of a career's ladder: the class's name as the game shows it,
/// and the season the class pulls (see data/seasons/), its path resolved
/// from the career file's directory.
struct CareerClass {
    std::string name;
    std::string season;
};

/// A career file (see data/careers/): its name, the ladder's rule and its
/// classes, lowest first (ladder.classes of them).
struct CareerDefinition {
    std::string name;
    career::Ladder ladder;
    std::vector<CareerClass> classes;
};

/// A career from TOML text; `source_name` names it in error messages, and
/// the season files it names are read through `read`, relative to
/// `base_directory`. Refuses a ladder career::validate refuses, a class
/// without a name or a season, a name used twice, and any season that does
/// not load (with that season's own message).
[[nodiscard]] Result<CareerDefinition> parse_career(std::string_view toml_text,
                                                    std::string_view source_name,
                                                    std::string_view base_directory,
                                                    const ReadFile& read);

/// As parse_career, reading this file through `read`.
[[nodiscard]] Result<CareerDefinition> load_career(std::string_view path, const ReadFile& read);

/// As load_career, from the file system.
[[nodiscard]] Result<CareerDefinition> load_career(const std::filesystem::path& path);

}  // namespace tp::data
