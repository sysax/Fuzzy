#!/usr/bin/env bash
# Provisiona clínicas en Turso (staging) con un solo comando (Fase 2).
#
# Por cada clínica crea:
#   · grupo Turso `fuzzy-clinic-<slug>` (D18: un grupo por clínica)
#   · base `fuzzy-clinic-<slug>` (dominio de la sede)
#   · base `fuzzy-inbox-<slug>` (buzón de altas de primera visita)
#   · token de grupo para el nodo (D1: el sync vive embebido en el nodo)
#
# El token del nodo (grupo) nunca sale del nodo; la app del dueño no pasa por
# él, va directo a su portal en Turso (D1/D26).
#
# Uso:
#   tools/provision-clinic.sh            # provisiona las dos clínicas de staging
#   tools/provision-clinic.sh --help
#
# Requisitos:
#   · Turso CLI en .tools/bin (o en $TURSO)
#   · Operador autenticado: `turso auth login`

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TURSO="${TURSO:-$ROOT/.tools/bin/turso}"

provision_clinic() {
    local slug="$1"
    local nombre="$2"
    local group="fuzzy-clinic-${slug}"

    echo "Provisionando clínica: ${nombre} (${slug})"

    "$TURSO" group create "$group" --wait
    "$TURSO" db create "fuzzy-clinic-${slug}" --group "$group" --wait
    "$TURSO" db create "fuzzy-inbox-${slug}" --group "$group" --wait

    local token
    token="$("$TURSO" db tokens create "fuzzy-clinic-${slug}" --group)"
    echo "Token de grupo de clínica ${slug}: ${token}"
}

provision_staging() {
    provision_clinic "staging-1" "Clínica Veterinaria Staging 1"
    provision_clinic "staging-2" "Clínica Veterinaria Staging 2"
}

if [[ "${1:-}" == "--help" || "${1:-}" == "-h" ]]; then
    sed -n '3,12p' "$0"
    exit 0
fi

provision_staging
