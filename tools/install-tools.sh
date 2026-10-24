#!/usr/bin/env bash
# Instala el toolchain de desarrollo en .tools/ (sin sudo, fuera de Git).
# Fija versiones: las mismas que usa CI.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="$ROOT/.tools/bin"
ATLAS_VERSION="v1.3.0"
TURSO_VERSION="v1.0.34"

mkdir -p "$BIN"

if [[ ! -x "$BIN/atlas" ]]; then
    printf 'Descargando atlas %s\n' "$ATLAS_VERSION"
    curl -sSfL \
        "https://atlasbinaries.com/atlas/atlas-community-linux-amd64-$ATLAS_VERSION" \
        -o "$BIN/atlas"
    chmod +x "$BIN/atlas"
fi

if [[ ! -x "$BIN/turso" ]]; then
    printf 'Descargando turso %s\n' "$TURSO_VERSION"
    tmp="$(mktemp -d)"
    curl -sSfL \
        "https://github.com/tursodatabase/turso-cli/releases/download/$TURSO_VERSION/turso-cli_Linux_x86_64.tar.gz" \
        -o "$tmp/turso.tar.gz"
    tar -xzf "$tmp/turso.tar.gz" -C "$BIN"
    rm -rf "$tmp"
    chmod +x "$BIN/turso"
fi

printf 'atlas: %s\n' "$("$BIN/atlas" version | head -1)"
printf 'turso: %s\n' "$("$BIN/turso" --version)"
