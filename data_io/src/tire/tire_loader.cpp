#include "tp/data/tire/tire_loader.hpp"

#include <optional>
#include <string>
#include <string_view>

#include "../common/toml_reader.hpp"
#include "tire_fields.hpp"

namespace tp::data {

namespace {

constexpr std::string_view kTable = "tire";
// Data files give the lug bearing in MPa, as cone index readings are.
constexpr double kPaPerMpa = 1.0e6;

// The optional [tire.pressure] table: without it the pressure is not set.
tire::Pressure read_pressure(detail::Reader& reader) {
    if (!reader.has("tire.pressure")) {
        return {};
    }
    return {.reference_bar = reader.number("tire.pressure.reference_bar"),
            .min_bar = reader.number("tire.pressure.min_bar"),
            .max_bar = reader.number("tire.pressure.max_bar"),
            .lug_squirm = reader.number("tire.pressure.lug_squirm")};
}

// The optional tread, by name: a farm tire's unless the file says
// otherwise. Only for drawing the tire.
tire::Tread read_tread(detail::Reader& reader) {
    if (!reader.has("tire.tread")) {
        return tire::Tread::kFarm;
    }
    const std::string tread = reader.text("tire.tread");
    if (tread == "cut") {
        return tire::Tread::kCut;
    }
    if (tread == "puller") {
        return tire::Tread::kPuller;
    }
    if (tread != "farm") {
        reader.fail("tire.tread", R"(must be "farm", "cut" or "puller")");
    }
    return tire::Tread::kFarm;
}

Result<tire::Params> read_tire(const toml::table& root, std::string_view source) {
    detail::Reader reader{root, source};
    const tire::Params params{
        .size = detail::read_tire_size(reader, kTable),
        .traction =
            {
                .limit = reader.number("tire.traction.limit"),
                .mobility_rate = reader.number("tire.traction.mobility_rate"),
                .slip_rate = reader.number("tire.traction.slip_rate"),
                // Optional: a tire that grips as much per unit of load
                // however hard it is pressed.
                .load_sensitivity =
                    reader.optional_number("tire.traction.load_sensitivity").value_or(0.0),
                .reference_load_n =
                    reader.optional_number("tire.traction.reference_load_n").value_or(0.0),
                // Optional: without them grip does not depend on how fast
                // the tread spins.
                .slow_spin_grip =
                    reader.optional_number("tire.traction.slow_spin_grip").value_or(1.0),
                .fast_spin_grip =
                    reader.optional_number("tire.traction.fast_spin_grip").value_or(1.0),
                .spin_speed_m_s =
                    reader.optional_number("tire.traction.spin_speed_m_s").value_or(1.0),
                .fall_speed_m_s =
                    reader.optional_number("tire.traction.fall_speed_m_s").value_or(0.0),
                // Optional: without them the tire's lugs always sink in.
                .lug_bearing_pa =
                    reader.optional_number("tire.traction.lug_bearing_mpa").value_or(0.0) *
                    kPaPerMpa,
                .unsunk_lug_grip =
                    reader.optional_number("tire.traction.unsunk_lug_grip").value_or(1.0),
            },
        .cornering = detail::read_cornering(reader, "tire.cornering"),
        .wheel_inertia_kg_m2 = reader.number("tire.wheel_inertia_kg_m2"),
        .min_reference_speed_m_s = reader.number("tire.min_reference_speed_m_s"),
        .pressure = read_pressure(reader),
        // Optional: without it the tire does not dig itself in.
        .digging =
            {
                .rate = reader.optional_number("tire.digging.rate").value_or(0.0),
                .escape_speed_m_s =
                    reader.optional_number("tire.digging.escape_speed_m_s").value_or(1.0),
            },
        .tread = read_tread(reader),
    };
    if (const std::optional<LoadError>& error = reader.error()) {
        return *error;
    }
    if (const auto error = tire::validate(params)) {
        const detail::TireFieldError field_error = detail::describe(*error);
        return detail::make_error(source, detail::field_path(kTable, field_error.field),
                                  field_error.reason);
    }
    return params;
}

}  // namespace

Result<tire::Params> parse_tire(std::string_view toml_text, std::string_view source_name) {
    return detail::parse_with<tire::Params>(toml_text, source_name, &read_tire);
}

Result<tire::Params> load_tire(std::string_view path, const ReadFile& read) {
    return detail::load_file_with<tire::Params>(path, read, &read_tire);
}

Result<tire::Params> load_tire(const std::filesystem::path& path) {
    return load_tire(path.generic_string(), read_from_disk);
}

}  // namespace tp::data
