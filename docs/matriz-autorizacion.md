# Matriz de autorización

Derivada de `FuzzyApp@f3afa94`: el enum `Accion` de `fuzzy_core/services/AuthService.h` y las 60 políticas RLS de `supabase/migrations`. Es el contrato que `Politicas::exigir` debe reproducir.

**Estado:** congelado en Fase 3 con el test de snapshot `tst_politicas` (`fuzzy_core`). Cualquier cambio exige editar este documento y el test en el mismo commit.

## 1. Planos de identidad

| Plano | Quién | Credencial | Alcance |
|---|---|---|---|
| Personal | Admin, Veterinario, Recepcion, Estilista | Sesión del nodo | Su clínica |
| Cliente | Dueño de mascota | Token de su portal | Solo sus datos |
| Operador | Técnico con acceso al equipo | CLI `fuzzy-admin` | Local, auditado |

El rol y `clinica_id` salen **siempre** de la sesión, nunca del cuerpo de la petición.

No existe una identidad global (D25). Una persona con cuenta en dos clínicas tiene **dos cuentas**, una por clínica, creadas por el Admin de cada una; el correo es único dentro de la clínica. Un cliente con mascotas en dos clínicas tiene **dos portales**, uno por clínica, y la app los consulta por separado (D26).

## 2. Acciones del escritorio

Tomadas de `permisosDe(Rol)` en `f3afa94`.

| Acción | Admin | Veterinario | Recepcion | Estilista |
|---|:---:|:---:|:---:|:---:|
| VerPanel | ✓ | ✓ | ✓ | ✓ |
| GestionarCitas | ✓ | ✓ | ✓ | ✓ |
| RegistrarConsulta | ✓ | ✓ | – | – |
| RegistrarCliente | ✓ | – | ✓ | – |
| EmitirFactura | ✓ | – | ✓ | – |
| CerrarCaja | ✓ | – | ✓ | – |
| GestionarInventario | ✓ | ✓ | ✓ | – |
| SuministrarMedicamento | ✓ | ✓ | – | – |
| GestionarUsuarios | ✓ | – | – | – |
| ConfigurarClinica | ✓ | – | – | – |

Acciones nuevas de este plan:

| Acción | Admin | Veterinario | Recepcion | Estilista | Fuente |
|---|:---:|:---:|:---:|:---:|---|
| AprobarVinculo | ✓ | – | ✓ | – | D9 |
| VerAuditoria | ✓ | – | – | – | D8, D9 |

## 3. Recursos

`C` crear · `R` leer · `U` actualizar · `D` borrar. `(propias)` = limitado por `cuenta_id`.

| Tabla | Admin | Veterinario | Recepcion | Estilista | Cliente |
|---|:---:|:---:|:---:|:---:|:---:|
| `clinicas` | CRUD | R | R | R | – |
| `usuarios` | CRUD + R propias | – | – | – | – |
| `roles` | CRUD | R | R | R | – |
| `clientes` | CRUD | CRUD | CRUD | CRUD | R + U (propia) |
| `mascotas` | CRUD | CRUD | CRUD | CRUD | R + C + U (propias) |
| `mascota_galeria` | R + C + D | R + C + D | R + C + D | R + C + D | – |
| `citas` | CRUD | CRUD | CRUD | CRUD | R (propias) |
| `estados_cita` | CRUD | R | R | R | – |
| `tipos_servicio_cita` | CRUD | R | R | R | – |
| `historial_cambios_cita` | R + C | R + C | R + C | R + C | – |
| `consultas_medicas` | CRUD | CRUD | R | R | R (propias) |
| `signos_vitales` | CRUD | CRUD | R | R | – |
| `recetas` | CRUD | CRUD | R | – | R (propias) |
| `receta_detalles` | CRUD | CRUD | R | – | R (propias) |
| `vacunas` | CRUD | CRUD | R | R | R (propias) |
| `servicios` | CRUD | R | R | R | – |
| `cargos` | CRUD | R + C | CRUD | – | – |
| `facturas` | CRUD | R | CRUD | R | – |
| `detalles_factura` | CRUD | R | CRUD | R | – |
| `cierres_caja` | CRUD | – | CRUD | – | – |
| `categorias_inventario` | CRUD | R | R | R | – |
| `productos_insumos` | CRUD | R | CRUD | R | – |
| `storage.objects` (galería) | R + C + D | R + C + D | R + C + D | R + C + D | – |

## 4. Reglas transversales

1. **Aislamiento por clínica.** Toda consulta filtra por `clinica_id` de la sesión. Un Admin de la clínica A no ve la clínica B.
2. **Último Admin.** No se puede desactivar ni degradar al último Admin activo de una clínica.
3. **Propio rol.** Nadie cambia su propio rol ni se desactiva a sí mismo.
4. **Ficha ajena.** Un cliente solo accede a la ficha vinculada a su cuenta; el vínculo lo aprueba el personal.
5. **Borrado lógico.** En el modelo nuevo `D` significa `deleted_at`, no `DELETE`. La excepción es el portal, que se borra por completo al revocar.
6. **Auditoría.** Alta de usuario, cambio de rol, reset de contraseña, aprobación de vínculo, fusión, desvinculación y bootstrap escriben en `auditoria` dentro de la misma transacción.
7. **Sin identidad global.** Un usuario pertenece a una sola clínica. La sesión emitida por la clínica A no vale en la B, aunque el correo coincida. No hay tabla de accesos cruzados ni base de control.
8. **Un portal, una clínica.** Cada portal pertenece a una sola clínica y a una sola cuenta. El selector del móvil elige qué portal consultar; ninguna consulta cruza portales ni reutiliza el token de otro. El `clinica_uuid` de `portal_meta` es la única sede válida para esa base.

## 5. Discrepancias a resolver en la fase 3

Detectadas al comparar el enum con las políticas; ninguna se resuelve por accidente.

| # | Discrepancia | Propuesta |
|---|---|---|
| 1 | Veterinario tiene `GestionarInventario`, pero en RLS solo lee `productos_insumos` | El inventario lo escriben Admin y Recepcion; el Veterinario descuenta por receta, no edita el catálogo. Retirar la acción o acotarla a `SuministrarMedicamento`. |
| 2 | No existe acción de estética y RLS da al Estilista CRUD en `citas` y galería | Añadir `RegistrarEstetica` si el módulo entra en la fase 9; mientras tanto, el Estilista agenda y galería. |
| 3 | `cargos` permite insertar al Veterinario pero el CRUD es de Admin/Recepcion | Unificar: el Veterinario no crea cargos; los genera la consulta. |
| 4 | RLS deja al Estilista leer `consultas_medicas` completa, pero el plan dice "sin notas clínicas" | Vista `consultas_resumen` sin notas para Recepcion y Estilista. SQLite no tiene RLS por columna. |
| 5 | `clientes` es CRUD para todo el personal, incluido `delete` | El modelo nuevo usa `deleted_at`; el borrado físico queda solo en la CLI de operador. |

## 6. Cómo se hace ejecutable

```cpp
enum class Permiso {
    VerPanel, GestionarCitas, RegistrarConsulta, RegistrarCliente,
    EmitirFactura, CerrarCaja, GestionarInventario, SuministrarMedicamento,
    GestionarUsuarios, ConfigurarClinica, AprobarVinculo, VerAuditoria,
    RegistrarEstetica
};

Politicas::exigir(sesion, Permiso::AprobarVinculo, contexto);
```

- Una sola función en `fuzzy_core`, usada por los servicios locales y por el módulo servidor.
- Test de snapshot: recorre las tablas de esta matriz (rol × permiso × ámbito) y compara con la implementación. Cualquier cambio exige editar este documento y el test en el mismo commit.
- Las vistas restringidas (`consultas_resumen`) se prueban aparte: la suite verifica que la vista no expone las columnas excluidas.
