#include "nyxbyte/monitor_layout.hpp"

#include <cmath>
#include <iostream>

namespace {

bool near(const double left, const double right) {
    return std::abs(left - right) < 0.001;
}

bool expect_point(
    const char* name,
    const nyxbyte::DesktopPoint actual,
    const double expected_x,
    const double expected_y) {
    if (near(actual.x, expected_x) && near(actual.y, expected_y)) {
        return true;
    }

    std::cerr << name << ": expected (" << expected_x << ", " << expected_y
              << "), got (" << actual.x << ", " << actual.y << ")\n";
    return false;
}

} // namespace

int main() {
    constexpr nyxbyte::DesktopSize pet{192.0, 208.0};
    constexpr nyxbyte::DesktopRect primary{0.0, 0.0, 2560.0, 1392.0};
    constexpr nyxbyte::DesktopRect left_portrait{-1440.0, 0.0, 0.0, 2512.0};
    constexpr nyxbyte::DesktopRect upper{0.0, -1200.0, 1920.0, 0.0};

    bool ok = true;
    ok &= expect_point(
        "primary interior",
        nyxbyte::clamp_window_origin({1200.0, 700.0}, pet, primary),
        1200.0,
        700.0);
    ok &= expect_point(
        "negative monitor interior",
        nyxbyte::clamp_window_origin({-800.0, 900.0}, pet, left_portrait),
        -800.0,
        900.0);
    ok &= expect_point(
        "negative monitor right edge",
        nyxbyte::clamp_window_origin({-40.0, 900.0}, pet, left_portrait),
        -192.0,
        900.0);
    ok &= expect_point(
        "negative monitor left edge",
        nyxbyte::clamp_window_origin({-1600.0, 900.0}, pet, left_portrait),
        -1440.0,
        900.0);
    ok &= expect_point(
        "stacked monitor bottom edge",
        nyxbyte::clamp_window_origin({600.0, -40.0}, pet, upper),
        600.0,
        -208.0);
    ok &= expect_point(
        "oversized window",
        nyxbyte::clamp_window_origin(
            {100.0, 100.0},
            {3000.0, 1800.0},
            primary),
        0.0,
        0.0);

    return ok ? 0 : 1;
}
