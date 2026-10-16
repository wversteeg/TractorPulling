#include "tp/data/engine/engine_loader.hpp"

#include "tp/core/engine/engine.hpp"
#include "tp/core/units/conversions.hpp"

#include <gtest/gtest.h>

#include <array>
#include <string>
#include <string_view>

namespace tp::data {
namespace {

using units::rpm_to_rad_s;

constexpr std::string_view kValidEngine = R"(
[engine]
inertia_kg_m2 = 1.5
idle_rpm = 800
rated_rpm = 2200
high_idle_rpm = 2400
stall_rpm = 450

[engine.friction]
constant_nm = 15.0
viscous_nm_per_1000_rpm = 10.0

[engine.turbo]
na_torque_fraction = 0.7
time_constant_s = 0.5
boost_onset_rpm = 1000
full_boost_rpm = 1600

[engine.torque_curve]
rpm = [700, 1400, 2500]
torque_nm = [300, 430, 290]
)";

// kValidEngine with the first occurrence of `from` replaced by `to`.
std::string with(std::string_view from, std::string_view to) {
    std::string text{kValidEngine};
    const auto pos = text.find(from);
    EXPECT_NE(pos, std::string::npos) << "test bug: '" << from << "' not found";
    if (pos != std::string::npos) {
        text.replace(pos, from.size(), to);
    }
    return text;
}

// The loader's error message, or a test failure if loading succeeded.
std::string error_of(std::string_view toml_text) {
    const auto result = parse_engine(toml_text, "test.toml");
    EXPECT_FALSE(result.ok());
    return result.ok() ? std::string{} : result.error().message;
}

TEST(EngineLoader, LoadsShippedReferenceEngine) {
    const auto result = load_engine(std::string{TP_DATA_DIR} + "/engines/farm_stock_light.toml");
    ASSERT_TRUE(result.ok()) << result.error().message;
    EXPECT_EQ(engine::validate(result.value()), std::nullopt);
    EXPECT_DOUBLE_EQ(result.value().full_load_torque_nm.max_y(), 430.0);
}

TEST(EngineLoader, ConvertsToSiUnits) {
    const auto result = parse_engine(kValidEngine, "test.toml");
    ASSERT_TRUE(result.ok()) << result.error().message;
    const engine::Params& params = result.value();
    EXPECT_DOUBLE_EQ(params.inertia_kg_m2, 1.5);
    EXPECT_DOUBLE_EQ(params.idle_omega_rad_s, rpm_to_rad_s(800.0));
    EXPECT_DOUBLE_EQ(params.rated_omega_rad_s, rpm_to_rad_s(2200.0));
    EXPECT_DOUBLE_EQ(params.high_idle_omega_rad_s, rpm_to_rad_s(2400.0));
    EXPECT_DOUBLE_EQ(params.stall_omega_rad_s, rpm_to_rad_s(450.0));
    EXPECT_DOUBLE_EQ(params.viscous_friction_nm_s_rad, 10.0 / rpm_to_rad_s(1000.0));
    EXPECT_DOUBLE_EQ(params.boost_onset_omega_rad_s, rpm_to_rad_s(1000.0));
    EXPECT_DOUBLE_EQ(params.full_load_torque_nm.min_x(), rpm_to_rad_s(700.0));
    EXPECT_DOUBLE_EQ(params.full_load_torque_nm.evaluate(rpm_to_rad_s(1400.0)), 430.0);
}

TEST(EngineLoader, ReportsMissingFile) {
    const auto result = load_engine("/nonexistent/engine.toml");
    ASSERT_FALSE(result.ok());
    EXPECT_NE(result.error().message.find("/nonexistent/engine.toml"), std::string::npos);
}

TEST(EngineLoader, ReportsSyntaxErrorWithLocation) {
    const std::string message = error_of(with("inertia_kg_m2 = 1.5", "inertia_kg_m2 = = 1.5"));
    EXPECT_NE(message.find("test.toml:3:"), std::string::npos) << message;
}

TEST(EngineLoader, ReportsMissingField) {
    const std::string message = error_of(with("stall_rpm = 450", ""));
    EXPECT_NE(message.find("engine.stall_rpm: missing"), std::string::npos) << message;
}

TEST(EngineLoader, ReportsWrongType) {
    const std::string message = error_of(with("inertia_kg_m2 = 1.5", R"(inertia_kg_m2 = "1.5")"));
    EXPECT_NE(message.find("engine.inertia_kg_m2: missing or not a number"), std::string::npos)
        << message;
}

TEST(EngineLoader, RejectsNonsenseValues) {
    struct Case {
        std::string_view from;
        std::string_view to;
        std::string_view expected;
    };
    const std::array<Case, 10> cases = {{
        {"inertia_kg_m2 = 1.5", "inertia_kg_m2 = -1.5", "engine.inertia_kg_m2: must be > 0"},
        {"idle_rpm = 800", "idle_rpm = 2300", "engine.idle_rpm: must be below rated_rpm"},
        {"stall_rpm = 450", "stall_rpm = 900", "engine.stall_rpm: must be below idle_rpm"},
        {"constant_nm = 15.0", "constant_nm = -1.0", "engine.friction: must not be negative"},
        {"na_torque_fraction = 0.7", "na_torque_fraction = 1.5", "engine.turbo.na_torque_fraction"},
        {"rpm = [700, 1400, 2500]", "rpm = [700, 700, 2500]",
         "engine.torque_curve: rpm must be strictly increasing"},
        {"rpm = [700, 1400, 2500]", "rpm = [900, 1400, 2500]",
         "engine.torque_curve.rpm: must span idle_rpm to high_idle_rpm"},
        {"torque_nm = [300, 430, 290]", "torque_nm = [300, -430, 290]",
         "engine.torque_curve.torque_nm: must not be negative"},
        {"torque_nm = [300, 430, 290]", "torque_nm = [300, 430]",
         "engine.torque_curve: rpm and torque_nm must have the same length"},
        {"torque_nm = [300, 430, 290]", R"(torque_nm = [300, "x", 290])",
         "engine.torque_curve.torque_nm: must contain only numbers"},
    }};
    for (const Case& c : cases) {
        const std::string message = error_of(with(c.from, c.to));
        EXPECT_NE(message.find(c.expected), std::string::npos)
            << "replacing '" << c.from << "': " << message;
        EXPECT_EQ(message.rfind("test.toml: ", 0), 0U) << message;
    }
}

}  // namespace
}  // namespace tp::data
