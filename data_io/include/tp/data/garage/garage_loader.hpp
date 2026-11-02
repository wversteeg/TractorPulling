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

/// A garage file (see data/garages/): the catalogue, and its names in the
/// catalogue's order.
struct GarageDefinition {
    garage::Catalogue catalogue;
    std::vector<UpgradeText> texts;
};

/// A garage from TOML text; `source_name` names it in error messages.
/// Refuses an unknown part, a part listed twice, a level without a value,
/// cost or label, a negative cost, a first level that costs anything, more
/// upgrades or levels than the catalogue holds. Whether the levels fit a
/// tractor and its class is garage::validate's (the season loader checks).
[[nodiscard]] Result<GarageDefinition> parse_garage(std::string_view toml_text,
                                                    std::string_view source_name);

/// As parse_garage, reading the file through `read` (see files.hpp).
[[nodiscard]] Result<GarageDefinition> load_garage(std::string_view path, const ReadFile& read);

}  // namespace tp::data
