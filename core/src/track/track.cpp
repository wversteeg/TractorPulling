#include "tp/core/track/track.hpp"

#include <algorithm>

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
    }
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
    if (index + 1 >= count_) {
        return samples_[count_ - 1];
    }
    return blend(samples_[index], samples_[index + 1], position - static_cast<double>(index));
}

}  // namespace tp::track
