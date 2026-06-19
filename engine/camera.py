"""A simple camera that follows a target and clamps to the map bounds."""

from __future__ import annotations


class Camera:
    def __init__(self, width: int, height: int):
        self.x = 0
        self.y = 0
        self.width = width
        self.height = height

    def center_on(self, px: float, py: float, map_pixel_w: int,
                  map_pixel_h: int) -> None:
        self.x = int(px - self.width // 2)
        self.y = int(py - self.height // 2)
        self._clamp(map_pixel_w, map_pixel_h)

    def move(self, dx: int, dy: int, map_pixel_w: int, map_pixel_h: int) -> None:
        self.x += dx
        self.y += dy
        self._clamp(map_pixel_w, map_pixel_h)

    def _clamp(self, map_pixel_w: int, map_pixel_h: int) -> None:
        # If the map is smaller than the view, keep it pinned at origin.
        max_x = max(0, map_pixel_w - self.width)
        max_y = max(0, map_pixel_h - self.height)
        self.x = max(0, min(self.x, max_x))
        self.y = max(0, min(self.y, max_y))
