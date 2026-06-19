"""Global configuration and shared constants for the engine."""

from __future__ import annotations

import os
import sys

# ---------------------------------------------------------------------------
# Display
# ---------------------------------------------------------------------------
SCREEN_WIDTH = 1024
SCREEN_HEIGHT = 640
FPS = 60
TITLE = "Yeshuahamashiah Engine"

# Default grid size in pixels. Individual projects may override this.
DEFAULT_TILE_SIZE = 32

# ---------------------------------------------------------------------------
# Colors
# ---------------------------------------------------------------------------
COLOR_BG = (24, 24, 32)
COLOR_PANEL = (40, 42, 54)
COLOR_PANEL_LIGHT = (58, 60, 78)
COLOR_TEXT = (236, 239, 244)
COLOR_TEXT_DIM = (150, 155, 170)
COLOR_ACCENT = (94, 168, 248)
COLOR_GRID = (70, 72, 90)
COLOR_COLLISION = (240, 90, 90)
COLOR_EVENT = (250, 200, 80)
COLOR_PLAYER_START = (120, 230, 140)
COLOR_SELECT = (255, 255, 255)

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------


def project_root() -> str:
    """Return the base directory for engine data.

    When running as a PyInstaller executable the source tree is read-only, so
    we store editable game projects next to the executable instead.
    """
    if getattr(sys, "frozen", False):
        return os.path.dirname(os.path.abspath(sys.executable))
    return os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def projects_dir() -> str:
    path = os.path.join(project_root(), "projects")
    os.makedirs(path, exist_ok=True)
    return path


# ---------------------------------------------------------------------------
# Application states
# ---------------------------------------------------------------------------
STATE_MENU = "menu"
STATE_PLAY = "play"
STATE_EDIT = "edit"
