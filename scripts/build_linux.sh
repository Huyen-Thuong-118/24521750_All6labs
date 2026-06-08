#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
: "${VCPKG_ROOT:=$HOME/vcpkg}"

cmake -B "$ROOT/build/linux" -S "$ROOT" -G Ninja \
      -DCMAKE_BUILD_TYPE=Release \
      "-DCMAKE_TOOLCHAIN_FILE=$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"

cmake --build "$ROOT/build/linux" --parallel "$(nproc)"
ctest --test-dir "$ROOT/build/linux" --output-on-failure
