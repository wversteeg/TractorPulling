#include "tp/data/rules/class_rules_loader.hpp"

#include "tp/core/units/conversions.hpp"

#include <optional>
#include <string_view>

#include "../common/toml_reader.hpp"

namespace tp::data {

namespace {

Result<rules::ClassRules> read_class_rules(const toml::table& root, std::string_view source) {
    detail::Reader reader{root, source};
    const rules::ClassRules class_rules{
        .max_mass_kg = reader.number("class.max_mass_kg"),
        .max_pto_power_w = units::metric_hp_to_w(reader.number("class.max_pto_power_hp")),
        .max_engine_speed_rad_s = units::rpm_to_rad_s(reader.number("class.max_engine_speed_rpm")),
        .max_hitch_height_m = reader.number("class.max_hitch_height_m"),
    };
    if (const std::optional<LoadError>& error = reader.error()) {
        return *error;
    }
    if (const auto error = rules::validate(class_rules)) {
        return detail::make_error(source, "class",
                                  error == rules::ParamError::kNotFinite
                                      ? "all limits must be finite"
                                      : "all limits must be > 0");
    }
    return class_rules;
}

}  // namespace

Result<rules::ClassRules> parse_class_rules(std::string_view toml_text,
                                            std::string_view source_name) {
    return detail::parse_with<rules::ClassRules>(toml_text, source_name, &read_class_rules);
}

Result<rules::ClassRules> load_class_rules(std::string_view path, const ReadFile& read) {
    return detail::load_file_with<rules::ClassRules>(path, read, &read_class_rules);
}

Result<rules::ClassRules> load_class_rules(const std::filesystem::path& path) {
    return load_class_rules(path.generic_string(), read_from_disk);
}

}  // namespace tp::data
