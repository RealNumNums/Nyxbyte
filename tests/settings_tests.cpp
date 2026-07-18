#include "nyxbyte/settings.hpp"

#include <windows.h>

#include <filesystem>
#include <iostream>

namespace {

bool expect(const bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

} // namespace

int main() {
    bool ok = true;

    nyxbyte::Settings invalid;
    invalid.scale_percent = 999;
    invalid.position = nyxbyte::SavedPosition{200000, 40};
    const auto sanitized = nyxbyte::sanitize_settings(invalid);
    ok &= expect(sanitized.scale_percent == 100, "invalid scale did not reset");
    ok &= expect(!sanitized.position.has_value(), "invalid position did not reset");

    const auto test_directory =
        std::filesystem::temp_directory_path()
        / ("NyxbyteSettingsTests-" + std::to_string(GetCurrentProcessId()));
    const auto test_path = test_directory / "config.ini";

    nyxbyte::Settings expected{
        .scale_percent = 125,
        .roaming_enabled = false,
        .always_on_top = false,
        .click_through = true,
        .position = nyxbyte::SavedPosition{-744, 876},
    };

    ok &= expect(
        nyxbyte::save_settings(test_path, expected),
        "settings file could not be saved");
    ok &= expect(
        nyxbyte::load_settings(test_path) == expected,
        "settings file did not round-trip");

    WritePrivateProfileStringW(
        L"Nyxbyte",
        L"scale_percent",
        L"not-a-number",
        test_path.c_str());
    WritePrivateProfileStringW(
        L"Nyxbyte",
        L"roaming_enabled",
        L"7",
        test_path.c_str());
    const auto recovered = nyxbyte::load_settings(test_path);
    ok &= expect(recovered.scale_percent == 100, "bad scale was not recovered");
    ok &= expect(recovered.roaming_enabled, "bad boolean was not recovered");

    std::error_code error;
    std::filesystem::remove_all(test_directory, error);
    return ok ? 0 : 1;
}
