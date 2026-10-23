#!/usr/bin/env bash
# Reconstruye las cuatro bases locales desde cero y verifica su integridad.
# Son bases vacías con el esquema aplicado: nunca datos reales.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

resolver_atlas() {
    if [[ -n "${ATLAS:-}" ]]; then
        printf '%s\n' "$ATLAS"
    elif command -v atlas >/dev/null 2>&1; then
        command -v atlas
    elif [[ -x "$ROOT/.tools/bin/atlas" ]]; then
        printf '%s\n' "$ROOT/.tools/bin/atlas"
    else
        printf 'No se encontró atlas. Ejecuta tools/install-tools.sh o define ATLAS.\n' >&2
        exit 1
    fi
}

ATLAS_BIN="$(resolver_atlas)"
OUT="$ROOT/db/local"

rm -rf "$OUT"
mkdir -p "$OUT"

for db in clinic portal inbox directory; do
    "$ATLAS_BIN" migrate apply \
        --config "file://$ROOT/db/atlas.hcl" \
        --env "$db"
done

verificar_tabla() {
    local db="$1" tabla="$2"
    local existe
    existe="$(sqlite3 "$OUT/$db.db" \
        "SELECT count(*) FROM sqlite_master WHERE type='table' AND name='$tabla';")"
    if [[ "$existe" != "1" ]]; then
        printf 'Falta la tabla %s en %s.db\n' "$tabla" "$db" >&2
        exit 1
    fi

    local integridad
    integridad="$(sqlite3 "$OUT/$db.db" 'PRAGMA integrity_check;')"
    if [[ "$integridad" != "ok" ]]; then
        printf 'Integridad fallida en %s.db: %s\n' "$db" "$integridad" >&2
        exit 1
    fi
}

verificar_tabla clinic clinicas
verificar_tabla clinic instalacion
verificar_tabla portal portal_meta
verificar_tabla inbox altas
verificar_tabla directory clinicas_publicas

printf 'Bases reconstruidas en db/local/\n'
