#pragma once

#include "tp/core/pull/pull.hpp"
#include "tp/core/run/run.hpp"
#include "tp/core/session/session.hpp"
#include "tp/core/vehicle/vehicle.hpp"

namespace tp::session {

/// The whole simulation inside a session, for tools and tests: what
/// tp_pull writes to its CSV, and what a test steps by hand to compare. Not
/// for the binding layer, which reads the snapshots; keeping this out of
/// session.hpp keeps that interface small.
class Inspector {
public:
    [[nodiscard]] static const pull::Model& model(const Session& session) noexcept {
        return session.model_;
    }
    [[nodiscard]] static const run::State& run_state(const Session& session) noexcept {
        return session.turn_.run;
    }
    [[nodiscard]] static const turn::State& turn_state(const Session& session) noexcept {
        return session.turn_;
    }
    /// The last step's output and the vehicle input it was given.
    [[nodiscard]] static const pull::Output& last_output(const Session& session) noexcept {
        return session.output_;
    }
    [[nodiscard]] static const vehicle::Input& last_input(const Session& session) noexcept {
        return session.input_;
    }
};

}  // namespace tp::session
