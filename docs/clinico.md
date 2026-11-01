# Flujo clínico (fase 9)

La fase 9 cierra la paridad con la fase 3 del producto: recepción y triaje,
consulta, recetas y cargos, catálogos, laboratorio, hospitalización, cirugía,
estética, caja e inventario. Toda la lógica de negocio vive en
`fuzzy::core::ServicioClinico`; el escritorio solo traduce formularios.

## 1. Capas

| Pieza | Dónde vive | Qué hace |
|---|---|---|
| `ServicioClinico` | `fuzzy_core/src/ServicioClinico.cpp` | RBAC, validaciones, transacciones, auditoría y cargos |
| `ClinicoController` | `fuzzy_desktop/clinico/` | Puente QML: listas, formularios y mensajes |
| `ClinicoView.qml` | `fuzzy_desktop/qml/` | Ocho pestañas de presentación; sin reglas |
| `0006_clinico.sql` | `db/migrations/clinic/` | Laboratorio, hospitalización, cirugía, estética e inventario |

El controlador no decide permisos ni totales: llama al servicio y publica el
error o el mensaje que este devuelve. La sesión (rol y `clinica_id`) se inyecta
desde `SessionController`; nunca sale del formulario.

## 2. Flujos de punta a punta

| Flujo | Pasos | Permiso que lo autoriza |
|---|---|---|
| Recepción → caja | recibir cita → triaje → abrir/cerrar consulta → receta → cargos → factura → cierre de caja | `GestionarCitas`, `RegistrarConsulta`, `EmitirFactura`, `CerrarCaja` |
| Catálogos → laboratorio | alta de examen → solicitar → iniciar → resultado → entregar → proyección al portal | `ConfigurarClinica`, `RegistrarConsulta` |
| Hospitalización | ingreso → evolución → alta | `RegistrarConsulta` |
| Cirugía | programar → iniciar → cerrar | `RegistrarConsulta` |
| Estética | registrar servicio → cargo opcional | `RegistrarEstetica` |
| Inventario | ajuste de entrada/salida con rastro | `GestionarInventario` |

Reglas verificadas por `tst_clinico`:

- Una cita no se recibe dos veces ni admite dos consultas.
- La consulta exige motivo al abrir y diagnóstico al cerrar; una consulta
  cerrada no se vuelve a cerrar.
- La receta exige medicamento, dosis y frecuencia; descuenta stock solo si se
  pide y falla completa si falta existencias. El precio se congela en la línea.
- El cargo es idempotente por `(origen_tipo, origen_uuid)`: repetirlo actualiza
  concepto y monto en vez de duplicarlo.
- La factura solo toma cargos `Pendiente`, escribe detalle y marca los cargos
  como `Pagado` o `Facturado` según haya método de pago.
- El cierre de caja calcula el esperado con las facturas pagadas del día y
  guarda el desfase.
- El laboratorio avanza `Solicitado → En proceso → Listo → Entregado` sin
  saltos.
- La hospitalización solo evoluciona y recibe alta mientras está `Activa`.
- La cirugía solo inicia desde `Programada` y cierra desde `En curso`.
- El ajuste de inventario no deja stock negativo y siempre escribe un
  movimiento.

## 3. Esquema

`0006_clinico.sql` añade:

| Tabla | Propósito |
|---|---|
| `tipos_examen_laboratorio` | Catálogo de exámenes con seis filas sembradas |
| `laboratorio_solicitudes` | Solicitud, prioridad, resultado y estado |
| `hospitalizaciones` | Ingreso, área, diagnóstico y alta |
| `hospitalizacion_evoluciones` | Notas y signos por ingreso |
| `cirugias` | Programación, anestesia y cierre |
| `estetica_servicios` | Servicio realizado, precio y estilista |
| `movimientos_inventario` | Rastro de entradas, salidas y ajustes |

Todas siguen las convenciones del nodo: `id` local + `uuid` global, timestamps
en milisegundos UTC, borrado lógico y `sync_state` para el outbox. Los cargos
nuevos usan `origen_uuid`; el índice único pasa a
`(origen_tipo, coalesce(origen_uuid, cast(origen_id AS TEXT)))`.

## 4. Autorización

La matriz completa está en [`matriz-autorizacion.md`](./matriz-autorizacion.md).
En concreto:

- Recepción recibe, hace triaje, factura y cierra caja; no abre consultas ni
  emite recetas.
- Veterinario registra consulta, receta, laboratorio, hospitalización y
  cirugía; no factura ni cierra caja.
- Estilista registra estética; no abre consultas ni factura.
- Admin puede todo lo anterior y además configura catálogos.
- Una sesión con `clinica_id` distinto al de la base recibe denegación aunque
  el rol sea Admin. `Politicas::exigir` compara el contexto con
  `ServicioClinico::clinicaUuid()`.

## 5. Portal

El proyector publica `laboratorio_solicitudes` en la tabla `laboratorio` del
portal del dueño: tipo de examen, estado, resultado, fechas y nombre del
veterinario. Nunca publica precio, nota interna, hospitalización, cirugía ni
estética; esa información se queda en el nodo.

## 6. Pruebas

```bash
ctest --test-dir build -R "tst_clinico|tst_politicas|tst_migration_runner"
ctest --test-dir build -R "tst_controladores|tst_visual"
qmllint -I "$QT_PREFIX/qml" fuzzy_desktop/qml/*.qml
```

- `fuzzy_core/tests/tst_clinico.cpp`: seis flujos y la matriz rol/clínica.
- `fuzzy_desktop/tests/tst_controladores.cpp`: puente QML del flujo clínico.
- `fuzzy_desktop/tests/visual/`: baselines `clinico` y `clinico-laboratorio`.
- `tst_migration_runner`: exige la versión `0006` y las siete tablas nuevas.

## 7. Smoke con móvil real (manual)

El entorno de CI no tiene dispositivo. Antes de cerrar el gate en staging:

1. Emparejar un portal desde Vínculos y abrir la app móvil con ese token.
2. En el escritorio, solicitar un laboratorio para una mascota del dueño.
3. Iniciar y registrar el resultado; comprobar que la app muestra el examen y
   el resultado tras sincronizar.
4. Entregar el examen y verificar que el estado cambia en el teléfono.
5. Repetir en modo avión: la app conserva la última proyección y no pierde la
   caché cifrada.
6. Confirmar que hospitalización, cirugía y estética **no** aparecen en el
   portal.

## 8. Límites

- El smoke con dispositivo real queda como checklist manual; no se ejecutó en
  este entorno.
- La impresión de tickets y la integración con pasarelas de pago siguen fuera
  del alcance de la fase 9.
- El cierre de caja es por día natural del nodo; no hay arqueo por turno.
