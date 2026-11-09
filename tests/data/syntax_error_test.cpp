// Every loader reports a TOML syntax error as a LoadError naming the file
// and the line. toml++ is built without exceptions (data_io/CMakeLists.txt),
// so this is a returned value on every platform, also in Godot's web export,
// which cannot catch a throw: a corrupt save file there must still be a
// message, not the end of the game.

#include "tp/data/career/career_loader.hpp"
#include "tp/data/clutch/clutch_loader.hpp"
#include "tp/data/controls/controls_loader.hpp"
#include "tp/data/driveline/driveline_loader.hpp"
#include "tp/data/engine/engine_loader.hpp"
#include "tp/data/event/event_loader.hpp"
#include "tp/data/files.hpp"
#include "tp/data/garage/garage_loader.hpp"
#include "tp/data/rules/class_rules_loader.hpp"
#include "tp/data/season/season_loader.hpp"
#include "tp/data/season/season_save.hpp"
#include "tp/data/sled/sled_loader.hpp"
#include "tp/data/tire/tire_loader.hpp"
#include "tp/data/track/track_loader.hpp"
#include "tp/data/vehicle/tractor_loader.hpp"

#include <gtest/gtest.h>

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace tp::data {
namespace {

// Valid up to line 3, where a value is missing.
constexpr std::string_view kBrokenToml = "[table]\nfine = 1\nbroken = = 2\n";
constexpr std::string_view kSource = "broken.toml";

// The message of a loader's result for kBrokenToml, or "" if it loaded.
using ErrorOf = std::function<std::string(std::string_view text, std::string_view source)>;

template <typename T>
std::string message_of(const Result<T>& result) {
    return result.ok() ? std::string{} : result.error().message;
}

std::optional<std::string> no_files(const std::string& /*path*/) {
    return std::nullopt;
}

struct Loader {
    std::string name;
    ErrorOf error_of;
};

std::vector<Loader> loaders() {
    const ReadFile read = no_files;
    return {
        {"career",
         [read](auto text, auto source) {
             return message_of(parse_career(text, source, "", read));
         }},
        {"clutch", [](auto text, auto source) { return message_of(parse_clutch(text, source)); }},
        {"easy controls",
         [](auto text, auto source) { return message_of(parse_easy_controls(text, source)); }},
        {"pro controls",
         [](auto text, auto source) { return message_of(parse_pro_controls(text, source)); }},
        {"control layout",
         [](auto text, auto source) { return message_of(parse_control_layout(text, source)); }},
        {"driveline",
         [](auto text, auto source) { return message_of(parse_driveline(text, source)); }},
        {"engine", [](auto text, auto source) { return message_of(parse_engine(text, source)); }},
        {"event", [read](auto text,
                         auto source) { return message_of(parse_event(text, source, "", read)); }},
        {"garage", [](auto text, auto source) { return message_of(parse_garage(text, source)); }},
        {"class rules",
         [](auto text, auto source) { return message_of(parse_class_rules(text, source)); }},
        {"season",
         [read](auto text, auto source) {
             return message_of(parse_season(text, source, "", read));
         }},
        {"season save",
         [](auto text, auto source) { return message_of(parse_season_save(text, source)); }},
        {"sled", [](auto text, auto source) { return message_of(parse_sled(text, source)); }},
        {"tire", [](auto text, auto source) { return message_of(parse_tire(text, source)); }},
        {"track", [](auto text, auto source) { return message_of(parse_track(text, source)); }},
        {"tractor",
         [read](auto text, auto source) {
             return message_of(parse_tractor(text, source, "", read));
         }},
    };
}

TEST(SyntaxError, EveryLoaderNamesTheFileAndLine) {
    for (const Loader& loader : loaders()) {
        const std::string message = loader.error_of(kBrokenToml, kSource);
        EXPECT_EQ(message.rfind("broken.toml:3:", 0), 0U) << loader.name << ": " << message;
    }
}

}  // namespace
}  // namespace tp::data
