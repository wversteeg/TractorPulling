#pragma once

#include "tp/core/career/career.hpp"
#include "tp/data/files.hpp"
#include "tp/data/result.hpp"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace tp::data {

/// One rung of a career's ladder: a class at one of its levels, as the game
/// shows them, and the season pulled there (see data/seasons/), its path
/// resolved from the career file's directory. A class's levels share its
/// tractor and garage; their fields differ.
struct CareerRung {
    std::string class_name;
    std::string level_name;
    std::string season;
};

/// A career file (see data/careers/): its name, the ladder's rule and its
/// rungs, lowest first (ladder.rungs of them): each class's levels in turn.
struct CareerDefinition {
    std::string name;
    career::Ladder ladder;
    std::vector<CareerRung> rungs;
};

/// A career from TOML text; `source_name` names it in error messages, and
/// the season files it names are read through `read`, relative to
/// `base_directory`. Refuses a ladder career::validate refuses (its
/// promotion place and minimum seasons on a rung), a class
/// without a name or levels, a level without a name or a season, a class
/// name used twice or a level name twice in a class, and any season that
/// does not load (with that season's own message).
[[nodiscard]] Result<CareerDefinition> parse_career(std::string_view toml_text,
                                                    std::string_view source_name,
                                                    std::string_view base_directory,
                                                    const ReadFile& read);

/// As parse_career, reading this file through `read`.
[[nodiscard]] Result<CareerDefinition> load_career(std::string_view path, const ReadFile& read);

/// As load_career, from the file system.
[[nodiscard]] Result<CareerDefinition> load_career(const std::filesystem::path& path);

}  // namespace tp::data
