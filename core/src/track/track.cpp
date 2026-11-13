#include "tp/core/track/track.hpp"

#include "tp/core/math/random.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <functional>
#include <iterator>
#include <span>

namespace tp::track {

namespace {

// Every profile point must make a valid soil with the rest of the
// composition.
bool profile_valid(const Params& params, const std::optional<math::Curve>& profile,
                   double soil::Composition::*field) noexcept {
    if (!profile) {
        return true;
    }
    return std::ranges::all_of(profile->points(), [&](const math::CurvePoint& point) {
        soil::Composition composition = params.soil;
        composition.*field = point.y;
        return !soil::validate(composition).has_value();
    });
}

soil::Surface blend(const soil::Surface& a, const soil::Surface& b, double t) noexcept {
    return {.cone_index_pa = a.cone_index_pa + t * (b.cone_index_pa - a.cone_index_pa),
            .grip_factor = a.grip_factor + t * (b.grip_factor - a.grip_factor),
            .spin_fall_off = a.spin_fall_off + t * (b.spin_fall_off - a.spin_fall_off)};
}

// The shape every spot table shares, and its size within [0, max_size].
struct SpotShape {
    double every_m = 0.0;
    double length_m = 0.0;
    double min_length_m = 0.0;
    double min_width_m = 0.0;
    double max_width_m = 0.0;
    double size = 0.0;
    std::uint32_t seed = 1;
};

bool spots_valid(const SpotShape& shape, double max_size, double across_m) noexcept {
    const std::array values = {shape.every_m,     shape.length_m,    shape.min_length_m,
                               shape.min_width_m, shape.max_width_m, shape.size};
    // No shortest (0) lays them all the longest's length.
    const bool lengths_ok = shape.min_length_m == 0.0 || (shape.min_length_m >= kMinSpotLength_m &&
                                                          shape.min_length_m <= shape.length_m);
    return std::ranges::all_of(values, [](double v) { return std::isfinite(v); }) &&
           shape.every_m >= kMinSpotSpacing_m && shape.length_m >= kMinSpotLength_m &&
           shape.length_m <= kMaxSpotLength_m && lengths_ok &&
           shape.min_width_m >= kMinSpotWidth_m && shape.min_width_m <= shape.max_width_m &&
           shape.max_width_m <= across_m && shape.size >= 0.0 && shape.size <= max_size &&
           shape.seed != 0;
}

// A spot table's seed for an event seeded `seed`: never 0.
std::uint32_t mixed_seed(std::uint32_t own, std::uint64_t seed) noexcept {
    math::SplitMix64 random{seed ^ (static_cast<std::uint64_t>(own) << 32U)};
    constexpr unsigned kHighHalf = 32U;
    const auto mixed = static_cast<std::uint32_t>(random.next() >> kHighHalf);
    return mixed == 0 ? 1 : mixed;
}

// A 32-bit xorshift: integer arithmetic only, so the hard spots fall in the
// same places on every platform.
class Xorshift32 {
public:
    explicit Xorshift32(std::uint32_t seed) noexcept : state_(seed) {}

    // Uniform in [0, 1), exactly representable (24 bits).
    double next() noexcept {
        state_ ^= state_ << 13U;
        state_ ^= state_ >> 17U;
        state_ ^= state_ << 5U;
        constexpr double kScale = 1.0 / 16777216.0;  // 2^-24
        return static_cast<double>(state_ >> 8U) * kScale;
    }

private:
    std::uint32_t state_;
};

// The first spot lies at most this many spacings past the start line, the
// gap to the next between these shares of the spacing, and each peaks at
// this share of the extra and above.
constexpr double kFirstSpotSpacings = 1.0;
constexpr double kGapLowShare = 0.5;
constexpr double kPeakLowShare = 0.6;
// Even the closest-packed spots the limits allow fit the table: none is
// dropped from the first kSpotsUntil_m. This holds while the first spot
// starts at 0 or later and each next one at least a minimum length and
// half a minimum spacing on (random.next() is never negative).
static_assert(Model::kSpotsUntil_m / (kMinSpotLength_m + kMinSpotSpacing_m * kGapLowShare) <
                  static_cast<double>(Model::kMaxSpots),
              "kMaxSpots too small for the hard spots' limits");

}  // namespace

std::optional<ParamError> validate(const Params& params) noexcept {
    if (soil::validate(params.soil)) {
        return ParamError::kSoil;
    }
    if (!profile_valid(params, params.moisture_along_m, &soil::Composition::moisture)) {
        return ParamError::kMoistureProfile;
    }
    if (!profile_valid(params, params.compaction_along_m, &soil::Composition::compaction)) {
        return ParamError::kCompactionProfile;
    }
    if (!std::isfinite(params.grip_scale) || params.grip_scale < kMinGripScale ||
        params.grip_scale > kMaxGripScale) {
        return ParamError::kGripScale;
    }
    const double across_m = params.spots_across_m;
    if (!std::isfinite(across_m) || across_m <= 0.0) {
        return ParamError::kSpotsAcross;
    }
    if (const auto& hard = params.hard_spots;
        hard && !spots_valid({.every_m = hard->every_m,
                              .length_m = hard->length_m,
                              .min_length_m = hard->min_length_m,
                              .min_width_m = hard->min_width_m,
                              .max_width_m = hard->max_width_m,
                              .size = hard->extra,
                              .seed = hard->seed},
                             kMaxHardSpotExtra, across_m)) {
        return ParamError::kHardSpots;
    }
    if (const auto& weak = params.weak_spots;
        weak && !spots_valid({.every_m = weak->every_m,
                              .length_m = weak->length_m,
                              .min_length_m = weak->min_length_m,
                              .min_width_m = weak->min_width_m,
                              .max_width_m = weak->max_width_m,
                              .size = weak->grip_loss,
                              .seed = weak->seed},
                             kMaxWeakSpotGripLoss, across_m)) {
        return ParamError::kWeakSpots;
    }
    return std::nullopt;
}

Params reseeded(const Params& params, std::uint64_t seed) noexcept {
    Params out = params;
    if (out.hard_spots) {
        out.hard_spots->seed = mixed_seed(out.hard_spots->seed, seed);
    }
    if (out.weak_spots) {
        out.weak_spots->seed = mixed_seed(out.weak_spots->seed, seed);
    }
    return out;
}

soil::Composition composition_at(const Params& params, double x_m) noexcept {
    soil::Composition composition = params.soil;
    if (params.moisture_along_m) {
        composition.moisture = params.moisture_along_m->evaluate(x_m);
    }
    if (params.compaction_along_m) {
        composition.compaction = params.compaction_along_m->evaluate(x_m);
    }
    return composition;
}

Model::Model(const Params& params) noexcept : params_(params) {
    // The profiles' joint span; outside it the surface holds its end values.
    double low_m = 0.0;
    double high_m = 0.0;
    bool profiled = false;
    for (const std::optional<math::Curve>& profile :
         {params.moisture_along_m, params.compaction_along_m}) {
        if (profile) {
            low_m = profiled ? std::min(low_m, profile->min_x()) : profile->min_x();
            high_m = profiled ? std::max(high_m, profile->max_x()) : profile->max_x();
            profiled = true;
        }
    }
    first_x_m_ = low_m;
    if (profiled) {
        const double span_m = high_m - low_m;
        const auto max_intervals = static_cast<double>(kMaxSamples - 1);
        spacing_m_ = std::max(kSampleSpacing_m, span_m / max_intervals);
        // One sample past the span when the spacing does not divide it.
        count_ = std::min(static_cast<std::size_t>(span_m / spacing_m_) + 2, kMaxSamples);
    }
    for (std::size_t i = 0; i < count_; ++i) {
        const double x_m = first_x_m_ + static_cast<double>(i) * spacing_m_;
        samples_[i] = soil::surface(composition_at(params_, x_m));
        samples_[i].grip_factor *= params_.grip_scale;
    }
    if (const auto& hard = params_.hard_spots) {
        hard_.lay({.every_m = hard->every_m,
                   .length_m = hard->length_m,
                   .min_length_m = hard->min_length_m,
                   .min_width_m = hard->min_width_m,
                   .max_width_m = hard->max_width_m,
                   .across_m = params_.spots_across_m,
                   .size = hard->extra,
                   .seed = hard->seed});
    }
    if (const auto& weak = params_.weak_spots) {
        weak_.lay({.every_m = weak->every_m,
                   .length_m = weak->length_m,
                   .min_length_m = weak->min_length_m,
                   .min_width_m = weak->min_width_m,
                   .max_width_m = weak->max_width_m,
                   .across_m = params_.spots_across_m,
                   .size = weak->grip_loss,
                   .seed = weak->seed});
    }
}

double strength_at(const Spot& spot, double x_m, double y_m) noexcept {
    const double half_length_m = spot.length_m / 2.0;
    const double along = (x_m - (spot.start_m + half_length_m)) / half_length_m;
    const double across = (y_m - spot.centre_y_m) / (spot.width_m / 2.0);
    // The share of the way from the middle to the ellipse's edge.
    const double out = std::sqrt(along * along + across * across);
    return out >= 1.0 ? 0.0 : spot.peak * (1.0 - out);
}

void Model::SpotLayout::lay(const Pattern& pattern) noexcept {
    Xorshift32 random{pattern.seed};
    double start_m = pattern.every_m * kFirstSpotSpacings * random.next();
    while (start_m < kSpotsUntil_m && count_ < kMaxSpots) {
        const double peak = pattern.size * (kPeakLowShare + (1.0 - kPeakLowShare) * random.next());
        const double width_m =
            pattern.min_width_m + (pattern.max_width_m - pattern.min_width_m) * random.next();
        // Centres spread so that every point of the band is as likely to
        // lie in a spot, its edges too: a spot may reach past the band's
        // edge.
        const double centre_y_m = (pattern.across_m + width_m) * (random.next() - 0.5);
        // Drawn only where the lengths vary, so a fixed length lays as
        // before.
        const double length_m =
            pattern.min_length_m > 0.0 && pattern.min_length_m < pattern.length_m
                ? pattern.min_length_m + (pattern.length_m - pattern.min_length_m) * random.next()
                : pattern.length_m;
        spots_[count_++] = {.start_m = start_m,
                            .length_m = length_m,
                            .centre_y_m = centre_y_m,
                            .width_m = width_m,
                            .peak = peak};
        start_m += length_m + pattern.every_m * (kGapLowShare + random.next());
    }
}

const Spot* Model::SpotLayout::under(double x_m) const noexcept {
    const std::span<const Spot> laid = spots();
    const auto after = std::ranges::upper_bound(laid, x_m, std::less{}, &Spot::start_m);
    return after == laid.begin() ? nullptr : &*std::prev(after);
}

double Model::SpotLayout::at(double x_m, double y_m) const noexcept {
    const Spot* spot = under(x_m);
    return spot != nullptr ? strength_at(*spot, x_m, y_m) : 0.0;
}

std::optional<Model> Model::create(const Params& params) noexcept {
    if (validate(params)) {
        return std::nullopt;
    }
    return Model{params};
}

soil::Surface Model::surface_without_spots(double x_m) const noexcept {
    const auto last = static_cast<double>(count_ - 1);
    const double position = std::clamp((x_m - first_x_m_) / spacing_m_, 0.0, last);
    // Truncation is exact for these non-negative values on every platform.
    const auto index = static_cast<std::size_t>(position);
    return index + 1 >= count_
               ? samples_[count_ - 1]
               : blend(samples_[index], samples_[index + 1], position - static_cast<double>(index));
}

soil::Surface Model::surface_at(double x_m, double y_m) const noexcept {
    soil::Surface surface = surface_without_spots(x_m);
    surface.drag_scale = 1.0 + hard_.at(x_m, y_m);
    surface.grip_factor *= 1.0 - weak_.at(x_m, y_m);
    return surface;
}

soil::Surface Model::surface_across(double x_m, double y_m, double half_width_m) const noexcept {
    constexpr double kPoints = 3.0;
    soil::Surface surface = surface_without_spots(x_m);
    // One search per kind: the same spot is under all three points.
    const auto across = [&](const Spot* spot) {
        return spot == nullptr
                   ? 0.0
                   : strength_at(*spot, x_m, y_m - half_width_m) + strength_at(*spot, x_m, y_m) +
                         strength_at(*spot, x_m, y_m + half_width_m);
    };
    const double hard = across(hard_.under(x_m));
    const double weak = across(weak_.under(x_m));
    surface.drag_scale = 1.0 + hard / kPoints;
    surface.grip_factor *= 1.0 - weak / kPoints;
    return surface;
}

}  // namespace tp::track
