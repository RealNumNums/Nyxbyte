#include <windows.h>
#include <windowsx.h>
#include <objidl.h>
#include <shellapi.h>
#include <gdiplus.h>

#include "nyxbyte/brain.hpp"
#include "resource.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>

namespace {

using ClockMs = std::chrono::milliseconds;
using Gdiplus::Color;
using Gdiplus::Graphics;
using Gdiplus::Image;
using Gdiplus::ImageAttributes;
using Gdiplus::Pen;
using Gdiplus::Rect;

constexpr wchar_t kWindowClass[] = L"NyxbyteDesktopCompanion";
constexpr wchar_t kWindowTitle[] = L"Nyxbyte";
constexpr UINT_PTR kAnimationTimer = 1;
constexpr UINT kAnimationTickMs = 16;
constexpr int kCellWidth = 192;
constexpr int kCellHeight = 208;
constexpr int kAtlasColumns = 8;
constexpr UINT kTrayMessage = WM_APP + 1;
constexpr UINT kHotkeyClickThrough = 0x4E59;

enum class Animation {
    idle,
    run_right,
    run_left,
    wave,
    jump,
    failed,
    waiting,
    working,
    review,
};

struct AnimationSpec {
    int row{};
    int frames{};
    std::array<int, 8> durations{};
};

constexpr AnimationSpec make_spec(
    const int row,
    const int frames,
    const std::array<int, 8>& durations) {
    return {row, frames, durations};
}

constexpr auto kAnimationSpecs = std::array{
    make_spec(0, 6, {280, 110, 110, 140, 140, 320, 0, 0}),
    make_spec(1, 8, {120, 120, 120, 120, 120, 120, 120, 220}),
    make_spec(2, 8, {120, 120, 120, 120, 120, 120, 120, 220}),
    make_spec(3, 4, {140, 140, 140, 280, 0, 0, 0, 0}),
    make_spec(4, 5, {140, 140, 140, 140, 280, 0, 0, 0}),
    make_spec(5, 8, {140, 140, 140, 140, 140, 140, 140, 240}),
    make_spec(6, 6, {150, 150, 150, 150, 150, 260, 0, 0}),
    make_spec(7, 6, {120, 120, 120, 120, 120, 220, 0, 0}),
    make_spec(8, 6, {150, 150, 150, 150, 150, 280, 0, 0}),
};

constexpr const AnimationSpec& spec(const Animation animation) {
    return kAnimationSpecs.at(static_cast<std::size_t>(animation));
}

enum MenuCommand : UINT {
    menu_wave = 1001,
    menu_jump,
    menu_focus,
    menu_review,
    menu_roaming,
    menu_always_on_top,
    menu_click_through,
    menu_scale_75,
    menu_scale_100,
    menu_scale_125,
    menu_about,
    menu_exit,
};

std::filesystem::path executable_directory() {
    std::wstring buffer(32768, L'\0');
    const DWORD length = GetModuleFileNameW(
        nullptr,
        buffer.data(),
        static_cast<DWORD>(buffer.size()));
    buffer.resize(length);
    return std::filesystem::path{buffer}.parent_path();
}

ClockMs monotonic_now() {
    return ClockMs{static_cast<ClockMs::rep>(GetTickCount64())};
}

class CompanionWindow {
public:
    explicit CompanionWindow(HINSTANCE instance)
        : instance_(instance) {
    }

    ~CompanionWindow() {
        remove_tray_icon();
        if (hotkey_registered_) {
            UnregisterHotKey(window_, kHotkeyClickThrough);
        }
        atlas_.reset();
        if (gdiplus_token_ != 0) {
            Gdiplus::GdiplusShutdown(gdiplus_token_);
            gdiplus_token_ = 0;
        }
    }

    bool create() {
        Gdiplus::GdiplusStartupInput input;
        if (Gdiplus::GdiplusStartup(&gdiplus_token_, &input, nullptr) != Gdiplus::Ok) {
            MessageBoxW(nullptr, L"Nyxbyte could not start GDI+.", kWindowTitle, MB_ICONERROR);
            return false;
        }

        const auto atlas_path = executable_directory() / L"assets" / L"spritesheet.png";
        atlas_ = std::make_unique<Image>(atlas_path.c_str());
        if (atlas_->GetLastStatus() != Gdiplus::Ok
            || atlas_->GetWidth() != kCellWidth * kAtlasColumns
            || atlas_->GetHeight() != kCellHeight * 11) {
            const std::wstring message =
                L"Nyxbyte could not load its v2 sprite atlas:\n\n" + atlas_path.wstring()
                + L"\n\nKeep the assets folder beside Nyxbyte.exe.";
            MessageBoxW(nullptr, message.c_str(), kWindowTitle, MB_ICONERROR);
            return false;
        }

        WNDCLASSEXW wc{sizeof(wc)};
        wc.style = CS_DBLCLKS;
        wc.lpfnWndProc = &CompanionWindow::window_proc;
        wc.hInstance = instance_;
        wc.hCursor = LoadCursorW(nullptr, IDC_HAND);
        wc.hIcon = static_cast<HICON>(LoadImageW(
            instance_,
            MAKEINTRESOURCEW(IDI_NYXBYTE),
            IMAGE_ICON,
            0,
            0,
            LR_DEFAULTSIZE));
        wc.lpszClassName = kWindowClass;
        if (RegisterClassExW(&wc) == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            return false;
        }

        SIZE size = scaled_size();
        const RECT work = primary_work_area();
        const int x = work.right - size.cx - 48;
        const int y = work.bottom - size.cy - 18;
        window_ = CreateWindowExW(
            extended_style(),
            kWindowClass,
            kWindowTitle,
            WS_POPUP,
            x,
            y,
            size.cx,
            size.cy,
            nullptr,
            nullptr,
            instance_,
            this);

        if (window_ == nullptr) {
            return false;
        }

        position_x_ = static_cast<double>(x);
        position_y_ = static_cast<double>(y);
        last_tick_ = monotonic_now();
        frame_started_ = last_tick_;
        brain_.reset(last_tick_);
        start_animation(Animation::wave, true);

        SetTimer(window_, kAnimationTimer, kAnimationTickMs, nullptr);
        hotkey_registered_ =
            RegisterHotKey(window_, kHotkeyClickThrough, MOD_CONTROL | MOD_ALT, 'N') != FALSE;
        add_tray_icon();
        ShowWindow(window_, SW_SHOWNOACTIVATE);
        render();
        return true;
    }

    HWND handle() const {
        return window_;
    }

private:
    static LRESULT CALLBACK window_proc(
        HWND hwnd,
        UINT message,
        WPARAM wparam,
        LPARAM lparam) {
        CompanionWindow* self = nullptr;
        if (message == WM_NCCREATE) {
            const auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
            self = static_cast<CompanionWindow*>(create->lpCreateParams);
            self->window_ = hwnd;
            SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        } else {
            self = reinterpret_cast<CompanionWindow*>(
                GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        }

        if (self != nullptr) {
            return self->handle_message(message, wparam, lparam);
        }
        return DefWindowProcW(hwnd, message, wparam, lparam);
    }

    LRESULT handle_message(UINT message, WPARAM wparam, LPARAM lparam) {
        switch (message) {
        case WM_TIMER:
            if (wparam == kAnimationTimer) {
                tick();
                return 0;
            }
            break;
        case WM_LBUTTONDOWN:
            begin_drag(GET_X_LPARAM(lparam), GET_Y_LPARAM(lparam));
            return 0;
        case WM_MOUSEMOVE:
            if (dragging_) {
                update_drag();
                return 0;
            }
            break;
        case WM_LBUTTONUP:
            end_drag();
            return 0;
        case WM_LBUTTONDBLCLK:
            roaming_enabled_ = !roaming_enabled_;
            brain_.reset(monotonic_now());
            show_balloon(
                roaming_enabled_ ? L"Roaming enabled" : L"Roaming paused");
            return 0;
        case WM_RBUTTONUP: {
            POINT point{};
            GetCursorPos(&point);
            show_context_menu(point);
            return 0;
        }
        case WM_COMMAND:
            handle_command(LOWORD(wparam));
            return 0;
        case WM_HOTKEY:
            if (wparam == kHotkeyClickThrough) {
                set_click_through(!click_through_);
                return 0;
            }
            break;
        case kTrayMessage:
            if (LOWORD(lparam) == WM_RBUTTONUP || LOWORD(lparam) == WM_CONTEXTMENU) {
                POINT point{};
                GetCursorPos(&point);
                show_context_menu(point);
                return 0;
            }
            if (LOWORD(lparam) == WM_LBUTTONUP) {
                start_animation(Animation::wave, true);
                return 0;
            }
            break;
        case WM_DISPLAYCHANGE:
        case WM_SETTINGCHANGE:
            clamp_to_work_area();
            return 0;
        case WM_DESTROY:
            remove_tray_icon();
            PostQuitMessage(0);
            return 0;
        default:
            break;
        }
        return DefWindowProcW(window_, message, wparam, lparam);
    }

    DWORD extended_style() const {
        DWORD style = WS_EX_LAYERED | WS_EX_TOOLWINDOW;
        if (always_on_top_) {
            style |= WS_EX_TOPMOST;
        }
        if (click_through_) {
            style |= WS_EX_TRANSPARENT;
        }
        return style;
    }

    SIZE scaled_size() const {
        return {
            MulDiv(kCellWidth, scale_percent_, 100),
            MulDiv(kCellHeight, scale_percent_, 100),
        };
    }

    RECT primary_work_area() const {
        RECT work{};
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
        return work;
    }

    RECT work_area_for_window() const {
        MONITORINFO monitor_info{sizeof(monitor_info)};
        const HMONITOR monitor = MonitorFromWindow(window_, MONITOR_DEFAULTTONEAREST);
        if (GetMonitorInfoW(monitor, &monitor_info)) {
            return monitor_info.rcWork;
        }
        return primary_work_area();
    }

    void tick() {
        const ClockMs now = monotonic_now();
        const auto elapsed = std::max<ClockMs>(ClockMs{0}, now - last_tick_);
        last_tick_ = now;

        advance_animation(now);
        advance_roaming(elapsed, now);

        const RECT work = work_area_for_window();
        const SIZE size = scaled_size();
        const nyxbyte::BrainContext context{
            .now = now,
            .roaming_enabled = roaming_enabled_,
            .animation_busy = one_shot_ || roam_until_.has_value(),
            .at_left_edge = position_x_ <= work.left + 1,
            .at_right_edge = position_x_ + size.cx >= work.right - 1,
        };
        apply_decision(brain_.think(context), now);
        render();
    }

    void advance_animation(const ClockMs now) {
        const AnimationSpec& current = spec(animation_);
        const int duration = current.durations.at(static_cast<std::size_t>(frame_));
        if (now - frame_started_ < ClockMs{duration}) {
            return;
        }

        frame_started_ = now;
        ++frame_;
        if (frame_ < current.frames) {
            return;
        }

        if (one_shot_) {
            start_animation(Animation::idle, false);
        } else {
            frame_ = 0;
        }
    }

    void advance_roaming(const ClockMs elapsed, const ClockMs now) {
        if (!roam_until_.has_value()) {
            return;
        }

        if (now >= *roam_until_ || !roaming_enabled_) {
            roam_until_.reset();
            start_animation(Animation::idle, false);
            brain_.reset(now);
            return;
        }

        const double direction = animation_ == Animation::run_right ? 1.0 : -1.0;
        const double distance =
            direction * 72.0 * static_cast<double>(elapsed.count()) / 1000.0;
        position_x_ += distance;

        const RECT work = work_area_for_window();
        const SIZE size = scaled_size();
        const double min_x = static_cast<double>(work.left);
        const double max_x = static_cast<double>(work.right - size.cx);
        const double clamped = std::clamp(position_x_, min_x, max_x);
        const bool hit_edge = std::abs(clamped - position_x_) > 0.01;
        position_x_ = clamped;

        SetWindowPos(
            window_,
            nullptr,
            static_cast<int>(std::lround(position_x_)),
            static_cast<int>(std::lround(position_y_)),
            0,
            0,
            SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);

        if (hit_edge) {
            roam_until_.reset();
            start_animation(Animation::idle, false);
            brain_.reset(now);
        }
    }

    void apply_decision(
        const nyxbyte::BrainDecision& decision,
        const ClockMs now) {
        using nyxbyte::Action;
        switch (decision.action) {
        case Action::none:
            break;
        case Action::idle:
            start_animation(Animation::idle, false);
            break;
        case Action::roam_left:
            start_animation(Animation::run_left, false);
            roam_until_ = now + decision.duration;
            break;
        case Action::roam_right:
            start_animation(Animation::run_right, false);
            roam_until_ = now + decision.duration;
            break;
        case Action::wave:
            start_animation(Animation::wave, true);
            break;
        case Action::jump:
            start_animation(Animation::jump, true);
            break;
        case Action::focus:
            start_animation(Animation::working, true);
            break;
        case Action::review:
            start_animation(Animation::review, true);
            break;
        }
    }

    void start_animation(const Animation animation, const bool one_shot) {
        animation_ = animation;
        one_shot_ = one_shot;
        frame_ = 0;
        frame_started_ = monotonic_now();
    }

    void render() {
        if (window_ == nullptr || atlas_ == nullptr) {
            return;
        }

        SIZE size = scaled_size();
        HDC screen = GetDC(nullptr);
        HDC memory = CreateCompatibleDC(screen);

        BITMAPINFO bitmap_info{};
        bitmap_info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bitmap_info.bmiHeader.biWidth = size.cx;
        bitmap_info.bmiHeader.biHeight = -size.cy;
        bitmap_info.bmiHeader.biPlanes = 1;
        bitmap_info.bmiHeader.biBitCount = 32;
        bitmap_info.bmiHeader.biCompression = BI_RGB;

        void* bits = nullptr;
        HBITMAP bitmap =
            CreateDIBSection(screen, &bitmap_info, DIB_RGB_COLORS, &bits, nullptr, 0);
        HGDIOBJ old_bitmap = SelectObject(memory, bitmap);

        {
            Graphics graphics(memory);
            graphics.Clear(Color(0, 0, 0, 0));
            graphics.SetCompositingMode(Gdiplus::CompositingModeSourceOver);
            graphics.SetCompositingQuality(Gdiplus::CompositingQualityHighQuality);
            graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);
            graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHighQuality);
            graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);

            const double pulse =
                (std::sin(static_cast<double>(GetTickCount64()) / 330.0) + 1.0) * 0.5;
            const int ring_alpha = static_cast<int>(48.0 + pulse * 54.0);
            const int ring_x = static_cast<int>(size.cx * 22 / 100);
            const int ring_y = static_cast<int>(size.cy * 84 / 100);
            const int ring_width = static_cast<int>(size.cx * 56 / 100);
            const int ring_height =
                std::max(8, static_cast<int>(size.cy * 9 / 100));
            const Rect ring{ring_x, ring_y, ring_width, ring_height};
            Pen outer(
                Color(static_cast<BYTE>(ring_alpha / 2), 45, 220, 255),
                std::max(2.0F, static_cast<float>(size.cx) / 70.0F));
            Pen inner(
                Color(static_cast<BYTE>(ring_alpha), 105, 245, 255),
                std::max(1.0F, static_cast<float>(size.cx) / 115.0F));
            graphics.DrawEllipse(&outer, ring);
            Rect inner_ring = ring;
            inner_ring.Inflate(
                -std::max(2, static_cast<int>(size.cx / 48)),
                -std::max(1, static_cast<int>(size.cy / 80)));
            graphics.DrawEllipse(&inner, inner_ring);

            const AnimationSpec& current = spec(animation_);
            const int source_x = frame_ * kCellWidth;
            const int source_y = current.row * kCellHeight;
            graphics.DrawImage(
                atlas_.get(),
                Rect{0, 0, size.cx, size.cy},
                source_x,
                source_y,
                kCellWidth,
                kCellHeight,
                Gdiplus::UnitPixel);
        }

        POINT destination{
            static_cast<LONG>(std::lround(position_x_)),
            static_cast<LONG>(std::lround(position_y_)),
        };
        POINT source_point{0, 0};
        BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
        UpdateLayeredWindow(
            window_,
            screen,
            &destination,
            &size,
            memory,
            &source_point,
            0,
            &blend,
            ULW_ALPHA);

        SelectObject(memory, old_bitmap);
        DeleteObject(bitmap);
        DeleteDC(memory);
        ReleaseDC(nullptr, screen);
    }

    void begin_drag(const int x, const int y) {
        SetCapture(window_);
        dragging_ = true;
        moved_during_drag_ = false;
        drag_origin_client_ = {x, y};
        POINT cursor{};
        GetCursorPos(&cursor);
        drag_origin_screen_ = cursor;
        drag_window_origin_ = {
            static_cast<LONG>(std::lround(position_x_)),
            static_cast<LONG>(std::lround(position_y_)),
        };
    }

    void update_drag() {
        POINT cursor{};
        GetCursorPos(&cursor);
        const int dx = cursor.x - drag_origin_screen_.x;
        const int dy = cursor.y - drag_origin_screen_.y;
        if (std::abs(dx) + std::abs(dy) > 4) {
            moved_during_drag_ = true;
        }

        position_x_ = static_cast<double>(drag_window_origin_.x + dx);
        position_y_ = static_cast<double>(drag_window_origin_.y + dy);
        clamp_to_work_area();
        SetWindowPos(
            window_,
            nullptr,
            static_cast<int>(std::lround(position_x_)),
            static_cast<int>(std::lround(position_y_)),
            0,
            0,
            SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);
    }

    void end_drag() {
        if (!dragging_) {
            return;
        }
        ReleaseCapture();
        dragging_ = false;
        if (moved_during_drag_) {
            roam_until_.reset();
            roaming_enabled_ = false;
            start_animation(Animation::idle, false);
            brain_.reset(monotonic_now());
        } else {
            start_animation(Animation::wave, true);
        }
    }

    void clamp_to_work_area() {
        if (window_ == nullptr) {
            return;
        }
        const RECT work = work_area_for_window();
        const SIZE size = scaled_size();
        position_x_ = std::clamp(
            position_x_,
            static_cast<double>(work.left),
            static_cast<double>(work.right - size.cx));
        position_y_ = std::clamp(
            position_y_,
            static_cast<double>(work.top),
            static_cast<double>(work.bottom - size.cy));
        SetWindowPos(
            window_,
            nullptr,
            static_cast<int>(std::lround(position_x_)),
            static_cast<int>(std::lround(position_y_)),
            0,
            0,
            SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER);
    }

    void show_context_menu(POINT point) {
        HMENU menu = CreatePopupMenu();
        AppendMenuW(menu, MF_STRING, menu_wave, L"Wave");
        AppendMenuW(menu, MF_STRING, menu_jump, L"Jump");
        AppendMenuW(menu, MF_STRING, menu_focus, L"Focus");
        AppendMenuW(menu, MF_STRING, menu_review, L"Review");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(
            menu,
            MF_STRING | (roaming_enabled_ ? MF_CHECKED : MF_UNCHECKED),
            menu_roaming,
            L"Roaming");
        AppendMenuW(
            menu,
            MF_STRING | (always_on_top_ ? MF_CHECKED : MF_UNCHECKED),
            menu_always_on_top,
            L"Always on top");
        AppendMenuW(
            menu,
            MF_STRING | (click_through_ ? MF_CHECKED : MF_UNCHECKED),
            menu_click_through,
            L"Click-through   Ctrl+Alt+N");

        HMENU scale_menu = CreatePopupMenu();
        AppendMenuW(
            scale_menu,
            MF_STRING | (scale_percent_ == 75 ? MF_CHECKED : MF_UNCHECKED),
            menu_scale_75,
            L"75%");
        AppendMenuW(
            scale_menu,
            MF_STRING | (scale_percent_ == 100 ? MF_CHECKED : MF_UNCHECKED),
            menu_scale_100,
            L"100%");
        AppendMenuW(
            scale_menu,
            MF_STRING | (scale_percent_ == 125 ? MF_CHECKED : MF_UNCHECKED),
            menu_scale_125,
            L"125%");
        AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(scale_menu), L"Scale");
        AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(menu, MF_STRING, menu_about, L"About Nyxbyte");
        AppendMenuW(menu, MF_STRING, menu_exit, L"Exit");

        SetForegroundWindow(window_);
        TrackPopupMenu(
            menu,
            TPM_RIGHTBUTTON | TPM_BOTTOMALIGN | TPM_LEFTALIGN,
            point.x,
            point.y,
            0,
            window_,
            nullptr);
        PostMessageW(window_, WM_NULL, 0, 0);
        DestroyMenu(menu);
    }

    void handle_command(const UINT command) {
        switch (command) {
        case menu_wave:
            start_animation(Animation::wave, true);
            break;
        case menu_jump:
            start_animation(Animation::jump, true);
            break;
        case menu_focus:
            start_animation(Animation::working, true);
            break;
        case menu_review:
            start_animation(Animation::review, true);
            break;
        case menu_roaming:
            roaming_enabled_ = !roaming_enabled_;
            roam_until_.reset();
            start_animation(Animation::idle, false);
            brain_.reset(monotonic_now());
            break;
        case menu_always_on_top:
            always_on_top_ = !always_on_top_;
            SetWindowPos(
                window_,
                always_on_top_ ? HWND_TOPMOST : HWND_NOTOPMOST,
                0,
                0,
                0,
                0,
                SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
            break;
        case menu_click_through:
            set_click_through(!click_through_);
            break;
        case menu_scale_75:
            set_scale(75);
            break;
        case menu_scale_100:
            set_scale(100);
            break;
        case menu_scale_125:
            set_scale(125);
            break;
        case menu_about:
            MessageBoxW(
                window_,
                L"Nyxbyte 0.1\n\nA native C++ desktop companion.\n"
                L"Renderer and behavior brain are separated for future AI features.",
                kWindowTitle,
                MB_OK | MB_ICONINFORMATION);
            break;
        case menu_exit:
            DestroyWindow(window_);
            break;
        default:
            break;
        }
    }

    void set_scale(const int percent) {
        scale_percent_ = percent;
        const SIZE size = scaled_size();
        SetWindowPos(
            window_,
            nullptr,
            0,
            0,
            size.cx,
            size.cy,
            SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOZORDER);
        clamp_to_work_area();
        render();
    }

    void set_click_through(const bool enabled) {
        click_through_ = enabled;
        LONG_PTR style = GetWindowLongPtrW(window_, GWL_EXSTYLE);
        if (click_through_) {
            style |= WS_EX_TRANSPARENT;
        } else {
            style &= ~static_cast<LONG_PTR>(WS_EX_TRANSPARENT);
        }
        SetWindowLongPtrW(window_, GWL_EXSTYLE, style);
        SetWindowPos(
            window_,
            nullptr,
            0,
            0,
            0,
            0,
            SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOZORDER | SWP_FRAMECHANGED);
        show_balloon(
            click_through_
                ? L"Click-through enabled. Press Ctrl+Alt+N or use the tray icon to restore."
                : L"Click-through disabled.");
    }

    void add_tray_icon() {
        tray_data_ = {};
        tray_data_.cbSize = sizeof(tray_data_);
        tray_data_.hWnd = window_;
        tray_data_.uID = 1;
        tray_data_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
        tray_data_.uCallbackMessage = kTrayMessage;
        tray_data_.hIcon = static_cast<HICON>(LoadImageW(
            instance_,
            MAKEINTRESOURCEW(IDI_NYXBYTE),
            IMAGE_ICON,
            0,
            0,
            LR_DEFAULTSIZE));
        wcscpy_s(tray_data_.szTip, L"Nyxbyte desktop companion");
        tray_added_ = Shell_NotifyIconW(NIM_ADD, &tray_data_) != FALSE;
        if (tray_added_) {
            tray_data_.uVersion = NOTIFYICON_VERSION_4;
            Shell_NotifyIconW(NIM_SETVERSION, &tray_data_);
        }
    }

    void remove_tray_icon() {
        if (tray_added_) {
            Shell_NotifyIconW(NIM_DELETE, &tray_data_);
            tray_added_ = false;
        }
    }

    void show_balloon(const wchar_t* message) {
        if (!tray_added_) {
            return;
        }
        NOTIFYICONDATAW balloon = tray_data_;
        balloon.uFlags = NIF_INFO;
        wcscpy_s(balloon.szInfoTitle, L"Nyxbyte");
        wcsncpy_s(balloon.szInfo, message, _TRUNCATE);
        balloon.dwInfoFlags = NIIF_INFO;
        Shell_NotifyIconW(NIM_MODIFY, &balloon);
    }

    HINSTANCE instance_{};
    HWND window_{};
    ULONG_PTR gdiplus_token_{};
    std::unique_ptr<Image> atlas_;
    nyxbyte::AmbientBrain brain_;
    NOTIFYICONDATAW tray_data_{};

    Animation animation_{Animation::idle};
    int frame_{};
    int scale_percent_{100};
    bool one_shot_{};
    bool roaming_enabled_{true};
    bool always_on_top_{true};
    bool click_through_{};
    bool tray_added_{};
    bool hotkey_registered_{};
    bool dragging_{};
    bool moved_during_drag_{};

    double position_x_{};
    double position_y_{};
    POINT drag_origin_client_{};
    POINT drag_origin_screen_{};
    POINT drag_window_origin_{};
    ClockMs last_tick_{};
    ClockMs frame_started_{};
    std::optional<ClockMs> roam_until_;
};

} // namespace

int WINAPI wWinMain(
    HINSTANCE instance,
    HINSTANCE,
    PWSTR,
    int) {
    CompanionWindow app{instance};
    if (!app.create()) {
        return 1;
    }

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
    return static_cast<int>(message.wParam);
}
