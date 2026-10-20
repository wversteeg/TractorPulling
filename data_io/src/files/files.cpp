#include "tp/data/files.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <vector>

namespace tp::data {

namespace {

constexpr std::string_view kScheme = "://";

bool is_absolute(std::string_view path) {
    return path.starts_with('/') || path.find(kScheme) != std::string_view::npos;
}

// The part of an absolute path ".." cannot climb above: "/" or "res://".
std::string_view root_of(std::string_view path) {
    if (const auto scheme = path.find(kScheme); scheme != std::string_view::npos) {
        return path.substr(0, scheme + kScheme.size());
    }
    return path.starts_with('/') ? path.substr(0, 1) : std::string_view{};
}

// Adds the segments of `path` to `segments`, folding "." and "..".
void append_segments(std::string_view path, bool rooted, std::vector<std::string_view>& segments) {
    while (!path.empty()) {
        const auto slash = path.find('/');
        const std::string_view segment = path.substr(0, slash);
        path = slash == std::string_view::npos ? std::string_view{} : path.substr(slash + 1);
        if (segment.empty() || segment == ".") {
            continue;
        }
        if (segment == "..") {
            if (!segments.empty() && segments.back() != "..") {
                segments.pop_back();
                continue;
            }
            if (rooted) {
                // A rooted path stays at its root; a relative one may lead
                // out of where it starts.
                continue;
            }
        }
        segments.push_back(segment);
    }
}

}  // namespace

std::optional<std::string> read_from_disk(const std::string& path) {
    const std::ifstream file{path, std::ios::binary};
    if (!file) {
        return std::nullopt;
    }
    std::ostringstream text;
    text << file.rdbuf();
    return text.str();
}

std::string directory_of(std::string_view path) {
    const auto slash = path.rfind('/');
    if (slash == std::string_view::npos) {
        return {};
    }
    const std::string_view root = root_of(path);
    return std::string{path.substr(0, std::max(slash, root.size()))};
}

std::string resolve(std::string_view base_directory, std::string_view relative) {
    if (is_absolute(relative)) {
        return std::string{relative};
    }
    const std::string_view root = root_of(base_directory);
    std::vector<std::string_view> segments;
    append_segments(base_directory.substr(root.size()), !root.empty(), segments);
    append_segments(relative, !root.empty(), segments);
    std::string path{root};
    for (std::size_t i = 0; i < segments.size(); ++i) {
        if (i > 0) {
            path += '/';
        }
        path += segments[i];
    }
    return path;
}

}  // namespace tp::data
