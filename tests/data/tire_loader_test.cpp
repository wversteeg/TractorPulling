#include "tp/data/tire/tire_loader.hpp"

#include <gtest/gtest.h>

#include <array>
#include <string>
#include <string_view>

#include "../tire/reference_tire.hpp"

namespace tp::data {
namespace {

constexpr std::string_view kValidTire = R"(
[tire]
radius_m = 0.7
section_width_m = 0.42
section_height_m = 0.35
deflection_m = 0.04
wheel_inertia_kg_m2 = 20.0
min_reference_speed_m_s = 0.3

[tire.traction]
limit = 0.9
mobility_rate = 0.1
slip_rate = 12.0
)";

std::string with(std::string_view from, std::string_view to) {
    std::string text{kValidTire};
    const auto pos = text.find(from);
    EXPECT_NE(pos, std::string::npos) << "test bug: '" << from << "' not found";
    if (pos != std::string::npos) {
        text.replace(pos, from.size(), to);
    }
    return text;
}

std::string error_of(std::string_view toml_text) {
    const auto result = parse_tire(toml_text, "test.toml");
    EXPECT_FALSE(result.ok());
    return result.ok() ? std::string{} : result.error().message;
}

// The shipped file and the parameters the core tests use must agree.
TEST(TireLoader, ShippedFileMatchesTestReference) {
    const auto result = load_tire(std::string{TP_DATA_DIR} + "/tires/pulling_rear.toml");
    ASSERT_TRUE(result.ok()) << result.error().message;
    const tire::Params& loaded = result.value();
    const tire::Params reference = tire::testing::reference_params();
    EXPECT_EQ(loaded.size.radius_m, reference.size.radius_m);
    EXPECT_EQ(loaded.size.section_width_m, reference.size.section_width_m);
    EXPECT_EQ(loaded.size.section_height_m, reference.size.section_height_m);
    EXPECT_EQ(loaded.size.deflection_m, reference.size.deflection_m);
    EXPECT_EQ(loaded.traction.limit, reference.traction.limit);
    EXPECT_EQ(loaded.traction.mobility_rate, reference.traction.mobility_rate);
    EXPECT_EQ(loaded.traction.slip_rate, reference.traction.slip_rate);
    EXPECT_EQ(loaded.wheel_inertia_kg_m2, reference.wheel_inertia_kg_m2);
    EXPECT_EQ(loaded.min_reference_speed_m_s, reference.min_reference_speed_m_s);
}

TEST(TireLoader, ParsesValues) {
    const auto result = parse_tire(kValidTire, "test.toml");
    ASSERT_TRUE(result.ok()) << result.error().message;
    const tire::Params& tire = result.value();
    EXPECT_DOUBLE_EQ(tire.size.radius_m, 0.7);
    EXPECT_DOUBLE_EQ(tire.size.section_width_m, 0.42);
    EXPECT_DOUBLE_EQ(tire.size.section_height_m, 0.35);
    EXPECT_DOUBLE_EQ(tire.size.deflection_m, 0.04);
    EXPECT_DOUBLE_EQ(tire.traction.limit, 0.9);
    EXPECT_DOUBLE_EQ(tire.traction.mobility_rate, 0.1);
    EXPECT_DOUBLE_EQ(tire.traction.slip_rate, 12.0);
    EXPECT_DOUBLE_EQ(tire.wheel_inertia_kg_m2, 20.0);
    EXPECT_DOUBLE_EQ(tire.min_reference_speed_m_s, 0.3);
}

TEST(TireLoader, RejectsNonsenseValues) {
    struct Case {
        std::string_view from;
        std::string_view to;
        std::string_view expected;
    };
    const std::array<Case, 9> cases = {{
        {"radius_m = 0.7", "", "tire.radius_m: missing or not a number"},
        {"radius_m = 0.7", "radius_m = 0.0", "tire.radius_m: must be > 0"},
        {"section_width_m = 0.42", "section_width_m = 0.0", "tire.section_width_m: section width"},
        {"deflection_m = 0.04", "deflection_m = 0.5",
         "tire.deflection_m: must be at least 0 and less than the section height"},
        {"limit = 0.9", "limit = 3.0", "tire.traction.limit: must be in (0, 2]"},
        {"slip_rate = 12.0", "slip_rate = 0.0", "tire.traction: mobility_rate and slip_rate"},
        {"mobility_rate = 0.1", "", "tire.traction.mobility_rate: missing or not a number"},
        {"wheel_inertia_kg_m2 = 20.0", "wheel_inertia_kg_m2 = -1.0",
         "tire.wheel_inertia_kg_m2: must be > 0"},
        {"min_reference_speed_m_s = 0.3", "min_reference_speed_m_s = 0.0",
         "tire.min_reference_speed_m_s: must be > 0"},
    }};
    for (const Case& c : cases) {
        const std::string message = error_of(with(c.from, c.to));
        EXPECT_NE(message.find(c.expected), std::string::npos)
            << "replacing '" << c.from << "': " << message;
    }
}

}  // namespace
}  // namespace tp::data
