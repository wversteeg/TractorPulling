#pragma once

#include "tp/core/track/track.hpp"
#include "tp/core/vehicle/vehicle.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace tp::crew {

/// What a team's crew knows of the track's hard and weak spots when it
/// walks the lane before a pull, and where it puts the sled because of it.
///
/// A crew sees the spots roughly: a little off along and across the track,
/// some missed altogether, their strength guessed. Each pull before its own
/// shows it more (the tractors' tracks, the sled's drag and the wheels
/// spinning where they did), and in the pull-off it has seen the whole
/// class pull: the report sharpens with what it has seen.

enum class SpotKind : std::uint8_t {
    /// The sled drags harder (track::HardSpots).
    kHard,
    /// The tires grip less (track::WeakSpots).
    kWeak,
};

/// What the crew has seen before this pull.
struct Knowledge {
    /// Pulls on this track today before this one.
    int pulls_seen = 0;
    /// The pull-off: the crew has seen the whole class pull.
    bool pull_off = false;
};

/// A spot as the crew reports it: about `at_m` along the track (its
/// middle, from the start line), centred about `centre_y_m` across it (to
/// the left of the centre line) and about `width_m` wide and `length_m`
/// long; `strength` its guessed peak (a hard spot's extra drag or a weak
/// spot's grip loss, shares).
struct ReportedSpot {
    SpotKind kind = SpotKind::kHard;
    double at_m = 0.0;
    double centre_y_m = 0.0;
    double width_m = 0.0;
    double length_m = 0.0;
    double strength = 0.0;
};

/// At most this many spots in a report: every spot the track can have.
inline constexpr std::size_t kMaxReported = 2 * track::Model::kMaxSpots;

struct Report {
    std::array<ReportedSpot, kMaxReported> spots{};
    std::size_t count = 0;
    /// How unsure the crew is, 1 before any pull, falling towards 0 with
    /// what it has seen (uncertainty()).
    double uncertainty = 1.0;

    [[nodiscard]] std::span<const ReportedSpot> view() const noexcept {
        return std::span{spots}.first(count);
    }
};

/// How unsure a crew with `knowledge` is: 1 before any pull, a quarter of a
/// pull's worth less each pull it has seen, halved in the pull-off.
[[nodiscard]] double uncertainty(const Knowledge& knowledge) noexcept;

/// What a crew with `knowledge` reports of the spots on `track` that start
/// before `until_m` along it. `seed` draws its errors and what it misses:
/// the same seed, the same report, on every platform. A crew that has seen
/// nothing misses up to kMissedShare of the spots; their place is off by
/// up to kAlongError_m along and kAcrossError_m across, their strength by
/// kStrengthError of itself and their size (width and length) by
/// kSizeError of itself, each shrinking with the uncertainty.
[[nodiscard]] Report report(const track::Model& track, const Knowledge& knowledge, double until_m,
                            std::uint64_t seed) noexcept;

inline constexpr double kMissedShare = 0.6;
inline constexpr double kAlongError_m = 4.0;
inline constexpr double kAcrossError_m = 1.5;
inline constexpr double kStrengthError = 0.4;
inline constexpr double kSizeError = 0.4;

/// Where the crew puts the sled across the lane for `tractor`, at most
/// `max_offset_m` from the centre line either way: the line that keeps the
/// reported weak spots from under the rear tires (a weak spot under one
/// side's wheels spins that side and, through a coupled differential, turns
/// the tractor towards it)
/// and the hard spots from under the sled's pan, as far as it can, weighed
/// by their strength and length. Between lines as good, the one nearest the
/// middle [m, to the left].
[[nodiscard]] double best_line_m(const Report& report, const vehicle::Params& tractor,
                                 double max_offset_m) noexcept;

/// The lines best_line_m() weighs, this far apart [m].
inline constexpr double kLineStep_m = 0.1;

}  // namespace tp::crew
