# Entornos

FuzzyCloud opera tres niveles de entorno: **local**, **staging** y **producción**. El plan de desarrollo
([`PLAN.md`](PLAN.md)) fija el diseño; este documento registra cómo se configuran y se ejecutan.

## Local

- Bases SQLite: `db/local/{clinic,portal,inbox,directory}.db` (fuera de Git).
- Migraciones: Atlas con los entornos locales de `db/atlas.hcl` (`clinic`, `portal`, `inbox`, `directory`).
- Reconstrucción desde cero: `tools/db-rebuild.sh`.
- Validación: `atlas migrate validate --dir "file://db/migrations/<schema>"`.
- Check completo (esquema + build + pruebas + QML): `tools/check.sh`.

## Staging (Turso)

- Bases: un grupo Turso por clínica — `fuzzy-clinic-<slug>` (dominio) y `fuzzy-inbox-<slug>` (buzón) —
  más sus portales (uno por dueño). Grupo de staging con dos clínicas provisionadas.
- Migraciones remotas: entornos `staging-*` en `db/atlas.hcl`; la URL se pasa por CLI con el token
  desde la variable de entorno (`--url "libsql://<db>?authToken=${TURSO_DATABASE_TOKEN_STAGING}"`):

  ```bash
  export TURSO_DATABASE_TOKEN_STAGING=<token>
  atlas migrate apply --config file://db/atlas.hcl --env staging-clinic \
    --url "libsql://fuzzy-staging-clinic?authToken=${TURSO_DATABASE_TOKEN_STAGING}"
  atlas migrate apply --config file://db/atlas.hcl --env staging-portal \
    --url "libsql://fuzzy-staging-portal?authToken=${TURSO_DATABASE_TOKEN_STAGING}"
  atlas migrate apply --config file://db/atlas.hcl --env staging-inbox \
    --url "libsql://fuzzy-staging-inbox?authToken=${TURSO_DATABASE_TOKEN_STAGING}"
  atlas migrate apply --config file://db/atlas.hcl --env staging-directory \
    --url "libsql://fuzzy-staging-directory?authToken=${TURSO_DATABASE_TOKEN_STAGING}"
  ```

- Provisionamiento (una sola invocación crea las dos clínicas de staging):

  ```bash
  tools/provision-clinic.sh
  ```

- Directorio público: `tools/publish-directory.sh` publica `directory/clinicas.json` en la base
  `fuzzy-directory` (variable `TURSO_DB_STAGING`, default `fuzzy-directory`).

## Producción

- Una cuenta de Turso por clínica (D18): grupo `fuzzy-clinic-<slug>`, base de dominio
  `fuzzy-clinic-<slug>` y base de buzón `fuzzy-inbox-<slug>`. Los portales son bases
  `fuzzy-portal-<uuid>`, una por dueño.
- Alta de una sede: `tools/provision-production.sh` (con `--dry-run` primero y
  `--confirmar-produccion` después). Crea el grupo, las bases, el token de grupo y aplica las
  migraciones de clínica y buzón. El token queda en `secrets/` con permisos `0600` y se mueve al
  keyring del nodo.
- Despliegue de esquema: workflow manual y protegido `deploy.yml`, con el entorno `production`
  (revisores, rama `main` y secreto `TURSO_DATABASE_TOKEN` propios). Por defecto ejecuta una
  simulación; solo escribe con `aplicar=true`.
- Nodo: el escritorio aplica las migraciones embebidas al arrancar; `fuzzy-admin migrar` cubre el
  caso de un nodo detenido o una base nueva.
- Respaldos: `tools/backup.sh --db` para el máster y `tools/backup.sh --turso` para Turso, con
  retención y verificación. Restauración: `fuzzy-admin restaurar --confirmar` (deja copia previa) y
  simulacro con `tools/rollback-drill.sh`.
- Operación: [`produccion.md`](produccion.md), [`runbook-master.md`](runbook-master.md),
  [`runbook-portal.md`](runbook-portal.md) y [`registro-clinicas.md`](registro-clinicas.md).

## Variables de entorno

| Variable | Uso |
|---|---|
| `TURSO_API_TOKEN` | Token de API de Turso para la CLI (scope mínimo, solo CI en adelante) |
| `TURSO_DATABASE_TOKEN_STAGING` | Token de base de staging (`libsql://...?authToken=...`) |
| `TURSO_DATABASE_TOKEN` | Token de base del entorno protegido en GitHub (`staging`/`production`) |
| `TURSO_DB_STAGING` | Nombre de la base del directorio (`fuzzy-directory` por defecto) |
| `TURSO_ORG` | Organización de Turso |
| `TURSO` | Ruta a la CLI de Turso (default `tools/../.tools/bin/turso` vía script) |
| `ATLAS` | Ruta a la CLI de Atlas |
| `FUZZY_ADMIN` | Ruta a `fuzzy-admin` (default `build/fuzzy_admin/fuzzy-admin`) |
| `FUZZY_BACKUP_DIR` | Destino de respaldos (default `backups/`, ignorado por Git) |
| `QT_PREFIX` | Instalación de Qt 6.11.2 para build/QML |

Los secretos (tokens de base, API) se configuran por entorno, nunca en Git. Ver
[`flujo-de-trabajo-profesional-con-turso-cli-git-pruebas-locales-y-despliegue.md`](flujo-de-trabajo-profesional-con-turso-cli-git-pruebas-locales-y-despliegue.md) §11.
