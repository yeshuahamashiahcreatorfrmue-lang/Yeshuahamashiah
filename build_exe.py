"""Build a standalone executable with PyInstaller.

Usage::

    pip install -r requirements.txt pyinstaller
    python build_exe.py

The result is ``dist/Yeshuahamashiah(.exe)``. Double-clicking it launches the
engine. Game projects are read from / written to a ``projects/`` folder created
next to the executable, so users can edit games without rebuilding.
"""

from __future__ import annotations

import subprocess
import sys


def main() -> int:
    cmd = [
        sys.executable, "-m", "PyInstaller",
        "--noconfirm",
        "--clean",
        "--onefile",
        "--windowed",
        "--name", "Yeshuahamashiah",
        "main.py",
    ]
    print("Running:", " ".join(cmd))
    return subprocess.call(cmd)


if __name__ == "__main__":
    raise SystemExit(main())
