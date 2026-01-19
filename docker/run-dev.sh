#!/usr/bin/env bash
set -euo pipefail

IMAGE="${IMAGE:-spike-ctr-dev:latest}"
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

docker run --rm -it \
  -v "$ROOT:/work" \
  -w /work \
  -e PATH="/work/.tools/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin" \
  -e SPIKE="/work/.tools/bin/spike" \
  "$IMAGE" \
  bash
