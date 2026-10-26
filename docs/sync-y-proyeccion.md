# Sync y proyección (fase 5)

Este documento describe el motor de sincronización entre nodos (5A) y la
proyección/recolección de portales (5B). La fuente de verdad del diseño sigue
siendo [`PLAN.md`](./PLAN.md) §4.2, §4.4, §5.2 y la fase 5 de §6.

## 1. Qué viaja y qué no

Viajan las entidades clínicas que alimentan el portal:

```
clientes → mascotas → citas / consultas_medicas → recetas → receta_detalles
                                      └→ vacunas / signos_vitales
```

No viajan catálogos (`roles`, `estados_cita`, `tipos_servicio_cita`, …): las
migraciones los siembran iguales en todos los nodos. Tampoco viajan tokens,
`portales`, `auditoria` ni las rutas de imágenes. Las imágenes llegan en la
fase 6 y su binario nunca sale del nodo.

Cada entidad se declara en `fuzzy_core/include/fuzzy/core/SyncEntities.h` con
sus columnas y sus referencias. Una referencia entera (`mascota_id`,
`consulta_id`, …) se transporta como **uuid del padre**, nunca como id local:
los ids de dos nodos pueden diferir aunque representen la misma fila.

## 2. Outbox (5A)

Antes de publicar, el nodo escribe en `sync_outbox`:

| Columna | Papel |
|---|---|
| `event_uuid` | uuid determinista de (nodo, entidad, entidad_uuid, `updated_at`, operación) |
| `entidad`, `entidad_uuid` | fila afectada; clave única para conservar solo la última edición |
| `operacion` | `upsert` o `delete` (tombstone) |
| `payload` | JSON con la fila completa y los uuid de sus padres |
| `enviado_at` | `NULL` mientras está pendiente |
| `intentos`, `ultimo_error` | diagnóstico de reintentos |

`SyncEngine::sincronizar()` hace, en orden:

1. `detectarPendientes()`: encola las filas con `sync_state = 'pendiente'`.
2. `empujar()`: inserta cada evento en `sync_change_log` de la base compartida.
3. `jalar()`: aplica los eventos de los demás nodos.

### Idempotencia y lote interrumpido

El `event_uuid` no es aleatorio: se deriva del estado de la fila. Si el proceso
muere después de publicar y antes de marcar `enviado_at`, el siguiente intento
reinserta **el mismo** `event_uuid`; el `ON CONFLICT(event_uuid) DO NOTHING` del
change log lo descarta. Por eso el push no necesita una transacción de varios
saltos y funciona igual con `SqliteExecutor` que con `TursoHttpExecutor`, que
no comparten una transacción entre peticiones HTTP.

Si la fila cambió mientras el evento viajaba, `marcarEnviado()` no la marca
como sincronizada (`WHERE uuid = ? AND updated_at = ?`): queda pendiente y el
siguiente lote publica la versión nueva.

### LWW y tombstones

Al aplicar un `upsert`:

```sql
INSERT INTO <tabla> (..., sync_state) VALUES (..., 'sincronizado')
ON CONFLICT(uuid) DO UPDATE SET ...
WHERE excluded.updated_at >= <tabla>.updated_at;
```

Gana el `updated_at` mayor. Un evento viejo se registra como aplicado pero no
pisa la fila local. Un `delete` es un `UPDATE` que escribe `deleted_at` con la
misma comparación; si la copia local es más nueva, se conserva. El borrado es
lógico: la fila no desaparece y el portal puede proyectar el tombstone.

### Marcador de agua y eventos aplazados

`sync_meta.ultimo_pull_id` evita releer todo el change log. El marcador solo
avanza hasta el evento anterior al primer aplazado. Un evento se aplaza cuando
falta el padre referenciado o el payload no se puede aplicar; el siguiente
`jalar()` lo reintenta. Los eventos ya aplicados se saltan con
`sync_aplicado`, así que repetir un jalado no reescribe filas convergidas.

## 3. Proyector (5B)

`PortalProjector::proyectar(portal_uuid)`:

1. Abre la base del dueño y aplica las migraciones pendientes.
2. Lee `portal_meta` y localiza la cuenta y la ficha en la clínica.
3. Construye upserts **solo** de las filas de esa ficha: perfil, mascotas,
   visitas, recetas, líneas de receta, vacunas, pesos y citas.
4. Marca tombstones para lo que ya no existe en la clínica.
5. Actualiza `portal_meta.proyeccion_version` y `proyectado_at`.

El proyector nunca escribe en `solicitudes_cita` ni en `mascota_fotos`, que son
las tablas donde escribe la app. Tampoco copia notas internas, auditoría,
costos, inventario ni usuarios: esas tablas no existen en el esquema del
portal, y el contrato lo verifica `tst_portal_contract`.

La migración de un portal es perezosa: `PortalProvisioner::migrar()` (o el
propio proyector) aplica únicamente las versiones que falten y conserva la
identidad del portal y su token.

## 4. Recolector de primeras visitas

`BuzonService::recolectar(clinica_uuid)` lee `altas` de esa clínica y, por cada
fila válida:

- reutiliza una ficha existente por correo o teléfono o crea una provisional;
- crea la mascota con especie `No indicada` (el alta no la trae);
- crea una cita con `origen = 'primera_visita'` y estado `Agendada`;
- borra la fila del buzón.

Los tres uuid se derivan del uuid del alta. Un corte entre la creación de la
cita y el borrado del alta no duplica nada al reintentar. Una fila inválida se
deja en el buzón, se cuenta como rechazada y no bloquea a las demás.

## 5. Ida y vuelta de solicitudes

`SolicitudCitaService::procesar(portal_uuid)`:

1. Lee `solicitudes_cita` con estado `Pendiente`.
2. Comprueba que la mascota pertenece a la ficha dueña del portal.
3. Interpreta `fecha_preferida` (`yyyy-MM-dd`) y `hora_preferida` (`HH:mm`).
4. Rechaza fechas pasadas, formatos inválidos y franjas ya ocupadas.
5. Si acepta, crea la cita en la clínica con uuid determinista, la proyecta en
   el portal y escribe `Aceptada`, `respuesta` y `cita_uuid` en la solicitud.

Repetir la llamada no crea otra cita: la solicitud ya no está `Pendiente` y el
uuid de la cita se deriva de la solicitud.

## 6. Gate de la fase

Las suites que sostienen el gate son:

| Suite | Qué prueba |
|---|---|
| `tst_portal_projector` | fixture de 3 dueños; cada portal solo contiene uuid propios, tombstones y respeto por las tablas de la app |
| `tst_portal_contract` | el portal tiene el esquema reducido y ninguna tabla interna |
| `tst_sync_engine` | dos nodos convergen, LWW, tombstone y reintento idempotente de un lote interrumpido |
| `tst_solicitud_cita` | ida y vuelta aceptada, rechazo por agenda y una sola cita por solicitud |
| `tst_buzon_recoleccion` | recolección idempotente, aislamiento por clínica y filas inválidas |
| `tst_portal_provisioner` | base, esquema, QR, huella y migración perezosa |
| `tst_migration_runner` | runner embebido y del directorio, idempotente |

`tst_portal_projector` falla el build si un uuid de otro dueño aparece en un
portal. El escaneo cubre todas las tablas y todos los valores de texto, no solo
las columnas que el proyector conoce.

## 7. Límites conocidos

- El catálogo de entidades sincronizadas es el de la historia clínica; las
  facturas y el inventario se incorporan cuando sus flujos existan en el
  escritorio (fases 7–9).
- No hay resolución automática de conflictos de negocio (dos citas en la misma
  franja): LWW es la red de seguridad y la agenda valida antes de aceptar.
- El binario de una imagen no viaja por sync ni por portal (fase 6).
- El nodo remoto debe tener aplicada la migración `0004_sync.sql`; el motor
  devuelve un error explícito si falta `sync_change_log`.
