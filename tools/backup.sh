#!/usr/bin/env bash
# Respaldo operativo del nodo (fase 10).
#
# Dos rutas:
#   · --db <archivo>   SQLite local del máster, con VACUUM INTO + checksum.
#   · --turso <base>   volcado .dump comprimido de una base Turso.
#
# Los respaldos van a backups/ (ignorado por Git) o a FUZZY_BACKUP_DIR.
#
# Uso:
#   tools/backup.sh --db db/local/clinic.db
#   tools/backup.sh --turso fuzzy-clinic-<slug>
#   tools/backup.sh --db <archivo> --out /ruta/externa --keep 30
#
# Requisitos:
#   · fuzzy-admin compilado para respaldos locales
#   · Turso CLI para respaldos remotos (TURSO o .tools/bin/turso)
#   · gzip, sha256sum

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ADMIN="${FUZZY_ADMIN:-$ROOT/build/fuzzy_admin/fuzzy-admin}"
TURSO="${TURSO:-$ROOT/.tools/bin/turso}"
OUT="${FUZZY_BACKUP_DIR:-$ROOT/backups}"
KEEP=14
DB=""
TURSO_DB=""

uso() {
    sed -n '2,20p' "$0"
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --db)
            DB="${2:-}"; shift 2 ;;
        --turso)
            TURSO_DB="${2:-}"; shift 2 ;;
        --out)
            OUT="${2:-}"; shift 2 ;;
        --keep)
            KEEP="${2:-}"; shift 2 ;;
        --help|-h)
            uso; exit 0 ;;
        *)
            printf 'Opción desconocida: %s\n' "$1" >&2
            uso >&2
            exit 1 ;;
    esac
done

if [[ -z "$DB" && -z "$TURSO_DB" ]]; then
    printf 'Indica --db o --turso.\n' >&2
    exit 1
fi
if [[ -n "$DB" && -n "$TURSO_DB" ]]; then
    printf 'Usa --db o --turso, no ambos.\n' >&2
    exit 1
fi
if ! [[ "$KEEP" =~ ^[0-9]+$ ]] || [[ "$KEEP" -lt 1 ]]; then
    printf '--keep debe ser un entero positivo.\n' >&2
    exit 1
fi

mkdir -p "$OUT"
TS="$(date -u +%Y%m%dT%H%M%SZ)"

rotar() {
    local prefijo="$1" extension="$2"
    local -a archivos=()
    mapfile -t archivos < <(
        find "$OUT" -maxdepth 1 -type f -name "${prefijo}-*${extension}" \
            -printf '%T@ %p\n' | sort -n | awk '{print $2}'
    )
    local sobrantes=$(( ${#archivos[@]} - KEEP ))
    local i
    for (( i = 0; i < sobrantes; i++ )); do
        local archivo="${archivos[$i]}"
        rm -f "$archivo" "$archivo.sha256" "$archivo.json"
    done
}

if [[ -n "$DB" ]]; then
    if [[ ! -x "$ADMIN" ]]; then
        printf 'No existe %s. Compila fuzzy_admin antes de respaldar.\n' "$ADMIN" >&2
        exit 1
    fi
    if [[ ! -f "$DB" ]]; then
        printf 'No existe la base: %s\n' "$DB" >&2
        exit 1
    fi

    NOMBRE="$(basename "$DB")"
    NOMBRE="${NOMBRE%.db}"
    DESTINO="$OUT/${NOMBRE}-${TS}.db"
    "$ADMIN" respaldar --db "$DB" --salida "$DESTINO"
    rotar "$NOMBRE" ".db"
    printf 'Respaldo local listo: %s\n' "$DESTINO"
    exit 0
fi

if [[ ! -x "$TURSO" ]]; then
    printf 'No existe Turso CLI en %s. Define TURSO.\n' "$TURSO" >&2
    exit 1
fi

DESTINO="$OUT/turso-${TURSO_DB}-${TS}.sql.gz"
"$TURSO" db shell "$TURSO_DB" .dump | gzip -9 > "$DESTINO"

if [[ ! -s "$DESTINO" ]] || ! gzip -t "$DESTINO"; then
    printf 'El volcado de %s quedó vacío o corrupto.\n' "$TURSO_DB" >&2
    rm -f "$DESTINO"
    exit 1
fi

SHA="$(sha256sum "$DESTINO" | awk '{print $1}')"
printf '%s  %s\n' "$SHA" "$(basename "$DESTINO")" > "$DESTINO.sha256"
BYTES="$(stat -c '%s' "$DESTINO")"
printf '{\n  "formato": 1,\n  "base": "%s",\n  "ruta": "%s",\n  "sha256": "%s",\n  "bytes": %s,\n  "creado_at": "%s"\n}\n' \
    "$TURSO_DB" "$DESTINO" "$SHA" "$BYTES" "$TS" > "$DESTINO.json"

rotar "turso-${TURSO_DB}" ".sql.gz"
printf 'Respaldo remoto listo: %s\n' "$DESTINO"
