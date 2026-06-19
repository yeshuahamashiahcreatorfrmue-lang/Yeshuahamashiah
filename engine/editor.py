"""Map editor scene.

Lets the user paint tile layers, toggle collision, place events and the
player start position, register new assets and save the map -- all inside the
same executable that runs the game.
"""

from __future__ import annotations

import os
from typing import Callable, Optional

import pygame

from . import assets as A
from . import config, ui
from .camera import Camera
from .scene import Scene
from .tilemap import (TileMap, Event, LAYER_GROUND, LAYER_OBJECT, LAYERS,
                      EMPTY)
from .tileset import Tileset

PANEL_W = 240

# Editor tools.
TOOL_GROUND = "ground"
TOOL_OBJECT = "object"
TOOL_COLLISION = "collision"
TOOL_EVENT = "event"
TOOL_START = "start"

_TOOL_KEYS = {
    pygame.K_1: TOOL_GROUND,
    pygame.K_2: TOOL_OBJECT,
    pygame.K_3: TOOL_COLLISION,
    pygame.K_4: TOOL_EVENT,
    pygame.K_5: TOOL_START,
}

_TOOL_LABELS = {
    TOOL_GROUND: "1 Ground",
    TOOL_OBJECT: "2 Object",
    TOOL_COLLISION: "3 Collision",
    TOOL_EVENT: "4 Event",
    TOOL_START: "5 Start",
}


class TextPrompt:
    """A modal single-line text input used for map names and event text."""

    def __init__(self):
        self.active = False
        self.title = ""
        self.text = ""
        self._callback: Optional[Callable[[str], None]] = None

    def open(self, title: str, callback: Callable[[str], None],
             initial: str = "") -> None:
        self.active = True
        self.title = title
        self.text = initial
        self._callback = callback

    def handle_event(self, event: pygame.event.Event) -> None:
        if not self.active or event.type != pygame.KEYDOWN:
            return
        if event.key == pygame.K_RETURN:
            cb, text = self._callback, self.text
            self.active = False
            if cb:
                cb(text)
        elif event.key == pygame.K_ESCAPE:
            self.active = False
        elif event.key == pygame.K_BACKSPACE:
            self.text = self.text[:-1]
        elif event.unicode and event.unicode.isprintable():
            self.text += event.unicode

    def draw(self, surface: pygame.Surface) -> None:
        if not self.active:
            return
        w, h = surface.get_size()
        overlay = pygame.Surface((w, h), pygame.SRCALPHA)
        overlay.fill((0, 0, 0, 140))
        surface.blit(overlay, (0, 0))
        box = pygame.Rect(w // 2 - 240, h // 2 - 50, 480, 100)
        pygame.draw.rect(surface, config.COLOR_PANEL, box, border_radius=8)
        pygame.draw.rect(surface, config.COLOR_ACCENT, box, width=2,
                         border_radius=8)
        ui.draw_text(surface, self.title, (box.x + 16, box.y + 12), 18)
        field = pygame.Rect(box.x + 16, box.y + 44, box.width - 32, 30)
        pygame.draw.rect(surface, (20, 20, 28), field, border_radius=4)
        ui.draw_text(surface, self.text + "_", (field.x + 8, field.y + 6), 18)
        ui.draw_text(surface, "[Enter] ok   [Esc] cancel",
                     (box.x + 16, box.bottom - 6), 12, config.COLOR_TEXT_DIM)


class EditorScene(Scene):
    def __init__(self, app, project, map_name: Optional[str] = None):
        super().__init__(app)
        self.project = project
        self.map_view_w = config.SCREEN_WIDTH - PANEL_W
        self.camera = Camera(self.map_view_w, config.SCREEN_HEIGHT)

        self.tool = TOOL_GROUND
        self.selected_tile = 0
        self.tileset: Optional[Tileset] = None
        self.tilemap: Optional[TileMap] = None
        self.prompt = TextPrompt()
        self.status = ""
        self._panning = False

        self._map_name = map_name or project.start_map
        self._palette_scroll = 0
        self._buttons: list[ui.Button] = []

    # -- setup -----------------------------------------------------------
    def on_enter(self) -> None:
        self.tilemap = self.project.load_map(self._map_name)
        if self.tilemap is None:
            # Create an empty map if the requested one is missing.
            self.tilemap = TileMap(self._map_name, 24, 16,
                                   self.project.tile_size,
                                   self._first_tileset_name())
            self.project.add_map(self.tilemap)
        self._load_tileset()
        self._build_buttons()

    def _first_tileset_name(self) -> str:
        names = self.project.assets.names(A.TILESET)
        return names[0] if names else ""

    def _load_tileset(self) -> None:
        name = self.tilemap.tileset_name or self._first_tileset_name()
        if not name:
            self.tileset = None
            return
        self.tilemap.tileset_name = name
        meta = self.project.assets.get_meta(A.TILESET, name)
        surface = self.project.assets.load_image(A.TILESET, name)
        if surface is not None:
            ts = meta.get("tile_size", self.tilemap.tile_size) if meta else \
                self.tilemap.tile_size
            self.tileset = Tileset(surface, ts)

    def _build_buttons(self) -> None:
        x = self.map_view_w + 12
        self._buttons = [
            ui.Button((x, config.SCREEN_HEIGHT - 96, PANEL_W - 24, 30),
                      "Save (Ctrl+S)", self._save, 16),
            ui.Button((x, config.SCREEN_HEIGHT - 60, PANEL_W - 24, 30),
                      "Play test (F1)", self._play, 16),
        ]

    # -- actions ---------------------------------------------------------
    def _save(self) -> None:
        self.project.save_map(self.tilemap)
        self.project.save()
        self.status = f"Saved {self.tilemap.name}.json"

    def _play(self) -> None:
        self._save()
        from .runtime import PlayScene
        self.next_scene = PlayScene(self.app, self.project,
                                    return_to_editor=True)

    def _import_asset(self) -> None:
        self.prompt.open("Asset file path (png) to import as tileset:",
                         self._do_import_tileset)

    def _do_import_tileset(self, path: str) -> None:
        path = path.strip().strip('"')
        if not path or not os.path.exists(path):
            self.status = "Import failed: file not found"
            return
        name = os.path.splitext(os.path.basename(path))[0]
        self.project.assets.register(A.TILESET, name, path,
                                     tile_size=self.project.tile_size)
        self.project.save()
        self.tilemap.tileset_name = name
        self._load_tileset()
        self.status = f"Registered tileset '{name}'"

    # -- events ----------------------------------------------------------
    def handle_event(self, event: pygame.event.Event) -> None:
        if self.prompt.active:
            self.prompt.handle_event(event)
            return

        for btn in self._buttons:
            btn.handle_event(event)

        if event.type == pygame.KEYDOWN:
            self._handle_key(event)
        elif event.type == pygame.MOUSEBUTTONDOWN:
            self._handle_mouse_down(event)
        elif event.type == pygame.MOUSEBUTTONUP:
            if event.button == 2:
                self._panning = False
        elif event.type == pygame.MOUSEMOTION:
            self._handle_mouse_motion(event)

    def _handle_key(self, event: pygame.event.Event) -> None:
        mods = pygame.key.get_mods()
        if event.key == pygame.K_s and mods & pygame.KMOD_CTRL:
            self._save()
        elif event.key == pygame.K_F1:
            self._play()
        elif event.key == pygame.K_ESCAPE:
            from .menu import MenuScene
            self.next_scene = MenuScene(self.app)
        elif event.key == pygame.K_i:
            self._import_asset()
        elif event.key == pygame.K_n:
            self.prompt.open("New map name:", self._create_map)
        elif event.key in _TOOL_KEYS:
            self.tool = _TOOL_KEYS[event.key]

    def _create_map(self, name: str) -> None:
        name = name.strip()
        if not name:
            return
        tm = TileMap(name, 24, 16, self.project.tile_size,
                     self._first_tileset_name())
        self.project.add_map(tm)
        self._map_name = name
        self.tilemap = tm
        self._load_tileset()
        self.status = f"Created map '{name}'"

    def _handle_mouse_down(self, event: pygame.event.Event) -> None:
        if event.pos[0] >= self.map_view_w:
            if event.button == 1:
                self._palette_click(event.pos)
            return
        if event.button == 2:
            self._panning = True
        elif event.button in (1, 3):
            self._apply_tool(event.pos, erase=event.button == 3)

    def _handle_mouse_motion(self, event: pygame.event.Event) -> None:
        if self._panning:
            self._pan(-event.rel[0], -event.rel[1])
            return
        if event.pos[0] >= self.map_view_w:
            return
        buttons = event.buttons
        if buttons[0]:
            self._apply_tool(event.pos, erase=False)
        elif buttons[2]:
            self._apply_tool(event.pos, erase=True)

    def _pan(self, dx: int, dy: int) -> None:
        map_w = self.tilemap.width * self.tilemap.tile_size
        map_h = self.tilemap.height * self.tilemap.tile_size
        self.camera.move(dx, dy, map_w, map_h)

    # -- painting --------------------------------------------------------
    def _mouse_to_tile(self, pos) -> tuple[int, int]:
        ts = self.tilemap.tile_size
        tx = (pos[0] + self.camera.x) // ts
        ty = (pos[1] + self.camera.y) // ts
        return tx, ty

    def _apply_tool(self, pos, erase: bool) -> None:
        if not self.tilemap:
            return
        tx, ty = self._mouse_to_tile(pos)
        if not self.tilemap.in_bounds(tx, ty):
            return
        if self.tool == TOOL_GROUND:
            self.tilemap.set_tile(LAYER_GROUND, tx, ty,
                                  EMPTY if erase else self.selected_tile)
        elif self.tool == TOOL_OBJECT:
            self.tilemap.set_tile(LAYER_OBJECT, tx, ty,
                                  EMPTY if erase else self.selected_tile)
        elif self.tool == TOOL_COLLISION:
            self.tilemap.set_solid(tx, ty, not erase)
        elif self.tool == TOOL_START:
            if not erase:
                self.project.set_start(self.tilemap.name, tx, ty)
        elif self.tool == TOOL_EVENT:
            if erase:
                self.tilemap.remove_event(tx, ty)
            else:
                self.prompt.open(
                    "Event message text:",
                    lambda text: self.tilemap.add_event(
                        Event(tx, ty, "message", text=text)))

    def _palette_click(self, pos) -> None:
        if self.tileset is None:
            return
        rel_x = pos[0] - (self.map_view_w + 12)
        rel_y = pos[1] - self._palette_top() + self._palette_scroll
        cell = self.tilemap.tile_size + 4
        col = rel_x // cell
        row = rel_y // cell
        per_row = (PANEL_W - 24) // cell
        if 0 <= col < per_row:
            index = int(row * per_row + col)
            if 0 <= index < len(self.tileset):
                self.selected_tile = index

    def _palette_top(self) -> int:
        return 150

    # -- update ----------------------------------------------------------
    def update(self, dt: float) -> None:
        # Keyboard panning.
        keys = pygame.key.get_pressed()
        speed = int(400 * dt)
        dx = (keys[pygame.K_RIGHT] - keys[pygame.K_LEFT]) * speed
        dy = (keys[pygame.K_DOWN] - keys[pygame.K_UP]) * speed
        if dx or dy:
            self._pan(dx, dy)

    # -- drawing ---------------------------------------------------------
    def draw(self, surface: pygame.Surface) -> None:
        surface.fill(config.COLOR_BG)
        self._draw_map(surface)
        self._draw_overlays(surface)
        self._draw_panel(surface)
        self.prompt.draw(surface)

    def _draw_map(self, surface: pygame.Surface) -> None:
        clip = surface.get_clip()
        surface.set_clip(pygame.Rect(0, 0, self.map_view_w,
                                     config.SCREEN_HEIGHT))
        if self.tilemap and self.tileset:
            for layer in LAYERS:
                self.tilemap.draw_layer(surface, self.tileset, layer,
                                        self.camera)
        surface.set_clip(clip)

    def _draw_overlays(self, surface: pygame.Surface) -> None:
        if not self.tilemap:
            return
        ts = self.tilemap.tile_size
        clip = surface.get_clip()
        surface.set_clip(pygame.Rect(0, 0, self.map_view_w,
                                     config.SCREEN_HEIGHT))
        cam = self.camera
        start_x = max(0, cam.x // ts)
        start_y = max(0, cam.y // ts)
        end_x = min(self.tilemap.width, (cam.x + self.map_view_w) // ts + 1)
        end_y = min(self.tilemap.height, (cam.y + config.SCREEN_HEIGHT) // ts + 1)

        for y in range(start_y, end_y):
            for x in range(start_x, end_x):
                sx, sy = x * ts - cam.x, y * ts - cam.y
                pygame.draw.rect(surface, config.COLOR_GRID,
                                 (sx, sy, ts, ts), 1)
                if self.tilemap.is_solid(x, y):
                    s = pygame.Surface((ts, ts), pygame.SRCALPHA)
                    s.fill((*config.COLOR_COLLISION, 80))
                    surface.blit(s, (sx, sy))
                    pygame.draw.line(surface, config.COLOR_COLLISION,
                                     (sx, sy), (sx + ts, sy + ts), 1)

        for ev in self.tilemap.events:
            sx, sy = ev.x * ts - cam.x, ev.y * ts - cam.y
            pygame.draw.rect(surface, config.COLOR_EVENT, (sx, sy, ts, ts), 2)
            ui.draw_text(surface, "E", (sx + 3, sy + 1), 14, config.COLOR_EVENT)

        # Player start marker.
        if self.project.start_map == self.tilemap.name:
            stx, sty = self.project.start_position
            sx, sy = stx * ts - cam.x, sty * ts - cam.y
            pygame.draw.rect(surface, config.COLOR_PLAYER_START,
                             (sx, sy, ts, ts), 3)
            ui.draw_text(surface, "P", (sx + 3, sy + 1), 14,
                         config.COLOR_PLAYER_START)
        surface.set_clip(clip)

    def _draw_panel(self, surface: pygame.Surface) -> None:
        panel = pygame.Rect(self.map_view_w, 0, PANEL_W, config.SCREEN_HEIGHT)
        pygame.draw.rect(surface, config.COLOR_PANEL, panel)
        pygame.draw.line(surface, config.COLOR_ACCENT,
                         (self.map_view_w, 0),
                         (self.map_view_w, config.SCREEN_HEIGHT), 2)
        x = self.map_view_w + 12
        ui.draw_text(surface, "EDITOR", (x, 10), 22, config.COLOR_ACCENT)
        ui.draw_text(surface, f"map: {self.tilemap.name if self.tilemap else '-'}",
                     (x, 40), 14, config.COLOR_TEXT_DIM)

        # Tool list.
        ty = 64
        for tool, label in _TOOL_LABELS.items():
            color = config.COLOR_ACCENT if tool == self.tool else \
                config.COLOR_TEXT
            ui.draw_text(surface, label, (x, ty), 16, color)
            ty += 18

        ui.draw_text(surface, "I import  N new map", (x, ty + 4), 13,
                     config.COLOR_TEXT_DIM)

        # Tile palette.
        self._draw_palette(surface, x)

        for btn in self._buttons:
            btn.draw(surface)

        if self.status:
            ui.draw_text(surface, self.status, (x, config.SCREEN_HEIGHT - 120),
                         13, config.COLOR_PLAYER_START)

    def _draw_palette(self, surface: pygame.Surface, x: int) -> None:
        top = self._palette_top()
        ui.draw_text(surface, "Tiles:", (x, top - 22), 14, config.COLOR_TEXT_DIM)
        if self.tileset is None:
            ui.draw_text(surface, "(no tileset)", (x, top), 14,
                         config.COLOR_TEXT_DIM)
            return
        ts = self.tilemap.tile_size
        cell = ts + 4
        per_row = (PANEL_W - 24) // cell
        for i in range(len(self.tileset)):
            col = i % per_row
            row = i // per_row
            sx = x + col * cell
            sy = top + row * cell - self._palette_scroll
            if sy + cell < top or sy > config.SCREEN_HEIGHT - 110:
                continue
            tile = self.tileset.tile(i)
            if tile is not None:
                surface.blit(tile, (sx, sy))
            if i == self.selected_tile:
                pygame.draw.rect(surface, config.COLOR_SELECT,
                                 (sx - 1, sy - 1, ts + 2, ts + 2), 2)
