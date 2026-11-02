#include "tp/data/rules/class_rules_loader.hpp"

#include "tp/core/units/conversions.hpp"
#include "tp/data/vehicle/tractor_loader.hpp"

#include <gtest/gtest.h>

#include <string>
#include <string_view>

#include "../rules/reference_class.hpp"

namespace tp::data {
namespace {

constexpr std::string_view kValidClass = R"(
[class]
max_mass_kg = 4500.0
max_pto_power_hp = 110.0
max_engine_speed_rpm = 2700.0
max_hitch_height_m = 0.5
)";

std::string with(std::string_view from, std::string_view to) {
    std::string text{kValidClass};
    const auto pos = text.find(from);
    EXPECT_NE(pos, std::string::npos) << "test bug: '" << from << "' not found";
    if (pos != std::string::npos) {
        text.replace(pos, from.size(), to);
    }
    return text;
}

std::string error_of(std::string_view toml_text) {
    const auto result = parse_class_rules(toml_text, "class.toml");
    EXPECT_FALSE(result.ok());
    return result.ok() ? std::string{} : result.error().message;
}

TEST(ClassRulesLoader, ShippedClassMatchesTestReference) {
    const auto result =
        load_class_rules(std::string{TP_DATA_DIR} + "/classes/ntto_standard_3500.toml");
    ASSERT_TRUE(result.ok()) << result.error().message;
    const rules::ClassRules reference = rules::testing::ntto_standard_3500();
    EXPECT_EQ(result.value().max_mass_kg, reference.max_mass_kg);
    EXPECT_EQ(result.value().max_pto_power_w, reference.max_pto_power_w);
    EXPECT_EQ(result.value().max_engine_speed_rad_s, reference.max_engine_speed_rad_s);
    EXPECT_EQ(result.value().max_hitch_height_m, reference.max_hitch_height_m);
}

// The shipped tractor is built for the shipped class.
TEST(ClassRulesLoader, ShippedTractorCompetesInShippedClass) {
    const auto rules =
        load_class_rules(std::string{TP_DATA_DIR} + "/classes/ntto_standard_3500.toml");
    const auto tractor = load_tractor(std::string{TP_DATA_DIR} + "/tractors/farm_stock_light.toml");
    ASSERT_TRUE(rules.ok()) << rules.error().message;
    ASSERT_TRUE(tractor.ok()) << tractor.error().message;
    EXPECT_TRUE(rules::check(rules.value(), tractor.value()).ok());
}

TEST(ClassRulesLoader, ConvertsToSiUnits) {
    const auto result = parse_class_rules(kValidClass, "class.toml");
    ASSERT_TRUE(result.ok()) << result.error().message;
    EXPECT_DOUBLE_EQ(result.value().max_pto_power_w, 110.0 * 735.49875);
    EXPECT_DOUBLE_EQ(result.value().max_engine_speed_rad_s, units::rpm_to_rad_s(2700.0));
    EXPECT_DOUBLE_EQ(result.value().max_mass_kg, 4500.0);
}

TEST(ClassRulesLoader, RejectsNonsense) {
    EXPECT_NE(error_of(with("max_mass_kg = 4500.0", "max_mass_kg = 0.0"))
                  .find("class.toml: class: all limits must be > 0"),
              std::string::npos);
    EXPECT_NE(error_of(with("max_pto_power_hp = 110.0", ""))
                  .find("class.max_pto_power_hp: missing or not a number"),
              std::string::npos);
    EXPECT_NE(error_of(with("max_hitch_height_m = 0.5", "max_hitch_height_m = nan"))
                  .find("class: all limits must be finite"),
              std::string::npos);
}

TEST(ClassRulesLoader, ReportsMissingFile) {
    const auto result = load_class_rules("/nonexistent/class.toml");
    ASSERT_FALSE(result.ok());
    EXPECT_NE(result.error().message.find("/nonexistent/class.toml"), std::string::npos);
}

}  // namespace
}  // namespace tp::data
