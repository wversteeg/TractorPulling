#include "tp/data/driveline/driveline_loader.hpp"

#include "tp/core/driveline/driveline.hpp"

#include <gtest/gtest.h>

#include <array>
#include <string>
#include <string_view>

#include "../driveline/reference_driveline.hpp"

namespace tp::data {
namespace {

constexpr std::string_view kValidDriveline = R"(
[driveline]
final_drive_ratio = 20.0
efficiency = 0.92
input_inertia_kg_m2 = 0.04
output_inertia_kg_m2 = 1.5

[driveline.gearbox]
ratios = [12.0, 6.0, 3.0]

[driveline.drag]
constant_nm = 1.0
viscous_nm_per_1000_rpm = 2.0
)";

std::string with(std::string_view from, std::string_view to) {
    std::string text{kValidDriveline};
    const auto pos = text.find(from);
    EXPECT_NE(pos, std::string::npos) << "test bug: '" << from << "' not found";
    if (pos != std::string::npos) {
        text.replace(pos, from.size(), to);
    }
    return text;
}

std::string error_of(std::string_view toml_text) {
    const auto result = parse_driveline(toml_text, "test.toml");
    EXPECT_FALSE(result.ok());
    return result.ok() ? std::string{} : result.error().message;
}

// The shipped file and the parameters the core tests use must agree.
TEST(DrivelineLoader, ShippedFileMatchesTestReference) {
    const auto result =
        load_driveline(std::string{TP_DATA_DIR} + "/drivelines/farm_stock_light.toml");
    ASSERT_TRUE(result.ok()) << result.error().message;
    const driveline::Params& loaded = result.value();
    const driveline::Params reference = driveline::testing::reference_params();
    EXPECT_EQ(loaded.gear_count, reference.gear_count);
    EXPECT_EQ(loaded.gear_ratios, reference.gear_ratios);
    EXPECT_EQ(loaded.final_drive_ratio, reference.final_drive_ratio);
    EXPECT_EQ(loaded.efficiency, reference.efficiency);
    EXPECT_EQ(loaded.input_inertia_kg_m2, reference.input_inertia_kg_m2);
    EXPECT_EQ(loaded.output_inertia_kg_m2, reference.output_inertia_kg_m2);
    EXPECT_EQ(loaded.drag_torque_nm, reference.drag_torque_nm);
    EXPECT_EQ(loaded.viscous_drag_nm_s_rad, reference.viscous_drag_nm_s_rad);
}

TEST(DrivelineLoader, ParsesAndConvertsValues) {
    const auto result = parse_driveline(kValidDriveline, "test.toml");
    ASSERT_TRUE(result.ok()) << result.error().message;
    const driveline::Params& p = result.value();
    EXPECT_EQ(p.gear_count, 3U);
    EXPECT_DOUBLE_EQ(driveline::overall_ratio(p, 1), 240.0);
    EXPECT_DOUBLE_EQ(driveline::overall_ratio(p, 3), 60.0);
    EXPECT_DOUBLE_EQ(p.efficiency, 0.92);
    EXPECT_DOUBLE_EQ(p.viscous_drag_nm_s_rad, 2.0 / (1000.0 * 2.0 * 3.141592653589793 / 60.0));
}

TEST(DrivelineLoader, ReportsMissingFile) {
    const auto result = load_driveline("/nonexistent/driveline.toml");
    ASSERT_FALSE(result.ok());
    EXPECT_NE(result.error().message.find("/nonexistent/driveline.toml"), std::string::npos);
}

TEST(DrivelineLoader, RejectsNonsenseValues) {
    struct Case {
        std::string_view from;
        std::string_view to;
        std::string_view expected;
    };
    const std::array<Case, 8> cases = {{
        {"efficiency = 0.92", "", "driveline.efficiency: missing or not a number"},
        {"ratios = [12.0, 6.0, 3.0]", "ratios = []",
         "driveline.gearbox.ratios: needs at least one gear"},
        {"ratios = [12.0, 6.0, 3.0]",
         "ratios = [17.0, 16.0, 15.0, 14.0, 13.0, 12.0, 11.0, 10.0, 9.0, 8.0, 7.0, 6.0, 5.0, 4.0, "
         "3.0, 2.0, 1.0]",
         "driveline.gearbox.ratios: has more than 16 gears"},
        {"ratios = [12.0, 6.0, 3.0]", "ratios = [12.0, 6.0, 6.0]",
         "driveline.gearbox.ratios: must decrease from 1st to top gear"},
        {"ratios = [12.0, 6.0, 3.0]", "ratios = [12.0, 6.0, 0.0]",
         "driveline.gearbox.ratios: must all be > 0"},
        {"final_drive_ratio = 20.0", "final_drive_ratio = 0.0",
         "driveline.final_drive_ratio: must be > 0"},
        {"efficiency = 0.92", "efficiency = 1.2", "driveline.efficiency: must be in (0, 1]"},
        {"constant_nm = 1.0", "constant_nm = -1.0", "driveline.drag: must not be negative"},
    }};
    for (const Case& c : cases) {
        const std::string message = error_of(with(c.from, c.to));
        EXPECT_NE(message.find(c.expected), std::string::npos)
            << "replacing '" << c.from << "': " << message;
    }
}

}  // namespace
}  // namespace tp::data
