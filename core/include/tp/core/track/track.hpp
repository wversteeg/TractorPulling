#pragma once

#include "tp/core/math/curve.hpp"
#include "tp/core/soil/soil.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace tp::track {

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
};

enum class ParamError : std::uint8_t {
    kSoil,
    /// A profile point gives a moisture or compaction the soil model
    /// rejects.
    kMoistureProfile,
    kCompactionProfile,
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

private:
    explicit Model(const Params& params) noexcept;

    Params params_;
    double first_x_m_ = 0.0;
    double spacing_m_ = kSampleSpacing_m;
    std::size_t count_ = 1;
    std::array<soil::Surface, kMaxSamples> samples_{};
};

}  // namespace tp::track
