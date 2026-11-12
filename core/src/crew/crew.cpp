#include "tp/core/crew/crew.hpp"

#include "tp/core/math/random.hpp"
#include "tp/core/sled/sled.hpp"

#include <algorithm>
#include <cmath>

namespace tp::crew {

namespace {

// Each pull seen adds this to the uncertainty's denominator; the pull-off
// halves what is left.
constexpr double kSeenPerPull = 0.25;
constexpr double kPullOffShare = 0.5;
// A hard spot's extra drag on the pan weighs this much against the same
// share of grip lost under the rear tires: the pan's drag is only part of
// the sled's, where the tires' grip is all the tractor has.
constexpr double kHardWeight = 0.5;
constexpr double kSides = 2.0;

// Adds `spots` of `kind` to `out` as the crew sees them. Every spot draws
// the same five numbers, seen or not, so one spot's luck never moves
// another's.
void add_seen(Report& out, std::span<const track::Spot> spots, SpotKind kind, double until_m,
              math::SplitMix64& random) noexcept {
    const double u = out.uncertainty;
    for (const track::Spot& spot : spots) {
        const double notice = random.uniform();
        const double along = random.uniform(-1.0, 1.0);
        const double across = random.uniform(-1.0, 1.0);
        const double strength = random.uniform(-1.0, 1.0);
        const double size = 1.0 + kSizeError * u * random.uniform(-1.0, 1.0);
        if (spot.start_m >= until_m || notice < kMissedShare * u || out.count >= out.spots.size()) {
            continue;
        }
        out.spots[out.count++] = {
            .kind = kind,
            .at_m = spot.start_m + spot.length_m / 2.0 + kAlongError_m * u * along,
            .centre_y_m = spot.centre_y_m + kAcrossError_m * u * across,
            .width_m = spot.width_m * size,
            .length_m = spot.length_m * size,
            .strength = spot.peak * (1.0 + kStrengthError * u * strength)};
    }
}

// How much of [low_m, high_m] lies within `half_m` of `centre_m`, as a
// share of its own width.
double overlap_share(double low_m, double high_m, double centre_m, double half_m) noexcept {
    const double covered_m =
        std::min(high_m, centre_m + half_m) - std::max(low_m, centre_m - half_m);
    return std::max(covered_m, 0.0) / (high_m - low_m);
}

// Whether a rear tire running along `tire_y_m` crosses `spot`.
bool tire_over(const ReportedSpot& spot, double tire_y_m, double half_tire_m) noexcept {
    return std::abs(tire_y_m - spot.centre_y_m) < spot.width_m / kSides + half_tire_m;
}

// What the reported spots cost a pull along `y_m`.
double cost(const Report& report, const vehicle::Params& tractor, double y_m) noexcept {
    const double half_track_m = tractor.chassis.rear_track_m / kSides;
    const double half_tire_m = tractor.rear_tire.size.section_width_m / kSides;
    double total = 0.0;
    for (const ReportedSpot& spot : report.view()) {
        if (spot.kind == SpotKind::kWeak) {
            // Either side's wheels over it: the open differential lets that
            // side spin, and the other gets no more than it.
            const bool under = tire_over(spot, y_m + half_track_m, half_tire_m) ||
                               tire_over(spot, y_m - half_track_m, half_tire_m);
            total += under ? spot.strength * spot.length_m : 0.0;
        } else {
            total += kHardWeight * spot.strength * spot.length_m *
                     overlap_share(y_m - sled::kPanHalfWidth_m, y_m + sled::kPanHalfWidth_m,
                                   spot.centre_y_m, spot.width_m / kSides);
        }
    }
    return total;
}

}  // namespace

double uncertainty(const Knowledge& knowledge) noexcept {
    const double seen = static_cast<double>(std::max(knowledge.pulls_seen, 0));
    const double u = 1.0 / (1.0 + kSeenPerPull * seen);
    return knowledge.pull_off ? kPullOffShare * u : u;
}

Report report(const track::Model& track, const Knowledge& knowledge, double until_m,
              std::uint64_t seed) noexcept {
    Report out;
    out.uncertainty = uncertainty(knowledge);
    math::SplitMix64 random{seed};
    add_seen(out, track.hard_spots(), SpotKind::kHard, until_m, random);
    add_seen(out, track.weak_spots(), SpotKind::kWeak, until_m, random);
    return out;
}

double best_line_m(const Report& report, const vehicle::Params& tractor,
                   double max_offset_m) noexcept {
    // Lines from the middle outwards, left before right: the first of the
    // cheapest is the one nearest the middle.
    const auto steps = static_cast<int>(std::max(max_offset_m, 0.0) / kLineStep_m);
    double best_y_m = 0.0;
    double best_cost = cost(report, tractor, 0.0);
    for (int i = 1; i <= steps; ++i) {
        for (const double side : {1.0, -1.0}) {
            const double y_m = side * kLineStep_m * static_cast<double>(i);
            const double here = cost(report, tractor, y_m);
            if (here < best_cost) {
                best_cost = here;
                best_y_m = y_m;
            }
        }
    }
    return best_y_m;
}

}  // namespace tp::crew
