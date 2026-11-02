#pragma once

#include "tp/core/tire/tire.hpp"

#include <string>
#include <string_view>

#include "../common/toml_reader.hpp"

namespace tp::data::detail {

/// Reads a tire's size from `table` (radius_m, section_width_m,
/// section_height_m, deflection_m). Shared by tire files and the tractor's
/// front tires.
[[nodiscard]] tire::Size read_tire_size(Reader& reader, std::string_view table);

/// Reads a tire's side grip from `table` (stiffness_per_rad, limit,
/// mobility_rate, relaxation_length_m). Shared by tire files and the
/// tractor's front tires.
[[nodiscard]] tire::Cornering read_cornering(Reader& reader, std::string_view table);

/// The field, under the tire's table, and the reason for a tire error.
struct TireFieldError {
    std::string_view field;
    std::string_view reason;
};

[[nodiscard]] TireFieldError describe(tire::ParamError error);

/// `table` + "." + `field`, or `table` alone when the error names no field.
[[nodiscard]] std::string field_path(std::string_view table, std::string_view field);

}  // namespace tp::data::detail
