#!/usr/bin/env bash
set -euo pipefail

ROOT="${1:-/work}"
PREFIX="${PREFIX:-$ROOT/.tools}"

rm -rf "$PREFIX"
mkdir -p "$PREFIX"

log() { printf '%s\n' "$*" >&2; }
die() { log "error: $*"; exit 1; }

build_spike() {
  local src="$ROOT"

  if [[ -x "$PREFIX/bin/spike" ]]; then
    log "[ok] spike already built: $PREFIX/bin/spike"
    return 0
  fi

  log "[build] spike -> $PREFIX"
  rm -rf "$src/build"
  mkdir -p "$src/build"

  pushd "$src/build" >/dev/null
  ../configure --prefix="$PREFIX"
  make -j"$(nproc)"
  make install
  popd >/dev/null
}


log "prefix: $PREFIX"

build_spike

log "[done] tools built under $PREFIX"
