"""Minimal immediate-mode style UI helpers used by the menu and editor."""

from __future__ import annotations

from typing import Callable, Optional

import pygame

from . import config

_font_cache: dict[int, pygame.font.Font] = {}


def get_font(size: int = 18) -> pygame.font.Font:
    if size not in _font_cache:
        _font_cache[size] = pygame.font.SysFont("consolas,dejavusansmono,monospace",
                                                 size)
    return _font_cache[size]


def draw_text(surface: pygame.Surface, text: str, pos, size: int = 18,
              color=config.COLOR_TEXT, center: bool = False) -> pygame.Rect:
    font = get_font(size)
    img = font.render(text, True, color)
    rect = img.get_rect()
    if center:
        rect.center = pos
    else:
        rect.topleft = pos
    surface.blit(img, rect)
    return rect


class Button:
    def __init__(self, rect: pygame.Rect, label: str,
                 on_click: Optional[Callable[[], None]] = None, size: int = 20):
        self.rect = pygame.Rect(rect)
        self.label = label
        self.on_click = on_click
        self.size = size
        self.hovered = False

    def handle_event(self, event: pygame.event.Event) -> bool:
        if event.type == pygame.MOUSEMOTION:
            self.hovered = self.rect.collidepoint(event.pos)
        elif event.type == pygame.MOUSEBUTTONDOWN and event.button == 1:
            if self.rect.collidepoint(event.pos):
                if self.on_click:
                    self.on_click()
                return True
        return False

    def draw(self, surface: pygame.Surface) -> None:
        color = config.COLOR_PANEL_LIGHT if self.hovered else config.COLOR_PANEL
        pygame.draw.rect(surface, color, self.rect, border_radius=6)
        pygame.draw.rect(surface, config.COLOR_ACCENT, self.rect, width=2,
                         border_radius=6)
        draw_text(surface, self.label, self.rect.center, self.size,
                  config.COLOR_TEXT, center=True)


class MessageBox:
    """A bottom-of-screen dialogue box shown during play."""

    def __init__(self):
        self.text = ""
        self.visible = False

    def show(self, text: str) -> None:
        self.text = text
        self.visible = True

    def hide(self) -> None:
        self.visible = False

    def draw(self, surface: pygame.Surface) -> None:
        if not self.visible:
            return
        w, h = surface.get_size()
        box = pygame.Rect(40, h - 140, w - 80, 100)
        panel = pygame.Surface(box.size, pygame.SRCALPHA)
        panel.fill((20, 22, 30, 235))
        surface.blit(panel, box.topleft)
        pygame.draw.rect(surface, config.COLOR_ACCENT, box, width=2,
                         border_radius=8)
        draw_text(surface, self.text, (box.x + 16, box.y + 16), 20)
        draw_text(surface, "[Z / Space] continue", (box.x + 16, box.bottom - 28),
                  14, config.COLOR_TEXT_DIM)
