"""Application bootstrap and the main loop / scene manager."""

from __future__ import annotations

import pygame

from . import config
from .menu import MenuScene
from .placeholder import ensure_sample_project
from .scene import Scene


class App:
    """Owns the window, the clock and the active scene."""

    def __init__(self):
        pygame.init()
        try:
            pygame.mixer.init()
        except pygame.error:
            # Audio is optional; carry on without it.
            pass
        self.screen = pygame.display.set_mode(
            (config.SCREEN_WIDTH, config.SCREEN_HEIGHT))
        pygame.display.set_caption(config.TITLE)
        self.clock = pygame.time.Clock()
        self.running = True
        self.scene: Scene | None = None

    def set_scene(self, scene: Scene) -> None:
        self.scene = scene
        self.scene.on_enter()

    def run(self) -> None:
        # Make sure there is always something to play / edit.
        ensure_sample_project(config.DEFAULT_TILE_SIZE)
        self.set_scene(MenuScene(self))

        while self.running:
            dt = self.clock.tick(config.FPS) / 1000.0
            dt = min(dt, 0.05)  # Clamp so a stutter never tunnels collisions.

            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    self.running = False
                elif self.scene:
                    self.scene.handle_event(event)

            if self.scene:
                self.scene.update(dt)
                self.scene.draw(self.screen)

                if self.scene.quit:
                    self.running = False
                elif self.scene.next_scene is not None:
                    self.set_scene(self.scene.next_scene)

            pygame.display.flip()

        pygame.quit()


def main() -> None:
    App().run()
