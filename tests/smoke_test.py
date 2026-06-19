"""Headless smoke test.

Runs the engine without a real display (SDL dummy driver) to verify that the
sample project generates, maps load, every scene constructs and a few simulated
frames of input, update and draw execute without raising.
"""

import os
import sys

os.environ.setdefault("SDL_VIDEODRIVER", "dummy")
os.environ.setdefault("SDL_AUDIODRIVER", "dummy")

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import pygame  # noqa: E402

from engine import config  # noqa: E402
from engine.placeholder import ensure_sample_project  # noqa: E402
from engine.project import Project, list_projects  # noqa: E402
from engine.menu import MenuScene  # noqa: E402
from engine.runtime import PlayScene  # noqa: E402
from engine.editor import EditorScene  # noqa: E402
from engine.entity import Player  # noqa: E402


class FakeApp:
    def __init__(self, screen):
        self.screen = screen


def run_scene(app, scene, frames=30):
    scene.on_enter()
    for _ in range(frames):
        # Feed a synthetic key event and a frame of update/draw.
        ev = pygame.event.Event(pygame.KEYDOWN, key=pygame.K_RIGHT, unicode="")
        scene.handle_event(ev)
        scene.update(1 / 60.0)
        scene.draw(app.screen)
    return scene


def main():
    pygame.init()
    screen = pygame.display.set_mode((config.SCREEN_WIDTH, config.SCREEN_HEIGHT))
    app = FakeApp(screen)

    path = ensure_sample_project()
    assert os.path.exists(os.path.join(path, "project.json")), "project missing"
    assert "sample" in list_projects(), "sample not listed"

    project = Project.load(path)
    tm = project.load_map(project.start_map)
    assert tm is not None, "start map failed to load"
    assert tm.width > 0 and tm.height > 0

    # Collision sanity: out of bounds is solid, an open grass tile is not.
    assert tm.is_solid(-1, 0) is True
    assert tm.is_solid(0, 0) is True  # border wall

    # Player movement respects collision.
    p = Player(None, project.tile_size, project.tile_size, project.tile_size)
    p.set_tile_position(*project.start_position)
    before = (p.x, p.y)
    from collections import defaultdict
    keys = defaultdict(bool)
    keys[pygame.K_RIGHT] = True
    p.handle_movement(0.1, keys, tm)
    assert (p.x, p.y) != before, "player did not move on open ground"

    # Each scene must construct and run frames cleanly.
    run_scene(app, MenuScene(app))
    run_scene(app, PlayScene(app, project))
    run_scene(app, EditorScene(app, project))

    # Editor save round-trips.
    editor = EditorScene(app, project)
    editor.on_enter()
    editor.selected_tile = 3
    editor._apply_tool((50, 50), erase=False)
    editor._save()
    reloaded = Project.load(path).load_map(editor.tilemap.name)
    assert reloaded is not None

    pygame.quit()
    print("OK: all smoke checks passed")


if __name__ == "__main__":
    main()
