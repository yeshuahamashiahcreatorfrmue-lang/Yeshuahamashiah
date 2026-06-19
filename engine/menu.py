"""Main menu: pick a project, then play it or open it in the editor."""

from __future__ import annotations

import pygame

from . import config, ui
from .project import Project, list_projects
from .scene import Scene


class MenuScene(Scene):
    def __init__(self, app):
        super().__init__(app)
        self.projects = list_projects()
        self.selected = 0 if self.projects else -1
        self.prompt_active = False
        self.new_name = ""
        self._buttons: list[ui.Button] = []

    def on_enter(self) -> None:
        self._build_buttons()

    def _build_buttons(self) -> None:
        cx = config.SCREEN_WIDTH // 2
        self._buttons = [
            ui.Button((cx - 110, 360, 220, 44), "Play", self._play),
            ui.Button((cx - 110, 414, 220, 44), "Open Editor", self._edit),
            ui.Button((cx - 110, 468, 220, 44), "New Project", self._new),
            ui.Button((cx - 110, 522, 220, 44), "Quit", self._quit),
        ]

    # -- actions ---------------------------------------------------------
    def _current_project(self) -> Project | None:
        if 0 <= self.selected < len(self.projects):
            from . import config as _c
            import os
            path = os.path.join(_c.projects_dir(), self.projects[self.selected])
            return Project.load(path)
        return None

    def _play(self) -> None:
        project = self._current_project()
        if project:
            from .runtime import PlayScene
            self.next_scene = PlayScene(self.app, project)

    def _edit(self) -> None:
        project = self._current_project()
        if project:
            from .editor import EditorScene
            self.next_scene = EditorScene(self.app, project)

    def _new(self) -> None:
        self.prompt_active = True
        self.new_name = ""

    def _quit(self) -> None:
        self.quit = True

    def _create_project(self, name: str) -> None:
        name = name.strip().replace(" ", "_")
        if not name:
            return
        project = Project.create(name)
        # Give the new project one empty map to start from.
        from .tilemap import TileMap
        tm = TileMap("map1", 24, 16, project.tile_size, "")
        project.add_map(tm)
        project.save()
        self.projects = list_projects()
        self.selected = self.projects.index(name)
        from .editor import EditorScene
        self.next_scene = EditorScene(self.app, project)

    # -- events ----------------------------------------------------------
    def handle_event(self, event: pygame.event.Event) -> None:
        if self.prompt_active:
            self._handle_prompt(event)
            return
        for btn in self._buttons:
            btn.handle_event(event)
        if event.type == pygame.KEYDOWN:
            if event.key in (pygame.K_DOWN, pygame.K_s):
                self.selected = min(len(self.projects) - 1, self.selected + 1)
            elif event.key in (pygame.K_UP, pygame.K_w):
                self.selected = max(0, self.selected - 1)
            elif event.key == pygame.K_RETURN:
                self._play()
            elif event.key == pygame.K_e:
                self._edit()
        elif event.type == pygame.MOUSEBUTTONDOWN and event.button == 1:
            self._click_project(event.pos)

    def _handle_prompt(self, event: pygame.event.Event) -> None:
        if event.type != pygame.KEYDOWN:
            return
        if event.key == pygame.K_RETURN:
            self.prompt_active = False
            self._create_project(self.new_name)
        elif event.key == pygame.K_ESCAPE:
            self.prompt_active = False
        elif event.key == pygame.K_BACKSPACE:
            self.new_name = self.new_name[:-1]
        elif event.unicode and event.unicode.isprintable():
            self.new_name += event.unicode

    def _click_project(self, pos) -> None:
        for i in range(len(self.projects)):
            rect = pygame.Rect(config.SCREEN_WIDTH // 2 - 160, 180 + i * 34,
                               320, 30)
            if rect.collidepoint(pos):
                self.selected = i

    # -- draw ------------------------------------------------------------
    def draw(self, surface: pygame.Surface) -> None:
        surface.fill(config.COLOR_BG)
        ui.draw_text(surface, config.TITLE,
                     (config.SCREEN_WIDTH // 2, 90), 40, config.COLOR_ACCENT,
                     center=True)
        ui.draw_text(surface, "RPG Maker style 2D game engine",
                     (config.SCREEN_WIDTH // 2, 130), 18, config.COLOR_TEXT_DIM,
                     center=True)

        ui.draw_text(surface, "Projects:",
                     (config.SCREEN_WIDTH // 2 - 160, 156), 16,
                     config.COLOR_TEXT_DIM)
        if not self.projects:
            ui.draw_text(surface, "(none - create one)",
                         (config.SCREEN_WIDTH // 2 - 160, 184), 16,
                         config.COLOR_TEXT_DIM)
        for i, name in enumerate(self.projects):
            rect = pygame.Rect(config.SCREEN_WIDTH // 2 - 160, 180 + i * 34,
                               320, 30)
            sel = i == self.selected
            pygame.draw.rect(surface,
                             config.COLOR_PANEL_LIGHT if sel else config.COLOR_PANEL,
                             rect, border_radius=4)
            if sel:
                pygame.draw.rect(surface, config.COLOR_ACCENT, rect, 2,
                                 border_radius=4)
            ui.draw_text(surface, name, (rect.x + 10, rect.y + 6), 16)

        for btn in self._buttons:
            btn.draw(surface)

        if self.prompt_active:
            self._draw_prompt(surface)

    def _draw_prompt(self, surface: pygame.Surface) -> None:
        w, h = surface.get_size()
        overlay = pygame.Surface((w, h), pygame.SRCALPHA)
        overlay.fill((0, 0, 0, 150))
        surface.blit(overlay, (0, 0))
        box = pygame.Rect(w // 2 - 220, h // 2 - 50, 440, 100)
        pygame.draw.rect(surface, config.COLOR_PANEL, box, border_radius=8)
        pygame.draw.rect(surface, config.COLOR_ACCENT, box, 2, border_radius=8)
        ui.draw_text(surface, "New project name:", (box.x + 16, box.y + 12), 18)
        field = pygame.Rect(box.x + 16, box.y + 44, box.width - 32, 30)
        pygame.draw.rect(surface, (20, 20, 28), field, border_radius=4)
        ui.draw_text(surface, self.new_name + "_", (field.x + 8, field.y + 6), 18)
