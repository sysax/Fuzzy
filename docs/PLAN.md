# Plan de desarrollo — Fuzzy (Turso + portal por cliente)

**Estado:** adoptado el 2026-10-23. Sustituye a los planes anteriores de este repositorio.
**Protocolo vinculante:** [`flujo-de-trabajo-profesional-con-turso-cli-git-pruebas-locales-y-despliegue.md`](./flujo-de-trabajo-profesional-con-turso-cli-git-pruebas-locales-y-despliegue.md), modificado por la [Enmienda 1](./enmienda-01-portal-por-cliente.md).
**Origen funcional:** `FuzzyApp` en el commit `f3afa94`, copia selectiva sin historial Git.
**Diagrama de arquitectura:** [`Turso_Flow.mmd`](./Turso_Flow.mmd).

---

## 0. Alcance

Fuzzy es el sistema de gestión de una clínica veterinaria: escritorio para el personal, móvil para el dueño de la mascota, sincronización con Turso y funcionamiento sin red.

Este plan fija **cómo** se construye. El alcance funcional se copia de `FuzzyApp` (fases 0–3 de `FuzzyApp/docs/PLAN.md`) y no se reinventa.

### Lo que este plan NO incluye

- P2P para imágenes.
- Almacenamiento externo (S3/R2) ni CDN.
- Proveedor de correo o de push.
- Endpoint público del nodo de la clínica.
- Auto-registro de personal ni códigos de invitación/reclamo.
- Roles dinámicos por clínica.
- Un panel de administración web.

Cada exclusión tiene una razón registrada en las decisiones; volver sobre una exige una enmienda, no un cambio silencioso.

---

## 1. Decisiones

| ID | Decisión |
|---|---|
| D1 | El servicio de sync vive **embebido en el nodo** (desktop). No hay backend en la nube ni endpoint público. El móvil no pasa por el nodo para leer: va directo a su portal en Turso. |
| D2 | Las imágenes clínicas viven **solo en el disco del nodo**. La foto de perfil de la mascota vive en el portal, en una sola fila que se reemplaza. |
| D3 | **Cuatro roles fijos**: Admin, Veterinario, Recepcion, Estilista. El cliente es un plano de identidad aparte. |
| D4 | El **Admin crea las cuentas del personal** desde Ajustes. No hay auto-registro. |
| D5 | **Sin códigos** de invitación ni de reclamo. El QR de emparejamiento entrega una credencial, no reclama una ficha. |
| D6 | El **bootstrap** se dispara cuando no existe instalación ni clínica, no cuando falta un Admin. |
| D7 | El último Admin se recupera con la **CLI de operador** (`fuzzy-admin`), nunca reabriendo el bootstrap. |
| D8 | Contraseña temporal **autogenerada de 12 caracteres, 72 h, cambio obligatorio** al primer inicio de sesión. |
| D9 | Clientes por **A+B**: cuenta creada en el mostrador, o auto-registro en la app con **aprobación del personal** (Admin y Recepcion), con verificación presencial o telefónica y motivo auditado. |
| D10 | Gracia offline del desktop: **7 días**. |
| D11 | **Sin proveedor de correo** en el MVP; las credenciales se muestran en la aplicación. |
| D12 | Identidad: **entero local + `uuid` global**; timestamps en milisegundos UTC; `deleted_at`; `sync_state`. |
| D13 | Copia selectiva de `FuzzyApp` en `f3afa94`, sin importar su historial Git. |
| D14 | **Runner de migraciones embebido** en el desktop; Atlas en CI y operaciones. |
| D15 | `fuzzy_core` compartido entre escritorio, móvil y el módulo servidor; el modo headless es opcional. |
| D16 | Sesiones del **personal**: access JWT HS256 de 15 min y refresh opaco rotativo de 30 días, con revocación. |
| D17 | Matriz de propiedad por agregado: la clínica escribe lo clínico, el cliente escribe su perfil y sus comandos. LWW solo como red de seguridad. |
| D18 | **Un grupo Turso por clínica** (`clinic`, `inbox`, N `portal`) y **una cuenta Turso por clínica**. |
| D19 | Contraseñas con **PBKDF2-HMAC-SHA256, 600 000 iteraciones**, salt por usuario, comparación en tiempo constante. |
| D20 | Read models como **vistas SQL versionadas**, no como consultas sueltas en C++. |
| D21 | **Buzón de solo inserción** para reservar antes de la primera visita; la confirmación es telefónica. |
| D22 | **Directorio público de clínicas**, publicado por CI desde el repositorio. |
| D23 | El token del portal es de **larga duración**, cifrado en el teléfono con una clave derivada de la contraseña; la revocación es por rotación o borrado, y la caché local tiene límites documentados. |
| D24 | El portal es una **proyección con esquema reducido**, no una copia filtrada del esquema completo. |

---

## 2. Arquitectura

```
┌──────────────────────────────────────────────┐
│ PC de la clínica (nodo)                      │
│  fuzzy_desktop                               │
│   ├─ QML                                     │
│   ├─ SQLite local (autoridad de la sede)     │
│   ├─ outbox + motor de sync                  │
│   ├─ proyector de portales                   │
│   └─ imágenes clínicas en disco              │
└───────────────┬──────────────────────────────┘
                │ HTTPS · token de grupo
                ▼
┌──────────────────────────────────────────────┐
│ Turso — única pieza compartida               │
│  grupo de la clínica                         │
│   ├─ fuzzy-clinic-<uuid>                     │
│   ├─ fuzzy-inbox-<uuid>                      │
│   └─ fuzzy-portal-<cuenta>  (uno por dueño)  │
│  fuzzy-directory (público, solo lectura)     │
└───────────────▲──────────────────────────────┘
                │ token limitado a SU portal
        App del dueño (casa, sin LAN)
```

### 2.1 Inventario de bases

| Base | Grupo | Escribe | Lee | Contenido |
|---|---|---|---|---|
| `fuzzy-directory` | Proveedor | CI | App (token público) | Nombre, ciudad, teléfono, `inbox_url` de cada clínica |
| `fuzzy-clinic-<uuid>` | Clínica | Nodo | Nodo | Todo el dominio de la sede |
| `fuzzy-inbox-<uuid>` | Clínica | App (`altas:data_add`) | Nodo | Altas de primera visita, sin datos clínicos |
| `fuzzy-portal-<cuenta>` | Clínica | Nodo + App (comandos) | App + Nodo | Proyección de un solo dueño |

### 2.2 Tokens y custodia

| Consumidor | Base | Permisos | Caducidad | Custodia |
|---|---|---|---|---|
| App | `fuzzy-directory` | `all:data_read` | Larga | Embebido en el binario (público) |
| App sin portal | `fuzzy-inbox-<uuid>` | `altas:data_add` | Larga | En el directorio (público) |
| App del dueño | `fuzzy-portal-<cuenta>` | `all:data_read`, `solicitudes_cita:data_add`, `perfil:data_update`, `mascota_fotos:data_add/data_update` | 365 d | Cifrado en el teléfono con clave PBKDF2 de la contraseña |
| Nodo | `fuzzy-clinic-<uuid>` y portales | Grupo: todo | Larga | Keyring del SO, `0600` como reserva |
| Nodo | Plataforma Turso | Crear y borrar bases, emitir y rotar tokens | Larga | Keyring del SO; nunca en Git |

El token de la clínica **nunca** sale del nodo. El token del portal **solo** abre la base de ese dueño: no puede nombrar otra base ni otra ficha.

### 2.3 Capas de persistencia

| Capa | Implementación | Uso |
|---|---|---|
| `IQueryExecutor` | interfaz en `fuzzy_core` | Todo el dominio depende de esta abstracción |
| `SqliteExecutor` | `QSqlDatabase` | Desktop y móvil |
| `TursoHttpExecutor` | pipeline HTTP/JSON | Nodo (sync, proyección) y móvil (portal) |

Mismo dialecto SQL, mismo dominio, misma matriz de permisos. Ninguna consulta del dominio conoce el transporte.

---

## 3. Identidad y roles

### 3.1 Arranque

```cpp
if (instalacion.existe() || clinicas.count() > 0)
    mostrarLogin();          // incluye "Ya tengo una clínica"
else
    mostrarBootstrap();      // crear clínica + primer Admin
```

`instalacion` es una fila única escrita en la misma transacción que la clínica y el Admin. El bootstrap **no se repite** aunque se pierda el último Admin.

### 3.2 Personal

```
Admin → Ajustes → Equipo → Nuevo usuario
  nombre · correo · rol · contraseña temporal (12 caracteres, copiar)
  → transacción local: cuentas + usuarios + auditoria
  → encolado para sync
  → primer inicio: cambio obligatorio (72 h)
```

Reglas: solo Admin; nunca su propio rol ni su propia desactivación; no se puede desactivar al último Admin activo; todo cambio va a `auditoria`.

### 3.3 Clientes

- **A — Mostrador:** recepción crea ficha, "Crear acceso a la app", contraseña temporal, portal y emparejamiento.
- **B — Auto-registro:** la app funciona en local, el cliente declara sus datos y su mascota; al llegar a la clínica se vincula y **entonces** se crea el portal. Si quiere reservar antes de pisar la clínica, usa el buzón (§5).
- La aprobación del vínculo la hacen Admin y Recepcion, con verificación presencial o telefónica y motivo auditado.
- Hasta la aprobación, el dueño ve solo sus datos provisionales; el historial clínico está bloqueado.

### 3.4 Permisos

`Politicas::exigir(sesion, Permiso, contexto)` en `fuzzy_core`, usada por los servicios locales y por el módulo servidor. El rol y `clinica_id` salen de la sesión, nunca del cuerpo de la petición.

| Rol | Escribe | Lee |
|---|---|---|
| Admin | Todo | Todo |
| Veterinario | Consulta, receta, laboratorio, hospitalización, cirugía | Panel, agenda, clientes, inventario, facturación |
| Recepcion | Citas, recepción/triage, clientes, vínculos, inventario, caja | Consultas, laboratorio |
| Estilista | Estética | Panel, agenda, clientes, consultas sin notas clínicas |

La matriz exacta se deriva de las 60 políticas RLS de `FuzzyApp` y se congela en `docs/matriz-autorizacion.md` con un test de snapshot sobre las combinaciones rol × permiso × ámbito.

### 3.5 Offline

Al iniciar sesión con éxito el desktop guarda `credenciales_cache` (hash PBKDF2, rol, clínica, `perm_ver`, `synced_at`). Sin red verifica contra esa caché y crea una sesión `origen=offline` con gracia de 7 días. Si el refresh de dispositivo está revocado, pasa a solo lectura y pide reautenticación.

---

## 4. Portal del cliente

### 4.1 Esquema (11 tablas)

```
perfil            mascotas          mascota_fotos
visitas           recetas           receta_items
vacunas           laboratorio       pesos
citas             solicitudes_cita  adjuntos
```

- `adjuntos` guarda **metadatos** (`nombre`, `mime`, `sha256`, tamaño). El binario nunca sale del nodo.
- No existen `notas_internas`, `auditoria`, `costos` ni `inventario`. La defensa no es filtrar: es que el dato no está en la base.
- `mascota_fotos` es una tabla aparte porque Turso autoriza por tabla, no por columna: así el dueño puede cambiar la foto sin poder editar campos clínicos de `mascotas`.

### 4.2 Proyección

Se dispara al aprobar un vínculo, cerrar una visita, emitir receta o factura, registrar vacuna o peso, confirmar una cita o cambiar la foto.

```
proyectar(cuenta_uuid):
  1. si el portal no existe → crear base + esquema + token + registrar en `portales`
  2. migrar el portal si `portal_meta.version` < versión actual
  3. construir un batch de upserts de ESE cliente
  4. ejecutar el batch (atómico o idempotente)
  5. registrar `proyeccion_version` y `proyectado_at`
```

Idempotente por `uuid`; borrados como tombstones. Un portal se puede reconstruir desde cero sin tocar los demás.

### 4.3 Emparejamiento

El nodo muestra un QR con `{v, url, token, portal_uuid, clinica}` y una huella corta de 6 caracteres. La app la muestra de vuelta y el operador confirma que coinciden. Al aceptar:

1. La app deriva una clave con PBKDF2 de la contraseña del dueño.
2. Cifra el token con AES-256-GCM y lo guarda.
3. Descarga el portal a su SQLite local, también cifrado.

Sin la contraseña no se descifra el token, aunque se extraiga el almacenamiento del teléfono.

### 4.4 Solicitudes de cita

La app inserta en `solicitudes_cita` de su portal. El nodo las recoge, las valida contra la agenda y escribe el resultado en el mismo portal; la app lo ve al abrir. Sin push en el MVP.

### 4.5 Foto de la mascota

Se valida y reencoda en el nodo (máximo 1024 px de lado mayor, 2 MB), se proyecta como BLOB y se reemplaza con `UPDATE` sobre la misma fila: la anterior desaparece, sin versiones ni huérfanos. La app borra su copia temporal y la caché de esa foto.

### 4.6 Revocación

Rotar el token del portal o borrar la base. La app falla en la siguiente consulta y pide re-emparejar en la clínica. Lo ya descargado no se puede borrar a distancia: la caché está cifrada, caduca a los 30 días y se elimina al cerrar sesión. Se documenta como límite del producto en `docs/limites-cache.md` (Fase 8).

---

## 5. Buzón y directorio

### 5.1 Directorio

`fuzzy-directory` es de solo lectura para la app y lo publica CI desde `directory/clinicas.json`. No lo escriben los nodos. Contiene nombre, ciudad, dirección, teléfono, `inbox_url` e `inbox_token`.

### 5.2 Primera visita

```
App nueva → elige clínica del directorio
  → declara nombre, correo, teléfono, mascota, motivo y franja
  → INSERT en fuzzy-inbox-<clinica>.altas (uuid idempotente)
  → "Solicitud enviada. La clínica te contactará al 300…"

Nodo (cada 5 min) → lee altas → valida → crea ficha provisional
  → cita con origen='primera_visita', estado='por_confirmar'
  → borra la fila

Recepción → confirma por teléfono → al llegar, empareja el teléfono
```

El buzón es de **un solo sentido** por diseño: con lectura, cualquier cliente podría leer solicitudes ajenas. El precio es que la primera confirmación no llega por la app.

---

## 6. Fases

| Fase | Tamaño | Contenido | Gate |
|---|---|---|---|
| **0 — Fundaciones** | S | Toolchain fijado (`turso`, `atlas`, `sqlite3`, Qt 6.11.2), esqueleto CMake, `.gitignore` (`*.db*`, `*.dump`, `.env*`, `secrets/`), `db/atlas.hcl`, `0001_init`, `tools/db-rebuild.sh`, `tools/check.sh`, CI esqueleto, `docs/origen.md`, `docs/matriz-autorizacion.md` | Rebuild desde cero + `atlas migrate validate` + CI verde |
| **1 — Esquema** | L | Cuatro esquemas: clínica, portal, buzón, directorio. `portales`, `mascota_fotos`, `adjuntos`, `citas.origen`, `solicitudes_cita`, `portal_meta`. Runner de migraciones. Suite de esquema sobre los cuatro | Los cuatro migran desde cero; ninguna tabla sincronizable sin `uuid` y timestamps; el portal no contiene ninguna tabla interna |
| **2 — Turso + CI/CD** | M | `tools/provision-clinic.sh` (cuenta, grupo, bases, tokens), `atlas.hcl` remoto, CI (`schema`, `qt`), `tools/publish-directory.sh`, `docs/entornos.md` | Una clínica de staging se crea con un comando; migraciones aplicadas; directorio publicado; cero secretos en Git |
| **3 — `fuzzy_core`** | XL | `IQueryExecutor` (`Sqlite`, `TursoHttp`), DAOs, read models, servicios, `Politicas`, auth local (PBKDF2, bootstrap, temporal, caché offline), `CryptoBox`, `FileStore` | `ctest` verde; matriz de permisos con snapshot; el core no hace red salvo por los ejecutores |
| **4 — Portales, emparejamiento y buzón** | L | Crear base, aplicar esquema, emitir token, QR con huella, consumir el buzón, confirmar primera visita, rotar y borrar | El token de un portal no abre la base de la clínica ni otro portal; fila del buzón consumida una sola vez; revocación deniega; borrado elimina la base |
| **5 — Sync y proyección** | XL | **5A** clínica ↔ Turso: outbox, change log, LWW, tombstones, idempotencia. **5B** proyector y recolector, migración perezosa de portales | Dos nodos convergen; con un fixture de 3 clientes, cada portal **no contiene ningún `uuid` ajeno** (el test rompe el build); ida y vuelta de una solicitud; lote interrumpido recuperable |
| **6 — Imágenes** | S | Foto de mascota (validación, reencodado, límites, reemplazo) y adjuntos clínicos que nunca salen del nodo | Reemplazo sin residuo; >2 MB rechazado; test que verifica que ningún binario clínico aparece en Turso |
| **7 — Desktop, paridad Fase 1** | L | `AccessView` con tres estados, `BootstrapView`, asistente de alta de clínica, shell/panel, agenda, pacientes/historial, vínculos, inventario, facturación, Ajustes/Equipo | 15 baselines + `qmllint` + pruebas de controladores |
| **8 — Mobile, paridad Fase 2** | XL | Directorio, buzón, registro local, importación al emparejar, portal directo, historial, agendar, mis citas, caché cifrada, modo avión | Flujo completo offline → sync; conflicto y duplicado resueltos; ningún token de la clínica en el binario |
| **9 — Flujo clínico, paridad Fase 3** | XL | Recepción/triage, consulta/recetas/cargos, catálogos, laboratorio, hospitalización/cirugía, estética, caja/inventario, QA de staging | Seis flujos de punta a punta; matriz por rol/clínica; smoke con móvil real |
| **10 — Producción y runbooks** | M | Alta de clínica en producción, environment protegido, backup, runbook del máster y del portal, CLI `fuzzy-admin`, simulacro de reversión | Despliegue reversible; runbook ejecutado en staging; cero secretos en Git o en binarios |

Cada subfase es una rama `feature/*` y un PR. Las fases 3 y 4 pueden solaparse; 7–9 no empiezan sin 4 y 5 cerrados.

---

## 7. Pruebas y CI

| Suite | Qué cubre |
|---|---|
| `tests/schema` | Los cuatro esquemas desde cero en SQLite en memoria; constraints, FKs, vistas, triggers |
| `fuzzy_core/tests` | Dominio, DAOs, auth local, PBKDF2, `CryptoBox`, matriz de permisos, caché offline |
| `tests/portal` | **Aislamiento**: proyección desde un fixture de 3 clientes y ausencia de `uuid` ajenos |
| `tests/inbox` | Validación de filas, idempotencia, límites de tamaño y consumo único |
| `tests/tokens` | Un token de portal no lee la clínica ni otro portal; no borra; caducidad |
| `tests/contract` | Portal con esquema `N` contra app `N-1` y viceversa |
| `fuzzy_desktop/tests/visual` | Baselines con DejaVu empaquetada |
| `fuzzy_mobile/tests` | Citas, offline, conflictos, emparejamiento, caché |

CI: `schema` (Atlas + SQLite efímero + seed), `qt-build` (Qt 6.11.2 + `qmllint` + `ctest`), `portal` (proyección y aislamiento), `contract`. El despliegue a producción es un runbook por clínica; solo el esquema de Turso y el directorio se automatizan.

---

## 8. Riesgos

| Riesgo | Tratamiento |
|---|---|
| Migraciones × N portales | Esquema de portal mínimo y migración perezosa; medir `ATTACH` en 5B |
| Token de inserción compartido (spam) | `CHECK` en el esquema, límites de tamaño, consumo rápido; rotación difundida por el directorio |
| Caché local tras revocación | Cifrada, caducidad de 30 días, borrado al cerrar sesión, límite documentado |
| Coste y operación de N bases | Una cuenta Turso por clínica; verificar límites del plan antes de Fase 2 |
| Sondeo de N portales | Sondeo directo en el MVP; señal en el buzón si N > 200 |
| Bug de proyección que filtre filas | Test de aislamiento por fixture + esquema que no contiene datos ajenos |
| Sin push | La app refresca al abrir; se documenta en la interfaz |
| Directorio público | Solo datos comerciales; token de inserción con permisos mínimos |
| Nodo apagado | Los portales siguen legibles en Turso; el buzón y la cola del móvil esperan |

---

## 9. Pendiente de verificar en Fase 2

No bloquea el diseño, pero decide detalles de implementación:

1. Límite real de bases del plan contratado (las fuentes públicas discrepan entre 100 y 500 en el plan gratuito).
2. Atomicidad de `batch` sobre el pipeline HTTP; si no es atómica, encapsular como operación idempotente.
3. Permiso `ATTACH` para leer o migrar varios portales en un solo viaje.
4. Que la Platform API permita crear y rotar bases con el token de la clínica, sin un token de organización global.

---

## 10. Referencias

- [`flujo-de-trabajo-profesional-con-turso-cli-git-pruebas-locales-y-despliegue.md`](./flujo-de-trabajo-profesional-con-turso-cli-git-pruebas-locales-y-despliegue.md)
- [`enmienda-01-portal-por-cliente.md`](./enmienda-01-portal-por-cliente.md)
- [`Turso_Flow.mmd`](./Turso_Flow.mmd)
- `FuzzyApp/docs/PLAN.md` (fases funcionales 0–3)
- `FuzzyApp/docs/STAGING.md` (modelo de aprovisionamiento que se reemplaza)
