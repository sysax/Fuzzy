#!/usr/bin/env bash
# Simulacro de reversión del máster de clínica (fase 10).
#
# Crea una base SQLite efímera con el esquema real, toma un respaldo con la
# CLI, introduce un cambio posterior y restaura el respaldo. Comprueba que el
# cambio desaparece y que la base restaurada conserva la integridad.
#
# No toca db/local/ ni datos reales: todo vive en un directorio temporal.
#
# Uso:
#   tools/rollback-drill.sh
#
# Requisitos:
#   · fuzzy-admin compilado (FUZZY_ADMIN o build/fuzzy_admin/fuzzy-admin)
#   · sqlite3

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ADMIN="${FUZZY_ADMIN:-$ROOT/build/fuzzy_admin/fuzzy-admin}"

if [[ "${1:-}" == "--help" || "${1:-}" == "-h" ]]; then
    sed -n '2,13p' "$0"
    exit 0
fi

if [[ ! -x "$ADMIN" ]]; then
    printf 'No existe %s. Compila fuzzy_admin antes del simulacro.\n' "$ADMIN" >&2
    exit 1
fi
if ! command -v sqlite3 >/dev/null 2>&1; then
    printf 'Falta sqlite3 en PATH.\n' >&2
    exit 1
fi

TMP="$(mktemp -d "${TMPDIR:-/tmp}/fuzzy-rollback.XXXXXX")"
trap 'rm -rf "$TMP"' EXIT

DB="$TMP/clinica.db"
RESPALDO="$TMP/clinica-respaldo.db"

printf '== esquema efímero ==\n'
"$ADMIN" migrar --db "$DB" >/dev/null

sqlite3 "$DB" "INSERT INTO clinicas (uuid, nombre, created_at, updated_at)
               VALUES ('dddddddd-dddd-4ddd-8ddd-dddddddddd01',
                       'Clínica Simulacro', 1, 1);"

printf '== respaldo verificado ==\n'
"$ADMIN" respaldar --db "$DB" --salida "$RESPALDO" >/dev/null
(cd "$TMP" && sha256sum -c "clinica-respaldo.db.sha256" >/dev/null)

printf '== cambio posterior al respaldo ==\n'
sqlite3 "$DB" "CREATE TABLE marcador_reversion (x INTEGER);
               INSERT INTO marcador_reversion VALUES (1);"
if [[ "$(sqlite3 "$DB" "SELECT count(*) FROM marcador_reversion;")" != "1" ]]; then
    printf 'No se pudo preparar el cambio posterior.\n' >&2
    exit 1
fi

printf '== restauración ==\n'
"$ADMIN" restaurar --db "$DB" --desde "$RESPALDO" --confirmar >/dev/null

if [[ "$(sqlite3 "$DB" "SELECT count(*) FROM sqlite_master
                        WHERE type = 'table' AND name = 'marcador_reversion';")" != "0" ]]; then
    printf 'La restauración no revirtió el cambio posterior.\n' >&2
    exit 1
fi
if [[ "$(sqlite3 "$DB" "SELECT nombre FROM clinicas LIMIT 1;")" != "Clínica Simulacro" ]]; then
    printf 'La restauración perdió los datos del respaldo.\n' >&2
    exit 1
fi

printf '== estado final ==\n'
ESTADO="$("$ADMIN" estado --db "$DB" --json)"
if [[ "$ESTADO" != *'"ok":true'* || "$ESTADO" != *'"version_esquema":"0006"'* ]]; then
    printf 'El estado final no es válido: %s\n' "$ESTADO" >&2
    exit 1
fi

printf 'Simulacro de reversión correcto (respaldo, cambio, restauración y checksum).\n'
