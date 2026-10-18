#include "tp/data/tire/tire_loader.hpp"

#include <optional>
#include <string_view>

#include "../common/toml_reader.hpp"
#include "tire_fields.hpp"

namespace tp::data {

namespace {

constexpr std::string_view kTable = "tire";

Result<tire::Params> read_tire(const toml::table& root, std::string_view source) {
    detail::Reader reader{root, source};
    const tire::Params params{
        .size = detail::read_tire_size(reader, kTable),
        .traction =
            {
                .limit = reader.number("tire.traction.limit"),
                .mobility_rate = reader.number("tire.traction.mobility_rate"),
                .slip_rate = reader.number("tire.traction.slip_rate"),
            },
        .cornering = detail::read_cornering(reader, "tire.cornering"),
        .wheel_inertia_kg_m2 = reader.number("tire.wheel_inertia_kg_m2"),
        .min_reference_speed_m_s = reader.number("tire.min_reference_speed_m_s"),
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

Result<tire::Params> load_tire(const std::filesystem::path& path) {
    return detail::load_file_with<tire::Params>(path, &read_tire);
}

}  // namespace tp::data
