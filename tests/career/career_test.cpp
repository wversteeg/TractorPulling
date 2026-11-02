#include "tp/core/career/career.hpp"

#include "tp/core/event/event.hpp"
#include "tp/core/season/season.hpp"

#include <gtest/gtest.h>

#include <array>

namespace tp::career {
namespace {

// The tractor path as it will be (docs/career.md): Standard 3.5 t, Sport
// 2.6 and 4.5 t, Supersport 3.4, 3.6 and 4.5 t, Pro Stock, Limited Super
// Stock and Super Stock; the championship wins a class.
constexpr Ladder kTractorPath{.classes = 9, .promotion_place = 1};

TEST(Career, ValidatesTheLadder) {
    EXPECT_FALSE(validate(kTractorPath));
    EXPECT_FALSE(validate(Ladder{.classes = 1, .promotion_place = 1}));
    EXPECT_FALSE(validate(Ladder{.classes = kMaxClasses, .promotion_place = event::kMaxPullers}));
    EXPECT_EQ(validate(Ladder{.classes = 0, .promotion_place = 1}),
              ParamError::kClassCountOutOfRange);
    EXPECT_EQ(validate(Ladder{.classes = kMaxClasses + 1, .promotion_place = 1}),
              ParamError::kClassCountOutOfRange);
    EXPECT_EQ(validate(Ladder{.classes = 9, .promotion_place = 0}),
              ParamError::kPromotionPlaceOutOfRange);
    EXPECT_EQ(validate(Ladder{.classes = 9, .promotion_place = event::kMaxPullers + 1}),
              ParamError::kPromotionPlaceOutOfRange);
}

TEST(Career, StartsInTheFirstClassWithNothingAboveOpen) {
    const Progress start;
    EXPECT_TRUE(valid(start, kTractorPath));
    EXPECT_EQ(start.class_index, 0);
    EXPECT_FALSE(may_move_up(start));
}

TEST(Career, TellsAProgressThatDoesNotFitTheLadder) {
    EXPECT_TRUE(valid(Progress{.class_index = 2, .unlocked = 5}, kTractorPath));
    EXPECT_FALSE(valid(Progress{.class_index = -1, .unlocked = 0}, kTractorPath));
    EXPECT_FALSE(valid(Progress{.class_index = 3, .unlocked = 2}, kTractorPath));
    EXPECT_FALSE(valid(Progress{.class_index = 0, .unlocked = kTractorPath.classes}, kTractorPath));
}

TEST(Career, WinningTheSeasonOpensTheClassAbove) {
    Progress p;
    EXPECT_TRUE(finish_season(p, kTractorPath, 1));
    EXPECT_EQ(p.unlocked, 1);
    // Opened, not moved: the player is asked.
    EXPECT_EQ(p.class_index, 0);
    EXPECT_TRUE(may_move_up(p));
}

TEST(Career, OnlyThePromotionPlaceOpensAClass) {
    Progress p;
    EXPECT_FALSE(finish_season(p, kTractorPath, 2));
    EXPECT_FALSE(finish_season(p, kTractorPath, 0));
    EXPECT_FALSE(may_move_up(p));
    // A ladder that promotes the podium.
    const Ladder podium{.classes = 9, .promotion_place = 3};
    EXPECT_TRUE(finish_season(p, podium, 3));
    EXPECT_EQ(p.unlocked, 1);
}

TEST(Career, AClassOpenedStaysOpenForAPlayerWhoStays) {
    Progress p;
    ASSERT_TRUE(finish_season(p, kTractorPath, 1));
    // Defending the title: a second title opens nothing more, a lost
    // season closes nothing.
    EXPECT_FALSE(finish_season(p, kTractorPath, 1));
    EXPECT_FALSE(finish_season(p, kTractorPath, 7));
    EXPECT_EQ(p.unlocked, 1);
    EXPECT_TRUE(may_move_up(p));
}

TEST(Career, MovesUpOneClassAtATime) {
    Progress p;
    EXPECT_FALSE(move_up(p));
    ASSERT_TRUE(finish_season(p, kTractorPath, 1));
    EXPECT_TRUE(move_up(p));
    EXPECT_EQ(p.class_index, 1);
    EXPECT_FALSE(may_move_up(p));
    EXPECT_FALSE(move_up(p));
    EXPECT_TRUE(valid(p, kTractorPath));
}

TEST(Career, ClimbsTheWholeLadderAndNoFurther) {
    Progress p;
    for (int season = 0; season < kTractorPath.classes - 1; ++season) {
        ASSERT_TRUE(finish_season(p, kTractorPath, 1));
        ASSERT_TRUE(move_up(p));
    }
    EXPECT_EQ(p.class_index, kTractorPath.classes - 1);
    // The top class's title opens nothing.
    EXPECT_FALSE(finish_season(p, kTractorPath, 1));
    EXPECT_FALSE(may_move_up(p));
    EXPECT_TRUE(valid(p, kTractorPath));
}

// The player's place is read from the championship, a shared title too.
TEST(Career, ReadsThePlayersPlaceFromTheChampionship) {
    const std::array<season::Standing, 3> second{{{.puller = 2, .place = 1, .points = 40},
                                                  {.puller = 0, .place = 2, .points = 37},
                                                  {.puller = 1, .place = 3, .points = 30}}};
    Progress p;
    EXPECT_FALSE(finish_season(p, kTractorPath, second, 0));
    EXPECT_TRUE(finish_season(p, kTractorPath, second, 2));
    EXPECT_EQ(p.unlocked, 1);

    const std::array<season::Standing, 2> shared{
        {{.puller = 1, .place = 1, .points = 40}, {.puller = 0, .place = 1, .points = 40}}};
    Progress q;
    EXPECT_TRUE(finish_season(q, kTractorPath, shared, 0));
    // Not in the field: nothing opens.
    Progress r;
    EXPECT_FALSE(finish_season(r, kTractorPath, shared, 5));
}

TEST(Career, AOneClassLadderNeverPromotes) {
    const Ladder one{.classes = 1, .promotion_place = 1};
    Progress p;
    EXPECT_FALSE(finish_season(p, one, 1));
    EXPECT_FALSE(may_move_up(p));
}

}  // namespace
}  // namespace tp::career
