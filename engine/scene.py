"""Base class for application scenes (menu, play, editor)."""

from __future__ import annotations

import pygame


class Scene:
    """A screen with its own event handling, update and draw logic.

    Scenes may request a transition by setting :attr:`next_scene` to another
    Scene instance, or request shutdown by setting :attr:`quit` to True. The
    owning :class:`~engine.app.App` polls these after each frame.
    """

    def __init__(self, app):
        self.app = app
        self.next_scene: "Scene | None" = None
        self.quit = False

    def on_enter(self) -> None:
        """Called once when the scene becomes active."""

    def handle_event(self, event: pygame.event.Event) -> None:
        ...

    def update(self, dt: float) -> None:
        ...

    def draw(self, surface: pygame.Surface) -> None:
        ...
