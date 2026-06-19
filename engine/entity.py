"""Entities: animated characters and the player controller.

Characters use an RPG-Maker style sprite sheet laid out as 3 walking frames
across and 4 facing directions down (down, left, right, up). Movement is pixel
based but collision is resolved against the tile grid so that the player can
never enter a solid cell.
"""

from __future__ import annotations

import pygame

# Direction -> sprite sheet row.
DIR_DOWN = 0
DIR_LEFT = 1
DIR_RIGHT = 2
DIR_UP = 3

# Walk animation cycles through these columns.
WALK_FRAMES = (0, 1, 2, 1)


class Character:
    """A drawable, animated actor positioned in pixel space."""

    def __init__(self, sheet: pygame.Surface | None, frame_w: int, frame_h: int,
                 tile_size: int):
        self.sheet = sheet
        self.frame_w = frame_w
        self.frame_h = frame_h
        self.tile_size = tile_size

        self.x = 0.0
        self.y = 0.0
        self.direction = DIR_DOWN
        self.moving = False

        self._anim_time = 0.0
        self._anim_index = 0

        self._frames = {}
        if sheet is not None:
            self._slice_frames()

    def _slice_frames(self) -> None:
        cols = max(1, self.sheet.get_width() // self.frame_w)
        for d in range(4):
            row_frames = []
            for c in range(min(3, cols)):
                rect = pygame.Rect(c * self.frame_w, d * self.frame_h,
                                   self.frame_w, self.frame_h)
                if (rect.right <= self.sheet.get_width()
                        and rect.bottom <= self.sheet.get_height()):
                    row_frames.append(self.sheet.subsurface(rect).copy())
            if row_frames:
                self._frames[d] = row_frames

    # -- placement -------------------------------------------------------
    def set_tile_position(self, tx: int, ty: int) -> None:
        ts = self.tile_size
        # Centre the sprite horizontally on the tile, feet at the bottom.
        self.x = tx * ts + (ts - self.frame_w) / 2
        self.y = ty * ts + (ts - self.frame_h)

    @property
    def rect(self) -> pygame.Rect:
        """Collision box: the bottom tile-sized portion of the sprite."""
        ts = self.tile_size
        return pygame.Rect(int(self.x + (self.frame_w - ts) / 2),
                           int(self.y + self.frame_h - ts), ts, ts)

    # -- animation -------------------------------------------------------
    def update_animation(self, dt: float) -> None:
        if self.moving:
            self._anim_time += dt
            if self._anim_time >= 0.15:
                self._anim_time = 0.0
                self._anim_index = (self._anim_index + 1) % len(WALK_FRAMES)
        else:
            self._anim_index = 0
            self._anim_time = 0.0

    def draw(self, surface: pygame.Surface, camera) -> None:
        screen_pos = (int(self.x - camera.x), int(self.y - camera.y))
        frames = self._frames.get(self.direction)
        if frames:
            col = WALK_FRAMES[self._anim_index] % len(frames)
            surface.blit(frames[col], screen_pos)
        else:
            # Fallback: a coloured square so the actor is always visible.
            pygame.draw.rect(surface, (230, 90, 90),
                             pygame.Rect(screen_pos, (self.frame_w, self.frame_h)))


class Player(Character):
    """Player controller with collision against the map."""

    SPEED = 140.0  # pixels per second

    def handle_movement(self, dt: float, keys, tilemap) -> None:
        dx = dy = 0.0
        if keys[pygame.K_LEFT] or keys[pygame.K_a]:
            dx -= 1
            self.direction = DIR_LEFT
        elif keys[pygame.K_RIGHT] or keys[pygame.K_d]:
            dx += 1
            self.direction = DIR_RIGHT
        elif keys[pygame.K_UP] or keys[pygame.K_w]:
            dy -= 1
            self.direction = DIR_UP
        elif keys[pygame.K_DOWN] or keys[pygame.K_s]:
            dy += 1
            self.direction = DIR_DOWN

        self.moving = dx != 0 or dy != 0
        if not self.moving:
            return

        step = self.SPEED * dt
        self._try_move(dx * step, 0, tilemap)
        self._try_move(0, dy * step, tilemap)

    def _try_move(self, dx: float, dy: float, tilemap) -> None:
        if dx == 0 and dy == 0:
            return
        new_x = self.x + dx
        new_y = self.y + dy
        box = self.rect.move(int(new_x - self.x), int(new_y - self.y))
        if self._box_free(box, tilemap):
            self.x = new_x
            self.y = new_y

    @staticmethod
    def _box_free(box: pygame.Rect, tilemap) -> bool:
        ts = tilemap.tile_size
        # Check every tile the collision box overlaps.
        for tx in range(box.left // ts, (box.right - 1) // ts + 1):
            for ty in range(box.top // ts, (box.bottom - 1) // ts + 1):
                if tilemap.is_solid(tx, ty):
                    return False
        return True

    def facing_tile(self) -> tuple[int, int]:
        """Tile coordinate directly in front of the player."""
        ts = self.tile_size
        cx = self.rect.centerx // ts
        cy = self.rect.centery // ts
        if self.direction == DIR_LEFT:
            return cx - 1, cy
        if self.direction == DIR_RIGHT:
            return cx + 1, cy
        if self.direction == DIR_UP:
            return cx, cy - 1
        return cx, cy + 1
