import importlib.util
from pathlib import Path
import sys


def _missing_runtime_deps() -> list[tuple[str, str]]:
    required = [
        ("PyQt5", "PyQt5"),
        ("PyQt5.QtWebEngineWidgets", "PyQtWebEngine"),
        ("serial", "pyserial"),
    ]
    missing = []
    for module_name, pip_name in required:
        try:
            available = importlib.util.find_spec(module_name) is not None
        except ModuleNotFoundError:
            available = False
        if not available:
            missing.append((module_name, pip_name))
    return missing


def main() -> int:
    missing = _missing_runtime_deps()
    if missing:
        names = ", ".join(module_name for module_name, _ in missing)
        requirements = Path(__file__).with_name("requirements.txt")
        install_cmd = f"{sys.executable} -m pip install -r {requirements}"
        print(f"Missing dependencies: {names}. Run: {install_cmd}")
        return 1

    from PyQt5.QtWidgets import QApplication
    try:
        from .gps_drift_gui import GpsDriftWindow
    except ImportError:
        from gps_drift_gui import GpsDriftWindow

    app = QApplication(sys.argv)
    win = GpsDriftWindow()
    win.show()
    return app.exec()


if __name__ == "__main__":
    sys.exit(main())
