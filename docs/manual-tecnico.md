# Manual técnico

Documento de referencia para desarrollar, probar, desplegar y operar
FuzzyCloud. Describe la arquitectura implementada, no el diseño ideal. Las
decisiones de producto están en [`PLAN.md`](PLAN.md); los procedimientos de
operación, en [`runbook-master.md`](runbook-master.md) y
[`runbook-portal.md`](runbook-portal.md).

## Contenido

1. [Alcance](#1-alcance)
2. [Arquitectura](#2-arquitectura)
3. [Estructura del repositorio](#3-estructura-del-repositorio)
4. [Compilación y ejecución](#4-compilación-y-ejecución)
5. [Modelo de datos](#5-modelo-de-datos)
6. [Núcleo de dominio](#6-núcleo-de-dominio)
7. [Identidad y autorización](#7-identidad-y-autorización)
8. [Sincronización y proyección](#8-sincronización-y-proyección)
9. [App móvil y caché cifrada](#9-app-móvil-y-caché-cifrada)
10. [Imágenes](#10-imágenes)
11. [Seguridad](#11-seguridad)
12. [Pruebas y CI](#12-pruebas-y-ci)
13. [Operación y despliegue](#13-operación-y-despliegue)
14. [Guía de extensión](#14-guía-de-extensión)
15. [Limitaciones y deuda técnica](#15-limitaciones-y-deuda-técnica)
16. [Referencias](#16-referencias)

---

## 1. Alcance

FuzzyCloud es un sistema de gestión para una clínica veterinaria con tres
binarios:

| Binario | Target | Rol |
|---|---|---|
| `fuzzy_desktop` | `build/fuzzy_desktop/fuzzy_desktop` | Nodo de la sede: base `clinic`, interfaz para el personal |
| `fuzzy_mobile` | `build/fuzzy_mobile/fuzzy_mobile` | App del dueño: base `mobile`, portales en Turso |
| `fuzzy-admin` | `build/fuzzy_admin/fuzzy-admin` | CLI de operador: estado, respaldos, restauración, recuperación de Admin y migraciones |

El producto no tiene backend propio en la nube. El nodo de la clínica es la
fuente de verdad local; Turso aloja las bases compartidas y los portales de
los dueños. La app móvil no pasa por el nodo para leer: habla directamente con
el portal en Turso mediante HTTP (Hrana).

El repositorio está en C++20 con Qt 6.11.2, CMake ≥ 3.24 y Ninja. No hay
dependencias de terceros en el código de producción salvo Qt y SQLite.

---

## 2. Arquitectura

### 2.1 Vista de componentes

```
┌──────────────────────────────┐        ┌──────────────────────────────┐
│ fuzzy_desktop                │        │ fuzzy_mobile                 │
│  QML (Fuzzy)                 │        │  QML (FuzzyMovil)            │
│  Controladores               │        │  MobileController            │
│  SessionController           │        │                              │
└──────────────┬───────────────┘        └──────────────┬───────────────┘
               │                                       │
               │ SQLite local                          │ HTTP (Hrana)
               ▼                                       ▼
      ┌─────────────────┐                    ┌─────────────────────┐
      │ clinica.db      │                    │ Portal Turso        │
      │ (clinic)        │                    │ (una base por dueño)│
      └────────┬────────┘                    └─────────────────────┘
               │
               │ fuzzy_core
               ▼
┌──────────────────────────────────────────────────────────────────────┐
│ Servicios de dominio                                                 │
│  AuthService · ServicioClinico · SyncEngine · PortalProjector        │
│  PortalProvisioner · PortalTokenService · BuzonService               │
│  SolicitudCitaService · MovilStore · PortalCliente · CryptoBox       │
│  PasswordHasher · FileStore · MascotaFotoService · GaleriaClinica    │
└──────────────────────────────────────────────────────────────────────┘
               │
               ▼
      ┌─────────────────┐        ┌─────────────────────┐
      │ IQueryExecutor  │───────▶│ SqliteExecutor      │
      │                 │───────▶│ TursoHttpExecutor   │
      └─────────────────┘        └─────────────────────┘
```

### 2.2 Reglas de dependencia

1. **`fuzzy_core` no depende de QML ni de la plataforma gráfica.** Solo Qt
   Core, Sql, Network y Gui (para imágenes).
2. **Las vistas no contienen reglas de negocio.** `fuzzy_desktop/qml/*.qml` y
   `fuzzy_mobile/qml/*.qml` llaman a controladores; los controladores llaman a
   servicios.
3. **El rol y la clínica salen de la sesión**, nunca del formulario ni del
   cuerpo de una petición.
4. **La persistencia se abstrae con `IQueryExecutor`.** El mismo servicio
   funciona sobre SQLite local y sobre Turso.
5. **Las migraciones son la única fuente del esquema.** No se crea una tabla
   desde código de producción.

### 2.3 Bibliotecas

| Biblioteca | Contenido | Consumidores |
|---|---|---|
| `fuzzy_core` | Dominio, servicios, migraciones embebidas, cifrado | Desktop, móvil, admin y tests |
| `fuzzy_desktop_controllers` | Controladores de escritorio y estilo | App de escritorio y `tst_controladores` |
| `fuzzy_mobile_controllers` | `MobileController` y estilo móvil | App móvil y `tst_mobile` |

`fuzzy_add_desktop_qml(target)` y `fuzzy_add_mobile_qml(target)` registran el
módulo QML, las vistas, la fuente DejaVu Sans y (en móvil) el directorio
embebido. La app y el arnés visual comparten exactamente el mismo `Main.qml`.

---

## 3. Estructura del repositorio

```
fuzzy_core/
  include/fuzzy/core/      APIs públicas del dominio
  src/                     implementación de servicios y transporte
  tests/                   20 suites de Qt Test
fuzzy_desktop/
  agenda/ ajustes/ clinico/ facturacion/ inventario/ pacientes/
  panel/ propietarios/ session/ style/ vinculos/ common/
  qml/                     vistas y componentes
  tests/                   controladores y baselines visuales
fuzzy_mobile/
  app/                     MobileController
  qml/                     vistas y diálogos
  tests/                   controlador, QML y cifrado
fuzzy_admin/               CLI de operador
db/
  migrations/{clinic,portal,inbox,directory,mobile}/
  atlas.hcl
directory/clinicas.json    directorio público embebido en el móvil
tools/                     build, esquema, respaldos, despliegue
docs/                      plan, módulos, runbooks y manuales
```

---

## 4. Compilación y ejecución

### 4.1 Toolchain

| Herramienta | Versión | Uso |
|---|---|---|
| Qt | 6.11.2 (`/home/samir/Qt/6.11.2/gcc_64`) | UI, red, SQL |
| CMake | ≥ 3.24 | configuración |
| Ninja | 1.11 | build |
| g++ | 13 (C++20) | compilación |
| sqlite3 | 3.50 | verificación de esquema |
| Atlas | v1.3.0 community | migraciones |
| Turso CLI | v1.0.34 | operaciones de bases |

`tools/install-tools.sh` descarga Atlas y Turso en `.tools/bin/`, que no entra
en Git.

### 4.2 Comandos

```bash
export QT_PREFIX=/home/samir/Qt/6.11.2/gcc_64

cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_PREFIX_PATH="$QT_PREFIX"
cmake --build build
ctest --test-dir build --output-on-failure
```

La puerta completa:

```bash
tools/check.sh
```

`tools/check.sh` reconstruye las cuatro bases de Atlas, valida migraciones,
configura y compila, ejecuta las 24 suites de `ctest`, pasa `qmllint` sobre
todas las vistas, comprueba que el binario móvil no contiene tokens de portal,
escanea secretos y corre el simulacro de reversión.

### 4.3 Targets y opciones

| Opción CMake | Default | Efecto |
|---|---|---|
| `FUZZY_BUILD_DESKTOP` | `ON` | Compila `fuzzy_desktop` y sus tests |
| `FUZZY_BUILD_MOBILE` | `ON` | Compila `fuzzy_mobile` y sus tests |
| `FUZZY_BUILD_ADMIN` | `ON` | Compila `fuzzy-admin` |

CMake exige Qt ≥ 6.11 (`find_package(Qt6 6.11 ...)`) y C++20 sin extensiones.

### 4.4 Datos locales

El escritorio usa `QStandardPaths::AppDataLocation` con nombre de aplicación
`FuzzyCloud`:

| Sistema | Ruta de la base |
|---|---|
| Linux | `~/.local/share/FuzzyCloud/FuzzyCloud/clinica.db` |
| Windows | `%LOCALAPPDATA%\FuzzyCloud\FuzzyCloud\clinica.db` |
| macOS | `~/Library/Application Support/FuzzyCloud/FuzzyCloud/clinica.db` |

Las bases de los portales locales viven en `<datos>/portales/`. El teléfono
usa `FuzzyCloud Móvil` como nombre de aplicación y guarda `movil.db` en su
propio `AppDataLocation`.

### 4.5 Ejecución aislada para pruebas

Para no mezclar datos reales con pruebas manuales:

```bash
export XDG_DATA_HOME=/tmp/fuzzy-prueba
./build/fuzzy_desktop/fuzzy_desktop
./build/fuzzy_mobile/fuzzy_mobile
```

Las rutas quedan en `/tmp/fuzzy-prueba/FuzzyCloud/FuzzyCloud/clinica.db` y
`/tmp/fuzzy-prueba/FuzzyCloud/FuzzyCloud Móvil/movil.db`. Borrar
`/tmp/fuzzy-prueba` reinicia ambas apps sin tocar la instalación real.

La app móvil también se puede comprobar sin pantalla:

```bash
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software ./build/fuzzy_mobile/fuzzy_mobile
```

---

## 5. Modelo de datos

### 5.1 Cinco bases

| Base | Directorio de migraciones | Dónde vive | Contenido |
|---|---|---|---|
| `clinic` | `db/migrations/clinic` | Nodo (SQLite) y Turso | Dominio de la sede |
| `portal` | `db/migrations/portal` | Turso o archivo local | Proyección de un dueño |
| `inbox` | `db/migrations/inbox` | Turso | Altas de primera visita |
| `directory` | `db/migrations/directory` | Turso y JSON embebido | Directorio público |
| `mobile` | `db/migrations/mobile` | Teléfono (SQLite) | Caché, cola, portales y claves |

`tools/db-rebuild.sh` reconstruye las cuatro bases de Atlas (`clinic`,
`portal`, `inbox`, `directory`) en `db/local/`. La base `mobile` no está en
`atlas.hcl`: se aplica desde `MigrationRunner::mobile()` al arrancar la app y
en las pruebas.

### 5.2 Migraciones

- Atlas es la herramienta de autoría y validación; el runner de la aplicación
  aplica las migraciones embebidas como recursos Qt.
- `fuzzy_core/CMakeLists.txt` incluye los `.sql` de `clinic`, `portal` y
  `mobile` bajo `:/fuzzy/migrations/`. Las bases `inbox` y `directory` no se
  aplican desde la app: se despliegan en Turso con Atlas.
- `MigrationRunner::clinic()`, `portal()` y `mobile()` resuelven el directorio
  embebido; `FUZZY_MIGRATIONS_DIR` permite apuntar a `db/migrations` en
  pruebas.
- El runner registra la versión aplicada y es idempotente.
- `tools/db-rebuild.sh` valida integridad con `PRAGMA integrity_check` y
  comprueba que existan las tablas centinela.

### 5.3 Base `clinic`

| Grupo | Tablas |
|---|---|
| Instalación | `clinicas`, `instalacion` |
| Identidad | `roles`, `usuarios`, `clientes`, `cuentas` |
| Pacientes | `mascotas`, `mascota_galeria`, `adjuntos` |
| Agenda | `citas`, `historial_cambios_cita`, `tipos_servicio_cita`, `estados_cita` |
| Clínica | `consultas_medicas`, `signos_vitales`, `vacunas`, `recetas`, `receta_detalles` |
| Fase 9 | `tipos_examen_laboratorio`, `laboratorio_solicitudes`, `hospitalizaciones`, `hospitalizacion_evoluciones`, `cirugias`, `estetica_servicios`, `movimientos_inventario` |
| Catálogos | `servicios` |
| Inventario | `categorias_inventario`, `productos_insumos` |
| Caja | `facturas`, `detalles_factura`, `cierres_caja`, `cargos` |
| Portal | `portales` |
| Auditoría | `auditoria` |
| Sync | `sync_outbox`, `sync_change_log`, `sync_aplicado`, `sync_meta` |

Convenciones:

- `uuid` es la identidad estable entre nodos; `id` es local y no viaja.
- `created_at` y `updated_at` son milisegundos desde epoch.
- El borrado es lógico con `deleted_at` (tombstone), salvo el borrado físico de
  una base de portal.
- `sync_state` marca filas `pendiente` o `sincronizado`.
- Los catálogos (`roles`, `estados_cita`, `tipos_servicio_cita`,
  `categorias_inventario`) se siembran igual en todos los nodos y no viajan.

### 5.4 Base `portal`

`portal_meta` guarda la identidad del portal (`portal_uuid`, `clinica_uuid`,
`esquema_version`). Las tablas de datos son:

```
perfil · mascotas · mascota_fotos · visitas · recetas · receta_items
vacunas · laboratorio · pesos · citas · solicitudes_cita · adjuntos
```

El portal solo contiene filas de **un** dueño. `PortalProjector` verifica que
`portal_meta.clinica_uuid` coincida con la clínica antes de escribir y nunca
toca `solicitudes_cita`, `mascota_fotos` ni las filas que escribe la app.

### 5.5 Bases `inbox` y `directory`

- `inbox.altas`: solicitudes de primera visita. Es de un solo sentido: la app
  inserta y no lee. `uuid` es la clave de idempotencia.
- `directory.clinicas_publicas`: espejo de `directory/clinicas.json`. Solo
  datos comerciales y el token de inserción del buzón; nunca identidades ni
  tokens de portal.

### 5.6 Base `mobile`

| Tabla | Contenido | Cifrado |
|---|---|---|
| `ajustes` | `kdf_salt`, `kdf_iteraciones`, `clave_check` | No (metadatos KDF) |
| `portales_conocidos` | Portal, clínica, URL, huella, estado, fechas | Token cifrado |
| `cache_filas` | `portal_uuid`, `tabla`, `uuid`, `updated_at`, `deleted_at`, `pendiente` | Sobre AES-GCM |
| `outbox` | Portal, tabla, uuid, timestamps | Payload cifrado |
| `altas` | Altas del buzón | Datos cifrados |
| `registro_local` | Perfil y mascotas creadas antes de emparejar | Datos cifrados |

Las columnas de indexación (ids, fechas, estado) quedan en claro para poder
listar y sincronizar sin descifrar. El detalle está en
[`limites-cache.md`](limites-cache.md).

---

## 6. Núcleo de dominio

### 6.1 `IQueryExecutor`

Contrato único de persistencia:

```cpp
struct ExecResult { bool ok; long long affected; QVariant last_insert_id; };
struct QueryRow   { std::vector<QVariant> values; };

class IQueryExecutor {
public:
    virtual ExecResult execute(const QString& sql,
                               const std::vector<QVariant>& params = {}) = 0;
    virtual std::vector<QueryRow> query(const QString& sql,
                                        const std::vector<QVariant>& params = {}) = 0;
};
```

Los servicios usan SQLite como dialecto. `TursoHttpExecutor` traduce los
parámetros al formato Hrana (enteros como texto, blobs en base64, etc.).

### 6.2 Transportes

- **`SqliteExecutor`**: una conexión Qt SQL por nombre lógico y ruta. Se usa en
  escritorio, móvil y pruebas.
- **`TursoHttpExecutor`**: `POST <url>/v1/execute` con `Authorization: Bearer`.
  `salud()` distingue `ok`, `sin_red` y `sin_acceso`; el contrato de consulta
  por sí solo no puede diferenciar una red caída de un token revocado.

### 6.3 Servicios

| Servicio | Responsabilidad |
|---|---|
| `AuthService` | Bootstrap, login, cambio de clave, alta y edición de usuarios, recuperación del último Admin |
| `Politicas` | Matriz rol × permiso y aislamiento por clínica |
| `ServicioClinico` | Recepción, triaje, consulta, recetas, cargos, laboratorio, hospitalización, cirugía, estética, caja e inventario |
| `SyncOutbox` | Encolar filas y serializar entidades |
| `SyncEngine` | Empujar y jalar eventos entre nodos |
| `PortalProvisioner` | Crear la base del portal, su esquema y su QR |
| `PortalTokenService` | Emitir, verificar, rotar, revocar y borrar tokens; guardar solo SHA-256 |
| `PortalProjector` | Proyectar la ficha del dueño en su portal, con tombstones |
| `BuzonService` | Consumir altas del buzón en el nodo |
| `SolicitudCitaService` | Convertir solicitudes del portal en citas de la clínica |
| `MovilStore` | Clave, caché cifrada, outbox, altas y registro local del teléfono |
| `PortalCliente` | Descargar y subir tablas del portal con lista blanca |
| `EmparejamientoService` | Validar QR, huella, descarga e importación |
| `DirectorioCliente` | Leer y buscar el directorio público |
| `BuzonCliente` | Validar y enviar altas al buzón |
| `CryptoBox` | AES-256-GCM y PBKDF2 del teléfono |
| `PasswordHasher` | PBKDF2-HMAC-SHA256 de contraseñas |
| `FileStore` | Guardar binarios clínicos en el disco del nodo |
| `MascotaFotoService` | Validar, reencodar y reemplazar la foto del portal |
| `GaleriaClinicaService` | Metadatos y archivos de la galería clínica |
| `OperadorService` | Estado, respaldo, restauración, recuperación de Admin y migración |

### 6.4 `ServicioClinico`

Es la única puerta de escritura del flujo clínico. El controlador traduce QML a
estructuras; el servicio aplica RBAC, validaciones y transacciones.

- **Permisos:** cada método llama a `permitido(sesion, permiso, accion,
  recurso)`; el rol sale de `Sesion`, no de un parámetro del formulario.
- **Transacciones:** `abrirConsulta`, `emitirReceta`, `facturarCargos` y
  `ajustarStock` usan `BEGIN`/`COMMIT`/`ROLLBACK`.
- **Auditoría:** las operaciones relevantes insertan en `auditoria` con
  usuario, acción, entidad, uuid y detalle.
- **Precios congelados:** las líneas de receta guardan el precio al emitir.
- **Stock:** la receta puede descontar stock; la factura descuenta productos al
  emitir; cada ajuste genera un movimiento.
- **Caja:** `cerrarCaja` suma las facturas pagadas del día natural en curso y
  guarda calculado, real, desfase y observaciones.

---

## 7. Identidad y autorización

### 7.1 Contraseñas

`PasswordHasher` usa PBKDF2-HMAC-SHA256 con 600 000 iteraciones, salt aleatorio
de 16 bytes y comparación en tiempo constante. El formato almacenado es:

```
pbkdf2_sha256$<iteraciones>$<salt-hex>$<hash-hex>
```

El número de iteraciones va en el hash para poder subirlo sin invalidar los
hashes anteriores.

La contraseña temporal es de 12 caracteres, sin caracteres ambiguos, válida
72 horas y con cambio obligatorio. Se muestra una sola vez.

### 7.2 Bootstrap y recuperación

- El bootstrap solo se ofrece cuando no existe instalación ni clínica.
  `instalacion` es una fila única que impide reabrirlo.
- El servicio rechaza un segundo alta aunque la interfaz la pidiera.
- El último Admin activo no se puede desactivar ni degradar.
- No hay recuperación por correo. `fuzzy-admin recuperar-admin` reactiva la
  cuenta, emite una temporal nueva y la muestra una vez.

### 7.3 Roles y permisos

`Politicas::permisosDe(Rol)` implementa la matriz de
[`matriz-autorizacion.md`](matriz-autorizacion.md). `Politicas::exigir`
comprueba tres cosas:

1. El rol tiene el permiso.
2. La acción está permitida para ese permiso.
3. El contexto pertenece a la clínica de la sesión.

La matriz está congelada por el test de snapshot `tst_politicas`: un cambio de
permisos exige editar el documento y el test en el mismo commit.

### 7.4 Planos de identidad

| Plano | Quién | Credencial | Alcance |
|---|---|---|---|
| Personal | Admin, Veterinario, Recepcion, Estilista | Sesión del nodo | Su clínica |
| Cliente | Dueño | Token de su portal | Solo sus datos |
| Operador | Técnico con acceso al equipo | CLI `fuzzy-admin` | Local, auditado |

No existe identidad global (D25). Una persona con cuentas en dos clínicas tiene
dos cuentas; un dueño con mascotas en dos clínicas tiene dos portales (D26).

### 7.5 Token del portal

`PortalTokenService` guarda solo el SHA-256 del token en `portales`. El token
real se entrega una vez en el QR. En Turso, el token lo emite la base y solo
abre esa base; el nodo debe custodiarlo en el keyring del sistema según el
procedimiento de [`produccion.md`](produccion.md). Esa custodia es operativa:
el código actual no integra un keyring.

La huella del QR son 6 caracteres hexadecimales derivados del token. La app
muestra la huella y el operador confirma que coincide antes de descargar.

### 7.6 CLI de operador

```bash
fuzzy-admin estado --db <ruta> [--json]
fuzzy-admin verificar --db <ruta> [--respaldo <archivo>]
fuzzy-admin respaldar --db <ruta> --salida <archivo>
fuzzy-admin restaurar --db <ruta> --desde <archivo> --confirmar
fuzzy-admin recuperar-admin --db <ruta> --correo <correo>
fuzzy-admin migrar --db <ruta>
fuzzy-admin --version
```

Códigos de salida: `0` correcto, `1` uso incorrecto, `2` operación fallida.
La restauración exige detener el escritorio y deja una copia
`<ruta>.antes-de-restaurar-<ts>`.

---

## 8. Sincronización y proyección

> **Estado de integración:** el motor de sync y el proyector están
> implementados en `fuzzy_core` y cubiertos por pruebas, pero la app de
> escritorio y `fuzzy-admin` **no los arrancan** en esta versión. El escritorio
> crea portales locales (`local://portal/<uuid>`) desde **Vínculos**. La app
> móvil solo habla HTTP con Turso. El emparejamiento de punta a punta no se
> completa. Ver §15.

### 8.1 Outbox y change log

- `sync_outbox` guarda una fila por `(entidad, entidad_uuid)`; varias
  ediciones seguidas conservan solo la última.
- Cada evento lleva `event_uuid`, `entidad`, `entidad_uuid`, `operacion`
  (`upsert` o `delete`), `payload` JSON, `updated_at` y `origen_nodo`.
- `sync_change_log` conserva el historial y es la fuente del pull.
- `sync_aplicado` registra los `event_uuid` ya aplicados: reintentar es
  idempotente.
- `sync_meta` guarda `nodo_uuid`, `ultimo_push_at`, `ultimo_pull_id` y
  `ultimo_pull_at`.

`SyncOutbox::registrar` serializa la fila actual. `detectarPendientes` recorre
las entidades replicables y encola las filas con `sync_state = 'pendiente'`.
`marcarEnviado` solo limpia el evento si la fila no cambió mientras tanto.

### 8.2 `SyncEngine`

1. `preparar()` verifica las tablas de sync en ambos extremos y registra el
   nodo en `sync_meta`.
2. `empujar()` publica los eventos pendientes en el change log remoto. Si el
   proceso muere después de publicar y antes de marcar el outbox, el siguiente
   lote reinserta el mismo `event_uuid` y el change log lo descarta.
3. `jalar()` lee el change log desde `ultimo_pull_id`, aplica los eventos y
   avanza el marcador de agua. Un evento aplazado (por ejemplo, un hijo cuyo
   padre aún no llegó) detiene el marcador en el evento anterior; nunca se
   salta.
4. Los eventos de entidades desconocidas también se aplazan.

### 8.3 Entidades replicables

`SyncEntities.h` declara las entidades, sus columnas y sus referencias. Las
referencias viajan como uuid del padre y se resuelven al aplicar, porque los
`id` locales difieren entre nodos.

```
clientes → mascotas → citas / consultas_medicas → recetas → receta_detalles
                                      └→ vacunas / signos_vitales
```

No viajan catálogos, tokens, `portales`, `auditoria` ni rutas de imágenes. La
lista exacta y las prioridades están en `fuzzy_core/src/SyncEntities.cpp`.

### 8.4 `PortalProjector`

`proyectar(portal_uuid)`:

1. Abre la base del portal y aplica su esquema (migración perezosa).
2. Verifica `portal_meta.clinica_uuid` contra la clínica del nodo.
3. Copia las filas del dueño con `ON CONFLICT ... WHERE excluded.updated_at >=
   actual.updated_at` (LWW).
4. Marca como borradas (`deleted_at`) las filas que ya no existen en la
   clínica.
5. No toca `solicitudes_cita`, `mascota_fotos` ni las filas escritas por la
   app.

Es idempotente y se puede reconstruir desde cero sin duplicar filas.

### 8.5 Buzón y solicitudes de cita

- `BuzonService` consume una sola vez el alta objetivo, valida que pertenezca a
  la clínica y crea cliente/mascota. Un uuid desconocido o parámetros vacíos no
  consumen nada.
- `SolicitudCitaService` lee `solicitudes_cita` del portal, valida fecha,
  mascota y agenda, crea la cita en la clínica y escribe el resultado en la
  misma fila. El uuid de la cita se deriva del uuid de la solicitud, así que
  reintentar no duplica.

---

## 9. App móvil y caché cifrada

### 9.1 `MovilStore`

Estados:

```
sin clave → crear clave → desbloqueada → (emparejar / registrar / agendar)
                                ↓
                             bloquear / cerrar sesión
```

- `configurarClave()` deriva la clave con PBKDF2, guarda salt, iteraciones y un
  testigo cifrado; la contraseña no se almacena.
- `desbloquear()` vuelve a derivar y compara el testigo en tiempo constante.
- La clave derivada vive solo en memoria y se borra con `fill('\0')` al
  bloquear.
- `cerrarSesion()` llama a `borrarTodo()`: portales, caché, cola, altas,
  registro local y filas KDF. El teléfono queda como nuevo.

### 9.2 `CryptoBox`

Sobre:

```
[versión:1][nonce:12][etiqueta:16][ciphertext:n]
```

- AES-256-GCM, nonce aleatorio de 96 bits, etiqueta de 128 bits.
- Clave de 32 bytes derivada con PBKDF2-HMAC-SHA256 (600 000 iteraciones).
- AAD por fila: `portal_uuid/tabla/uuid`; por token:
  `portal_uuid/token/portal_uuid`. Mover un sobre a otra fila o portal hace
  fallar la autenticación.
- Implementación propia porque el entorno no tenía cabeceras de OpenSSL. Está
  validada contra vectores de `cryptography` (Python), pero no ha pasado una
  auditoría criptográfica externa.

### 9.3 `PortalCliente`

- Lista blanca cerrada de tablas y columnas (`tablas()`, `columnasDe()`). El SQL
  se construye solo con esos nombres, nunca con texto del QR o de la interfaz.
- `descargar()` rechaza la descarga si `portal_meta.clinica_uuid` no coincide
  con la clínica esperada.
- `sincronizar()` sube la cola y luego descarga. Un corte deja la cola intacta.
- `solicitarCita()` y `actualizarPerfil()` escriben primero en la caché y la
  cola; el uuid es idempotente.

### 9.4 Emparejamiento

El QR lleva:

```json
{"v":1,"url":"…","token":"…","portal_uuid":"…","clinica_uuid":"…",
 "clinica_nombre":"…","huella":"…"}
```

`EmparejamientoService::leerQr` valida versión, campos y huella.
`emparejar()` guarda el portal con el token cifrado, descarga las tablas
permitidas, verifica la clínica e importa el registro local. Si la descarga
falla, deshace el alta.

La importación compara el perfil por correo, DNI o teléfono, y la mascota por
nombre y especie sin distinguir mayúsculas. Las coincidencias se marcan
`importado`; las demás quedan pendientes.

### 9.5 Conflictos

`MovilStore::guardarFila()` aplica LWW por `updated_at`:

- Gana la marca de tiempo mayor; en empate, la remota.
- Una fila local pendiente está protegida hasta que su entrada de `outbox`
  desaparezca.
- Un tombstone remoto más nuevo oculta la fila pero no la borra del archivo.
- La clave primaria `(portal_uuid, tabla, uuid)` hace que descargar dos veces
  la misma versión sea idempotente.

### 9.6 Ciclo de vida de la caché

| Acción | Efecto |
|---|---|
| `limpiarCache()` | Borra filas descargadas; conserva cola, portales, tokens y clave |
| `olvidarPortal(uuid)` | Borra caché y token de ese portal |
| `purgarVencida()` | Borra caché con `ultima_sync` de más de 30 días |
| `bloquear()` | Borra la clave de memoria |
| `cerrarSesion()` | Borra todo y deja el teléfono como nuevo |
| Revocación en el nodo | La sonda devuelve `sin_acceso`; el portal queda `revocado` |

---

## 10. Imágenes

Hay dos rutas separadas:

| Ruta | Dónde vive | Qué viaja |
|---|---|---|
| Foto de perfil | `mascota_fotos` del portal | La escribe la app; el proyector no la toca |
| Galería clínica | Disco del nodo (`mascota_galeria`) | Solo metadatos de `adjuntos` en el change log |

`MascotaFotoService::reemplazar()` valida y reencoda:

| Regla | Valor |
|---|---|
| Tamaño máximo de entrada | 2 MB |
| Lado mayor | 1024 px (sin ampliar) |
| Píxeles máximos | 40 MP |
| Formatos | Cualquiera que Qt pueda decodificar |
| Salida | JPEG, calidad 85→45 si hace falta |
| EXIF | Se aplica orientación y se descarta al recodificar |
| Transparencia | Se compone sobre blanco |

La foto es una sola fila por mascota (`mascota_uuid UNIQUE`); el reemplazo es
un UPSERT que conserva el uuid. Los binarios clínicos nunca entran a Turso.

---

## 11. Seguridad

### 11.1 Modelo de amenaza

| Escenario | Protegido |
|---|---|
| Robo del teléfono con la app bloqueada | Sí: sin clave no se descifra caché ni tokens |
| Copia de `movil.db` | Sí: el contenido de negocio está cifrado; quedan ids y fechas |
| QR manipulado o fotografiado | Sí: huella de 6 hex confirmada con el operador y verificación de clínica |
| Portal revocado | Parcial: deja de sincronizar, pero no borra lo ya descargado |
| Dueño que olvida su clave | Datos irrecuperables por diseño |
| Malware o root con la app desbloqueada | No: la clave está en memoria |
| Capturas o backups del sistema operativo | Fuera del alcance de la app |

### 11.2 Secretos

- El token de un portal nunca se guarda en claro en la base de la clínica: solo
  su SHA-256.
- El binario móvil no contiene `token_portal`, `portal_token`, `token-clinica`
  ni `token-portal`; `tools/check.sh` y el job `movil` lo verifican con
  `strings`.
- `tools/scan-secrets.sh` corre en CI y en `check.sh`.
- Los tokens de Turso viven en el entorno protegido de GitHub o en el keyring
  del nodo, nunca en Git.
- La base `clinic` **no está cifrada en reposo**: se apoya en los permisos del
  sistema operativo y en el control físico del equipo. Es una limitación
  conocida.

### 11.3 Aislamiento

- El rol y `clinica_id` salen de la sesión.
- `Politicas::exigir` compara el contexto con la clínica de la sesión.
- Cada portal se abre con su propio ejecutor y su propio token; no se recibe el
  `clinica_uuid` de otra clínica.
- La caché móvil se indexa por `portal_uuid`; la clave de un portal no abre
  filas de otro.
- La revocación es por portal: una sonda `sin_acceso` marca solo ese portal.

---

## 12. Pruebas y CI

### 12.1 Suites (24)

| Suite | Qué cubre |
|---|---|
| `tst_version` | Versión del proyecto |
| `tst_sqlite_executor` | Contrato SQLite |
| `tst_politicas` | Matriz rol × permiso (snapshot) |
| `tst_portal_token_service` | Emisión, verificación, rotación y revocación |
| `tst_buzon_service` | Consumo único y aislamiento del buzón |
| `tst_turso_http_executor` | Traducción Hrana y salud del transporte |
| `tst_migration_runner` | Migraciones embebidas e idempotencia |
| `tst_portal_provisioner` | Creación de base, meta y QR |
| `tst_portal_projector` | Proyección por dueño, tombstones e idempotencia |
| `tst_portal_contract` | El portal solo contiene el esquema del dueño |
| `tst_buzon_recoleccion` | Recolección de altas en el nodo |
| `tst_solicitud_cita` | Solicitud → cita sin duplicados |
| `tst_sync_engine` | Dos nodos convergen, LWW y reintento idempotente |
| `tst_clinico` | Flujo clínico completo y proyección |
| `tst_operador` | Estado, respaldo, restauración y recuperación de Admin |
| `tst_file_store` | Guardado y borrado de binarios |
| `tst_mascota_foto` | Validación y reencodado de la foto |
| `tst_adjuntos_clinicos` | Metadatos de adjuntos sin binarios |
| `tst_cryptobox` | AES-GCM/PBKDF2 contra vectores externos |
| `tst_movil_servicios` | Clave, caché, outbox, LWW, caducidad y aislamiento |
| `tst_controladores` | 11 casos de escritorio contra SQLite real |
| `tst_visual` | 17 capturas y navegación por clics |
| `tst_mobile` | 8 casos de la capa que ve QML |
| `tst_mobile_qml` | Carga real del módulo QML y directorio sin tokens |

`tst_controladores` enlaza solo `fuzzy_desktop_controllers`; no usa dobles:
cada caso migra una base temporal y ejercita los servicios reales.

### 12.2 Baselines visuales

`tst_visual` levanta la app completa contra una base temporal sembrada y
captura 17 pantallas. Para que sea reproducible:

- Ventana 1440×900, estilo `Basic`, `QT_QPA_PLATFORM=offscreen` y
  `QT_QUICK_BACKEND=software`.
- Fuente DejaVu Sans empaquetada.
- `FUZZY_HOY=2026-03-09` fija la fecha.
- Comparación en escala de grises 192×120: falla si la diferencia media supera
  `2.5` o un bloque de 8×8 supera `24.0`.

Regenerar tras un cambio deliberado:

```bash
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
  FUZZY_UPDATE_BASELINES=1 ./build/fuzzy_desktop/tests/tst_visual
```

### 12.3 Puertas

```bash
tools/check.sh
```

Incluye:

1. `tools/db-rebuild.sh` y `atlas migrate validate` de los cuatro esquemas.
2. Build completo.
3. `ctest` (24 suites).
4. `qmllint` de escritorio y móvil.
5. Escaneo del binario móvil en busca de tokens de portal.
6. `tools/scan-secrets.sh`.
7. `tools/rollback-drill.sh`.

### 12.4 CI

`.github/workflows/ci.yml` tiene 9 jobs: **Esquema**, **Qt**, **Portal y
sync**, **Imágenes**, **Escritorio**, **Móvil**, **Clínico**, **Producción** y
**Contrato del portal**. Todos usan Qt 6.11.2 y compilan antes de ejecutar su
subconjunto de pruebas.

`.github/workflows/deploy.yml` es manual, con entornos `staging` y
`production`, y por defecto hace `aplicar=false` (dry-run).

---

## 13. Operación y despliegue

### 13.1 Entornos

| Entorno | Uso | Datos | Quién despliega |
|---|---|---|---|
| Local | Desarrollo y pruebas | Fixtures | Cualquiera |
| Staging | Ensayo de runbooks | Sintéticos | CI y operador |
| Producción | Sede real | Clínicos | Operador con aprobación |

El entorno `production` de GitHub exige revisores distintos de quien dispara,
rama `main` y su propio secreto `TURSO_DATABASE_TOKEN`. La configuración es
externa al repositorio.

### 13.2 Alta de una clínica

```bash
tools/provision-production.sh \
  --slug aurora --nombre "Clínica Aurora" --ciudad "Bogotá" \
  --direccion "Calle 1 #2-3" --telefono "+57 300 000 0000" --dry-run

# Tras revisar el plan:
tools/provision-production.sh \
  --slug aurora --nombre "Clínica Aurora" --ciudad "Bogotá" \
  --direccion "Calle 1 #2-3" --telefono "+57 300 000 0000" \
  --confirmar-produccion
```

Crea el grupo `fuzzy-clinic-<slug>`, las bases de dominio y buzón, emite el
token de grupo, aplica las migraciones y deja el token en
`secrets/produccion-<slug>.token` con permisos `0600`. No crea portales ni
publica el directorio.

### 13.3 Esquema remoto

```bash
gh workflow run deploy.yml \
  -f entorno=production \
  -f componente=clinic \
  -f base=fuzzy-clinic-aurora \
  -f aplicar=false
```

El workflow valida el destino, muestra `migrate status`, ejecuta
`migrate apply --dry-run`, aplica solo si `aplicar=true` y vuelve a mostrar el
estado. Una base por ejecución; no se encadenan sedes.

### 13.4 Nodo local

El escritorio aplica las migraciones embebidas al arrancar. Para un nodo
detenido o una base nueva:

```bash
fuzzy-admin migrar --db "<ruta>/clinica.db"
fuzzy-admin estado --db "<ruta>/clinica.db"
```

Orden: respaldo verificado → ensayo en staging → actualización del nodo →
verificación → registro.

### 13.5 Respaldos y restauración

```bash
# Local (VACUUM INTO + .sha256 + .json)
tools/backup.sh --db "<ruta>/clinica.db" --out /mnt/respaldos/aurora --keep 30

# Turso (.dump comprimido + .sha256 + .json)
tools/backup.sh --turso fuzzy-clinic-aurora --out /mnt/respaldos/aurora --keep 30

# Verificar y restaurar
fuzzy-admin verificar --db "<ruta>/clinica.db" --respaldo /mnt/respaldos/aurora/clinica-<ts>.db
fuzzy-admin restaurar --db "<ruta>/clinica.db" \
  --desde /mnt/respaldos/aurora/clinica-<ts>.db --confirmar
```

La restauración deja `<ruta>.antes-de-restaurar-<ts>` y exige detener el
escritorio. Una copia no cuenta hasta que la verificación pasa.

### 13.6 Portales

Los portales se migran de forma perezosa antes de proyectar
(`PortalProvisioner::migrar`). No hay despliegue masivo: cada portal se
actualiza cuando el nodo le proyecta datos. La creación, rotación, revocación y
borrado están en [`runbook-portal.md`](runbook-portal.md).

---

## 14. Guía de extensión

### 14.1 Añadir una migración

1. Crea `db/migrations/<base>/000N_descripcion.sql` con el siguiente número.
2. Si la base es `clinic`, `portal` o `mobile`, añádela a
   `fuzzy_core/CMakeLists.txt` en `qt_add_resources`. `inbox` y `directory`
   se aplican solo con Atlas en Turso.
3. Actualiza el centinela de `tools/db-rebuild.sh` si crea una tabla nueva.
4. Ejecuta `tools/db-rebuild.sh`, `atlas migrate validate` y `ctest`.
5. Si cambia el contrato del portal, actualiza `tst_portal_contract`.

### 14.2 Añadir una entidad al sync

1. Declara la tabla en `SyncEntities.cpp` con columnas, referencias y
   prioridad.
2. Añade la entidad a `entidadesSync()`.
3. Asegúrate de que la tabla tenga `uuid`, `created_at`, `updated_at`,
   `deleted_at` y `sync_state`.
4. Añade una prueba en `tst_sync_engine` con dos nodos y un caso LWW.
5. Si la entidad se proyecta al portal, añádela a `PortalProjector` y a
   `tst_portal_projector`.

### 14.3 Añadir un permiso

1. Añade el valor a `enum class Permiso` en `Permisos.h`.
2. Añade el permiso a `permisosDe(Rol)` en `Politicas.cpp`.
3. Añade la acción permitida en `accionPermitida`.
4. Actualiza `docs/matriz-autorizacion.md` y el snapshot de `tst_politicas` en
   el mismo commit.
5. Si la UI lo necesita, declara el permiso en `ShellView.qml` o en la vista
   correspondiente.

### 14.4 Añadir una vista o un controlador

1. Crea el controlador en `fuzzy_desktop/<módulo>/` y expón propiedades con
   `Q_PROPERTY` y acciones con `Q_INVOKABLE`.
2. Añade el controlador a `fuzzy_desktop_controllers` y a `main.cpp` con
   `setContextProperty`.
3. Crea la vista en `fuzzy_desktop/qml/` y añádela al `StackLayout` de
   `ShellView.qml` con su permiso.
4. Añade una captura a `tst_visual` si la vista es principal.
5. Pasa `qmllint` y `tst_controladores`.

### 14.5 Añadir una prueba

- **Core:** crea `fuzzy_core/tests/tst_<tema>.cpp`; si necesita esquema, usa
  `fuzzy_esquema_test(tst_<tema>)` para que reciba `FUZZY_MIGRATIONS_DIR`.
- **Escritorio:** añade el caso a `tst_controladores` o una captura a
  `tst_visual`.
- **Móvil:** añade el caso a `tst_mobile` o `tst_mobile_qml`.
- **Cifrado:** añade vectores a `tst_cryptobox`.

---

## 15. Limitaciones y deuda técnica

1. **Sync y proyección no están cableados a la app.** `SyncEngine`,
   `PortalProjector`, `BuzonService` y `SolicitudCitaService` existen y están
   probados, pero el escritorio no los ejecuta ni expone una pantalla de sync.
   `fuzzy-admin` tampoco sincroniza. Los runbooks describen el procedimiento
   previsto, no una automatización presente.
2. **Portales locales vs. móvil HTTP.** `VinculosController` crea el portal con
   URL vacía, que `PortalProvisioner` convierte en `local://portal/<uuid>`. El
   móvil usa `TursoHttpExecutor`, que solo entiende HTTP. El emparejamiento
   completo no funciona contra el escritorio actual.
3. **Sin integración de keyring.** El token real del portal no se persiste en
   la base (solo su SHA-256), pero el nodo no tiene un almacén de secretos
   implementado; la custodia es manual.
4. **Base `clinic` sin cifrar en reposo.** Depende de permisos del sistema y del
   control físico del equipo.
5. **`CryptoBox` propio sin auditoría externa.** Validado con vectores, pero
   sustituible por OpenSSL o libsodium sin cambiar el formato del sobre.
6. **LWW depende del reloj.** Un `updated_at` muy desviado puede hacer ganar
   una edición vieja; el empate favorece al remoto y las filas pendientes están
   protegidas.
7. **Cierre de caja sin unicidad por día.** `cerrarCaja` calcula el día natural
   y registra el cierre, pero no impide varios cierres del mismo día.
8. **Huecos de interfaz:** el botón **Exportar** de Propietarios no exporta; el
   botón **En espera** del detalle de cita no cambia el estado; no hay botón
   para ingresar una hospitalización; no hay pantalla de auditoría; la galería
   clínica no se muestra y no hay UI para subir foto.
9. **Directorio embebido de staging.** `directory/clinicas.json` contiene dos
   clínicas de ejemplo. Producción debe publicar el directorio real con
   `tools/publish-directory.sh`.
10. **Sin notificaciones push, impresión ni pasarela de pago.**
11. **Sin identidad global.** Una cuenta y un portal por clínica.
12. **Sin recuperación de la clave móvil.** Es una decisión de diseño, no una
    carencia.
13. **La revocación no borra lo descargado.** El teléfono conserva la última
    copia hasta que olvida el portal, cierra sesión o vence la caché (30 días).

---

## 16. Referencias

- [`PLAN.md`](PLAN.md) — decisiones, arquitectura y fases
- [`escritorio.md`](escritorio.md) — módulos y pruebas del escritorio
- [`movil.md`](movil.md) — estados, vistas y servicios del teléfono
- [`clinico.md`](clinico.md) — flujo clínico y permisos
- [`matriz-autorizacion.md`](matriz-autorizacion.md) — roles y acciones
- [`sync-y-proyeccion.md`](sync-y-proyeccion.md) — outbox, LWW y proyector
- [`imagenes.md`](imagenes.md) — foto del portal y galería clínica
- [`limites-cache.md`](limites-cache.md) — modelo de amenaza del teléfono
- [`produccion.md`](produccion.md) — entornos, secretos, alta y respaldos
- [`entornos.md`](entornos.md) — configuración local, staging y producción
- [`runbook-master.md`](runbook-master.md) — operación del nodo
- [`runbook-portal.md`](runbook-portal.md) — operación de un portal
- [`manual-usuario.md`](manual-usuario.md) — guía para el personal y el dueño
