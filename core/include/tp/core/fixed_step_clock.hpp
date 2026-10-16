#pragma once

#include <cstdint>
#include <optional>

namespace tp {

inline constexpr std::int64_t kNanosecondsPerSecond = 1'000'000'000;

/// Simulation step: 1 kHz.
inline constexpr std::int64_t kSimulationStepNs = 1'000'000;

/// Converts variable frame time into a whole number of fixed simulation steps.
///
/// Time is counted in integer nanoseconds so that accumulated rounding error
/// can never change the number of steps taken: 1 s of frames gives exactly
/// 1000 steps at 1 kHz, regardless of how the second is split into frames.
///
/// A single call never yields more than `max_steps_per_advance` steps. Time
/// beyond that budget is dropped (and reported) instead of being carried over,
/// so a slow frame cannot snowball into ever longer frames.
///
/// The clock never reads system time; the caller passes elapsed time in.
class FixedStepClock {
public:
    struct Config {
        std::int64_t step_ns = kSimulationStepNs;
        /// 100 steps = 100 ms of simulated time at 1 kHz.
        std::uint32_t max_steps_per_advance = 100;
    };

    struct Advance {
        /// Number of fixed steps the caller should run now.
        std::uint32_t steps = 0;
        /// Elapsed time discarded because it exceeded the per-call budget.
        std::int64_t dropped_ns = 0;
    };

    /// Returns `std::nullopt` if the configuration is invalid: a non-positive
    /// step, zero max steps, or a per-call budget that could overflow.
    [[nodiscard]] static std::optional<FixedStepClock> create(Config config) noexcept;

    /// Adds `elapsed_ns` of frame time. Non-positive values add nothing.
    [[nodiscard]] Advance advance(std::int64_t elapsed_ns) noexcept;

    /// Fraction of a step accumulated but not yet simulated, in [0, 1).
    /// Front ends use it to interpolate between the last two states.
    [[nodiscard]] double alpha() const noexcept;

    [[nodiscard]] std::int64_t step_ns() const noexcept { return step_ns_; }
    [[nodiscard]] double step_s() const noexcept;
    [[nodiscard]] std::int64_t accumulated_ns() const noexcept { return accumulated_ns_; }

    /// Discards any accumulated time.
    void reset() noexcept { accumulated_ns_ = 0; }

private:
    FixedStepClock(std::int64_t step_ns, std::uint32_t max_steps_per_advance) noexcept
        : step_ns_(step_ns), max_steps_per_advance_(max_steps_per_advance) {}

    std::int64_t step_ns_;
    std::uint32_t max_steps_per_advance_;
    /// Invariant: 0 <= accumulated_ns_ < step_ns_ between calls.
    std::int64_t accumulated_ns_ = 0;
};

}  // namespace tp
