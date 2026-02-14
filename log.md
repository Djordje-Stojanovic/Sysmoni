# Project Log

## 2026-02-14 - Ship Readiness Snapshot

What we are building:
- Aura (Sysmoni): a native C++ desktop system monitor (Windows-first, Qt6/QML/OpenGL).

Architecture:
- 4-module parallel ownership: sensor, render, shell, platform.
- All modules are now 100% native C++ with CMake/CTest verification.
- Build: CMake + Visual Studio 2022 generator on Windows.

What is implemented:
- Telemetry engine: CPU, memory, disk, network collectors with graceful degradation (native C++).
- Render pipeline: C ABI Qt renderer hooks, style-token computation, frame-disciplined callback pipeline (native C++).
- Shell cockpit: dock-state-driven tabbed panel composition, telemetry/render bridges, cockpit controller (native C++ with Qt6).
- Platform runtime: CLI with JSON output, atomic SQLite store, TOML config, LTTB DVR downsampling, one-command launchers (native C++).
- Native test suites for all 4 modules via CTest.
- One-command launchers: `.\aura.cmd --json --no-persist` (CLI), `.\aura.cmd --gui` (GUI).

Current verified state:
- All native modules build and pass CTest on Windows with VS 2022.
- CLI smoke: `.\aura.cmd --json --no-persist` produces telemetry JSON output.
- GUI smoke: `.\aura.cmd --gui` launches native shell (requires Qt6 install).

Readiness estimate:
- Native CLI alpha: ~85-90%
- Native GUI alpha: ~60-70% (Qt6 cockpit wired but needs visual polish)
- Full product ship: ~40-50%

Remaining for ship:
- CI/CD pipeline (.github workflows)
- Installer/packaging (MSI or MSIX)
- Visual polish and theming
- User settings persistence UI
- Auto-update mechanism
- Licensing/activation surface

## 2026-02-12 23:01:19 +01:00 - Ship Readiness Snapshot (Historical)

Archived: project was at ~10-15% ship readiness with Python-first prototype.
The Python stack has since been fully replaced by native C++.
