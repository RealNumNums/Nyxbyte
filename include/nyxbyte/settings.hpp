#pragma once

#include <filesystem>
#include <optional>

namespace nyxbyte {

struct SavedPosition {
    int x{};
    int y{};

    bool operator==(const SavedPosition&) const = default;
};

struct Settings {
    int scale_percent{100};
    bool roaming_enabled{true};
    bool always_on_top{true};
    bool click_through{false};
    std::optional<SavedPosition> position;

    bool operator==(const Settings&) const = default;
};

constexpr bool is_supported_scale(const int percent) noexcept {
    return percent == 75 || percent == 100 || percent == 125;
}

Settings sanitize_settings(Settings settings) noexcept;
std::filesystem::path default_settings_path();
Settings load_settings(const std::filesystem::path& path);
bool save_settings(const std::filesystem::path& path, const Settings& settings);

} // namespace nyxbyte
