#include "tp/data/clutch/clutch_loader.hpp"

#include "tp/core/clutch/clutch.hpp"

#include <gtest/gtest.h>

#include <array>
#include <string>
#include <string_view>

#include "../clutch/reference_clutch.hpp"

namespace tp::data {
namespace {

constexpr std::string_view kValidClutch = R"(
[clutch]
static_torque_capacity_nm = 600.0
kinetic_ratio = 0.75

[clutch.engagement]
pedal = [0.0, 0.5, 1.0]
fraction = [1.0, 0.2, 0.0]
)";

std::string with(std::string_view from, std::string_view to) {
    std::string text{kValidClutch};
    const auto pos = text.find(from);
    EXPECT_NE(pos, std::string::npos) << "test bug: '" << from << "' not found";
    if (pos != std::string::npos) {
        text.replace(pos, from.size(), to);
    }
    return text;
}

std::string error_of(std::string_view toml_text) {
    const auto result = parse_clutch(toml_text, "test.toml");
    EXPECT_FALSE(result.ok());
    return result.ok() ? std::string{} : result.error().message;
}

// The shipped file and the parameters the core tests use must agree, so
// those tests describe the clutch the game actually ships.
TEST(ClutchLoader, ShippedFileMatchesTestReference) {
    const auto result = load_clutch(std::string{TP_DATA_DIR} + "/clutches/farm_stock_light.toml");
    ASSERT_TRUE(result.ok()) << result.error().message;
    const clutch::Params& loaded = result.value();
    const clutch::Params reference = clutch::testing::reference_params();
    EXPECT_EQ(loaded.static_torque_capacity_nm, reference.static_torque_capacity_nm);
    EXPECT_EQ(loaded.kinetic_ratio, reference.kinetic_ratio);
    ASSERT_EQ(loaded.engagement.points().size(), reference.engagement.points().size());
    for (std::size_t i = 0; i < loaded.engagement.points().size(); ++i) {
        EXPECT_EQ(loaded.engagement.points()[i].x, reference.engagement.points()[i].x);
        EXPECT_EQ(loaded.engagement.points()[i].y, reference.engagement.points()[i].y);
    }
}

TEST(ClutchLoader, ParsesValues) {
    const auto result = parse_clutch(kValidClutch, "test.toml");
    ASSERT_TRUE(result.ok()) << result.error().message;
    EXPECT_DOUBLE_EQ(result.value().static_torque_capacity_nm, 600.0);
    EXPECT_DOUBLE_EQ(result.value().kinetic_ratio, 0.75);
    EXPECT_DOUBLE_EQ(result.value().engagement.evaluate(0.5), 0.2);
}

TEST(ClutchLoader, ReportsMissingFile) {
    const auto result = load_clutch("/nonexistent/clutch.toml");
    ASSERT_FALSE(result.ok());
    EXPECT_NE(result.error().message.find("/nonexistent/clutch.toml"), std::string::npos);
}

TEST(ClutchLoader, RejectsNonsenseValues) {
    struct Case {
        std::string_view from;
        std::string_view to;
        std::string_view expected;
    };
    const std::array<Case, 8> cases = {{
        {"kinetic_ratio = 0.75", "", "clutch.kinetic_ratio: missing or not a number"},
        {"static_torque_capacity_nm = 600.0", "static_torque_capacity_nm = 0.0",
         "clutch.static_torque_capacity_nm: must be > 0"},
        {"kinetic_ratio = 0.75", "kinetic_ratio = 1.5", "clutch.kinetic_ratio: must be in (0, 1]"},
        {"pedal = [0.0, 0.5, 1.0]", "pedal = [0.0, 0.5, 0.9]",
         "clutch.engagement.pedal: must run from 0 to 1"},
        {"pedal = [0.0, 0.5, 1.0]", "pedal = [0.0, 1.0, 0.5]",
         "clutch.engagement: pedal must be strictly increasing"},
        {"fraction = [1.0, 0.2, 0.0]", "fraction = [1.0, 0.0, 0.2]",
         "clutch.engagement.fraction: must not increase with pedal travel"},
        {"fraction = [1.0, 0.2, 0.0]", "fraction = [0.9, 0.2, 0.0]",
         "clutch.engagement.fraction: must be 1 with the pedal released"},
        {"fraction = [1.0, 0.2, 0.0]", "fraction = [1.0, 0.2]",
         "clutch.engagement: pedal and fraction must have the same length"},
    }};
    for (const Case& c : cases) {
        const std::string message = error_of(with(c.from, c.to));
        EXPECT_NE(message.find(c.expected), std::string::npos)
            << "replacing '" << c.from << "': " << message;
    }
}

}  // namespace
}  // namespace tp::data
