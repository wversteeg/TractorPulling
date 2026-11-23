#include "tp/data/tire/tire_loader.hpp"

#include <gtest/gtest.h>

#include <array>
#include <string>
#include <string_view>
#include <utility>

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

[tire.cornering]
stiffness_per_rad = 4.5
limit = 0.75
mobility_rate = 0.2
relaxation_length_m = 0.6

[tire.pressure]
reference_bar = 1.0
min_bar = 0.6
max_bar = 1.4
lug_squirm = 0.4
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
    EXPECT_EQ(loaded.cornering.stiffness_per_rad, reference.cornering.stiffness_per_rad);
    EXPECT_EQ(loaded.cornering.limit, reference.cornering.limit);
    EXPECT_EQ(loaded.cornering.mobility_rate, reference.cornering.mobility_rate);
    EXPECT_EQ(loaded.cornering.relaxation_length_m, reference.cornering.relaxation_length_m);
    EXPECT_EQ(loaded.pressure.reference_bar, reference.pressure.reference_bar);
    EXPECT_EQ(loaded.pressure.min_bar, reference.pressure.min_bar);
    EXPECT_EQ(loaded.pressure.max_bar, reference.pressure.max_bar);
    EXPECT_EQ(loaded.pressure.lug_squirm, reference.pressure.lug_squirm);
    EXPECT_EQ(loaded.traction.lug_bearing_pa, reference.traction.lug_bearing_pa);
    EXPECT_EQ(loaded.traction.unsunk_lug_grip, reference.traction.unsunk_lug_grip);
    EXPECT_EQ(loaded.digging.rate, reference.digging.rate);
    EXPECT_EQ(loaded.digging.escape_speed_m_s, reference.digging.escape_speed_m_s);
}

// The lugs' bearing is read in MPa, as cone index readings are given; the
// lugs and the digging are optional: without them the lugs always sink in
// and the tire does not dig.
TEST(TireLoader, ReadsTheLugsAndTheDigging) {
    const auto plain = parse_tire(kValidTire, "test.toml");
    ASSERT_TRUE(plain.ok()) << plain.error().message;
    EXPECT_EQ(plain.value().traction.lug_bearing_pa, 0.0);
    EXPECT_EQ(plain.value().traction.unsunk_lug_grip, 1.0);
    EXPECT_EQ(plain.value().digging.rate, 0.0);
    const auto set =
        parse_tire(with("slip_rate = 12.0\n",
                        "slip_rate = 12.0\nlug_bearing_mpa = 1.5\nunsunk_lug_grip = 0.6\n") +
                       "\n[tire.digging]\nrate = 0.01\nescape_speed_m_s = 0.4\n",
                   "test.toml");
    ASSERT_TRUE(set.ok()) << set.error().message;
    EXPECT_DOUBLE_EQ(set.value().traction.lug_bearing_pa, 1.5e6);
    EXPECT_DOUBLE_EQ(set.value().traction.unsunk_lug_grip, 0.6);
    EXPECT_DOUBLE_EQ(set.value().digging.rate, 0.01);
    EXPECT_DOUBLE_EQ(set.value().digging.escape_speed_m_s, 0.4);
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
    EXPECT_DOUBLE_EQ(tire.cornering.stiffness_per_rad, 4.5);
    EXPECT_DOUBLE_EQ(tire.cornering.limit, 0.75);
    EXPECT_DOUBLE_EQ(tire.cornering.mobility_rate, 0.2);
    EXPECT_DOUBLE_EQ(tire.cornering.relaxation_length_m, 0.6);
    EXPECT_DOUBLE_EQ(tire.pressure.reference_bar, 1.0);
    EXPECT_DOUBLE_EQ(tire.pressure.min_bar, 0.6);
    EXPECT_DOUBLE_EQ(tire.pressure.max_bar, 1.4);
    EXPECT_DOUBLE_EQ(tire.pressure.lug_squirm, 0.4);
}

// A tire without the table has no pressure to set.
TEST(TireLoader, ThePressureIsOptional) {
    std::string text{kValidTire};
    text.erase(text.find("[tire.pressure]"));
    const auto result = parse_tire(text, "test.toml");
    ASSERT_TRUE(result.ok()) << result.error().message;
    EXPECT_EQ(result.value().pressure.reference_bar, 0.0);
}

// The tread is only for drawing: a farm tire's unless the file names it.
TEST(TireLoader, TheTreadIsAFarmTiresUnlessNamed) {
    const auto plain = parse_tire(kValidTire, "test.toml");
    ASSERT_TRUE(plain.ok()) << plain.error().message;
    EXPECT_EQ(plain.value().tread, tire::Tread::kFarm);
    for (const auto& [name, tread] :
         {std::pair{"farm", tire::Tread::kFarm}, std::pair{"cut", tire::Tread::kCut},
          std::pair{"puller", tire::Tread::kPuller}}) {
        const auto named = parse_tire(
            with("[tire]\n", std::string{"[tire]\ntread = \""} + name + "\"\n"), "test.toml");
        ASSERT_TRUE(named.ok()) << named.error().message;
        EXPECT_EQ(named.value().tread, tread) << name;
    }
}

// Each shipped rear tire is drawn as what it is: the standard class's
// farm tire, the Sport classes' cut farm tires, the Supersport's modified
// farm tire and its puller tire.
TEST(TireLoader, ShippedTiresNameTheirTread) {
    for (const auto& [file, tread] :
         {std::pair{"pulling_rear.toml", tire::Tread::kFarm},
          std::pair{"sport_rear.toml", tire::Tread::kCut},
          std::pair{"sport_4500_rear.toml", tire::Tread::kCut},
          std::pair{"supersport_farm_rear.toml", tire::Tread::kFarm},
          std::pair{"supersport_puller_rear.toml", tire::Tread::kPuller}}) {
        const auto result = load_tire(std::string{TP_DATA_DIR} + "/tires/" + file);
        ASSERT_TRUE(result.ok()) << result.error().message;
        EXPECT_EQ(result.value().tread, tread) << file;
    }
}

TEST(TireLoader, RejectsNonsenseValues) {
    struct Case {
        std::string_view from;
        std::string_view to;
        std::string_view expected;
    };
    const std::array<Case, 18> cases = {{
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
        {"limit = 0.75", "limit = 0.0", "tire.cornering.limit: must be in (0, 2]"},
        {"relaxation_length_m = 0.6", "relaxation_length_m = 0.0",
         "tire.cornering: stiffness_per_rad, mobility_rate and relaxation_length_m must be > 0"},
        {"stiffness_per_rad = 4.5", "",
         "tire.cornering.stiffness_per_rad: missing or not a number"},
        {"min_bar = 0.6", "min_bar = 1.2", "tire.pressure: need 0 < min_bar <= reference_bar"},
        // So soft the tire would deflect past its section height.
        {"min_bar = 0.6", "min_bar = 0.1", "tire.pressure: need 0 < min_bar <= reference_bar"},
        {"lug_squirm = 0.4", "", "tire.pressure.lug_squirm: missing or not a number"},
        {"[tire]\n", "[tire]\ntread = \"slick\"\n",
         R"(tire.tread: must be "farm", "cut" or "puller")"},
        {"slip_rate = 12.0\n", "slip_rate = 12.0\nunsunk_lug_grip = 0.0\n",
         "tire.traction: lug_bearing_mpa must be >= 0 and unsunk_lug_grip in (0, 1]"},
        {"[tire.cornering]", "[tire.digging]\nrate = -1.0\n\n[tire.cornering]",
         "tire.digging: rate must be >= 0 and escape_speed_m_s > 0"},
    }};
    for (const Case& c : cases) {
        const std::string message = error_of(with(c.from, c.to));
        EXPECT_NE(message.find(c.expected), std::string::npos)
            << "replacing '" << c.from << "': " << message;
    }
}

}  // namespace
}  // namespace tp::data
