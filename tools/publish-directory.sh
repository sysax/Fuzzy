#!/usr/bin/env bash
# Publica el directorio de clínicas (directory/clinicas.json) en Turso (staging).
# Fase 2 del plan.
#
# El directorio es de solo lectura para la app (D22) y lo publica CI desde
# directory/clinicas.json; aquí se empuja el contenido a la base `fuzzy-directory`.
#
# Uso:
#   tools/publish-directory.sh
#
# Requisitos:
#   · Turso CLI en .tools/bin (o en $TURSO)
#   · Variable TURSO_DB_STAGING (default: fuzzy-directory)
#   · Variable TURSO_API_TOKEN / sesión de Turso para ejecutar SQL

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TURSO="${TURSO:-$ROOT/.tools/bin/turso}"
TURSO_DB="${TURSO_DB_STAGING:-fuzzy-directory}"

if [[ ! -f "$ROOT/directory/clinicas.json" ]]; then
    printf 'Falta %s/directory/clinicas.json\n' "$ROOT" >&2
    exit 1
fi

SQL="$(python3 - "$ROOT/directory/clinicas.json" <<'PY'
import json, sys

with open(sys.argv[1]) as f:
    data = json.load(f)

clinicas = data.get("clinicas", data) if isinstance(data, dict) else data

def ts(v):
    return int(v or 0)

lines = []
for c in clinicas:
    lines.append(
        "INSERT INTO clinicas_publicas (uuid, nombre, ciudad, direccion, telefono, "
        "inbox_url, inbox_token, activa, updated_at) VALUES "
        f"('{c['uuid']}', '{c['nombre']}', '{c.get('ciudad') or ''}', "
        f"'{c.get('direccion') or ''}', '{c.get('telefono') or ''}', "
        f"'{c.get('inbox_url') or ''}', '{c.get('inbox_token') or ''}', "
        f"{1 if c.get('activa', True) else 0}, {ts(c.get('updated_at'))});"
    )

print("\n".join(lines))
PY
)"

"$TURSO" db shell "$TURSO_DB" "$SQL"
