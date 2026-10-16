// Proves toml++ builds with our flags. Real data loading arrives with the
// Milestone 1 components.

#include <gtest/gtest.h>
#include <toml++/toml.hpp>

#include <string_view>

namespace {

TEST(TomlSmoke, ParsesEngineTable) {
    constexpr std::string_view kDoc = R"(
[engine]
idle_rpm = 800
inertia_kg_m2 = 1.25
)";
    const toml::table table = toml::parse(kDoc);
    EXPECT_EQ(table["engine"]["idle_rpm"].value<std::int64_t>(), 800);
    EXPECT_EQ(table["engine"]["inertia_kg_m2"].value<double>(), 1.25);
}

}  // namespace
