#pragma once

#include "tp/core/math/curve.hpp"
#include "tp/core/soil/soil.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace tp::track {

/// Hard spots along the track: short patches where the sled drags harder
/// (track preparation never makes the ground even), so its resistance comes
/// in jumps rather than rising smoothly. Laid out once from `seed`, the
/// same on every platform: on average one every `every_m`, each `length_m`
/// long, rising to and falling from a peak of up to `extra` above the
/// soil's own drag (60 to 100 % of it).
struct HardSpots {
    double every_m = 0.0;
    double length_m = 0.0;
    double extra = 0.0;
    std::uint32_t seed = 1;
};

/// The limits a track's hard spots may have, inclusive [m, m, share].
inline constexpr double kMinHardSpotSpacing_m = 2.0;
inline constexpr double kMinHardSpotLength_m = 0.5;
inline constexpr double kMaxHardSpotLength_m = 10.0;
inline constexpr double kMaxHardSpotExtra = 2.0;

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
    /// Hard spots closer together than kMinHardSpotSpacing_m, shorter or
    /// longer than the length limits, their extra outside [0,
    /// kMaxHardSpotExtra], or a seed of 0.
    kHardSpots,
};

[[nodiscard]] std::optional<ParamError> validate(const Params& params) noexcept;

/// The soil's composition at `x_m` along the track.
[[nodiscard]] soil::Composition composition_at(const Params& params, double x_m) noexcept;

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

    /// The surface at `x_m` along the track.
    [[nodiscard]] soil::Surface surface_at(double x_m) const noexcept;

    /// Hard spots are laid out over this much track from the start line
    /// (past it the drag is even), at most this many of them [m].
    static constexpr double kHardSpotsUntil_m = 200.0;
    static constexpr std::size_t kMaxHardSpots = 160;

private:
    explicit Model(const Params& params) noexcept;

    Params params_;
    double first_x_m_ = 0.0;
    double spacing_m_ = kSampleSpacing_m;
    std::size_t count_ = 1;
    std::array<soil::Surface, kMaxSamples> samples_{};
    // The hard spots, by where they start: each a triangle, evaluated
    // exactly in the step (no sampling, so a short spot keeps its peak).
    struct Spot {
        double start_m = 0.0;
        double peak = 0.0;
    };
    std::array<Spot, kMaxHardSpots> spots_{};
    std::size_t spot_count_ = 0;
    double spot_length_m_ = 1.0;
    [[nodiscard]] double drag_scale_at(double x_m) const noexcept;
};

}  // namespace tp::track
