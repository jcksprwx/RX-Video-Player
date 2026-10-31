#pragma once

#include <string>
#include <filesystem>

namespace rx {

struct PlaylistItem {
    std::string path;
    std::string title;
    double durationSec = 0.0;

    static PlaylistItem fromFile(const std::filesystem::path& p) {
        PlaylistItem item;
        item.path = p.string();
        item.title = p.stem().string();
        return item;
    }
};

} // namespace rx
