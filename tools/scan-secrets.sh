#!/usr/bin/env bash
# Puerta de secretos (fase 10): ni credenciales en Git ni en los binarios.
#
# No sustituye a un escáner completo: cubre los formatos que este proyecto
# puede filtrar por accidente (tokens de Turso, GitHub, AWS, claves privadas y
# URLs con authToken). Se ejecuta en CI y en tools/check.sh.
#
# Uso:
#   tools/scan-secrets.sh

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

fallo=0
salida="$(mktemp)"
trap 'rm -f "$salida"' EXIT

avisar() {
    printf 'SECRETO POSIBLE: %s\n' "$1" >&2
    fallo=1
}

# 1. Archivos que nunca deben estar versionados.
while IFS= read -r archivo; do
    case "$archivo" in
        .env|.env.*|secrets/*|*/.env|*/.env.*|*/secrets/*)
            avisar "archivo versionado: $archivo" ;;
        *.db|*.db-wal|*.db-shm|*.sqlite|*.sqlite3|*.dump)
            avisar "base o volcado versionado: $archivo" ;;
    esac
done < <(git ls-files)

# 2. Patrones de credenciales en archivos de texto versionados.
PATRONES=(
    '(TURSO_(API|DATABASE)_TOKEN|TURSO_DATABASE_TOKEN_(STAGING|PRODUCTION)|GITHUB_TOKEN|GH_TOKEN)[[:space:]]*=[[:space:]]*[A-Za-z0-9._-]{20,}'
    'libsql://[^"'"'"'[:space:]]*authToken=[A-Za-z0-9._-]{20,}'
    'gh[pousr]_[A-Za-z0-9]{30,}'
    'github_pat_[A-Za-z0-9_]{50,}'
    'AKIA[0-9A-Z]{16}'
    '-----BEGIN (RSA |EC |OPENSSH )?PRIVATE KEY-----'
)

for patron in "${PATRONES[@]}"; do
    if git grep -I -n -E -- "$patron" > "$salida" 2>/dev/null; then
        cat "$salida" >&2
        avisar "patrón en Git: $patron"
    fi

    # Los archivos nuevos todavía no están versionados, pero van a entrar en el
    # próximo commit: la puerta local también los revisa.
    mapfile -d '' NUEVOS < <(git ls-files --others --exclude-standard -z)
    if [[ ${#NUEVOS[@]} -gt 0 ]] \
        && grep -I -n -E -- "$patron" "${NUEVOS[@]}" > "$salida" 2>/dev/null; then
        cat "$salida" >&2
        avisar "patrón en archivo no versionado: $patron"
    fi
done

# 3. Los binarios distribuidos no llevan credenciales embebidas.
for binario in \
    build/fuzzy_mobile/fuzzy_mobile \
    build/fuzzy_desktop/fuzzy_desktop \
    build/fuzzy_admin/fuzzy-admin; do
    [[ -x "$binario" ]] || continue
    if strings -a "$binario" | grep -E \
        'gh[pousr]_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{50,}|AKIA[0-9A-Z]{16}|BEGIN (RSA |EC |OPENSSH )?PRIVATE KEY|libsql://[^[:space:]]*authToken=[A-Za-z0-9._-]{20,}' \
        > "$salida"; then
        cat "$salida" >&2
        avisar "credencial embebida en $binario"
    fi
done

if [[ "$fallo" -ne 0 ]]; then
    printf '\nLa puerta de secretos falló.\n' >&2
    exit 1
fi

printf 'Sin secretos detectados en Git ni en los binarios.\n'
