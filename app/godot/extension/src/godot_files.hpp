#pragma once

#include <optional>
#include <string>

namespace tp::godot_binding {

/// Reads a data file through Godot's FileAccess, so res:// paths (inside the
/// project, or the exported app package) and user:// work: the
/// tp::data::ReadFile the loaders read through in the game. Nothing if the
/// file does not exist or cannot be read.
[[nodiscard]] std::optional<std::string> read_godot_file(const std::string& path);

}  // namespace tp::godot_binding
