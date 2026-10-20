#include "godot_files.hpp"

#include <cstddef>
#include <cstring>

#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/variant/char_string.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/string.hpp>

namespace tp::godot_binding {

std::optional<std::string> read_godot_file(const std::string& path) {
    const godot::String godot_path = godot::String::utf8(path.c_str());
    if (!godot::FileAccess::file_exists(godot_path)) {
        return std::nullopt;
    }
    // The bytes as they are: the TOML parser checks the UTF-8 itself.
    const godot::PackedByteArray bytes = godot::FileAccess::get_file_as_bytes(godot_path);
    if (godot::FileAccess::get_open_error() != godot::OK) {
        return std::nullopt;
    }
    std::string text(static_cast<std::size_t>(bytes.size()), '\0');
    if (!text.empty()) {
        std::memcpy(text.data(), bytes.ptr(), text.size());
    }
    return text;
}

}  // namespace tp::godot_binding
