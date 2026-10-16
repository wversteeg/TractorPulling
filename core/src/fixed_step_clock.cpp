#include "tp/core/fixed_step_clock.hpp"

#include <limits>

namespace tp {

namespace {

// The per-call budget plus a carried-over partial step must fit in int64.
constexpr std::int64_t kMaxBudgetNs = std::numeric_limits<std::int64_t>::max() / 2;

}  // namespace

std::optional<FixedStepClock> FixedStepClock::create(Config config) noexcept {
    if (config.step_ns <= 0 || config.max_steps_per_advance == 0) {
        return std::nullopt;
    }
    if (static_cast<std::int64_t>(config.max_steps_per_advance) > kMaxBudgetNs / config.step_ns) {
        return std::nullopt;
    }
    return FixedStepClock{config.step_ns, config.max_steps_per_advance};
}

FixedStepClock::Advance FixedStepClock::advance(std::int64_t elapsed_ns) noexcept {
    Advance result;
    if (elapsed_ns <= 0) {
        return result;
    }

    const std::int64_t budget_ns = static_cast<std::int64_t>(max_steps_per_advance_) * step_ns_;
    if (elapsed_ns > budget_ns) {
        result.dropped_ns = elapsed_ns - budget_ns;
        elapsed_ns = budget_ns;
    }

    // accumulated_ns_ < step_ns_ and elapsed_ns <= budget_ns, so the quotient
    // is at most max_steps_per_advance_.
    accumulated_ns_ += elapsed_ns;
    const std::int64_t steps = accumulated_ns_ / step_ns_;
    accumulated_ns_ -= steps * step_ns_;
    result.steps = static_cast<std::uint32_t>(steps);
    return result;
}

double FixedStepClock::alpha() const noexcept {
    return static_cast<double>(accumulated_ns_) / static_cast<double>(step_ns_);
}

double FixedStepClock::step_s() const noexcept {
    return static_cast<double>(step_ns_) / static_cast<double>(kNanosecondsPerSecond);
}

}  // namespace tp
