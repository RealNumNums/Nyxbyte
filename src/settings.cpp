#include "nyxbyte/settings.hpp"

#include <windows.h>

#include <array>
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <string>
#include <system_error>

namespace {

constexpr wchar_t kSection[] = L"Nyxbyte";
constexpr int kCoordinateLimit = 100000;

std::optional<int> read_integer(
    const std::filesystem::path& path,
    const wchar_t* key) {
    std::array<wchar_t, 64> buffer{};
    const DWORD length = GetPrivateProfileStringW(
        kSection,
        key,
        L"",
        buffer.data(),
        static_cast<DWORD>(buffer.size()),
        path.c_str());
    if (length == 0) {
        return std::nullopt;
    }

    wchar_t* end = nullptr;
    errno = 0;
    const long value = std::wcstol(buffer.data(), &end, 10);
    if (errno == ERANGE
        || end == buffer.data()
        || *end != L'\0'
        || value < INT_MIN
        || value > INT_MAX) {
        return std::nullopt;
    }
    return static_cast<int>(value);
}

bool read_boolean(
    const std::filesystem::path& path,
    const wchar_t* key,
    const bool fallback) {
    const auto value = read_integer(path, key);
    if (!value.has_value() || (*value != 0 && *value != 1)) {
        return fallback;
    }
    return *value == 1;
}

bool write_value(
    const std::filesystem::path& path,
    const wchar_t* key,
    const std::wstring& value) {
    return WritePrivateProfileStringW(
               kSection,
               key,
               value.c_str(),
               path.c_str())
        != FALSE;
}

} // namespace

namespace nyxbyte {

Settings sanitize_settings(Settings settings) noexcept {
    const Settings defaults;
    if (!is_supported_scale(settings.scale_percent)) {
        settings.scale_percent = defaults.scale_percent;
    }

    if (settings.position.has_value()) {
        const auto position = *settings.position;
        if (position.x < -kCoordinateLimit
            || position.x > kCoordinateLimit
            || position.y < -kCoordinateLimit
            || position.y > kCoordinateLimit) {
            settings.position.reset();
        }
    }
    return settings;
}

std::filesystem::path default_settings_path() {
    std::wstring override_path(32768, L'\0');
    const DWORD override_length = GetEnvironmentVariableW(
        L"NYXBYTE_CONFIG_PATH",
        override_path.data(),
        static_cast<DWORD>(override_path.size()));
    if (override_length > 0 && override_length < override_path.size()) {
        override_path.resize(override_length);
        return std::filesystem::path{override_path};
    }

    std::wstring local_app_data(32768, L'\0');
    const DWORD local_length = GetEnvironmentVariableW(
        L"LOCALAPPDATA",
        local_app_data.data(),
        static_cast<DWORD>(local_app_data.size()));
    if (local_length > 0 && local_length < local_app_data.size()) {
        local_app_data.resize(local_length);
        return std::filesystem::path{local_app_data}
            / L"Nyxbyte"
            / L"config.ini";
    }

    std::error_code error;
    const auto temporary = std::filesystem::temp_directory_path(error);
    return (error ? std::filesystem::current_path() : temporary)
        / L"Nyxbyte"
        / L"config.ini";
}

Settings load_settings(const std::filesystem::path& path) {
    const Settings defaults;
    Settings loaded{
        .scale_percent =
            read_integer(path, L"scale_percent").value_or(defaults.scale_percent),
        .roaming_enabled =
            read_boolean(path, L"roaming_enabled", defaults.roaming_enabled),
        .always_on_top =
            read_boolean(path, L"always_on_top", defaults.always_on_top),
        .click_through =
            read_boolean(path, L"click_through", defaults.click_through),
    };

    const auto position_x = read_integer(path, L"position_x");
    const auto position_y = read_integer(path, L"position_y");
    if (position_x.has_value() && position_y.has_value()) {
        loaded.position = SavedPosition{*position_x, *position_y};
    }
    return sanitize_settings(loaded);
}

bool save_settings(
    const std::filesystem::path& path,
    const Settings& settings) {
    const Settings safe = sanitize_settings(settings);
    std::error_code error;
    if (!path.parent_path().empty()) {
        std::filesystem::create_directories(path.parent_path(), error);
        if (error) {
            return false;
        }
    }

    bool saved = true;
    saved &= write_value(path, L"version", L"1");
    saved &= write_value(
        path,
        L"scale_percent",
        std::to_wstring(safe.scale_percent));
    saved &= write_value(
        path,
        L"roaming_enabled",
        safe.roaming_enabled ? L"1" : L"0");
    saved &= write_value(
        path,
        L"always_on_top",
        safe.always_on_top ? L"1" : L"0");
    saved &= write_value(
        path,
        L"click_through",
        safe.click_through ? L"1" : L"0");

    if (safe.position.has_value()) {
        saved &= write_value(
            path,
            L"position_x",
            std::to_wstring(safe.position->x));
        saved &= write_value(
            path,
            L"position_y",
            std::to_wstring(safe.position->y));
    } else {
        saved &= WritePrivateProfileStringW(
                     kSection,
                     L"position_x",
                     nullptr,
                     path.c_str())
            != FALSE;
        saved &= WritePrivateProfileStringW(
                     kSection,
                     L"position_y",
                     nullptr,
                     path.c_str())
            != FALSE;
    }

    WritePrivateProfileStringW(nullptr, nullptr, nullptr, path.c_str());
    return saved;
}

} // namespace nyxbyte
