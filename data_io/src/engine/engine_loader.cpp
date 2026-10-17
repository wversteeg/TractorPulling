#include "tp/data/engine/engine_loader.hpp"

#include "tp/core/math/curve.hpp"
#include "tp/core/units/conversions.hpp"

#include <optional>
#include <string_view>

#include "../common/toml_reader.hpp"

namespace tp::data {

namespace {

using units::rpm_to_rad_s;

// Rpm per "1000 rpm", for friction given per 1000 rpm in the data files.
constexpr double kRpmPerKiloRpm = 1000.0;

struct FieldError {
    std::string_view field;
    std::string_view reason;
};

FieldError describe(engine::ParamError error) {
    using engine::ParamError;
    switch (error) {
        case ParamError::kNotFinite:
            return {"engine", "all values must be finite"};
        case ParamError::kInertiaNotPositive:
            return {"engine.inertia_kg_m2", "must be > 0"};
        case ParamError::kStallNotPositive:
            return {"engine.stall_rpm", "must be > 0"};
        case ParamError::kStallNotBelowIdle:
            return {"engine.stall_rpm", "must be below idle_rpm"};
        case ParamError::kIdleNotBelowRated:
            return {"engine.idle_rpm", "must be below rated_rpm"};
        case ParamError::kRatedNotBelowHighIdle:
            return {"engine.rated_rpm", "must be below high_idle_rpm"};
        case ParamError::kFrictionNegative:
            return {"engine.friction", "must not be negative"};
        case ParamError::kNaTorqueFractionOutOfRange:
            return {"engine.turbo.na_torque_fraction", "must be in (0, 1]"};
        case ParamError::kTurboTimeConstantNotPositive:
            return {"engine.turbo.time_constant_s", "must be > 0"};
        case ParamError::kBoostRangeInvalid:
            return {"engine.turbo", "need 0 <= boost_onset_rpm < full_boost_rpm"};
        case ParamError::kTorqueCurveNegative:
            return {"engine.torque_curve.torque_nm", "must not be negative"};
        case ParamError::kTorqueCurveDoesNotCoverRange:
            return {"engine.torque_curve.rpm", "must span idle_rpm to high_idle_rpm"};
    }
    return {"engine", "invalid"};
}

Result<engine::Params> read_engine(const toml::table& root, std::string_view source) {
    detail::Reader reader{root, source};

    const std::optional<math::Curve> torque_curve =
        reader.curve("engine.torque_curve", "rpm", "torque_nm", units::kRadPerSecPerRpm);
    const double inertia_kg_m2 = reader.number("engine.inertia_kg_m2");
    const double idle_rpm = reader.number("engine.idle_rpm");
    const double rated_rpm = reader.number("engine.rated_rpm");
    const double high_idle_rpm = reader.number("engine.high_idle_rpm");
    const double stall_rpm = reader.number("engine.stall_rpm");
    const double friction_nm = reader.number("engine.friction.constant_nm");
    const double viscous_nm_per_krpm = reader.number("engine.friction.viscous_nm_per_1000_rpm");
    const double na_torque_fraction = reader.number("engine.turbo.na_torque_fraction");
    const double turbo_time_constant_s = reader.number("engine.turbo.time_constant_s");
    const double boost_onset_rpm = reader.number("engine.turbo.boost_onset_rpm");
    const double full_boost_rpm = reader.number("engine.turbo.full_boost_rpm");
    if (const std::optional<LoadError>& error = reader.error()) {
        return *error;
    }
    if (!torque_curve) {
        return detail::make_error(source, "engine.torque_curve", "invalid");
    }

    const engine::Params params{
        .full_load_torque_nm = *torque_curve,
        .inertia_kg_m2 = inertia_kg_m2,
        .idle_omega_rad_s = rpm_to_rad_s(idle_rpm),
        .rated_omega_rad_s = rpm_to_rad_s(rated_rpm),
        .high_idle_omega_rad_s = rpm_to_rad_s(high_idle_rpm),
        .stall_omega_rad_s = rpm_to_rad_s(stall_rpm),
        .friction_torque_nm = friction_nm,
        .viscous_friction_nm_s_rad = viscous_nm_per_krpm / rpm_to_rad_s(kRpmPerKiloRpm),
        .na_torque_fraction = na_torque_fraction,
        .turbo_time_constant_s = turbo_time_constant_s,
        .boost_onset_omega_rad_s = rpm_to_rad_s(boost_onset_rpm),
        .full_boost_omega_rad_s = rpm_to_rad_s(full_boost_rpm),
    };
    if (const auto error = engine::validate(params)) {
        const FieldError field_error = describe(*error);
        return detail::make_error(source, field_error.field, field_error.reason);
    }
    return params;
}

}  // namespace

Result<engine::Params> parse_engine(std::string_view toml_text, std::string_view source_name) {
    return detail::parse_with<engine::Params>(toml_text, source_name, &read_engine);
}

Result<engine::Params> load_engine(const std::filesystem::path& path) {
    return detail::load_file_with<engine::Params>(path, &read_engine);
}

}  // namespace tp::data
