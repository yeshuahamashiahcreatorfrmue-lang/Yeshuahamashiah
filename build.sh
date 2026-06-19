#!/usr/bin/env bash
# One-click build for Linux / macOS.
# Produces ./bin/TsukuruEngine (the editor + game runtime) and ./bin/tsukuru_selftest.
set -e
cd "$(dirname "$0")"

echo "==> Configuring (Release)..."
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

echo "==> Building..."
cmake --build build -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"

echo
echo "Done!  Run the engine with:"
echo "    ./bin/TsukuruEngine"
echo
echo "(First run loads the bundled sample game 'Hero's Adventure'.)"
