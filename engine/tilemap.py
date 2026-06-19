"""Tile map model: layers, collision grid and events.

A :class:`TileMap` is a pure data object plus rendering helpers. It knows how
to serialise itself to / from the JSON dictionaries stored on disk but it does
not own its tileset surface; the owning scene supplies a :class:`Tileset`
when drawing.
"""

from __future__ import annotations

from typing import Dict, List, Optional

import pygame

from .tileset import Tileset

# Names of the visual layers, drawn bottom to top.
LAYER_GROUND = "ground"
LAYER_OBJECT = "object"
LAYERS = (LAYER_GROUND, LAYER_OBJECT)

EMPTY = -1


class Event:
    """A scripted interaction attached to a tile cell."""

    def __init__(self, x: int, y: int, kind: str = "message", **data):
        self.x = x
        self.y = y
        self.kind = kind
        self.data = data

    def to_dict(self) -> dict:
        return {"x": self.x, "y": self.y, "kind": self.kind, "data": self.data}

    @classmethod
    def from_dict(cls, d: dict) -> "Event":
        return cls(d["x"], d["y"], d.get("kind", "message"), **d.get("data", {}))


class TileMap:
    def __init__(self, name: str, width: int, height: int, tile_size: int,
                 tileset_name: str):
        self.name = name
        self.width = width
        self.height = height
        self.tile_size = tile_size
        self.tileset_name = tileset_name

        self.layers: Dict[str, List[List[int]]] = {
            layer: self._blank_grid(EMPTY) for layer in LAYERS
        }
        self.collision: List[List[int]] = self._blank_grid(0)
        self.events: List[Event] = []

    # -- construction helpers -------------------------------------------
    def _blank_grid(self, fill: int) -> List[List[int]]:
        return [[fill for _ in range(self.width)] for _ in range(self.height)]

    def in_bounds(self, x: int, y: int) -> bool:
        return 0 <= x < self.width and 0 <= y < self.height

    # -- tile access -----------------------------------------------------
    def get_tile(self, layer: str, x: int, y: int) -> int:
        if self.in_bounds(x, y):
            return self.layers[layer][y][x]
        return EMPTY

    def set_tile(self, layer: str, x: int, y: int, index: int) -> None:
        if self.in_bounds(x, y):
            self.layers[layer][y][x] = index

    # -- collision -------------------------------------------------------
    def is_solid(self, x: int, y: int) -> bool:
        if not self.in_bounds(x, y):
            return True  # Out of bounds blocks movement.
        return self.collision[y][x] == 1

    def set_solid(self, x: int, y: int, solid: bool) -> None:
        if self.in_bounds(x, y):
            self.collision[y][x] = 1 if solid else 0

    # -- events ----------------------------------------------------------
    def event_at(self, x: int, y: int) -> Optional[Event]:
        for event in self.events:
            if event.x == x and event.y == y:
                return event
        return None

    def add_event(self, event: Event) -> None:
        existing = self.event_at(event.x, event.y)
        if existing:
            self.events.remove(existing)
        self.events.append(event)

    def remove_event(self, x: int, y: int) -> None:
        existing = self.event_at(x, y)
        if existing:
            self.events.remove(existing)

    # -- rendering -------------------------------------------------------
    def draw_layer(self, surface: pygame.Surface, tileset: Tileset, layer: str,
                   camera) -> None:
        ts = self.tile_size
        grid = self.layers[layer]
        # Only iterate over the tiles intersecting the camera view.
        start_x = max(0, camera.x // ts)
        start_y = max(0, camera.y // ts)
        end_x = min(self.width, (camera.x + camera.width) // ts + 1)
        end_y = min(self.height, (camera.y + camera.height) // ts + 1)
        for y in range(start_y, end_y):
            row = grid[y]
            for x in range(start_x, end_x):
                index = row[x]
                if index == EMPTY:
                    continue
                tile = tileset.tile(index)
                if tile is not None:
                    surface.blit(tile, (x * ts - camera.x, y * ts - camera.y))

    # -- serialisation ---------------------------------------------------
    def to_dict(self) -> dict:
        return {
            "name": self.name,
            "width": self.width,
            "height": self.height,
            "tile_size": self.tile_size,
            "tileset": self.tileset_name,
            "layers": self.layers,
            "collision": self.collision,
            "events": [e.to_dict() for e in self.events],
        }

    @classmethod
    def from_dict(cls, d: dict) -> "TileMap":
        tm = cls(d["name"], d["width"], d["height"], d["tile_size"], d["tileset"])
        for layer in LAYERS:
            if layer in d.get("layers", {}):
                tm.layers[layer] = d["layers"][layer]
        if "collision" in d:
            tm.collision = d["collision"]
        tm.events = [Event.from_dict(e) for e in d.get("events", [])]
        return tm
