#pragma once

#include <cstdint>
#include <optional>

namespace tp::soil {

/// What a pulling track is made of and how it was prepared.
struct Composition {
    /// Share of clay in the mix, 0 (pure sand) to 1 (pure clay).
    double clay_fraction = 0.0;
    /// Gravimetric water content: kg of water per kg of dry soil.
    double moisture = 0.0;
    /// 0 loose (freshly worked) to 1 rolled hard.
    double compaction = 0.0;
};

enum class ParamError : std::uint8_t {
    kNotFinite,
    kClayFractionOutOfRange,
    kMoistureOutOfRange,
    kCompactionOutOfRange,
};

/// Moisture above this is mud, not a pulling track.
inline constexpr double kMaxMoisture = 0.5;

[[nodiscard]] std::optional<ParamError> validate(const Composition& composition) noexcept;

/// How the ground behaves under a tire or the sled's pan.
struct Surface {
    /// Soil strength as a cone penetrometer reads it [Pa]. Drives the
    /// tires' mobility (Brixius) and how deep the pan ploughs.
    double cone_index_pa = 0.0;
    /// Scale on the tires' grip from the surface itself, 1 on a clean
    /// moist track: a dust layer on dry clay or a slick film on wet clay
    /// lowers it.
    double grip_factor = 1.0;
    /// Share of grip lost at full wheelspin: tires smear wet clay and dig
    /// into sand.
    double spin_fall_off = 0.0;
};

/// The surface of a soil, from a semi-empirical model (see soil.cpp). Sand
/// is strongest at a moderate moisture (dry sand runs, saturated sand
/// flows); clay gets weaker the wetter it is. Compaction raises both.
[[nodiscard]] Surface surface(const Composition& composition) noexcept;

}  // namespace tp::soil
