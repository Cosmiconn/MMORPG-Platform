#!/usr/bin/env bash
set -euo pipefail
PRESET="${1:-linux-release}"
cmake --preset "$PRESET"
cmake --build --preset "$PRESET" --parallel $(nproc)
if [[ "$PRESET" == *"debug"* ]]; then
  ctest --preset "$PRESET" --output-on-failure
fi
