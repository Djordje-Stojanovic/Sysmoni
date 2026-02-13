from __future__ import annotations

import importlib
import pathlib
import runpy
import sys
import tomllib
import unittest


PROJECT_ROOT = pathlib.Path(__file__).resolve().parents[2]
SRC_ROOT = PROJECT_ROOT / "src"
if str(SRC_ROOT) not in sys.path:
    sys.path.insert(0, str(SRC_ROOT))


def _load_pyproject() -> dict[str, object]:
    raw = (PROJECT_ROOT / "pyproject.toml").read_bytes()
    return tomllib.loads(raw.decode("utf-8"))


def _resolve_target(target: str) -> object:
    module_name, attribute_name = target.split(":", maxsplit=1)
    module = importlib.import_module(module_name)
    return getattr(module, attribute_name)


class PackagingSurfaceTests(unittest.TestCase):
    def test_uv_package_mode_is_enabled(self) -> None:
        pyproject = _load_pyproject()
        tool = pyproject["tool"]
        uv_cfg = tool["uv"]  # type: ignore[index]
        self.assertTrue(uv_cfg["package"])  # type: ignore[index]

    def test_pyproject_declares_cli_and_gui_entrypoints(self) -> None:
        pyproject = _load_pyproject()
        project = pyproject["project"]
        scripts = project["scripts"]  # type: ignore[index]
        gui_scripts = project["gui-scripts"]  # type: ignore[index]

        self.assertEqual(scripts["aura"], "runtime.main:main")  # type: ignore[index]
        self.assertEqual(gui_scripts["aura-gui"], "runtime.app:launch_gui")  # type: ignore[index]

    def test_entrypoint_targets_are_importable_callables(self) -> None:
        pyproject = _load_pyproject()
        project = pyproject["project"]
        scripts = project["scripts"]  # type: ignore[index]
        gui_scripts = project["gui-scripts"]  # type: ignore[index]

        aura_target = _resolve_target(scripts["aura"])  # type: ignore[index]
        gui_target = _resolve_target(gui_scripts["aura-gui"])  # type: ignore[index]

        self.assertTrue(callable(aura_target))
        self.assertTrue(callable(gui_target))

    def test_src_main_wrapper_dispatches_runtime_main(self) -> None:
        runtime_main = importlib.import_module("runtime.main")
        original_main = runtime_main.main
        original_argv = sys.argv
        seen: dict[str, object] = {}

        def _stub_main(argv: list[str] | None = None) -> int:
            seen["argv"] = argv
            return 17

        runtime_main.main = _stub_main
        sys.argv = [str(SRC_ROOT / "main.py")]
        try:
            with self.assertRaises(SystemExit) as ctx:
                runpy.run_path(str(SRC_ROOT / "main.py"), run_name="__main__")
        finally:
            runtime_main.main = original_main
            sys.argv = original_argv

        self.assertEqual(ctx.exception.code, 17)
        self.assertIsNone(seen["argv"])


if __name__ == "__main__":
    unittest.main()
