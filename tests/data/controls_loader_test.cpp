#include "tp/data/controls/controls_loader.hpp"

#include <gtest/gtest.h>

#include <string>
#include <string_view>

#include "../controls/reference_controls.hpp"

namespace tp::data {
namespace {

constexpr std::string_view kValidEasy = R"(
[easy]
steer_share = 0.5
steer_dead_zone = 0.05
steer_rate_per_s = 2.0

[easy.clutch_assist]
engage_time_s = 1.5
release_time_s = 0.2
guard_speed_share = 0.7
guard_band_share = 0.15
min_throttle = 0.1
)";

std::string with(std::string_view from, std::string_view to) {
    std::string text{kValidEasy};
    const auto pos = text.find(from);
    EXPECT_NE(pos, std::string::npos) << "test bug: '" << from << "' not found";
    if (pos != std::string::npos) {
        text.replace(pos, from.size(), to);
    }
    return text;
}

std::string error_of(std::string_view toml_text) {
    const auto result = parse_easy_controls(toml_text, "test.toml");
    EXPECT_FALSE(result.ok());
    return result.ok() ? std::string{} : result.error().message;
}

// The shipped file and the parameters the core tests use must agree.
TEST(ControlsLoader, ShippedFileMatchesTestReference) {
    const auto result = load_easy_controls(std::string{TP_DATA_DIR} + "/controls/easy.toml");
    ASSERT_TRUE(result.ok()) << result.error().message;
    const controls::EasyParams& loaded = result.value();
    const controls::EasyParams& reference = controls::testing::kReferenceEasy;
    EXPECT_EQ(loaded.steer_share, reference.steer_share);
    EXPECT_EQ(loaded.steer_dead_zone, reference.steer_dead_zone);
    EXPECT_EQ(loaded.steer_rate_per_s, reference.steer_rate_per_s);
    EXPECT_EQ(loaded.clutch_assist.engage_time_s, reference.clutch_assist.engage_time_s);
    EXPECT_EQ(loaded.clutch_assist.release_time_s, reference.clutch_assist.release_time_s);
    EXPECT_EQ(loaded.clutch_assist.guard_speed_share, reference.clutch_assist.guard_speed_share);
    EXPECT_EQ(loaded.clutch_assist.guard_band_share, reference.clutch_assist.guard_band_share);
    EXPECT_EQ(loaded.clutch_assist.min_throttle, reference.clutch_assist.min_throttle);
}

TEST(ControlsLoader, ParsesValues) {
    const auto result = parse_easy_controls(kValidEasy, "test.toml");
    ASSERT_TRUE(result.ok()) << result.error().message;
    EXPECT_EQ(result.value().steer_share, 0.5);
    EXPECT_EQ(result.value().steer_dead_zone, 0.05);
    EXPECT_EQ(result.value().steer_rate_per_s, 2.0);
    EXPECT_EQ(result.value().clutch_assist.engage_time_s, 1.5);
    EXPECT_EQ(result.value().clutch_assist.min_throttle, 0.1);
}

TEST(ControlsLoader, RejectsNonsenseNamingTheField) {
    EXPECT_NE(error_of(with("steer_share = 0.5", "steer_share = 0.0")).find("easy.steer_share"),
              std::string::npos);
    EXPECT_NE(error_of(with("steer_dead_zone = 0.05", "steer_dead_zone = 0.5"))
                  .find("easy.steer_dead_zone"),
              std::string::npos);
    EXPECT_NE(error_of(with("steer_rate_per_s = 2.0", "steer_rate_per_s = 0.0"))
                  .find("easy.steer_rate_per_s"),
              std::string::npos);
    EXPECT_NE(error_of(with("release_time_s = 0.2", "release_time_s = 0.0")).find("release_time_s"),
              std::string::npos);
    EXPECT_NE(error_of(with("guard_band_share = 0.15", "guard_band_share = 0.9"))
                  .find("guard_band_share"),
              std::string::npos);
    EXPECT_NE(error_of(with("min_throttle = 0.1", "min_throttle = 1.0")).find("min_throttle"),
              std::string::npos);
    EXPECT_NE(error_of(with("engage_time_s = 1.5\n", "")).find("engage_time_s"), std::string::npos);
}

}  // namespace
}  // namespace tp::data
