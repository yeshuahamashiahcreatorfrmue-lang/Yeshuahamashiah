"""Tileset: slices a source image into individually addressable tiles."""

from __future__ import annotations

from typing import List

import pygame


class Tileset:
    """A grid of tiles cut from a single source surface.

    Tiles are addressed by a flat index ``row * columns + column``. An index
    of ``-1`` is treated as "empty" throughout the engine.
    """

    def __init__(self, surface: pygame.Surface, tile_size: int):
        self.surface = surface
        self.tile_size = tile_size
        self.columns = max(1, surface.get_width() // tile_size)
        self.rows = max(1, surface.get_height() // tile_size)
        self.count = self.columns * self.rows
        self._tiles: List[pygame.Surface] = []
        self._slice()

    def _slice(self) -> None:
        ts = self.tile_size
        for row in range(self.rows):
            for col in range(self.columns):
                rect = pygame.Rect(col * ts, row * ts, ts, ts)
                self._tiles.append(self.surface.subsurface(rect).copy())

    def tile(self, index: int) -> pygame.Surface | None:
        if 0 <= index < self.count:
            return self._tiles[index]
        return None

    def __len__(self) -> int:
        return self.count
