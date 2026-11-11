#include "tp/core/track/track.hpp"

#include <algorithm>
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

bool spots_valid(double every_m, double length_m, double size, double max_size,
                 std::uint32_t seed) noexcept {
    return std::isfinite(every_m) && std::isfinite(length_m) && std::isfinite(size) &&
           every_m >= kMinHardSpotSpacing_m && length_m >= kMinHardSpotLength_m &&
           length_m <= kMaxHardSpotLength_m && size >= 0.0 && size <= max_size && seed != 0;
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
// dropped from the first kHardSpotsUntil_m.
static_assert(Model::kHardSpotsUntil_m /
                      (kMinHardSpotLength_m + kMinHardSpotSpacing_m * kGapLowShare) <
                  static_cast<double>(Model::kMaxHardSpots),
              "kMaxHardSpots too small for the hard spots' limits");

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
    if (const auto& hard = params.hard_spots;
        hard &&
        !spots_valid(hard->every_m, hard->length_m, hard->extra, kMaxHardSpotExtra, hard->seed)) {
        return ParamError::kHardSpots;
    }
    if (const auto& weak = params.weak_spots;
        weak && !spots_valid(weak->every_m, weak->length_m, weak->grip_loss, kMaxWeakSpotGripLoss,
                             weak->seed)) {
        return ParamError::kWeakSpots;
    }
    return std::nullopt;
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
        hard_.lay(hard->every_m, hard->length_m, hard->extra, hard->seed);
    }
    if (const auto& weak = params_.weak_spots) {
        weak_.lay(weak->every_m, weak->length_m, weak->grip_loss, weak->seed);
    }
}

void Model::SpotLayout::lay(double every_m, double length_m, double size,
                            std::uint32_t seed) noexcept {
    length_m_ = length_m;
    Xorshift32 random{seed};
    double start_m = every_m * kFirstSpotSpacings * random.next();
    while (start_m < kHardSpotsUntil_m && count_ < kMaxHardSpots) {
        const double peak = size * (kPeakLowShare + (1.0 - kPeakLowShare) * random.next());
        spots_[count_++] = {.start_m = start_m, .peak = peak};
        start_m += length_m + every_m * (kGapLowShare + random.next());
    }
}

double Model::SpotLayout::at(double x_m) const noexcept {
    // The last spot starting at or before x_m, if x_m is within it.
    const auto spots = std::span{spots_}.first(count_);
    const auto after = std::ranges::upper_bound(spots, x_m, std::less{}, &Spot::start_m);
    if (after == spots.begin()) {
        return 0.0;
    }
    const Spot& spot = *std::prev(after);
    const double half_m = length_m_ / 2.0;
    const double from_middle = std::abs(x_m - (spot.start_m + half_m)) / half_m;
    return from_middle < 1.0 ? spot.peak * (1.0 - from_middle) : 0.0;
}

std::optional<Model> Model::create(const Params& params) noexcept {
    if (validate(params)) {
        return std::nullopt;
    }
    return Model{params};
}

soil::Surface Model::surface_at(double x_m) const noexcept {
    const auto last = static_cast<double>(count_ - 1);
    const double position = std::clamp((x_m - first_x_m_) / spacing_m_, 0.0, last);
    // Truncation is exact for these non-negative values on every platform.
    const auto index = static_cast<std::size_t>(position);
    soil::Surface surface = index + 1 >= count_ ? samples_[count_ - 1]
                                                : blend(samples_[index], samples_[index + 1],
                                                        position - static_cast<double>(index));
    surface.drag_scale = 1.0 + hard_.at(x_m);
    surface.grip_factor *= 1.0 - weak_.at(x_m);
    return surface;
}

}  // namespace tp::track
