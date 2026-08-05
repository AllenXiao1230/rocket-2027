"""Locate shared Ground Station components without depending on the CWD."""

from pathlib import Path
import sys


def _find_repository_root() -> Path:
    for parent in Path(__file__).resolve().parents:
        if (parent / "AGENTS.md").is_file() and (
            parent / "apps" / "ground_station"
        ).is_dir():
            return parent
    raise RuntimeError("cannot locate the Rocket 2027 repository root")


REPOSITORY_ROOT = _find_repository_root()
APP_ROOT = REPOSITORY_ROOT / "apps" / "ground_station"
if str(APP_ROOT) not in sys.path:
    sys.path.insert(0, str(APP_ROOT))
