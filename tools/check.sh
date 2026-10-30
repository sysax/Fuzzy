#!/usr/bin/env bash
# Comprobación previa a integrar: esquema, compilación, pruebas y QML.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

QT_PREFIX="${QT_PREFIX:-/home/samir/Qt/6.11.2/gcc_64}"

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

printf '== esquema ==\n'
"$ROOT/tools/db-rebuild.sh"
for db in clinic portal inbox directory; do
    "$ATLAS_BIN" migrate validate --dir "file://$ROOT/db/migrations/$db"
done

printf '== compilación ==\n'
if [[ ! -d "$QT_PREFIX" ]]; then
    printf 'No existe QT_PREFIX=%s. Define QT_PREFIX con tu instalación de Qt 6.11.\n' "$QT_PREFIX" >&2
    exit 1
fi

cmake -S "$ROOT" -B "$ROOT/build" -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_PREFIX_PATH="$QT_PREFIX"
cmake --build "$ROOT/build"
ctest --test-dir "$ROOT/build" --output-on-failure

printf '== qml ==\n'
# Todo el árbol QML, no solo la raíz: las vistas de la fase 7 viven en
# fuzzy_desktop/qml/ y un error de tipos ahí no lo ve el lint de Main.qml.
"$QT_PREFIX/bin/qmllint" -I "$QT_PREFIX/qml" fuzzy_desktop/qml/*.qml

printf '\nTodo verde.\n'
