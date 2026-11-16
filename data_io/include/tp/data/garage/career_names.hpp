#pragma once

#include "tp/core/garage/garage.hpp"
#include "tp/data/garage/garage_loader.hpp"
#include "tp/data/season/season_save.hpp"

namespace tp::data {

/// The career's upgrade levels, choices, model and paint by the garage's
/// names for them,
/// as a save keeps them (money and the career's ladder are left as they
/// are in `out`).
void name_career(const garage::Career& career, const GarageDefinition& garage, SavedCareer& out);

/// A career from `saved`'s money, upgrades, choices, model and paint,
/// matched by name to `garage`: an upgrade, choice or alternative the
/// garage does not offer is left behind, one it offers that was not saved
/// starts at its first level or alternative, and a fitted alternative not
/// owned gives way to the first. A level past its top is held to it
/// (garage::equipped). A model the dealer does not sell is not bought (and
/// a paint it does not offer is its first); a save from before the
/// dealers (no SavedLook) has the dealer's first model.
[[nodiscard]] garage::Career career_from_names(const SavedCareer& saved,
                                               const GarageDefinition& garage);

/// `career`, bought in garage `from`, carried into garage `to` by name (as
/// a save is read): a new class's tractor keeps what fits it (its model
/// too, if the dealer sells one by that name).
[[nodiscard]] garage::Career carry_career(const garage::Career& career,
                                          const GarageDefinition& from, const GarageDefinition& to);

}  // namespace tp::data
