#!/usr/bin/env bash
# Alta de una clínica de producción en Turso (fase 10).
#
# Crea el grupo de la clínica, las bases de dominio y buzón, emite el token de
# grupo del nodo y aplica las migraciones de clínica y buzón. El token se
# escribe una sola vez en un archivo 0600 fuera de Git (secrets/ por defecto).
#
# No crea portales: cada portal nace al aprobar un vínculo desde el escritorio.
# Tampoco publica el directorio: eso lo hace CI desde directory/clinicas.json.
#
# Uso:
#   tools/provision-production.sh --slug aurora --nombre "Clínica Aurora" \
#       --ciudad Bogotá --confirmar-produccion
#   tools/provision-production.sh --slug aurora --nombre "Clínica Aurora" --dry-run
#
# Requisitos:
#   · Turso CLI autenticada como dueña de la cuenta de la clínica (D18)
#   · Atlas (ATLAS, PATH o .tools/bin/atlas)
#   · Token de plataforma solo para crear; el nodo recibe el token del grupo.

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TURSO="${TURSO:-$ROOT/.tools/bin/turso}"
ATLAS="${ATLAS:-$ROOT/.tools/bin/atlas}"

SLUG=""
NOMBRE=""
CIUDAD=""
DIRECCION=""
TELEFONO=""
CONFIRMAR=0
DRY_RUN=0
TOKEN_FILE=""

uso() {
    sed -n '2,22p' "$0"
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --slug) SLUG="${2:-}"; shift 2 ;;
        --nombre) NOMBRE="${2:-}"; shift 2 ;;
        --ciudad) CIUDAD="${2:-}"; shift 2 ;;
        --direccion) DIRECCION="${2:-}"; shift 2 ;;
        --telefono) TELEFONO="${2:-}"; shift 2 ;;
        --token-file) TOKEN_FILE="${2:-}"; shift 2 ;;
        --confirmar-produccion) CONFIRMAR=1; shift ;;
        --dry-run) DRY_RUN=1; shift ;;
        --help|-h) uso; exit 0 ;;
        *) printf 'Opción desconocida: %s\n' "$1" >&2; uso >&2; exit 1 ;;
    esac
done

if [[ -z "$SLUG" || -z "$NOMBRE" ]]; then
    printf 'Faltan --slug y --nombre.\n' >&2
    uso >&2
    exit 1
fi
if ! [[ "$SLUG" =~ ^[a-z0-9]([a-z0-9-]{0,30}[a-z0-9])?$ ]]; then
    printf 'Slug inválido: usa minúsculas, números y guiones (2–32).\n' >&2
    exit 1
fi
if [[ "$DRY_RUN" -eq 0 && "$CONFIRMAR" -ne 1 ]]; then
    printf 'Producción exige --confirmar-produccion (o usa --dry-run).\n' >&2
    exit 1
fi

TOKEN_FILE="${TOKEN_FILE:-$ROOT/secrets/produccion-${SLUG}.token}"
if [[ "$TOKEN_FILE" == "$ROOT/"* && "$TOKEN_FILE" != "$ROOT/secrets/"* ]]; then
    printf 'El token debe quedar en secrets/ (ignorado por Git) o fuera del repositorio.\n' >&2
    exit 1
fi
if [[ -e "$TOKEN_FILE" ]]; then
    printf 'Ya existe %s; no se sobrescribe un token.\n' "$TOKEN_FILE" >&2
    exit 1
fi

GRUPO="fuzzy-clinic-${SLUG}"
CLINICA_DB="fuzzy-clinic-${SLUG}"
INBOX_DB="fuzzy-inbox-${SLUG}"

imprimir_plan() {
    printf 'Clínica:      %s (%s)\n' "$NOMBRE" "$SLUG"
    printf 'Grupo:        %s\n' "$GRUPO"
    printf 'Base dominio: %s\n' "$CLINICA_DB"
    printf 'Base buzón:   %s\n' "$INBOX_DB"
    printf 'Token:        %s (0600, fuera de Git)\n' "$TOKEN_FILE"
    if [[ -n "$CIUDAD" ]]; then printf 'Ciudad:       %s\n' "$CIUDAD"; fi
    if [[ -n "$DIRECCION" ]]; then printf 'Dirección:    %s\n' "$DIRECCION"; fi
    if [[ -n "$TELEFONO" ]]; then printf 'Teléfono:     %s\n' "$TELEFONO"; fi
}

if [[ "$DRY_RUN" -eq 1 ]]; then
    printf '== simulación (no se ejecuta nada) ==\n'
    imprimir_plan
    printf '\nComandos:\n'
    printf '  turso group create %s --wait\n' "$GRUPO"
    printf '  turso db create %s --group %s --wait\n' "$CLINICA_DB" "$GRUPO"
    printf '  turso db create %s --group %s --wait\n' "$INBOX_DB" "$GRUPO"
    printf '  turso db tokens create %s --group > %s\n' "$CLINICA_DB" "$TOKEN_FILE"
    printf '  atlas migrate apply --dir file://db/migrations/clinic --url "libsql://%s?authToken=<token>"\n' "$CLINICA_DB"
    printf '  atlas migrate apply --dir file://db/migrations/inbox --url "libsql://%s?authToken=<token>"\n' "$INBOX_DB"
    exit 0
fi

if [[ ! -x "$TURSO" ]]; then
    printf 'No existe Turso CLI en %s. Define TURSO.\n' "$TURSO" >&2
    exit 1
fi
if [[ ! -x "$ATLAS" ]]; then
    printf 'No existe Atlas en %s. Define ATLAS.\n' "$ATLAS" >&2
    exit 1
fi

printf '== alta de producción ==\n'
imprimir_plan

"$TURSO" group create "$GRUPO" --wait
"$TURSO" db create "$CLINICA_DB" --group "$GRUPO" --wait
"$TURSO" db create "$INBOX_DB" --group "$GRUPO" --wait

umask 077
mkdir -p "$(dirname "$TOKEN_FILE")"
chmod 700 "$(dirname "$TOKEN_FILE")"
TOKEN="$("$TURSO" db tokens create "$CLINICA_DB" --group)"
printf '%s\n' "$TOKEN" > "$TOKEN_FILE"
chmod 600 "$TOKEN_FILE"

printf '== migraciones ==\n'
"$ATLAS" migrate apply \
    --dir "file://$ROOT/db/migrations/clinic" \
    --url "libsql://${CLINICA_DB}?authToken=${TOKEN}"
"$ATLAS" migrate apply \
    --dir "file://$ROOT/db/migrations/inbox" \
    --url "libsql://${INBOX_DB}?authToken=${TOKEN}"

printf '\nAlta terminada.\n'
printf 'Siguientes pasos:\n'
printf '  1. Copia el token al keyring del nodo y borra %s.\n' "$TOKEN_FILE"
printf '  2. Abre el escritorio en la sede y ejecuta el bootstrap (nombre, ciudad, Admin).\n'
printf '  3. Añade la clínica a directory/clinicas.json y publica el directorio.\n'
printf '  4. Programa tools/backup.sh --turso %s.\n' "$CLINICA_DB"
printf '  5. Registra la sede en docs/registro-clinicas.md.\n'
