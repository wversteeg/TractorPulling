#include "tp/data/vehicle/tractor_loader.hpp"

#include "tp/core/hash/fnv1a.hpp"
#include "tp/core/vehicle/vehicle.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

#include "../vehicle/reference_vehicle.hpp"

namespace tp::data {
namespace {

const std::filesystem::path kDataDir{TP_DATA_DIR};

constexpr std::string_view kTractor = R"(
[tractor]
engine = "engines/farm_stock_light.toml"
clutch = "clutches/farm_stock_light.toml"
driveline = "drivelines/farm_stock_light.toml"
rear_tire = "tires/pulling_rear.toml"
rear_tire_count = 2
pto_efficiency = 0.9

[tractor.front_tire]
radius_m = 0.5
section_width_m = 0.28
section_height_m = 0.25
deflection_m = 0.025

[tractor.steering]
max_angle_deg = 40.0

[tractor.steering.cornering]
stiffness_per_rad = 5.0
limit = 0.7
mobility_rate = 0.1
relaxation_length_m = 0.5

[tractor.chassis]
mass_kg = 3500.0
wheelbase_m = 2.4
cg_ahead_of_rear_axle_m = 0.96
cg_height_m = 1.0
pitch_inertia_kg_m2 = 2800.0
yaw_inertia_kg_m2 = 5500.0
hitch_height_m = 0.5
hitch_behind_rear_axle_m = 0.4

[tractor.chassis.front_tires]
stiffness_n_per_m = 400000.0
damping_n_s_per_m = 6000.0

[tractor.chassis.wheelie_bar]
behind_rear_axle_m = 1.3
tip_height_m = 0.15
stiffness_n_per_m = 2.0e6
damping_n_s_per_m = 50000.0
rolling_resistance_coefficient = 0.05
)";

std::string with(std::string_view from, std::string_view to) {
    std::string text{kTractor};
    const auto pos = text.find(from);
    EXPECT_NE(pos, std::string::npos) << "test bug: '" << from << "' not found";
    if (pos != std::string::npos) {
        text.replace(pos, from.size(), to);
    }
    return text;
}

std::string error_of(std::string_view toml_text) {
    const auto result = parse_tractor(toml_text, "tractor.toml", kDataDir);
    EXPECT_FALSE(result.ok());
    return result.ok() ? std::string{} : result.error().message;
}

// Hash of a few seconds of pulling, to compare two tractors' behaviour.
std::uint64_t pull_signature(const vehicle::Params& params) {
    const auto model = vehicle::Model::create(params);
    EXPECT_TRUE(model.has_value());
    if (!model) {
        return 0;
    }
    vehicle::State state = vehicle::initial_state(*model);
    hash::Fnv1a hash;
    for (int i = 0; i < 6000; ++i) {
        const double t_s = i * 0.001;
        const vehicle::Input input{
            .throttle = 1.0, .clutch_pedal = t_s < 2.0 ? 1.0 : 0.0, .gear = 3};
        vehicle::step(state, *model, input, vehicle::testing::reference_ground(),
                      {.pull_n = 6000.0}, 0.001);
        vehicle::hash_append(hash, state);
    }
    return hash.value();
}

// The shipped tractor composes the shipped components and behaves exactly
// like the reference the core tests use.
TEST(TractorLoader, ShippedTractorMatchesTestReference) {
    const auto result = load_tractor(kDataDir / "tractors/farm_stock_light.toml");
    ASSERT_TRUE(result.ok()) << result.error().message;
    EXPECT_EQ(vehicle::validate(result.value()), std::nullopt);
    EXPECT_EQ(result.value().rear_tire_count, 2);
    EXPECT_EQ(result.value().pto_efficiency, vehicle::testing::reference_params().pto_efficiency);
    EXPECT_EQ(pull_signature(result.value()), pull_signature(vehicle::testing::reference_params()));
}

TEST(TractorLoader, ResolvesComponentsRelativeToTheTractor) {
    const auto result = parse_tractor(kTractor, "tractor.toml", kDataDir);
    ASSERT_TRUE(result.ok()) << result.error().message;
    EXPECT_DOUBLE_EQ(result.value().rear_tire.size.radius_m, 0.8);
}

// The rear axle rides at the rear tire's radius; the file does not repeat it.
TEST(TractorLoader, TakesTheRearAxleHeightFromTheTire) {
    const auto result = parse_tractor(kTractor, "tractor.toml", kDataDir);
    ASSERT_TRUE(result.ok()) << result.error().message;
    EXPECT_EQ(result.value().chassis.rear_axle_height_m, result.value().rear_tire.size.radius_m);
    EXPECT_EQ(vehicle::validate(result.value()), std::nullopt);
}

TEST(TractorLoader, ReportsAMissingComponentFile) {
    const std::string message =
        error_of(with("tires/pulling_rear.toml", "tires/does_not_exist.toml"));
    EXPECT_NE(message.find("does_not_exist.toml: cannot open file"), std::string::npos) << message;
}

// A broken component is reported against its own file and field.
TEST(TractorLoader, ReportsAnInvalidComponentInItsOwnFile) {
    const std::filesystem::path dir =
        std::filesystem::path{::testing::TempDir()} / "tractor_loader_test";
    std::filesystem::create_directories(dir);
    {
        const std::ifstream in{kDataDir / "tires/pulling_rear.toml"};
        std::ostringstream buffer;
        buffer << in.rdbuf();
        std::string text = buffer.str();
        text.replace(text.find("radius_m = 0.8"), 14, "radius_m = -1.0");
        std::ofstream{dir / "bad_tire.toml"} << text;
    }
    const std::string tire_path = (dir / "bad_tire.toml").string();
    const std::string message = error_of(with("tires/pulling_rear.toml", tire_path));
    EXPECT_NE(message.find("bad_tire.toml: tire.radius_m: must be > 0"), std::string::npos)
        << message;
}

TEST(TractorLoader, ReadsTheFrontTire) {
    const auto result = parse_tractor(kTractor, "tractor.toml", kDataDir);
    ASSERT_TRUE(result.ok()) << result.error().message;
    EXPECT_DOUBLE_EQ(result.value().front_tire.radius_m, 0.5);
    EXPECT_DOUBLE_EQ(result.value().front_tire.section_width_m, 0.28);
    EXPECT_DOUBLE_EQ(result.value().front_tire.section_height_m, 0.25);
    EXPECT_DOUBLE_EQ(result.value().front_tire.deflection_m, 0.025);
}

TEST(TractorLoader, ReadsTheSteering) {
    const auto result = parse_tractor(kTractor, "test.toml", kDataDir);
    ASSERT_TRUE(result.ok()) << result.error().message;
    const vehicle::Steering& steering = result.value().steering;
    const vehicle::Steering reference = vehicle::testing::reference_params().steering;
    EXPECT_DOUBLE_EQ(steering.max_angle_rad, reference.max_angle_rad);
    EXPECT_EQ(steering.front_tires.stiffness_per_rad, reference.front_tires.stiffness_per_rad);
    EXPECT_EQ(steering.front_tires.limit, reference.front_tires.limit);
    EXPECT_EQ(steering.front_tires.mobility_rate, reference.front_tires.mobility_rate);
    EXPECT_EQ(steering.front_tires.relaxation_length_m, reference.front_tires.relaxation_length_m);
    EXPECT_EQ(result.value().chassis.yaw_inertia_kg_m2,
              vehicle::testing::reference_params().chassis.yaw_inertia_kg_m2);
}

TEST(TractorLoader, RejectsNonsenseSteering) {
    EXPECT_NE(error_of(with("max_angle_deg = 40.0", "max_angle_deg = 50.0"))
                  .find("tractor.steering.max_angle_deg: must be in (0, 45]"),
              std::string::npos);
    EXPECT_NE(error_of(with("max_angle_deg = 40.0", "max_angle_deg = 0.0"))
                  .find("tractor.steering.max_angle_deg: must be in (0, 45]"),
              std::string::npos);
    EXPECT_NE(error_of(with("limit = 0.7", "limit = -0.7"))
                  .find("tractor.steering.cornering.limit: must be in (0, 2]"),
              std::string::npos);
    EXPECT_NE(error_of(with("relaxation_length_m = 0.5", ""))
                  .find("tractor.steering.cornering.relaxation_length_m: missing"),
              std::string::npos);
    EXPECT_NE(error_of(with("yaw_inertia_kg_m2 = 5500.0", "yaw_inertia_kg_m2 = 0.0"))
                  .find("tractor.chassis.yaw_inertia_kg_m2: must be > 0"),
              std::string::npos);
}

TEST(TractorLoader, RejectsANonsenseFrontTire) {
    EXPECT_NE(error_of(with("deflection_m = 0.025", "deflection_m = 0.3"))
                  .find("tractor.toml: tractor.front_tire.deflection_m: must be at least 0 and "
                        "less than the section height"),
              std::string::npos);
    EXPECT_NE(error_of(with("section_width_m = 0.28", ""))
                  .find("tractor.front_tire.section_width_m: missing or not a number"),
              std::string::npos);
}

TEST(TractorLoader, RejectsNonsenseChassisValues) {
    EXPECT_NE(error_of(with("mass_kg = 3500.0", "mass_kg = 0.0"))
                  .find("tractor.toml: tractor.chassis.mass_kg: must be > 0"),
              std::string::npos);
    EXPECT_NE(error_of(with("cg_ahead_of_rear_axle_m = 0.96", "cg_ahead_of_rear_axle_m = 3.0"))
                  .find("tractor.chassis.cg_ahead_of_rear_axle_m: must lie between the axles"),
              std::string::npos);
    EXPECT_NE(error_of(with("pitch_inertia_kg_m2 = 2800.0", "pitch_inertia_kg_m2 = -1.0"))
                  .find("tractor.chassis.pitch_inertia_kg_m2: must be > 0"),
              std::string::npos);
    EXPECT_NE(error_of(with("damping_n_s_per_m = 6000.0", "damping_n_s_per_m = -1.0"))
                  .find("tractor.chassis.front_tires.damping_n_s_per_m: must not be negative"),
              std::string::npos);
    EXPECT_NE(error_of(with("tip_height_m = 0.15", "tip_height_m = 0.9"))
                  .find("tractor.chassis.wheelie_bar: the tip must touch down before 30°"),
              std::string::npos);
    EXPECT_NE(error_of(with("stiffness_n_per_m = 2.0e6", "stiffness_n_per_m = 0.0"))
                  .find("tractor.chassis.wheelie_bar.stiffness_n_per_m: must be > 0"),
              std::string::npos);
    EXPECT_NE(error_of(with("tip_height_m = 0.15", ""))
                  .find("tractor.chassis.wheelie_bar.tip_height_m: missing or not a number"),
              std::string::npos);
    EXPECT_NE(error_of(with("pto_efficiency = 0.9", "pto_efficiency = 1.5"))
                  .find("tractor.pto_efficiency: must be in (0, 1]"),
              std::string::npos);
    EXPECT_NE(error_of(with("rear_tire_count = 2", "rear_tire_count = 0"))
                  .find("tractor.rear_tire_count: must be 1 to 8"),
              std::string::npos);
    EXPECT_NE(error_of(with("rear_tire_count = 2", "rear_tire_count = 2.5"))
                  .find("tractor.rear_tire_count: missing or not an integer"),
              std::string::npos);
    EXPECT_NE(error_of(with(R"(engine = "engines/farm_stock_light.toml")", "engine = 3"))
                  .find("tractor.engine: missing or not a string"),
              std::string::npos);
}

}  // namespace
}  // namespace tp::data
