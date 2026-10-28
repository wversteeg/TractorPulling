#pragma once

#include "tp/core/driver/driver.hpp"
#include "tp/core/event/event.hpp"
#include "tp/core/math/random.hpp"
#include "tp/core/rules/class_rules.hpp"
#include "tp/core/run/run.hpp"
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
    /// How often the driver picks the best gear for the day's track, 0 to 1.
    double track_reading = 1.0;
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
    /// Gears the drivers choose between for each track, inclusive.
    int lowest_gear = 0;
    int highest_gear = 0;
    Range clutch_slip_in_s;
    /// How often a driver picks the best gear for the day's track.
    Range track_reading;
    /// When a driver shifts down under load (driver::Plan): within [0, 1).
    Range downshift_share;
    /// The drawbar's height [m]: farm drawbars sit lower than the class
    /// allows. Above 0, held to the class's limit; {0, 0} keeps the
    /// reference's.
    Range hitch_height_m;
    /// How many gears the powershift steps down under load
    /// (driveline::Params::load_downshift_gears), inclusive: a puller draws
    /// each count as often. Both -1 keep the reference's.
    int fewest_load_downshift_gears = -1;
    int most_load_downshift_gears = -1;
    /// The governor's rated speed [rad/s] (engine::with_rated_speed). Above
    /// 0, held to the class's unloaded engine speed; {0, 0} keeps the
    /// reference's.
    Range rated_omega_rad_s;
};

/// How one pull differs from the next for the same puller: the spot on the
/// track under the tires, and the driver's timing. Each pull draws both
/// uniformly within plus or minus these.
struct PullToPull {
    /// Share of the rear tires' grip.
    double grip_share = 0.0;
    /// On the time the driver lets the clutch in [s].
    double clutch_slip_in_s = 0.0;
};

enum class ParamError : std::uint8_t {
    kNotFinite,
    kRangeOutOfOrder,
    kGearsOutOfRange,
    /// The powershift's counts out of order, or not fewer than the gears.
    kLoadDownshiftsOutOfRange,
    kCountOutOfRange,
    /// The reference tractor, driver or class rules themselves.
    kReference,
    /// A governor the reference engine cannot take: below its idle, or a
    /// high idle past its torque curve.
    kGovernorOutOfRange,
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
/// driver keeps pulling past the line until the sled stops, at the rules'
/// track length at the latest (or after their max duration). A tractor
/// tire touching a chalk line before the full-pull line ends it out of
/// bounds at 0 m. Nothing if the puller, sled, track or rules are invalid.
/// Runs a whole pull: for setup and scripted events, not inside a frame.
[[nodiscard]] std::optional<run::Result> run_pull(const Puller& puller, const sled::Params& sled,
                                                  const track::Params& track,
                                                  const run::Rules& rules) noexcept;

[[nodiscard]] std::optional<ParamError> validate(const Puller& reference,
                                                 const Variation& variation,
                                                 const rules::ClassRules& class_rules,
                                                 int count) noexcept;

/// The gear among `lowest` to `highest` that takes `puller` furthest on this
/// sled and track: what a driver who reads the track right picks. Runs a
/// pull per gear; for setup, not inside a frame.
[[nodiscard]] std::optional<int> best_gear(const Puller& puller, const sled::Params& sled,
                                           const track::Params& track, int lowest, int highest,
                                           const run::Rules& rules) noexcept;

/// The driver's guess: the best gear as often as they read the track
/// right, otherwise one gear off (within `lowest` to `highest`).
[[nodiscard]] int guess_gear(const Puller& puller, int best, int lowest, int highest,
                             math::SplitMix64& random) noexcept;

/// The puller as they are on this pull: grip and clutch timing drawn
/// within `variation`.
[[nodiscard]] Puller on_this_pull(const Puller& puller, const PullToPull& variation,
                                  math::SplitMix64& random) noexcept;

}  // namespace tp::field
