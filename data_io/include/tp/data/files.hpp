#pragma once

#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace tp::data {

/// Reads a data file: its whole text, or nothing if it cannot be read.
/// Paths are '/'-separated. A front end supplies its own, so the loaders
/// read wherever it keeps its files: Godot's res:// inside the app package,
/// an Android asset, a test's files in memory.
using ReadFile = std::function<std::optional<std::string>(const std::string& path)>;

/// Reads from the file system.
[[nodiscard]] std::optional<std::string> read_from_disk(const std::string& path);

/// The directory the file at `path` is in: everything before its last '/'
/// ("" for a bare file name, "/" for a file at the root).
[[nodiscard]] std::string directory_of(std::string_view path);

/// The path of a file another data file names (`relative`, e.g.
/// "../engines/x.toml") as seen from `base_directory`, the directory that
/// file is in: "res://data/tractors" and "../engines/x.toml" give
/// "res://data/engines/x.toml". "." and ".." are folded in by name, without
/// asking the file system. An absolute path (from '/', or with a scheme such
/// as "res://") is kept as it is.
[[nodiscard]] std::string resolve(std::string_view base_directory, std::string_view relative);

}  // namespace tp::data
