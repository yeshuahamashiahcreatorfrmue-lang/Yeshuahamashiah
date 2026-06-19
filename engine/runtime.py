"""Play scene: runs a project as a playable game."""

from __future__ import annotations

import pygame

from . import assets as A
from . import config, ui
from .camera import Camera
from .entity import Player
from .scene import Scene
from .tileset import Tileset
from .tilemap import LAYERS, LAYER_OBJECT


class PlayScene(Scene):
    """Loads a project's start map and lets the player walk around it."""

    def __init__(self, app, project, return_to_editor: bool = False):
        super().__init__(app)
        self.project = project
        self.return_to_editor = return_to_editor
        self.camera = Camera(config.SCREEN_WIDTH, config.SCREEN_HEIGHT)
        self.message = ui.MessageBox()
        self.tileset: Tileset | None = None
        self.tilemap = None
        self.player: Player | None = None

    def on_enter(self) -> None:
        self.tilemap = self.project.load_map(self.project.start_map)
        if self.tilemap is None:
            # Nothing to play; bounce back.
            self._exit()
            return
        self._load_tileset()
        self._spawn_player()

    def _load_tileset(self) -> None:
        meta = self.project.assets.get_meta(A.TILESET, self.tilemap.tileset_name)
        surface = self.project.assets.load_image(A.TILESET,
                                                  self.tilemap.tileset_name)
        if surface is not None:
            ts = meta.get("tile_size", self.tilemap.tile_size) if meta else \
                self.tilemap.tile_size
            self.tileset = Tileset(surface, ts)

    def _spawn_player(self) -> None:
        names = self.project.assets.names(A.CHARACTER)
        sheet = None
        fw = fh = self.project.tile_size
        if names:
            meta = self.project.assets.get_meta(A.CHARACTER, names[0])
            sheet = self.project.assets.load_image(A.CHARACTER, names[0])
            if meta:
                fw = meta.get("frame_w", fw)
                fh = meta.get("frame_h", fh)
        self.player = Player(sheet, fw, fh, self.project.tile_size)
        sx, sy = self.project.start_position
        self.player.set_tile_position(sx, sy)

    # -- events ----------------------------------------------------------
    def handle_event(self, event: pygame.event.Event) -> None:
        if event.type != pygame.KEYDOWN:
            return
        if event.key == pygame.K_F1:
            self._exit()
        elif event.key == pygame.K_ESCAPE:
            from .menu import MenuScene
            self.next_scene = MenuScene(self.app)
        elif event.key in (pygame.K_z, pygame.K_SPACE, pygame.K_RETURN):
            if self.message.visible:
                self.message.hide()
            else:
                self._interact()

    def _interact(self) -> None:
        if not self.player or not self.tilemap:
            return
        fx, fy = self.player.facing_tile()
        event = self.tilemap.event_at(fx, fy)
        if event and event.kind == "message":
            self.message.show(event.data.get("text", "..."))

    def _exit(self) -> None:
        if self.return_to_editor:
            from .editor import EditorScene
            self.next_scene = EditorScene(self.app, self.project)
        else:
            from .menu import MenuScene
            self.next_scene = MenuScene(self.app)

    # -- update ----------------------------------------------------------
    def update(self, dt: float) -> None:
        if not self.player or not self.tilemap:
            return
        if not self.message.visible:
            keys = pygame.key.get_pressed()
            self.player.handle_movement(dt, keys, self.tilemap)
            # Auto-trigger step-on events.
            self._check_step_event()
        else:
            self.player.moving = False
        self.player.update_animation(dt)

        map_w = self.tilemap.width * self.tilemap.tile_size
        map_h = self.tilemap.height * self.tilemap.tile_size
        self.camera.center_on(self.player.rect.centerx,
                              self.player.rect.centery, map_w, map_h)

    def _check_step_event(self) -> None:
        ts = self.tilemap.tile_size
        cx = self.player.rect.centerx // ts
        cy = self.player.rect.centery // ts
        event = self.tilemap.event_at(cx, cy)
        if event and event.data.get("on_step") and not self.message.visible:
            if event.kind == "message":
                self.message.show(event.data.get("text", "..."))

    # -- draw ------------------------------------------------------------
    def draw(self, surface: pygame.Surface) -> None:
        surface.fill(config.COLOR_BG)
        if self.tilemap and self.tileset:
            # Draw order: ground, player, then object layer so the player
            # walks on the ground but passes behind trees and walls.
            for layer in LAYERS:
                if layer == LAYER_OBJECT:
                    self.player.draw(surface, self.camera)
                self.tilemap.draw_layer(surface, self.tileset, layer,
                                        self.camera)
        elif self.player:
            self.player.draw(surface, self.camera)

        self._draw_hud(surface)
        self.message.draw(surface)

    def _draw_hud(self, surface: pygame.Surface) -> None:
        ui.draw_text(surface, f"{self.project.name}  -  Play mode", (12, 10), 16,
                     config.COLOR_TEXT)
        ui.draw_text(surface, "Arrows/WASD move   Z interact   F1 editor   "
                     "Esc menu", (12, 30), 14, config.COLOR_TEXT_DIM)
