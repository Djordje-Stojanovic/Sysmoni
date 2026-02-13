from __future__ import annotations

import pathlib
import unittest


PROJECT_ROOT = pathlib.Path(__file__).resolve().parents[2]
SHELL_ROOT = PROJECT_ROOT / "src" / "shell"
NATIVE_ROOT = SHELL_ROOT / "native"


class NativeShellScaffoldTests(unittest.TestCase):
    def test_shell_module_is_cpp_only(self) -> None:
        python_files = sorted(
            path.relative_to(PROJECT_ROOT).as_posix()
            for path in SHELL_ROOT.rglob("*.py")
        )
        self.assertEqual(
            python_files,
            [],
            msg=f"shell module must remain native-only, found: {python_files}",
        )

    def test_native_tree_contains_required_files(self) -> None:
        required = [
            NATIVE_ROOT / "CMakeLists.txt",
            NATIVE_ROOT / "README.md",
            NATIVE_ROOT / "qml" / "CockpitScene.qml",
            NATIVE_ROOT / "src" / "main.cpp",
            NATIVE_ROOT / "src" / "dock_model.cpp",
            NATIVE_ROOT / "include" / "aura_shell" / "dock_model.hpp",
            NATIVE_ROOT / "tests" / "dock_model_tests.cpp",
        ]
        missing = [path.relative_to(PROJECT_ROOT).as_posix() for path in required if not path.exists()]
        self.assertEqual(missing, [])

    def test_cmake_declares_qt_hybrid_targets(self) -> None:
        content = (NATIVE_ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
        self.assertIn("find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets QuickWidgets Qml)", content)
        self.assertIn("add_executable(aura_shell", content)
        self.assertIn("AURA_SHELL_QML_PATH", content)
        self.assertIn("add_executable(aura_shell_dock_model_tests", content)

    def test_dock_model_api_matches_shell_needs(self) -> None:
        header = (NATIVE_ROOT / "include" / "aura_shell" / "dock_model.hpp").read_text(
            encoding="utf-8"
        )
        impl = (NATIVE_ROOT / "src" / "dock_model.cpp").read_text(encoding="utf-8")

        for panel in (
            "TelemetryOverview",
            "TopProcesses",
            "DvrTimeline",
            "RenderSurface",
        ):
            self.assertIn(panel, header)

        for symbol in (
            "build_default_dock_state",
            "move_panel",
            "set_active_tab",
            "active_panel",
        ):
            self.assertIn(symbol, header)
            self.assertIn(symbol, impl)

    def test_qml_scene_has_accent_animation(self) -> None:
        qml = (NATIVE_ROOT / "qml" / "CockpitScene.qml").read_text(encoding="utf-8")
        self.assertIn("SequentialAnimation on accentIntensity", qml)
        self.assertIn("Widgets + QML bridge active", qml)


if __name__ == "__main__":
    unittest.main()

