#include "tp/data/garage/career_names.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace tp::data {

void name_career(const garage::Career& career, const GarageDefinition& garage, SavedCareer& out) {
    out.upgrades.clear();
    for (std::size_t i = 0; i < garage.texts.size() && i < garage::kMaxUpgrades; ++i) {
        out.upgrades.push_back({.name = garage.texts[i].name, .level = career.levels.at(i)});
    }
    out.choices.clear();
    for (std::size_t c = 0; c < garage.choice_texts.size() && c < garage::kMaxChoices; ++c) {
        const std::vector<std::string>& labels = garage.choice_texts[c].alternative_labels;
        SavedChoice choice{.name = garage.choice_texts[c].name,
                           .fitted = labels.at(garage::fitted(garage.catalogue, career, c)),
                           .owned = {}};
        // The first comes with the tractor: only those bought are kept.
        for (std::size_t a = 1; a < labels.size(); ++a) {
            if (garage::owns(career, c, a)) {
                choice.owned.push_back(labels[a]);
            }
        }
        out.choices.push_back(std::move(choice));
    }
}

namespace {

// Saved choice `saved` into choice `c` of `career`, which `labels` name.
void read_choice(const SavedChoice& saved, std::size_t c, const std::vector<std::string>& labels,
                 garage::Career& career) {
    for (std::size_t a = 1; a < labels.size() && a < garage::kMaxAlternatives; ++a) {
        if (std::ranges::find(saved.owned, labels[a]) != saved.owned.end()) {
            career.owned.at(c) = static_cast<std::uint8_t>(career.owned.at(c) | (1U << a));
        }
    }
    const auto fitted = std::ranges::find(labels, saved.fitted);
    const auto alternative = static_cast<std::size_t>(fitted - labels.begin());
    if (fitted != labels.end() && garage::owns(career, c, alternative)) {
        career.fitted.at(c) = static_cast<int>(alternative);
    }
}

}  // namespace

garage::Career career_from_names(const SavedCareer& saved, const GarageDefinition& garage) {
    garage::Career career{.money_eur = saved.money_eur, .levels = {}, .fitted = {}, .owned = {}};
    for (const SavedUpgrade& upgrade : saved.upgrades) {
        for (std::size_t i = 0; i < garage.texts.size() && i < garage::kMaxUpgrades; ++i) {
            if (garage.texts[i].name == upgrade.name) {
                career.levels.at(i) = upgrade.level;
            }
        }
    }
    for (const SavedChoice& choice : saved.choices) {
        for (std::size_t c = 0; c < garage.choice_texts.size() && c < garage::kMaxChoices; ++c) {
            if (garage.choice_texts[c].name == choice.name) {
                read_choice(choice, c, garage.choice_texts[c].alternative_labels, career);
            }
        }
    }
    return career;
}

garage::Career carry_career(const garage::Career& career, const GarageDefinition& from,
                            const GarageDefinition& to) {
    SavedCareer named;
    named.money_eur = career.money_eur;
    name_career(career, from, named);
    return career_from_names(named, to);
}

}  // namespace tp::data
