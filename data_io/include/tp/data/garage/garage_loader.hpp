#pragma once

#include "tp/core/garage/garage.hpp"
#include "tp/data/files.hpp"
#include "tp/data/result.hpp"

#include <string>
#include <string_view>
#include <vector>

namespace tp::data {

/// How the game names an upgrade and each of its levels.
struct UpgradeText {
    std::string name;
    std::vector<std::string> level_labels;
};

/// How the game names a choice and each of its alternatives.
struct ChoiceText {
    std::string name;
    std::vector<std::string> alternative_labels;
};

/// A garage file (see data/garages/): the catalogue, and its names in the
/// catalogue's order.
struct GarageDefinition {
    garage::Catalogue catalogue;
    std::vector<UpgradeText> texts;
    std::vector<ChoiceText> choice_texts;
};

/// A garage from TOML text; `source_name` names it in error messages, and
/// the component files its choices name are read through `read`, relative
/// to `base_directory`. Refuses an unknown part or slot, a part listed
/// twice where both could count, a level or alternative without its value
/// (or file), cost or label, a negative cost, a first level or alternative
/// that costs anything, more upgrades, levels, choices or alternatives than
/// the catalogue holds, a name used twice, an upgrade naming a choice or
/// alternative there is not, and a component file that does not load (with
/// that file's own message). Whether the levels fit a tractor and its class
/// is garage::validate's (the season loader checks).
[[nodiscard]] Result<GarageDefinition> parse_garage(std::string_view toml_text,
                                                    std::string_view source_name,
                                                    std::string_view base_directory,
                                                    const ReadFile& read);

/// As parse_garage, the files from the disk relative to the working
/// directory.
[[nodiscard]] Result<GarageDefinition> parse_garage(std::string_view toml_text,
                                                    std::string_view source_name);

/// As parse_garage, reading the file through `read` (see files.hpp).
[[nodiscard]] Result<GarageDefinition> load_garage(std::string_view path, const ReadFile& read);

}  // namespace tp::data
