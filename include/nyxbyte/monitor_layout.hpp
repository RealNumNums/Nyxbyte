#pragma once

#include <algorithm>

namespace nyxbyte {

struct DesktopPoint {
    double x{};
    double y{};
};

struct DesktopSize {
    double width{};
    double height{};
};

struct DesktopRect {
    double left{};
    double top{};
    double right{};
    double bottom{};
};

constexpr DesktopPoint clamp_window_origin(
    const DesktopPoint desired,
    const DesktopSize window,
    const DesktopRect work_area) noexcept {
    const double max_x = std::max(work_area.left, work_area.right - window.width);
    const double max_y = std::max(work_area.top, work_area.bottom - window.height);
    return {
        std::clamp(desired.x, work_area.left, max_x),
        std::clamp(desired.y, work_area.top, max_y),
    };
}

} // namespace nyxbyte
