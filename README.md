# FuzzyCloud

Gestión para clínica veterinaria: escritorio para el personal, app para el dueño de la mascota, sincronización con Turso y funcionamiento sin red.

## Estado

**Fase 10 — producción y runbooks, cerrada.** Las fases 0–10 están integradas. El flujo clínico y su matriz de autorización se explican en [`docs/clinico.md`](docs/clinico.md); el motor de sync y la proyección, en [`docs/sync-y-proyeccion.md`](docs/sync-y-proyeccion.md); las dos rutas de imagen y su límite, en [`docs/imagenes.md`](docs/imagenes.md); el alta, los respaldos y los runbooks, en [`docs/produccion.md`](docs/produccion.md); el plan completo, con las decisiones y las fases, está en [`docs/PLAN.md`](docs/PLAN.md). Para el uso diario, consulta el [manual de usuario](docs/manual-usuario.md); para el desarrollo y la operación, el [manual técnico](docs/manual-tecnico.md).

## Toolchain

| Herramienta | Versión | Uso |
|---|---|---|
| Qt | 6.11.2 (`/home/samir/Qt/6.11.2/gcc_64`) | UI y core |
| CMake | ≥ 3.24 | build |
| Ninja | 1.11 | build |
| g++ | 13 (C++20) | build |
| sqlite3 | 3.50 | verificación de esquema |
| Atlas | v1.3.0 community | migraciones |
| Turso CLI | v1.0.34 | operaciones de bases |

`tools/install-tools.sh` descarga Atlas y Turso en `.tools/bin/`, que no entra en Git.

```bash
tools/install-tools.sh
```

## Compilar y probar

```bash
export QT_PREFIX=/home/samir/Qt/6.11.2/gcc_64
tools/check.sh
```

El script reconstruye las cuatro bases, valida las migraciones, compila, ejecuta `ctest`, pasa `qmllint`, comprueba que el binario móvil no lleva tokens, escanea secretos y corre el simulacro de reversión. Por separado:

```bash
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH="$QT_PREFIX"
cmake --build build
ctest --test-dir build --output-on-failure
```

## Esquema

Cuatro bases SQLite independientes, cada una con su directorio de migraciones:

| Base | Directorio | Contenido |
|---|---|---|
| `clinic` | `db/migrations/clinic` | Dominio de la sede |
| `portal` | `db/migrations/portal` | Proyección de un dueño |
| `inbox` | `db/migrations/inbox` | Altas de primera visita |
| `directory` | `db/migrations/directory` | Directorio público |

```bash
tools/db-rebuild.sh                     # desde cero en db/local/
atlas migrate apply --config file://db/atlas.hcl --env clinic
atlas migrate validate --dir file://db/migrations/clinic
```

## Estructura

```
fuzzy_core/       núcleo compartido (dominio, servicios, persistencia)
fuzzy_desktop/    aplicación de escritorio (Qt Quick)
fuzzy_mobile/     aplicación del dueño (Qt Quick)
fuzzy_admin/      CLI de operador (fuzzy-admin)
db/               migraciones y atlas.hcl
tools/            scripts de desarrollo y operación
docs/             plan, origen, runbooks y matriz de autorización
tests/            suites transversales
```

## Documentos

- [`docs/manual-usuario.md`](docs/manual-usuario.md) — manual de usuario del escritorio y del móvil
- [`docs/manual-tecnico.md`](docs/manual-tecnico.md) — arquitectura, datos, seguridad, pruebas y despliegue
- [`docs/PLAN.md`](docs/PLAN.md) — plan y decisiones
- [`docs/sync-y-proyeccion.md`](docs/sync-y-proyeccion.md) — outbox, LWW, proyector y recolección
- [`docs/imagenes.md`](docs/imagenes.md) — foto del portal y galería clínica en el nodo
- [`docs/origen.md`](docs/origen.md) — procedencia del código
- [`docs/matriz-autorizacion.md`](docs/matriz-autorizacion.md) — roles y permisos
- [`docs/produccion.md`](docs/produccion.md) — entornos, secretos, alta, respaldos y release
- [`docs/runbook-master.md`](docs/runbook-master.md) — operación del nodo de la sede
- [`docs/runbook-portal.md`](docs/runbook-portal.md) — operación del portal de un dueño
- [`docs/registro-clinicas.md`](docs/registro-clinicas.md) — sedes dadas de alta
