# Móvil (fase 8)

La app del teléfono alcanza la paridad con la fase 2 del producto: directorio,
buzón, registro local, importación al emparejar, portal directo, historial,
agenda, mis citas, lista de portales con selector, caché cifrada y modo avión.
El diseño de origen está en [`PLAN.md`](./PLAN.md) §6 y las reglas del portal en
§4.

## 1. Capas

| Pieza | Dónde vive | Qué hace |
|---|---|---|
| Servicios de dominio | `fuzzy_core/src/{MovilStore,PortalCliente,BuzonCliente,DirectorioCliente,EmparejamientoService}.cpp` | Cifrado por fila, LWW, cola, validación y aislamiento por portal |
| `CryptoBox` | `fuzzy_core/src/CryptoBox.cpp` | AES-256-GCM y PBKDF2-HMAC-SHA256 sin depender de OpenSSL |
| Controlador | `fuzzy_mobile/app/MobileController.cpp` | Orquesta los servicios y expone propiedades enlazables a QML |
| `fuzzy_mobile_controllers` | biblioteca estática | Misma lógica para la app y para las pruebas, sin QML |
| `fuzzy_add_mobile_qml(target)` | `fuzzy_mobile/CMakeLists.txt` | Registra el módulo `FuzzyMovil`, la fuente, el directorio y las vistas |
| `main.cpp` | `fuzzy_mobile/main.cpp` | Migra la base local, crea el controlador e inyecta `movil`, `estilo` y `versionApp` |
| Vistas | `fuzzy_mobile/qml/*.qml` | Solo presentación; ninguna regla de negocio |

El teléfono no tiene base de clínica: su única persistencia es
`movil.db`, cifrada con la clave del dueño. Todo lo demás viene del portal o del
directorio.

## 2. Estados de la app

```
sin clave → crear clave → desbloqueada → (emparejar / registrar / agendar)
                                ↓
                             bloquear / cerrar sesión
```

- **Sin clave**: `BloqueoView` pide una clave de al menos 8 caracteres dos veces.
  `MovilStore::configurarClave()` deriva la clave con PBKDF2 y guarda salt,
  iteraciones y un testigo cifrado; la contraseña no se almacena.
- **Bloqueada**: la clave derivada vive solo en memoria.
  `MovilStore::desbloquear()` la vuelve a derivar y comprueba el testigo.
- **Desbloqueada**: se leen portales, caché y cola.
- **Cerrar sesión**: `MovilStore::borrarTodo()` elimina portales, caché, cola,
  altas, registro local **y las filas de la clave**. El teléfono queda como
  recién instalado; hay que crear clave y volver a emparejar.

## 3. Vistas

| Vista | Alcance |
|---|---|
| `BloqueoView` | Crear clave / desbloquear |
| `InicioView` | Perfil, mascotas de todos los portales, registro local pendiente |
| `CitasView` | Citas confirmadas y solicitudes enviadas |
| `HistorialView` | Visitas, vacunas, pesos, laboratorio y recetas; filtro por mascota |
| `PortalesView` | Lista de portales, selector, olvidar, lectura del QR y confirmación de huella |
| `DirectorioView` | Buscar clínica y enviar una solicitud de primera visita |
| `AjustesView` | Modo avión, sincronización, bloqueo, borrado de caché y cierre de sesión |
| Diálogos | Registro de perfil, registro de mascota, ficha, solicitud de cita y alta |

`ShellView` mantiene la clínica activa y el estado de sincronización en la
cabecera. La barra inferior cambia de página sin lógica: cada vista enlaza el
controlador y emite intenciones.

## 4. Emparejamiento e importación

El QR del nodo lleva
`{v, url, token, portal_uuid, clinica_uuid, clinica_nombre, huella}`.

1. `leerQr()` valida versión, campos y huella; la app muestra la huella de 6 hex
   y el nombre de la clínica.
2. El operador confirma que coincide y `confirmarEmparejamiento()` descarga el
   portal.
3. `EmparejamientoService::emparejar()` guarda el portal con el token cifrado,
   verifica que `portal_meta.clinica_uuid` sea la clínica del QR y descarga las
   tablas permitidas.
4. `importarRegistro()` compara el registro local con lo descargado:
   - perfil por **correo**, **DNI** o **teléfono**;
   - mascota por **nombre + especie**, sin distinguir mayúsculas.
   Las coincidencias se marcan `importado`; las demás quedan pendientes y se
   muestran en Inicio como «aún no registradas en la clínica».
5. Si la descarga falla, el alta se deshace: no queda un portal a medio
   emparejar.

La huella se compara en minúsculas en ambos lados; el servicio la devuelve en
mayúsculas para mostrarla.

## 5. Portal activo y aislamiento

Un portal por clínica (D25, D26). `PortalCliente` se construye con el ejecutor
de **una** base de portal y nunca recibe el `clinica_uuid` de otra:

- La caché se indexa por `portal_uuid`; la clave del portal A no abre filas de B.
- `descargar()` rechaza la descarga si `portal_meta.clinica_uuid` no coincide con
  la clínica emparejada.
- La revocación es por portal: una sonda `sin_acceso` marca solo ese portal como
  `revocado`; el otro sigue sincronizando.
- «Olvidar portal» borra su caché y su token; los demás no se tocan.
- Las acciones de escritura comprueban que la mascota pertenezca al portal
  activo antes de encolar una solicitud.

## 6. Modo avión y cola

Toda escritura del dueño va primero a la caché local y a la tabla `outbox`.
Después:

- **Modo avión**: no se intenta red. La cita o el cambio de perfil quedan
  `pendiente`, visibles en la app, y la cabecera muestra «N cambio(s) por
  enviar».
- **Con red**: `sincronizar()` sube la cola y luego descarga. Las filas locales
  pendientes no se pisan con una descarga que no las incluya; cuando el portal
  devuelve la fila, `pendiente` se limpia y la cola se vacía.
- Un corte a mitad deja la cola intacta; el siguiente intento reenvía lo mismo.
  El `uuid` es idempotente: reintentar no duplica la solicitud.
- «Sin red» y «token revocado» se distinguen por la sonda del transporte
  (`TursoHttpExecutor::salud()`), no por el resultado vacío de una consulta.

## 7. Conflictos y duplicados

`MovilStore::guardarFila()` aplica LWW por `updated_at`:

- Gana la fila con marca de tiempo mayor.
- Empate: gana la remota.
- Una fila local pendiente está protegida hasta que su entrada de `outbox`
  desaparezca.
- Un tombstone remoto más nuevo oculta la fila (`deleted_at`) pero no la borra
  del archivo; sigue disponible para auditoría local.
- Descargar dos veces la misma versión es idempotente: la clave primaria es
  `(portal_uuid, tabla, uuid)`.

## 8. Directorio y buzón

`directory/clinicas.json` viaja embebido en el binario. Es público por diseño
(D22): solo datos comerciales y el token de **inserción** del buzón. La app:

- busca por nombre, ciudad o dirección en memoria;
- valida la solicitud con `BuzonCliente::normalizar()` **antes** de guardarla o
  enviarla (nombre 3–120, teléfono 7–20, correo con forma válida, límites de
  texto);
- guarda el alta con un `uuid` fijo, así que reintentar es idempotente;
- si no hay red, la deja `pendiente` y la reintenta desde la vista del
  directorio.

## 9. Pruebas

`fuzzy_core/tests/tst_movil_servicios.cpp` — 13 casos sobre SQLite real:

```
desbloqueoYClave · directorioDesdeJsonYBuscador · altaInvalidaNoSaleDelTelefono
altaIdempotentePorUuid · emparejamientoVerificaHuella · emparejamientoDescargaEImporta
dosPortalesAisladosYRevocacionIndependiente · modoAvionYColaSincroniza
conflictoLwwDuplicadoYTombstone · caducidadDeLaCache
tokenYCamposClinicosNoEstanEnClaro
```

`fuzzy_mobile/tests/tst_mobile.cpp` — 8 casos sobre la capa que ve QML:

```
flujoOfflineASyncConAltaYEmparejamiento · dosPortalesAisladosYSelector
huellaIncorrectaNoEmpareja · revocacionNoTumbaElOtroPortal
cerrarSesionBorraTodo · mascotaAjenaNoSePuedeAgendar
```

`fuzzy_mobile/tests/tst_mobile_qml.cpp` — carga real del módulo QML (bloqueo y
shell desbloqueado) y comprobación de que el directorio embebido solo lleva las
claves permitidas y ningún token de portal.

## 10. Gate

```bash
ctest --test-dir build -R "tst_cryptobox|tst_movil_servicios|tst_mobile|tst_mobile_qml"
qmllint -I "$QT_PREFIX/qml" fuzzy_mobile/qml/*.qml
strings -a build/fuzzy_mobile/fuzzy_mobile | grep -i token_portal   # debe fallar
```

`tools/check.sh` y el job `movil` de CI repiten esa secuencia y fallan si el
binario contiene `token_portal`, `portal_token`, `token-clinica` o
`token-portal`. Los avisos de `qmllint` por acceso no cualificado a `movil`,
`estilo` y `versionApp` son esperados: se inyectan con `setContextProperty`.

La app se puede ejecutar sin pantalla para una comprobación rápida:

```bash
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software ./build/fuzzy_mobile/fuzzy_mobile
```
