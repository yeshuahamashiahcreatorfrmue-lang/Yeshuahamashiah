"""Asset management and registration.

The :class:`AssetManager` is responsible for importing external files into a
project, recording them in the project's asset registry and loading them as
pygame surfaces / sounds on demand. Loaded resources are cached so repeated
lookups are cheap.
"""

from __future__ import annotations

import os
import shutil
from typing import Dict, Optional

import pygame

# Asset categories understood by the engine.
TILESET = "tilesets"
CHARACTER = "characters"
AUDIO = "audio"

IMAGE_EXTS = {".png", ".jpg", ".jpeg", ".bmp", ".gif"}
AUDIO_EXTS = {".wav", ".ogg", ".mp3"}


class AssetManager:
    """Imports, registers and loads the assets that belong to a project.

    Parameters
    ----------
    project_dir:
        Absolute path to the project folder. Imported files are copied into
        ``<project_dir>/assets/<category>/``.
    registry:
        The ``assets`` section of ``project.json``. It is mutated in place so
        that saving the project persists newly registered assets.
    """

    def __init__(self, project_dir: str, registry: Dict[str, dict]):
        self.project_dir = project_dir
        self.registry = registry
        for category in (TILESET, CHARACTER, AUDIO):
            self.registry.setdefault(category, {})

        self._image_cache: Dict[str, pygame.Surface] = {}
        self._sound_cache: Dict[str, pygame.mixer.Sound] = {}

    # -- registration ----------------------------------------------------
    def register(self, category: str, name: str, source_path: str, **meta) -> dict:
        """Import ``source_path`` into the project and register it.

        Returns the metadata dict stored in the registry. The file is copied
        into the project's asset directory so the project stays self
        contained and portable.
        """
        if category not in (TILESET, CHARACTER, AUDIO):
            raise ValueError(f"unknown asset category: {category}")

        dest_dir = os.path.join(self.project_dir, "assets", category)
        os.makedirs(dest_dir, exist_ok=True)
        filename = os.path.basename(source_path)
        dest_path = os.path.join(dest_dir, filename)
        if os.path.abspath(source_path) != os.path.abspath(dest_path):
            shutil.copy2(source_path, dest_path)

        rel = os.path.relpath(dest_path, self.project_dir).replace(os.sep, "/")
        entry = {"path": rel}
        entry.update(meta)
        self.registry[category][name] = entry
        # Drop any stale cache for this name.
        self._image_cache.pop(rel, None)
        self._sound_cache.pop(rel, None)
        return entry

    def unregister(self, category: str, name: str) -> None:
        self.registry.get(category, {}).pop(name, None)

    def names(self, category: str):
        return list(self.registry.get(category, {}).keys())

    def get_meta(self, category: str, name: str) -> Optional[dict]:
        return self.registry.get(category, {}).get(name)

    # -- loading ---------------------------------------------------------
    def _abs(self, rel_path: str) -> str:
        return os.path.join(self.project_dir, rel_path)

    def load_image(self, category: str, name: str) -> Optional[pygame.Surface]:
        meta = self.get_meta(category, name)
        if not meta:
            return None
        rel = meta["path"]
        if rel in self._image_cache:
            return self._image_cache[rel]
        abs_path = self._abs(rel)
        if not os.path.exists(abs_path):
            return None
        surface = pygame.image.load(abs_path).convert_alpha()
        self._image_cache[rel] = surface
        return surface

    def load_sound(self, name: str) -> Optional[pygame.mixer.Sound]:
        meta = self.get_meta(AUDIO, name)
        if not meta:
            return None
        rel = meta["path"]
        if rel in self._sound_cache:
            return self._sound_cache[rel]
        abs_path = self._abs(rel)
        if not os.path.exists(abs_path) or not pygame.mixer.get_init():
            return None
        sound = pygame.mixer.Sound(abs_path)
        self._sound_cache[rel] = sound
        return sound

    @staticmethod
    def classify(path: str) -> Optional[str]:
        """Guess the asset category from a file extension."""
        ext = os.path.splitext(path)[1].lower()
        if ext in AUDIO_EXTS:
            return AUDIO
        if ext in IMAGE_EXTS:
            # Caller decides tileset vs character; default to tileset.
            return TILESET
        return None
