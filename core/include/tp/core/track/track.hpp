#pragma once

#include "tp/core/math/curve.hpp"
#include "tp/core/soil/soil.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

namespace tp::track {

/// Hard spots along the track: short patches where the sled drags harder
/// (track preparation never makes the ground even), so its resistance comes
/// in jumps rather than rising smoothly. Laid out once from `seed`, the
/// same on every platform (an event lays its own: reseeded()): on average
/// one every `every_m` along the track, each `length_m` long and from
/// `min_width_m` to `max_width_m` wide (a patch or a seam across most of
/// the lane), somewhere across the lane (any line down the lane as likely
/// to cross one; a spot may reach past a chalk line), rising to and
/// falling from a peak of up to `extra` above the soil's own drag (60 to
/// 100 % of it).
struct HardSpots {
    double every_m = 0.0;
    double length_m = 0.0;
    double min_width_m = 0.0;
    double max_width_m = 0.0;
    double extra = 0.0;
    std::uint32_t seed = 1;
};

/// Weak spots along the track: short patches where the tires grip less (a
/// looser or wetter bit the preparation missed), so a wheel over one slips
/// for a moment. Laid out as hard spots are, from their own `seed`: the
/// grip falls by up to `grip_loss` (60 to 100 % of it) at a spot's middle.
struct WeakSpots {
    double every_m = 0.0;
    double length_m = 0.0;
    double min_width_m = 0.0;
    double max_width_m = 0.0;
    double grip_loss = 0.0;
    std::uint32_t seed = 1;
};

/// The limits a track's hard and weak spots may have, inclusive [m, m, m,
/// share, share]. A spot is at most as wide as the band it lies in
/// (Params::spots_across_m), its narrowest no wider than its widest.
inline constexpr double kMinSpotSpacing_m = 2.0;
inline constexpr double kMinSpotLength_m = 0.5;
inline constexpr double kMaxSpotLength_m = 10.0;
inline constexpr double kMinSpotWidth_m = 0.5;
inline constexpr double kMaxHardSpotExtra = 2.0;
inline constexpr double kMaxWeakSpotGripLoss = 0.5;

/// One hard or weak spot as laid: from `start_m` along the track (from the
/// start line) for `length_m`, centred `centre_y_m` across it (y positive
/// to the left, 0 the lane's middle) and `width_m` wide. Along the track it
/// rises to `peak` at its middle and falls again (a triangle); across, it
/// is at its full strength over the middle half of its width and fades to
/// nothing at its edges.
struct Spot {
    double start_m = 0.0;
    double length_m = 0.0;
    double centre_y_m = 0.0;
    double width_m = 0.0;
    double peak = 0.0;
};

/// How strong `spot` is at (`x_m`, `y_m`): 0 off it, `peak` at its middle.
[[nodiscard]] double strength_at(const Spot& spot, double x_m, double y_m) noexcept;

/// The NTTO's minimum prepared lane width, the band the spots lie in
/// unless a track says otherwise [m].
inline constexpr double kDefaultSpotsAcross_m = 12.0;

/// A pulling track's ground: one soil, and optionally how its moisture and
/// compaction change along the track (a wet patch where it was watered, a
/// looser stretch where it was worked). Each profile gives the absolute
/// value against x, metres from the start line, where the sled's rear
/// wheels stand at the start; outside the profile it holds its end values.
/// Without a profile the soil's own value holds everywhere.
struct Params {
    soil::Composition soil;
    std::optional<math::Curve> moisture_along_m;
    std::optional<math::Curve> compaction_along_m;
    /// Scale on the soil's surface grip (soil::Surface::grip_factor): how
    /// the track was prepared, its topsoil worked and rolled, which the
    /// soil's make-up alone does not say. 1 is a well-prepared track;
    /// tracks differ by tens of per cent, from one where tractors stand on
    /// their wheelie bars to one where they spin early.
    double grip_scale = 1.0;
    std::optional<HardSpots> hard_spots = std::nullopt;
    std::optional<WeakSpots> weak_spots = std::nullopt;
    /// The band across the track the spots lie in, centred on its middle:
    /// the lane between the chalk lines [m].
    double spots_across_m = kDefaultSpotsAcross_m;
};

/// The grip scale a track may have, inclusive.
inline constexpr double kMinGripScale = 0.5;
inline constexpr double kMaxGripScale = 1.5;

enum class ParamError : std::uint8_t {
    kSoil,
    /// A profile point gives a moisture or compaction the soil model
    /// rejects.
    kMoistureProfile,
    kCompactionProfile,
    /// The grip scale is not finite or outside [kMinGripScale,
    /// kMaxGripScale].
    kGripScale,
    /// Hard spots closer together than kMinSpotSpacing_m, shorter or
    /// longer than the length limits, narrower than kMinSpotWidth_m or
    /// wider than spots_across_m, their extra outside [0,
    /// kMaxHardSpotExtra], or a seed of 0.
    kHardSpots,
    /// Weak spots outside the same spacing, length and width limits, their
    /// grip loss outside [0, kMaxWeakSpotGripLoss], or a seed of 0.
    kWeakSpots,
    /// The spots' band across the track is not finite or not above 0.
    kSpotsAcross,
};

[[nodiscard]] std::optional<ParamError> validate(const Params& params) noexcept;

/// The soil's composition at `x_m` along the track.
[[nodiscard]] soil::Composition composition_at(const Params& params, double x_m) noexcept;

/// `params` with its hard and weak spots laid afresh for `seed` (an
/// event's): the same kinds, sizes and number, elsewhere. Each spot table's
/// own seed is mixed with `seed`, so the same track and seed give the same
/// spots on every platform, and another seed (another season's event)
/// others.
[[nodiscard]] Params reseeded(const Params& params, std::uint64_t seed) noexcept;

/// The track sampled once at setup: in the step a surface is a lookup and a
/// blend between two samples, never the soil model itself.
class Model {
public:
    /// Enough for a 100 m track with run-off at 0.5 m resolution; longer
    /// profiles are sampled more coarsely.
    static constexpr std::size_t kMaxSamples = 512;
    static constexpr double kSampleSpacing_m = 0.5;

    [[nodiscard]] static std::optional<Model> create(const Params& params) noexcept;

    [[nodiscard]] const Params& params() const noexcept { return params_; }

    /// The surface at `x_m` along the track and `y_m` across it (0 its
    /// middle, positive to the left): the hard and weak spots under that
    /// point included.
    [[nodiscard]] soil::Surface surface_at(double x_m, double y_m) const noexcept;
    /// The surface under something `2 · half_width_m` wide centred at
    /// (`x_m`, `y_m`), such as the sled's pan: the spots' effect is the mean
    /// of its middle and its two edges.
    [[nodiscard]] soil::Surface surface_across(double x_m, double y_m,
                                               double half_width_m) const noexcept;
    /// The surface along the middle of the track.
    [[nodiscard]] soil::Surface surface_at(double x_m) const noexcept {
        return surface_at(x_m, 0.0);
    }

    /// The spots as laid, by where they start along the track.
    [[nodiscard]] std::span<const Spot> hard_spots() const noexcept { return hard_.spots(); }
    [[nodiscard]] std::span<const Spot> weak_spots() const noexcept { return weak_.spots(); }

    /// Hard and weak spots are laid out over this much track from the
    /// start line (past it the ground is even), at most this many of each
    /// [m].
    static constexpr double kSpotsUntil_m = 200.0;
    static constexpr std::size_t kMaxSpots = 160;

private:
    explicit Model(const Params& params) noexcept;
    // The soil's own surface at x_m, its profiles blended between samples.
    [[nodiscard]] soil::Surface surface_without_spots(double x_m) const noexcept;

    Params params_;
    double first_x_m_ = 0.0;
    double spacing_m_ = kSampleSpacing_m;
    std::size_t count_ = 1;
    std::array<soil::Surface, kMaxSamples> samples_{};
    // Spots along the track, by where they start: evaluated exactly in the
    // step (no sampling, so a short spot keeps its peak); 0 between them.
    class SpotLayout {
    public:
        // How spots are laid: one every `every_m` on average, each
        // `length_m` long and `min_width_m` to `max_width_m` wide somewhere
        // across `across_m` (every point of it as likely to be in one),
        // peaking at 60 to 100 % of `size`, drawn from `seed`.
        struct Pattern {
            double every_m = 0.0;
            double length_m = 0.0;
            double min_width_m = 0.0;
            double max_width_m = 0.0;
            double across_m = 0.0;
            double size = 0.0;
            std::uint32_t seed = 1;
        };
        // Laid end to end along the track with gaps between.
        void lay(const Pattern& pattern) noexcept;
        [[nodiscard]] double at(double x_m, double y_m) const noexcept;
        // The one spot that may be under x_m (spots never overlap along
        // the track): the last to start at or before it; nullptr if none.
        [[nodiscard]] const Spot* under(double x_m) const noexcept;
        [[nodiscard]] std::span<const Spot> spots() const noexcept {
            return std::span{spots_}.first(count_);
        }

    private:
        std::array<Spot, kMaxSpots> spots_{};
        std::size_t count_ = 0;
    };
    SpotLayout hard_;
    SpotLayout weak_;
};

}  // namespace tp::track
