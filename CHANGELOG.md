# Changelog

All notable changes to Nyxbyte are documented here.

## 0.2.0 - 2026-07-18

### Added

- Automatic persistence for scale, position, roaming, always-on-top, and
  click-through preferences.
- Readable `%LOCALAPPDATA%\Nyxbyte\config.ini` configuration file.
- **Open config folder** command in the pet and tray menus.
- Settings validation, corruption recovery, and automated round-trip tests.

## 0.1.1 - 2026-07-18

### Fixed

- Dragging now follows the monitor under the cursor instead of clamping Nyxbyte
  to the monitor where the drag began.
- Left-side and stacked monitors with negative desktop coordinates now work.
- Mixed landscape and portrait monitor layouts keep Nyxbyte inside the target
  monitor's usable work area.

### Added

- Automated monitor-layout regression tests in local and GitHub Actions builds.

## 0.1.0 - 2026-07-18

### Added

- Native transparent Win32 desktop overlay
- Animated idle, roam, wave, jump, focus, and review states
- Drag positioning, scaling, always-on-top, and click-through controls
- System tray menu and `Ctrl+Alt+N` click-through shortcut
- Offline ambient behavior through the replaceable `ICompanionBrain` interface
- Public Windows x64 release package
