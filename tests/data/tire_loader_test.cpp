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
nominal_load_n = 8000.0
load_sensitivity = 0.05
rolling_resistance_coefficient = 0.04
wheel_inertia_kg_m2 = 20.0
min_reference_speed_m_s = 0.3

[tire.traction]
slip = [0.0, 0.2, 1.0]
mu = [0.0, 0.8, 0.6]
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
    EXPECT_EQ(loaded.radius_m, reference.radius_m);
    EXPECT_EQ(loaded.nominal_load_n, reference.nominal_load_n);
    EXPECT_EQ(loaded.load_sensitivity, reference.load_sensitivity);
    EXPECT_EQ(loaded.rolling_resistance_coefficient, reference.rolling_resistance_coefficient);
    EXPECT_EQ(loaded.wheel_inertia_kg_m2, reference.wheel_inertia_kg_m2);
    EXPECT_EQ(loaded.min_reference_speed_m_s, reference.min_reference_speed_m_s);
    const auto loaded_points = loaded.traction_coefficient.points();
    const auto reference_points = reference.traction_coefficient.points();
    ASSERT_EQ(loaded_points.size(), reference_points.size());
    for (std::size_t i = 0; i < loaded_points.size(); ++i) {
        EXPECT_EQ(loaded_points[i].x, reference_points[i].x);
        EXPECT_EQ(loaded_points[i].y, reference_points[i].y);
    }
}

TEST(TireLoader, ParsesValues) {
    const auto result = parse_tire(kValidTire, "test.toml");
    ASSERT_TRUE(result.ok()) << result.error().message;
    EXPECT_DOUBLE_EQ(result.value().radius_m, 0.7);
    EXPECT_DOUBLE_EQ(result.value().traction_coefficient.evaluate(0.2), 0.8);
}

TEST(TireLoader, RejectsNonsenseValues) {
    struct Case {
        std::string_view from;
        std::string_view to;
        std::string_view expected;
    };
    const std::array<Case, 6> cases = {{
        {"radius_m = 0.7", "", "tire.radius_m: missing or not a number"},
        {"radius_m = 0.7", "radius_m = 0.0", "tire.radius_m: must be > 0"},
        {"slip = [0.0, 0.2, 1.0]", "slip = [0.0, 0.2, 0.9]",
         "tire.traction.slip: must run from 0 to 1"},
        {"mu = [0.0, 0.8, 0.6]", "mu = [0.1, 0.8, 0.6]",
         "tire.traction.mu: must be 0 at zero slip"},
        {"load_sensitivity = 0.05", "load_sensitivity = 1.0",
         "tire.load_sensitivity: must be in [0, 1)"},
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
