# Origen del código

FuzzyCloud no nace de cero: reutiliza el dominio y parte de la interfaz de `FuzzyApp`. Este documento fija qué se copia, qué no, y con qué reglas, para que la procedencia no dependa de la memoria de nadie.

## Referencia

| Dato | Valor |
|---|---|
| Repositorio | `FuzzyApp` |
| Rama | `develop` |
| Commit | `f3afa94` |
| Fecha de corte | 2026-10-23 |
| Licencia | la del repositorio de origen |

El commit es la única referencia válida. Si se necesita una corrección posterior de `FuzzyApp`, se anota aquí con su commit y se revisa como cualquier otro cambio.

## Qué se copia

| Elemento | Tratamiento |
|---|---|
| `fuzzy_core/domain` | Copia directa, ajustando `#include` y nombres de espacio |
| `fuzzy_core/services` | Copia con revisión: auth, facturación y agenda se adaptan al nuevo esquema |
| `fuzzy_core/repositories` | Se conservan las interfaces; las implementaciones se reescriben sobre `IQueryExecutor` |
| `fuzzy_desktop` QML | Se copian patrones y componentes; el flujo de acceso se reescribe (bootstrap, login, temporal) |
| `fuzzy_mobile` QML | Se copian patrones; el flujo de datos pasa a ser portal + buzón |
| `fuzzy_core/tests` | Se copian como base y se adaptan; son la red de seguridad de la paridad |
| Migraciones Postgres | **Solo como especificación.** No se traducen literalmente ni se ejecutan |

## Qué no se copia

| Elemento | Motivo |
|---|---|
| Historial Git de `FuzzyApp` | El repositorio nuevo no hereda ramas, etiquetas ni autores |
| Gateways de Supabase (`fuzzy_core/network`) | Se sustituyen por DAOs sobre SQLite y por el portal |
| `supabase/migrations` | Postgres con RLS no es trasladable a SQLite; se reescribe |
| Funciones PL/pgSQL y triggers | Se convierten en servicios de dominio |
| Edge Function `invitar-usuario` | Sustituida por el alta de usuarios desde Ajustes |
| Políticas RLS | Se convierten en `Politicas::exigir` (ver `matriz-autorizacion.md`) |
| `PasswordHasher` con SHA-256 sin salt | Se reescribe con PBKDF2-HMAC-SHA256 |
| `SqliteCacheStore` como caché | Pasa a ser la base local real, con migraciones |

## Reglas

1. **Sin `git subtree` ni `git filter-repo`.** La copia es manual y revisada; cada archivo copiado se entiende antes de entrar.
2. **Sin arrastrar deuda.** Si una pieza de `FuzzyApp` está sin terminar (por ejemplo, la migración de consultas sin commitear), se trata como especificación, no como código.
3. **Los tests acompañan al código.** Un servicio copiado entra con su suite o con una equivalente.
4. **Procedencia en el commit.** Los commits que copian código indican `origen: FuzzyApp@f3afa94` y el archivo de partida.
5. **Nada de Supabase.** Ni URLs, ni claves, ni `service_role`, ni referencias a `auth.users`.

## Inventario de partida

Tomado del análisis de `FuzzyApp` en `f3afa94`:

- 21 migraciones Postgres (4 874 líneas), 60 políticas RLS, 64 funciones y triggers.
- 279 aserciones pgTAP en 19 suites.
- 17 suites QtTest en `fuzzy_core`.
- 13 gateways de red, 32 vistas QML de escritorio, 15 de móvil.
- Una Edge Function (`invitar-usuario`).

Este inventario es la línea base de paridad: la fase 9 no se cierra hasta que los flujos de las fases 0–3 de `FuzzyApp` funcionen sobre el modelo nuevo.
