#include "tp/data/files.hpp"

#include "tp/data/event/event_loader.hpp"
#include "tp/data/season/season_loader.hpp"
#include "tp/data/track/track_loader.hpp"
#include "tp/data/vehicle/tractor_loader.hpp"

#include <gtest/gtest.h>

#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>

namespace tp::data {
namespace {

TEST(DirectoryOf, IsEverythingBeforeTheLastSlash) {
    EXPECT_EQ(directory_of("data/tractors/a.toml"), "data/tractors");
    EXPECT_EQ(directory_of("res://data/a.toml"), "res://data");
    EXPECT_EQ(directory_of("res://a.toml"), "res://");
    EXPECT_EQ(directory_of("/a.toml"), "/");
    EXPECT_EQ(directory_of("a.toml"), "");
}

TEST(Resolve, FoldsDotsByName) {
    EXPECT_EQ(resolve("data/tractors", "../engines/a.toml"), "data/engines/a.toml");
    EXPECT_EQ(resolve("res://data/tractors", "../engines/a.toml"), "res://data/engines/a.toml");
    EXPECT_EQ(resolve("/srv/data/tractors", "./../engines//a.toml"), "/srv/data/engines/a.toml");
    EXPECT_EQ(resolve("data", "a.toml"), "data/a.toml");
    EXPECT_EQ(resolve("", "a.toml"), "a.toml");
}

TEST(Resolve, MayLeaveARelativeDirectoryButNotARoot) {
    EXPECT_EQ(resolve("data", "../../a.toml"), "../a.toml");
    EXPECT_EQ(resolve("", "../a.toml"), "../a.toml");
    EXPECT_EQ(resolve("/data", "../../a.toml"), "/a.toml");
    EXPECT_EQ(resolve("res://data", "../../a.toml"), "res://a.toml");
}

TEST(Resolve, KeepsAnAbsolutePath) {
    EXPECT_EQ(resolve("data/tractors", "/srv/a.toml"), "/srv/a.toml");
    EXPECT_EQ(resolve("data/tractors", "user://a.toml"), "user://a.toml");
}

// A front end's files, as Godot keeps them under res://: here they are the
// shipped data/ files, and every path asked for is noted.
class ResFiles {
public:
    static constexpr std::string_view kRoot = "res://data/";

    [[nodiscard]] ReadFile reader() {
        return [this](const std::string& path) -> std::optional<std::string> {
            requested_.insert(path);
            if (!path.starts_with(kRoot)) {
                return std::nullopt;
            }
            if (const auto file = overrides_.find(path); file != overrides_.end()) {
                return file->second;
            }
            return read_from_disk(std::string{TP_DATA_DIR} + "/" + path.substr(kRoot.size()));
        };
    }

    void set(const std::string& path, const std::string& text) { overrides_[path] = text; }
    [[nodiscard]] const std::set<std::string>& requested() const { return requested_; }

private:
    std::map<std::string, std::string> overrides_;
    std::set<std::string> requested_;
};

TEST(LoadThroughAReader, ReadsATractorAndTheFilesItNames) {
    ResFiles files;
    const auto through_reader =
        load_tractor("res://data/tractors/farm_stock_light.toml", files.reader());
    ASSERT_TRUE(through_reader) << through_reader.error().message;
    const std::set<std::string> expected = {
        "res://data/tractors/farm_stock_light.toml", "res://data/engines/farm_stock_light.toml",
        "res://data/clutches/farm_stock_light.toml", "res://data/drivelines/farm_stock_light.toml",
        "res://data/tires/pulling_rear.toml",
    };
    EXPECT_EQ(files.requested(), expected);

    // The same tractor as from the disk.
    const auto from_disk =
        load_tractor(std::filesystem::path{TP_DATA_DIR} / "tractors/farm_stock_light.toml");
    ASSERT_TRUE(from_disk) << from_disk.error().message;
    EXPECT_EQ(through_reader.value().chassis.mass_kg, from_disk.value().chassis.mass_kg);
    EXPECT_EQ(through_reader.value().engine.rated_omega_rad_s,
              from_disk.value().engine.rated_omega_rad_s);
}

TEST(LoadThroughAReader, ReadsASeasonDownToEveryTrack) {
    ResFiles files;
    const auto season = load_season("res://data/seasons/club_season.toml", files.reader());
    ASSERT_TRUE(season) << season.error().message;
    EXPECT_FALSE(season.value().rounds.empty());
    EXPECT_TRUE(files.requested().contains("res://data/events/club_pull.toml"));
    EXPECT_TRUE(files.requested().contains("res://data/classes/ntto_standard_3500.toml"));
    EXPECT_TRUE(files.requested().contains("res://data/tracks/firm_loam.toml"));
}

TEST(LoadThroughAReader, NamesAFileItCannotRead) {
    ResFiles files;
    const auto missing = load_track("res://data/tracks/no_such_track.toml", files.reader());
    ASSERT_FALSE(missing);
    EXPECT_EQ(missing.error().message, "res://data/tracks/no_such_track.toml: cannot open file");
}

TEST(LoadThroughAReader, NamesTheComponentFileThatIsWrong) {
    ResFiles files;
    files.set("res://data/engines/farm_stock_light.toml", "[engine]\ninertia_kg_m2 = -1.0\n");
    const auto tractor = load_tractor("res://data/tractors/farm_stock_light.toml", files.reader());
    ASSERT_FALSE(tractor);
    EXPECT_TRUE(tractor.error().message.starts_with("res://data/engines/farm_stock_light.toml"))
        << tractor.error().message;
}

TEST(ParseThroughAReader, ResolvesTheFilesAnEventNamesFromItsDirectory) {
    ResFiles files;
    const std::optional<std::string> text = files.reader()("res://data/events/club_pull.toml");
    if (!text) {
        FAIL() << "cannot read the event";
    }
    const auto event = parse_event(*text, "club_pull.toml", "res://data/events", files.reader());
    ASSERT_TRUE(event) << event.error().message;
    EXPECT_TRUE(files.requested().contains("res://data/sleds/light_class.toml"));
}

}  // namespace
}  // namespace tp::data
