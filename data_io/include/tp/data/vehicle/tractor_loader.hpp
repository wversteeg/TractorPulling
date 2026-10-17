#pragma once

#include "tp/core/vehicle/vehicle.hpp"
#include "tp/data/result.hpp"

#include <filesystem>
#include <string_view>

namespace tp::data {

/// Loads a tractor (see data/tractors/): its chassis, and the engine, clutch,
/// driveline and rear tire files it names, resolved relative to the tractor
/// file. Errors in a component file name that file.
[[nodiscard]] Result<vehicle::Params> load_tractor(const std::filesystem::path& path);

/// As load_tractor, from TOML text; component paths are resolved relative to
/// `base_directory`.
[[nodiscard]] Result<vehicle::Params> parse_tractor(std::string_view toml_text,
                                                    std::string_view source_name,
                                                    const std::filesystem::path& base_directory);

}  // namespace tp::data
