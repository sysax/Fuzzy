# Escritorio (fase 7)

La aplicación de escritorio alcanza la paridad con las fases 1 y 3 del
producto: acceso, shell, panel, agenda, pacientes e historial, propietarios,
flujo clínico (fase 9), inventario, facturación, vínculos y ajustes. El diseño
de origen está en [`PLAN.md`](./PLAN.md) §6, las reglas de identidad en §3 y el
detalle clínico en [`clinico.md`](./clinico.md).

## 1. Capas

| Pieza | Dónde vive | Qué hace |
|---|---|---|
| Controladores | `fuzzy_desktop/{agenda,ajustes,…}/*Controller.cpp` | Consultan el SQLite del nodo y adaptan el dominio a propiedades QML |
| `fuzzy_desktop_controllers` | biblioteca estática | Misma lógica para la app y para las pruebas, sin QML ni plataforma gráfica |
| `fuzzy_add_desktop_qml(target)` | `fuzzy_desktop/CMakeLists.txt` | Registra el módulo `Fuzzy` y los recursos (QML, iconos, fuente) en cualquier target |
| `main.cpp` | `fuzzy_desktop/main.cpp` | Aplica migraciones, crea servicios y controladores, e inyecta las propiedades de contexto |
| Vistas | `fuzzy_desktop/qml/*.qml` | Sin lógica de negocio: solo presentación y llamadas a controladores |

La app y el arnés visual comparten el **mismo** `Main.qml` y el mismo módulo
QML. No hay una copia de vistas para pruebas.

## 2. Acceso y alta

`AccessView` tiene tres estados con un único formulario:

| Estado | Cuándo aparece | Acción |
|---|---|---|
| `login` | Ya existe instalación | `sesion.iniciarSesion()` |
| `bootstrap` | No hay instalación ni clínica | `sesion.bootstrap()` crea clínica, Admin y sesión |
| `cambio` | El usuario entró con una clave temporal | `sesion.cambiarPassword()` |

Reglas aplicadas por `AuthService` (D4, D6, D8, D16, D19):

- PBKDF2-HMAC-SHA256 con 600 000 iteraciones, salt aleatorio por usuario y
  comparación en tiempo constante (`PasswordHasher`).
- Clave temporal de 12 caracteres, válida **72 horas**; al entrar obliga a
  cambiarla.
- El bootstrap solo se ofrece mientras no exista instalación; el servicio
  rechaza un segundo alta aunque la interfaz la pidiera.
- El correo es único por clínica sin distinguir mayúsculas.
- El último Admin activo no se puede degradar ni desactivar.
- Sin recuperación por correo ni invitaciones: el Admin restablece la clave y
  entrega el temporal en mano (D5, D11).

## 3. Shell y navegación

`ShellView` muestra la barra lateral y la barra superior con el usuario y su
rol. Cada entrada declara el permiso que necesita (`VerPanel`,
`GestionarCitas`, `RegistrarConsulta`, …); `SessionController.puede()` decide
si se muestra. El contenido es un `StackLayout` con una página por módulo y
cada página consulta su controlador al cargarse.

Las entradas se habilitan contra `sesion.permisos`, una propiedad con NOTIFY
que devuelve los nombres de permiso del rol actual. No se usa `puede()` en un
enlace: es una invocación sin señal, así que al arrancar sin sesión el enlace
se evaluaba una sola vez con el rol `Desconocido` y el sidebar quedaba muerto
después de entrar. `puede()` sigue disponible para usos imperativos y se
implementa sobre la misma lista.

## 4. Módulos

| Vista | Controlador | Alcance |
|---|---|---|
| Panel | `PanelController` | KPIs del día, agenda, sala de espera, semana y alertas de stock |
| Agenda | `AgendaController` | Semana/día, alta, cambio de estado, mover y cancelar |
| Pacientes | `PacientesController` | Búsqueda, filtro por especie, alta y ficha |
| Ficha | `PacientesController` | Historia, signos, vacunas, alergias y contacto |
| Historial clínico | `PacientesController` | Consulta transversal de todas las mascotas |
| Propietarios | `PropietariosController` | Directorio, alta, edición y mascotas |
| Clínico | `ClinicoController` | Recepción, triaje, consulta, recetas, laboratorio, hospitalización, cirugía, estética y caja |
| Inventario | `InventarioController` | Stock, mínimo, caducidad, ajustes y pedidos |
| Facturación | `FacturacionController` | Borrador, emisión, cobro, anulación y cierre de caja |
| Vínculos | `VinculosController` | Alta de portal, QR de emparejamiento, revocación y borrado |
| Ajustes | `AjustesController` | Datos de la sede y equipo (roles, claves temporales) |

Los filtros y la paginación se resuelven en SQL para que sigan siendo
correctos con muchos registros. Las cifras de cabecera describen la lista
filtrada que ve el usuario.

## 5. Pruebas

### Controladores

`fuzzy_desktop/tests/tst_controladores.cpp` enlaza solo
`fuzzy_desktop_controllers` y ejecuta 13 casos contra SQLite real:

```
bootstrapYLogin · reglasDelEquipo · restablecerPassword · pacientesYHistoria
propietarios · inventario · facturacionYCaja · vinculosDelPortal
agendaDeCitas · ajustesDeClinica · clinicoFlujo
```

Cubren el alta única, la clave temporal, el último Admin, la unicidad de
correo, el historial, el stock, la caja, el token del portal, el cambio de
estado de una cita y el puente QML del flujo clínico. No usan dobles: cada caso
migra una base temporal y ejercita los servicios reales.

### Baselines visuales

`fuzzy_desktop/tests/visual/tst_visual.cpp` levanta la app completa contra una
base temporal sembrada y captura 17 pantallas:

```
access-login · access-bootstrap · access-change
panel · agenda-semana · agenda-dia
pacientes · ficha-paciente · historial · propietarios
clinico · clinico-laboratorio
inventario · facturacion · vinculos
ajustes-clinica · ajustes-equipo
```

Detalles que hacen las capturas reproducibles:

- Ventana de 1440×900, estilo `Basic`, `QT_QPA_PLATFORM=offscreen` y
  `QT_QUICK_BACKEND=software`.
- Fuente DejaVu Sans empaquetada (`fuzzy_desktop/assets/fonts`): no depende de
  las fuentes del sistema ni de CI.
- `FUZZY_HOY=2026-03-09` (lunes) fija la fecha de todos los controladores.
- Semilla con datos reales (propietarios, mascotas, consultas, signos, vacunas,
  citas, productos, facturas, exámenes, hospitalizaciones, cirugías, estética,
  cargos y un portal emparejado), creada con los mismos servicios que la app.
- Comparación en escala de grises 192×120: falla si la diferencia media supera
  `2.5` o si un bloque de 8×8 supera `24.0`.

Para regenerar tras un cambio deliberado:

```bash
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software \
  FUZZY_UPDATE_BASELINES=1 ./build/fuzzy_desktop/tests/tst_visual
```

Las capturas de la última ejecución quedan en
`$FUZZY_VISUAL_ARTIFACT_DIR` (por defecto, el directorio temporal del sistema),
lo que permite revisar una regresión sin volver a ejecutar la suite.

El mismo binario tiene un caso de comportamiento, `navegacionPorClicks`, que no
compara imágenes: carga la app **sin sesión** (el orden real de arranque),
entra con un usuario y hace clic en las entradas del sidebar. Reproduce el
fallo del enlace de permisos congelado y verifica que el `StackLayout` cambia
de página.

## 6. Gate

`ctest` ejecuta `tst_controladores` y `tst_visual`; `tools/check.sh` añade el
lint de **todas** las vistas:

```bash
qmllint -I "$QT_PREFIX/qml" fuzzy_desktop/qml/*.qml
```

En CI, el job `desktop` repite build, ambas suites y el lint. Los avisos de
`qmllint` por acceso no cualificado a propiedades de contexto son esperados:
la app inyecta controladores con `setContextProperty` para no depender de
singletons QML.
