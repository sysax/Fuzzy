# Enmienda 2 — Multi-clínica sin base de control

**Estado:** adoptada el 2026-10-23.
**Modifica:** [`PLAN.md`](./PLAN.md) y [`matriz-autorizacion.md`](./matriz-autorizacion.md).
**Origen:** revisión de `casos_uso_multitenant_turso.md` y del volcado `data (1).db` descargado de Turso.

---

## 1. Motivo

El documento `casos_uso_multitenant_turso.md` propone una base central (`doctorcloud-control`) que enruta a cada clínica y entrega a los clientes el token de lectura/escritura de la base de la clínica. Resuelve dos casos de uso reales que el plan no describía:

1. Un veterinario que trabaja en dos clínicas.
2. Un dueño con mascotas en dos clínicas.

Los casos son válidos. La arquitectura propuesta no lo es: contradice D3, D18, D19 y D23, y en la práctica entrega la historia clínica completa de una clínica a cualquier cliente con cuenta. Esta enmienda conserva los casos y rechaza el mecanismo.

---

## 2. Revisión de la propuesta

| # | Lo que propone | Por qué no entra |
|---|---|---|
| H1 | Token de **solo lectura incrustado en el binario** sobre la base de control | Ese token lee `usuarios` (hashes) y `clinicas` (`token_acceso` de todas las clínicas). Es un secreto público que entrega todos los secretos. |
| H2 | `clinicas.token_acceso` con el token completo de cada clínica | El token de la clínica queda en una tabla que cualquier app puede consultar. Viola la regla de la Enmienda 1 §3: el token de la clínica nunca sale del nodo. |
| H3 | `password_hash` con SHA-256 calculado en el cliente | Viola D19 (PBKDF2-HMAC-SHA256, 600 000 iteraciones, salt por usuario). SHA-256 sin salt es reversible por diccionario. |
| H4 | Login con `WHERE email = ? AND password_hash = ?` | El hash se convierte en la contraseña: quien lo lea entra sin conocerla (*pass-the-hash*). Además no hay comparación en tiempo constante. |
| H5 | `accesos_clinicas.rol TEXT` con `'veterinario'` o `'cliente'` | Mezcla los dos planos de identidad y sustituye los cuatro roles fijos por texto libre (D3). |
| H6 | Intercambio en caliente del token de la clínica en el cliente | El cliente pasa a custodiar el token de la clínica. Basta un volcado de memoria o un error de enrutamiento para leer otra sede. |
| H7 | El cliente descarga el token de la clínica y consulta `pacientes` | El token es de la base completa: Laura leería las mascotas de todos los dueños. La afirmación "no existe riesgo de que consulte bases ajenas" es falsa; el riesgo está dentro de su propia base. |
| H8 | Tabla `pacientes` con `dueño_usuario_id` | No existe en el esquema de origen (`mascotas`, `id_cliente`) ni en el modelo nuevo. Es un boceto, no una especificación. |

Lo aprovechable: los dos casos de uso y la idea de un selector. El selector se implementa sin intercambio de tokens y sin base de control.

---

## 3. Modelo adoptado

### 3.1 Personal en dos clínicas

- Cada clínica es un tenant completo: su cuenta Turso, su grupo y su base `fuzzy-clinic-<uuid>` (D18).
- El personal **no tiene identidad global**. El Admin de cada clínica crea la cuenta en su instalación; la misma persona tiene **una cuenta por clínica**, con la contraseña que fije cada clínica.
- `usuarios.correo` es único **dentro de la clínica**, no entre clínicas. La misma dirección puede existir en dos sedes.
- Un `usuario` pertenece a **una** clínica. No hay tabla de accesos cruzados ni rol por sede.
- El escritorio es el nodo de **una** clínica por instalación. No tiene selector de clínica ni cambia de credenciales en caliente: para trabajar en la otra sede se usa el equipo de esa sede, con su cuenta.
- El veterinario que quiera consultar desde su casa no tiene un canal de personal: el móvil es del dueño. Fuera del alcance del MVP.

Esto no pierde el caso de uso: el Dr. Carlos trabaja en las dos clínicas con dos cuentas y dos instalaciones, y **nunca** ve historiales cruzados. Lo que se pierde es la comodidad de un único inicio de sesión, y se gana que un token comprometido no abra la otra sede.

### 3.2 Dueño con mascotas en dos clínicas

- Cada clínica crea **su** portal para ese dueño. Un cliente con mascotas en dos clínicas tiene **dos portales**, dos tokens y dos emparejamientos.
- La app guarda una **lista local de portales** (`portal_uuid`, `clinica`, `url`, token cifrado). Cada token se envuelve con la contraseña usada en ese emparejamiento (D23); no se copia ni se canjea.
- El selector de clínica de la app solo elige **qué portal consultar**. No hay consulta que cruce portales ni token que sirva para dos bases.
- La vista "mis mascotas" se arma en el teléfono: se consulta cada portal por separado y se etiqueta cada fila con su clínica. No se fusionan historiales clínicos; cada clínica conserva su registro legal.
- Revocar un portal (rotar token o borrar la base) no afecta al otro. La app muestra el portal revocado como "requiere re-emparejamiento" y sigue operando con los demás.
- El límite práctico es de producto: N portales pequeños; se documenta que la app no es un agregador nacional.

### 3.3 Lo único global

`fuzzy-directory` sigue siendo el único dato compartido: nombre, ciudad, dirección, teléfono, `inbox_url` e `inbox_token` de cada clínica. No contiene identidades, ni correos de clientes, ni tokens de clínica, ni relaciones persona–clínica. El directorio permite **elegir** una clínica; no autentica en ella.

---

## 4. ¿Pueden convivir varias clínicas en una sola base?

Técnicamente SQLite no tiene concepto de tenant: una base puede guardar filas de varias clínicas si **todas** las tablas llevan la clave de clínica y **todas** las consultas la filtran. En este producto no se adopta, por dos razones independientes:

1. **Turso autoriza por base y por tabla, no por fila.** Un token que puede leer `citas` lee las citas de todas las clínicas. La separación quedaría a cargo del cliente, que es justo quien no debe custodiarla.
2. **El esquema inspeccionado no está preparado.** En `data (1).db`:

| Tabla | `id_clinica` | Consecuencia |
|---|---|---|
| `clinicas` | sí (PK) | – |
| `citas`, `consultas_medicas`, `facturas`, `signos_vitales`, `vacunas` | sí, `NOT NULL`, FK | Aisladas |
| `clientes` | **no** | Un cliente es global; no se sabe de qué clínica es |
| `mascotas` | **no** | Solo cuelga de `clientes`; una mascota no pertenece a una sede |
| `usuarios` | **no** | Un usuario es global; no hay rol por clínica |
| `roles` | **no** | Catálogo global |

Además:

- `usuarios.correo` es `UNIQUE` global y `clinicas.nombre_clinica` es `UNIQUE` global: impiden la misma persona o el mismo nombre en dos sedes.
- No existe una tabla de accesos persona–clínica; `clinicas.owner_id` da un solo propietario por clínica, no un rol por sede.
- El volcado tiene 2 clínicas y 2 usuarios, y las tablas clínicas están vacías. No hay datos que permitan validar el aislamiento; solo la estructura, y la estructura no lo garantiza.

Conclusión: **una base compartida no es el modelo de producción**. Si en el futuro hace falta un archivo único —por ejemplo, para importar el volcado heredado— se trata como **espacio de trabajo temporal de importación**, no como tenancy: se añade `id_clinica` a `clientes`, `mascotas` y `usuarios`, se valida cada fila y se exporta a las bases por clínica. La base compartida no se publica ni se le entrega un token a un cliente.

---

## 5. Decisiones nuevas

| ID | Decisión |
|---|---|
| D25 | **Sin identidad global.** Cada clínica emite sus cuentas de personal; una persona en dos clínicas tiene una cuenta por clínica. El correo es único por clínica, no entre clínicas. No hay base de control ni tabla de accesos cruzados. |
| D26 | **El escritorio pertenece a una clínica por instalación.** No hay selector de sede ni intercambio de credenciales en el nodo. La app del dueño sí mantiene una lista local de portales, uno por clínica, y un selector que solo elige qué portal consultar. |
| D27 | **Se rechaza la base de control con token público de lectura** y cualquier variante que ponga el token de una clínica en una tabla, en el binario o en la memoria del cliente. Motivo: el token de la clínica nunca sale del nodo (Enmienda 1 §3). |
| D28 | **Se rechaza la co-tenencia de varias clínicas en una base.** Turso no autoriza por fila y el esquema heredado no tiene clave de clínica en `clientes`, `mascotas` ni `usuarios`. Un archivo único solo se admite como espacio temporal de importación, validado y exportado. |

---

## 6. Cambios aplicados a `PLAN.md`

| Sección | Cambio |
|---|---|
| Cabecera | Se añade esta enmienda como modificatoria del plan. |
| §0 | Se excluye la identidad global entre clínicas y la co-tenencia en una base. |
| §1 | Decisiones D25–D28. |
| §2.1 | Nota: cada clínica es un tenant con sus cuentas; no hay base de control. |
| §3.6 | Nueva sección de multi-clínica (personal y cliente). |
| §4.7 | Lista local de portales y selector en la app. |
| §6 | Fases 2, 4 y 8 incorporan cuentas por clínica, portales múltiples y selector. |
| §7 | Pruebas de aislamiento entre portales de la misma persona. |
| §8 | Riesgo de confusión de contexto y de reutilización de contraseña entre sedes. |
| Anexo A | Hechos del volcado `data (1).db`. |

---

## 7. Gates nuevos

1. Un usuario con cuenta en dos clínicas no puede autenticarse en la sede equivocada: la sesión del nodo A rechaza credenciales emitidas por B.
2. Un cliente con dos portales no puede leer el portal del otro desde el primero: no hay URL ni token compartidos.
3. La app no envía a un portal ninguna consulta con `clinica_uuid` distinto al de `portal_meta`.
4. El directorio no contiene correos de clientes, tokens de clínica ni relaciones persona–clínica.
5. El binario del móvil no contiene ningún token de clínica (ya cubierto en Fase 8; se mantiene).

---

## 8. Lo que no cambia

- Cuatro roles fijos y cliente como plano aparte.
- Una cuenta Turso y un grupo Turso por clínica (D18).
- Token del portal limitado a su base, cifrado con la contraseña (D23).
- Portal como proyección reducida (D24).
- Sin backend, sin P2P, sin correo, sin endpoint público del nodo.
- Bootstrap por instalación y recuperación del último Admin por CLI.
