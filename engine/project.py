"""Project model: load/save the project manifest, maps and asset registry."""

from __future__ import annotations

import json
import os
from typing import Dict, List, Optional

from . import config
from .assets import AssetManager
from .tilemap import TileMap


class Project:
    """A self-contained game project stored under ``projects/<name>/``.

    Layout::

        projects/<name>/
            project.json        # manifest + asset registry
            maps/<map>.json     # one file per map
            assets/<category>/  # imported asset files
    """

    def __init__(self, path: str, data: dict):
        self.path = path
        self.data = data
        self.assets = AssetManager(path, data.setdefault("assets", {}))
        self._map_cache: Dict[str, TileMap] = {}

    # -- properties ------------------------------------------------------
    @property
    def name(self) -> str:
        return self.data.get("name", os.path.basename(self.path))

    @property
    def tile_size(self) -> int:
        return self.data.get("tile_size", config.DEFAULT_TILE_SIZE)

    @property
    def start_map(self) -> str:
        return self.data.get("start_map", "map1")

    @property
    def start_position(self) -> tuple[int, int]:
        return self.data.get("start_x", 1), self.data.get("start_y", 1)

    def set_start(self, map_name: str, x: int, y: int) -> None:
        self.data["start_map"] = map_name
        self.data["start_x"] = x
        self.data["start_y"] = y

    # -- maps ------------------------------------------------------------
    def maps_dir(self) -> str:
        path = os.path.join(self.path, "maps")
        os.makedirs(path, exist_ok=True)
        return path

    def list_maps(self) -> List[str]:
        d = self.maps_dir()
        return sorted(os.path.splitext(f)[0] for f in os.listdir(d)
                      if f.endswith(".json"))

    def load_map(self, name: str) -> Optional[TileMap]:
        if name in self._map_cache:
            return self._map_cache[name]
        path = os.path.join(self.maps_dir(), f"{name}.json")
        if not os.path.exists(path):
            return None
        with open(path, "r", encoding="utf-8") as fh:
            tm = TileMap.from_dict(json.load(fh))
        self._map_cache[name] = tm
        return tm

    def save_map(self, tilemap: TileMap) -> None:
        path = os.path.join(self.maps_dir(), f"{tilemap.name}.json")
        with open(path, "w", encoding="utf-8") as fh:
            json.dump(tilemap.to_dict(), fh, indent=2)
        self._map_cache[tilemap.name] = tilemap

    def add_map(self, tilemap: TileMap) -> None:
        self._map_cache[tilemap.name] = tilemap
        self.save_map(tilemap)

    # -- persistence -----------------------------------------------------
    def save(self) -> None:
        os.makedirs(self.path, exist_ok=True)
        with open(os.path.join(self.path, "project.json"), "w",
                  encoding="utf-8") as fh:
            json.dump(self.data, fh, indent=2)

    @classmethod
    def load(cls, path: str) -> "Project":
        with open(os.path.join(path, "project.json"), "r", encoding="utf-8") as fh:
            return cls(path, json.load(fh))

    @classmethod
    def create(cls, name: str, tile_size: int = config.DEFAULT_TILE_SIZE) -> "Project":
        path = os.path.join(config.projects_dir(), name)
        os.makedirs(path, exist_ok=True)
        data = {
            "name": name,
            "tile_size": tile_size,
            "start_map": "map1",
            "start_x": 3,
            "start_y": 3,
            "assets": {},
        }
        return cls(path, data)


def list_projects() -> List[str]:
    root = config.projects_dir()
    out = []
    for entry in sorted(os.listdir(root)):
        if os.path.exists(os.path.join(root, entry, "project.json")):
            out.append(entry)
    return out
