"""Generates placeholder assets and a sample project.

So that the engine runs out of the box with nothing but pygame installed, this
module draws a small tileset and a character sprite sheet programmatically and
writes a starter map. Real projects replace these via the editor's asset
import.
"""

from __future__ import annotations

import os

import pygame

from . import assets as A
from . import config
from .project import Project
from .tilemap import TileMap, Event, LAYER_GROUND, LAYER_OBJECT

SAMPLE_NAME = "sample"

# Tile palette: (name, base color, accent color) drawn into the tileset.
_TILES = [
    ("grass", (76, 156, 76), (66, 140, 66)),
    ("dirt", (150, 113, 75), (132, 98, 64)),
    ("water", (64, 110, 200), (84, 130, 220)),
    ("sand", (214, 198, 130), (198, 182, 116)),
    ("stone_floor", (140, 140, 150), (120, 120, 130)),
    ("wall", (90, 78, 70), (70, 60, 54)),
    ("tree", (40, 100, 50), (60, 130, 64)),
    ("flower", (90, 160, 90), (230, 90, 120)),
]


def _make_tileset(tile_size: int) -> pygame.Surface:
    cols = len(_TILES)
    sheet = pygame.Surface((cols * tile_size, tile_size), pygame.SRCALPHA)
    for i, (name, base, accent) in enumerate(_TILES):
        rect = pygame.Rect(i * tile_size, 0, tile_size, tile_size)
        sheet.fill(base, rect)
        if name == "water":
            for wy in range(4, tile_size, 8):
                pygame.draw.line(sheet, accent, (rect.x + 2, rect.y + wy),
                                 (rect.right - 2, rect.y + wy), 2)
        elif name == "tree":
            pygame.draw.circle(sheet, accent, rect.center, tile_size // 2 - 3)
            pygame.draw.rect(sheet, (110, 80, 50),
                             (rect.centerx - 3, rect.bottom - 10, 6, 10))
        elif name == "flower":
            pygame.draw.circle(sheet, accent,
                               (rect.centerx, rect.centery), 4)
        elif name == "wall":
            for by in range(0, tile_size, 8):
                pygame.draw.line(sheet, accent, (rect.x, rect.y + by),
                                 (rect.right, rect.y + by), 1)
        else:
            pygame.draw.rect(sheet, accent, rect, width=1)
    return sheet


def _make_character(frame: int) -> pygame.Surface:
    """A 3x4 sprite sheet: 3 walk frames across, 4 directions down."""
    sheet = pygame.Surface((frame * 3, frame * 4), pygame.SRCALPHA)
    skin = (240, 205, 170)
    shirt = (70, 120, 210)
    pants = (60, 60, 70)
    for d in range(4):
        for c in range(3):
            ox, oy = c * frame, d * frame
            # Walking bob.
            bob = 0 if c == 1 else (1 if c == 0 else -1)
            # Body.
            pygame.draw.rect(sheet, shirt,
                             (ox + frame // 2 - 5, oy + 12 + bob, 10, 9))
            pygame.draw.rect(sheet, pants,
                             (ox + frame // 2 - 5, oy + 21 + bob, 10, 7))
            # Head.
            pygame.draw.circle(sheet, skin,
                               (ox + frame // 2, oy + 9 + bob), 6)
            # Facing dot.
            eye = (30, 30, 30)
            cx, cy = ox + frame // 2, oy + 9 + bob
            if d == 0:    # down
                pygame.draw.circle(sheet, eye, (cx - 2, cy + 1), 1)
                pygame.draw.circle(sheet, eye, (cx + 2, cy + 1), 1)
            elif d == 1:  # left
                pygame.draw.circle(sheet, eye, (cx - 3, cy), 1)
            elif d == 2:  # right
                pygame.draw.circle(sheet, eye, (cx + 3, cy), 1)
    return sheet


def ensure_sample_project(tile_size: int = config.DEFAULT_TILE_SIZE) -> str:
    """Create the sample project on disk if it does not yet exist.

    Returns the absolute path to the project folder.
    """
    path = os.path.join(config.projects_dir(), SAMPLE_NAME)
    if os.path.exists(os.path.join(path, "project.json")):
        return path

    project = Project.create(SAMPLE_NAME, tile_size)

    # Write the generated assets to disk, then register them.
    assets_img_dir = os.path.join(path, "assets", A.TILESET)
    assets_chr_dir = os.path.join(path, "assets", A.CHARACTER)
    os.makedirs(assets_img_dir, exist_ok=True)
    os.makedirs(assets_chr_dir, exist_ok=True)

    tileset_path = os.path.join(assets_img_dir, "overworld.png")
    char_path = os.path.join(assets_chr_dir, "hero.png")
    pygame.image.save(_make_tileset(tile_size), tileset_path)
    pygame.image.save(_make_character(tile_size), char_path)

    project.assets.register(A.TILESET, "overworld", tileset_path,
                            tile_size=tile_size)
    project.assets.register(A.CHARACTER, "hero", char_path,
                            frame_w=tile_size, frame_h=tile_size)

    # Build a starter map.
    tm = TileMap("map1", 28, 18, tile_size, "overworld")
    for y in range(tm.height):
        for x in range(tm.width):
            tm.set_tile(LAYER_GROUND, x, y, 0)  # grass everywhere
    # A dirt path.
    for x in range(3, 24):
        tm.set_tile(LAYER_GROUND, x, 9, 1)
    # A little pond.
    for y in range(3, 6):
        for x in range(5, 9):
            tm.set_tile(LAYER_GROUND, x, y, 2)
            tm.set_solid(x, y, True)
    # Border walls.
    for x in range(tm.width):
        tm.set_tile(LAYER_OBJECT, x, 0, 5)
        tm.set_tile(LAYER_OBJECT, x, tm.height - 1, 5)
        tm.set_solid(x, 0, True)
        tm.set_solid(x, tm.height - 1, True)
    for y in range(tm.height):
        tm.set_tile(LAYER_OBJECT, 0, y, 5)
        tm.set_tile(LAYER_OBJECT, tm.width - 1, y, 5)
        tm.set_solid(0, y, True)
        tm.set_solid(tm.width - 1, y, True)
    # A few trees.
    for tx, ty in [(20, 4), (21, 5), (22, 3), (15, 13), (16, 14)]:
        tm.set_tile(LAYER_OBJECT, tx, ty, 6)
        tm.set_solid(tx, ty, True)
    # A welcome sign event.
    tm.add_event(Event(12, 8, "message",
                        text="Welcome to the Yeshuahamashiah Engine!"))
    tm.add_event(Event(18, 9, "message",
                        text="Press F1 anytime to switch to the editor."))

    project.add_map(tm)
    project.set_start("map1", 3, 10)
    project.save()
    return path
