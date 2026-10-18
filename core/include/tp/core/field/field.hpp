#pragma once

#include "tp/core/driver/driver.hpp"
#include "tp/core/event/event.hpp"
#include "tp/core/rules/class_rules.hpp"
#include "tp/core/sled/sled.hpp"
#include "tp/core/track/track.hpp"
#include "tp/core/vehicle/vehicle.hpp"

#include <array>
#include <cstdint>
#include <optional>

namespace tp::field {

/// One competitor: a tractor and the way its driver pulls.
struct Puller {
    vehicle::Params tractor;
    driver::Plan driver;
};

struct Range {
    double low = 0.0;
    double high = 0.0;
};

/// How much the pullers in a class differ from the reference tractor and
/// driver. Each puller draws every value uniformly from its range.
struct Variation {
    /// Scale on the engine's full-load torque.
    Range torque_scale;
    /// The whole tractor as weighed [kg].
    Range mass_kg;
    /// The rear tires' traction limit (rubber, pressure, lugs).
    Range traction_limit;
    /// Gears the drivers choose between, inclusive.
    int lowest_gear = 0;
    int highest_gear = 0;
    Range clutch_slip_in_s;
};

enum class ParamError : std::uint8_t {
    kNotFinite,
    kRangeOutOfOrder,
    kGearsOutOfRange,
    kCountOutOfRange,
    /// The reference tractor, driver or class rules themselves.
    kReference,
};

/// The first `count` pullers are the field; the rest are unused copies of
/// the reference (a puller has no empty state).
struct Field {
    std::array<Puller, event::kMaxPullers> pullers;
    int count = 0;
};

/// A field of `count` pullers for `seed`, each the reference varied within
/// `variation` and kept within the class's rules: the engine never makes
/// more PTO power than allowed and the tractor never weighs more. The same
/// seed gives the same field on every platform.
[[nodiscard]] std::optional<Field> generate(const Puller& reference, const Variation& variation,
                                            const rules::ClassRules& class_rules, int count,
                                            std::uint64_t seed) noexcept;

/// Where the sled stops when `puller` pulls it on `track`: the scripted
/// driver keeps pulling past the line until the sled stops, at
/// `max_distance_m` at the latest (or after `max_duration_s`). Nothing if
/// the puller, sled or track is invalid. Runs a whole pull: for setup and
/// scripted events, not inside a frame.
[[nodiscard]] std::optional<double> pull_distance(const Puller& puller, const sled::Params& sled,
                                                  const track::Params& track, double max_distance_m,
                                                  double max_duration_s) noexcept;

[[nodiscard]] std::optional<ParamError> validate(const Puller& reference,
                                                 const Variation& variation,
                                                 const rules::ClassRules& class_rules,
                                                 int count) noexcept;

}  // namespace tp::field
