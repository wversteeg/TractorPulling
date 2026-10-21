#pragma once

#include "tp/core/controls/controls.hpp"
#include "tp/core/session/session.hpp"
#include "tp/data/files.hpp"
#include "tp/data/result.hpp"

#include <filesystem>
#include <string_view>

namespace tp::data {

/// Loads and validates the easy control layout and its clutch assist (see
/// data/controls/easy.toml).
[[nodiscard]] Result<controls::EasyParams> load_easy_controls(const std::filesystem::path& path);

/// As load_easy_controls, reading through `read` (see files.hpp).
[[nodiscard]] Result<controls::EasyParams> load_easy_controls(std::string_view path,
                                                              const ReadFile& read);

/// As load_easy_controls, from TOML text. `source_name` prefixes error
/// messages.
[[nodiscard]] Result<controls::EasyParams> parse_easy_controls(std::string_view toml_text,
                                                               std::string_view source_name);

/// Loads and validates the pro control layout's steering (see
/// data/controls/pro.toml).
[[nodiscard]] Result<controls::ProParams> load_pro_controls(const std::filesystem::path& path);

/// As load_pro_controls, reading through `read` (see files.hpp).
[[nodiscard]] Result<controls::ProParams> load_pro_controls(std::string_view path,
                                                            const ReadFile& read);

/// As load_pro_controls, from TOML text. `source_name` prefixes error
/// messages.
[[nodiscard]] Result<controls::ProParams> parse_pro_controls(std::string_view toml_text,
                                                             std::string_view source_name);

/// A control layout as a game picks it: a controls file with either an
/// [easy] or a [pro] table, and that layout's parameters (the other's stay
/// at their defaults).
struct ControlLayout {
    session::Layout layout = session::Layout::kPro;
    controls::EasyParams easy;
    controls::ProParams pro;
};

/// Loads easy.toml, pro.toml or any file of either layout, naming the
/// layout by its table.
[[nodiscard]] Result<ControlLayout> load_control_layout(std::string_view path,
                                                        const ReadFile& read);

/// As load_control_layout, from TOML text.
[[nodiscard]] Result<ControlLayout> parse_control_layout(std::string_view toml_text,
                                                         std::string_view source_name);

}  // namespace tp::data
