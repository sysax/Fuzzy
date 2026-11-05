# Manual de usuario

FuzzyCloud tiene dos aplicaciones que comparten los mismos datos:

| Aplicación | Para quién | Qué permite |
|---|---|---|
| **Escritorio** (`fuzzy_desktop`) | Personal de la clínica | Agenda, pacientes, historia clínica, caja, inventario, facturación, equipo y vínculos con el portal del dueño |
| **Móvil** (`fuzzy_mobile`) | Dueño de la mascota | Perfil, mascotas, citas, historia visible, portales de sus clínicas y solicitudes de primera visita |

Este manual describe el uso diario del producto tal como está implementado en
la versión actual. No presenta como disponibles las funciones que todavía no
lo están; al final hay una sección de [limitaciones conocidas](#19-limitaciones-conocidas).

> **Convención:** los nombres en **negrita** son textos que aparecen en la
> pantalla. `→` indica una ruta de navegación. Las fechas se escriben
> `AAAA-MM-DD` y las horas `HH:MM`.

## Contenido

1. [Antes de empezar](#1-antes-de-empezar)
2. [Escritorio: primer arranque y acceso](#2-escritorio-primer-arranque-y-acceso)
3. [Escritorio: navegación y permisos](#3-escritorio-navegación-y-permisos)
4. [Panel](#4-panel)
5. [Agenda](#5-agenda)
6. [Pacientes e historia clínica](#6-pacientes-e-historia-clínica)
7. [Propietarios](#7-propietarios)
8. [Flujo clínico](#8-flujo-clínico)
9. [Inventario](#9-inventario)
10. [Facturación y cierre de caja](#10-facturación-y-cierre-de-caja)
11. [Vínculos y portal del dueño](#11-vínculos-y-portal-del-dueño)
12. [Ajustes de la clínica y del equipo](#12-ajustes-de-la-clínica-y-del-equipo)
13. [Móvil: instalación y clave](#13-móvil-instalación-y-clave)
14. [Móvil: inicio, perfil y mascotas](#14-móvil-inicio-perfil-y-mascotas)
15. [Móvil: citas](#15-móvil-citas)
16. [Móvil: historia visible](#16-móvil-historia-visible)
17. [Móvil: portales y emparejamiento](#17-móvil-portales-y-emparejamiento)
18. [Móvil: directorio y primera visita](#18-móvil-directorio-y-primera-visita)
19. [Limitaciones conocidas](#19-limitaciones-conocidas)
20. [Problemas frecuentes](#20-problemas-frecuentes)
21. [Glosario](#21-glosario)

---

## 1. Antes de empezar

### 1.1 Roles del personal

El acceso al escritorio se controla por rol. El rol y la clínica se toman de la
sesión, nunca de lo que se escriba en un formulario.

| Rol | Para qué sirve |
|---|---|
| **Admin** | Todo: configuración, equipo, agenda, clínica, inventario, facturación, vínculos y catálogos |
| **Veterinario** | Agenda, pacientes, historia clínica, consultas, recetas y laboratorio |
| **Recepcion** | Agenda, propietarios, recepción, cobros, cierre de caja, inventario y vínculos |
| **Estilista** | Agenda, estética e inventario |

Un mismo módulo puede mostrarse a varios roles, pero las acciones concretas se
habilitan según el permiso. Si una entrada del menú aparece atenuada, el rol
no tiene permiso para ese módulo.

### 1.2 Requisitos

- **Escritorio:** un equipo con la aplicación instalada y acceso al directorio
  de datos del usuario. No necesita conexión a Internet para funcionar.
- **Móvil:** un teléfono con la aplicación instalada. La caché se cifra con una
  clave que solo conoce el dueño.
- **Datos:** cada clínica es independiente. Un mismo correo puede existir en
  dos clínicas, pero son dos cuentas distintas.

---

## 2. Escritorio: primer arranque y acceso

### 2.1 Crear la clínica (solo la primera vez)

La primera vez que se abre el escritorio en un equipo sin instalación, la
pantalla muestra **Alta de la clínica**.

1. Escribe el **Nombre de la clínica**, la **Ciudad**, el **Administrador**
   (nombre completo), el **Correo** y una **Contraseña** de al menos 8
   caracteres.
2. Repite la contraseña en **Repetir contraseña**.
3. Pulsa **Crear clínica y entrar**.

El alta crea la clínica y la primera cuenta Admin, y deja la sesión iniciada.
Solo se ofrece mientras el equipo no tenga una clínica: si ya existe, el
botón **Crear la clínica en este equipo** no vuelve a aparecer.

> Si el equipo ya tuvo una instalación, no se debe borrar la base para
> “volver a empezar”. Para recuperar al último Admin se usa la herramienta de
> operador (ver §2.4).

### 2.2 Iniciar sesión

1. Escribe el **Correo** y la **Contraseña** que te entregó el Admin.
2. Pulsa **Entrar**.

Si el correo o la contraseña no coinciden, la pantalla muestra un aviso rojo.
El correo no distingue mayúsculas de minúsculas.

### 2.3 Cambiar una contraseña temporal

Cuando el Admin crea una cuenta o restablece una contraseña, entrega una
**contraseña temporal de 12 caracteres válida 72 horas**. Al entrar con ella,
la aplicación muestra **Cambia tu contraseña** y no deja continuar sin
cambiarla.

1. Escribe la **Contraseña temporal**.
2. Escribe la **Nueva contraseña** (mínimo 8 caracteres) y repítela.
3. Pulsa **Guardar contraseña**.

Si la contraseña temporal vence, pide al Admin que la restablezca desde
**Ajustes → Equipo → Clave**.

### 2.4 Recuperar al último Admin

No hay recuperación por correo. Si el último Admin no puede entrar, un
operador con acceso físico al equipo ejecuta:

```bash
fuzzy-admin recuperar-admin --db "<ruta>/clinica.db" --correo admin@clinica.co
```

El comando reactiva la cuenta y devuelve una contraseña temporal nueva, válida
72 horas y con cambio obligatorio. Se muestra una sola vez. En Linux, la base
suele estar en `~/.local/share/FuzzyCloud/FuzzyCloud/clinica.db`.

### 2.5 Cerrar sesión

En la parte inferior de la barra lateral, junto a tu nombre y rol, pulsa el
icono de salida (una flecha hacia la derecha). La sesión se cierra, pero los
datos de la clínica permanecen en el equipo.

---

## 3. Escritorio: navegación y permisos

La barra lateral contiene las entradas del producto. Cada una se muestra
atenuada si el rol no tiene el permiso correspondiente.

| Entrada | Permiso | Admin | Veterinario | Recepcion | Estilista |
|---|---|:---:|:---:|:---:|:---:|
| **Panel** | `VerPanel` | ✓ | ✓ | ✓ | ✓ |
| **Agenda** | `GestionarCitas` | ✓ | ✓ | ✓ | ✓ |
| **Pacientes** | `RegistrarConsulta` | ✓ | ✓ | — | — |
| **Historial clínico** | `RegistrarConsulta` | ✓ | ✓ | — | — |
| **Propietarios** | `RegistrarCliente` | ✓ | — | ✓ | — |
| **Clínico** | `GestionarCitas` | ✓ | ✓ | ✓ | ✓ |
| **Inventario** | `GestionarInventario` | ✓ | ✓ | ✓ | ✓ |
| **Facturación** | `EmitirFactura` | ✓ | — | ✓ | — |
| **Vínculos** | `AprobarVinculo` | ✓ | — | ✓ | — |
| **Ajustes** | `GestionarUsuarios` | ✓ | — | — | — |

La barra superior muestra el nombre de la clínica, el usuario, su rol y el
aviso **Nodo local**. Los mensajes de éxito aparecen en verde y los errores en
rojo; se pueden cerrar con la **×**.

---

## 4. Panel

**Panel** resume el día en cuatro indicadores y cuatro bloques:

- **Citas de hoy**, **En espera**, **En curso** e **Ingresos de hoy**
  (facturas pagadas).
- **Agenda de hoy**, con hora, mascota, tipo y propietario.
- **Sala de espera**, con las citas recibidas que aún no han entrado a consulta.
- **Citas de la semana**, con el total por día.
- **Alertas de inventario**, con los productos en o por debajo del mínimo.

Los botones **Ver agenda** y **Nueva cita** llevan a la agenda del día.

> Los indicadores describen el nodo local de esta clínica. No hay una vista
> consolidada de varias sedes.

---

## 5. Agenda

### 5.1 Cambiar de vista

- **Día** y **Semana** alternan el modo.
- **← Anterior** y **Siguiente →** mueven el periodo.
- **Hoy** vuelve a la fecha actual.
- La cabecera indica el periodo y cuántas citas contiene.

### 5.2 Agendar una cita

1. Pulsa **Agendar cita**.
2. Elige la **Mascota** y el **Tipo** (Médica, Peluquería / Estética o
   Domicilio).
3. Escribe la **Fecha** (`AAAA-MM-DD`) y la **Hora** (`HH:MM`).
4. Añade **Notas** si hace falta (motivo o indicaciones).
5. Marca **Marcar como emergencia** cuando corresponda.
6. Pulsa **Agendar**.

La cita nace en estado **Agendada**.

### 5.3 Ver y modificar una cita

Pulsa una cita de la lista o del calendario. El detalle muestra tipo, estado,
profesional y propietario, y permite:

- **Mover la cita:** cambia **Nueva fecha** y **Nueva hora**, y pulsa
  **Mover**. La cita pasa a **Reprogramada**.
- **Cancelar cita:** la deja en **Cancelada**.
- **En espera:** botón presente en el detalle. En esta versión no cambia el
  estado; para pasar una cita a **En espera** usa **Clínico → Recepción →
  Recibir** (ver §8.1).

Estados disponibles: **Agendada**, **Cancelada**, **Reprogramada**,
**Completada**, **En espera** y **En curso**.

### 5.4 Errores habituales

- **“elige una mascota”**: no se eligió un valor en la lista.
- **“la fecha y la hora de la cita no son válidas”**: revisa el formato
  `AAAA-MM-DD` y `HH:MM`.
- **“la mascota ya tiene una cita en esa franja”**: cambia la hora o el día.
- Una cita cancelada no se borra: queda en el historial con su estado.

---

## 6. Pacientes e historia clínica

### 6.1 Buscar y filtrar

**Pacientes** muestra la lista paginada. Usa el buscador por nombre de la
mascota, nombre del propietario o número de chip, y el filtro por especie. La
cabecera indica cuántos resultados hay.

### 6.2 Registrar una mascota

1. Pulsa **Nuevo paciente**.
2. Elige el propietario.
3. Completa **Nombre**, **Especie**, **Raza**, **Peso (kg)**, **Nacimiento**
   (`AAAA-MM-DD`), **Número de chip**, **Alergias** y, si corresponde,
   **Castrado**.
4. Pulsa **Registrar**.

El propietario debe existir antes; si no está, créalo en **Propietarios**
(§7).

### 6.3 Ficha del paciente

Al abrir una mascota se muestra su perfil con:

- Nombre, especie, raza, peso, nacimiento, chip, alergias y atención especial.
- Estado **Activo** o **Archivado**.
- Contacto del propietario.
- Historia de consultas, signos vitales y vacunas.

Acciones de la ficha:

| Botón | Qué hace |
|---|---|
| **Signos** | Registra peso, temperatura, frecuencia cardiaca y respiratoria, y notas |
| **Vacuna** | Registra nombre, aplicación y próxima dosis |
| **Editar** | Cambia los datos de la mascota, sus alergias o su estado |
| **Nueva consulta** | Abre una consulta con motivo, diagnóstico, tratamiento y examen; permite marcarla como emergencia |

La ficha no permite borrar una mascota: se **archiva** para conservar su
historia.

### 6.4 Historial clínico transversal

**Historial clínico** reúne las consultas de todas las mascotas. Se puede
buscar por mascota, propietario, motivo o diagnóstico. **Ver ficha** abre el
perfil correspondiente.

---

## 7. Propietarios

**Propietarios** es el directorio de dueños.

1. Pulsa **Nuevo propietario**.
2. Completa **Nombre completo**, **Teléfono**, **Correo**, **Dirección** y
   **DNI** (opcional).
3. Pulsa **Registrar**.

La lista muestra propietario, teléfono y número de mascotas. Al seleccionar
una fila se puede **Editar** o consultar sus mascotas.

> El botón **Exportar** todavía no genera un archivo; en esta versión solo
> refresca la lista. La exportación está pendiente.

---

## 8. Flujo clínico

**Clínico** agrupa ocho pestañas: **Recepción**, **Consultas**,
**Laboratorio**, **Hospitalización**, **Cirugía**, **Estética**, **Caja** y
**Catálogos**. La cabecera muestra citas de hoy, consultas abiertas y exámenes
pendientes.

### 8.1 Recepción y triaje

1. En **Recepción**, localiza la cita del día.
2. Pulsa **Recibir** cuando el dueño llegue. La cita pasa a **En espera**.
3. Pulsa **Triaje** para registrar peso, temperatura, frecuencia cardiaca,
   frecuencia respiratoria y notas.
4. Pulsa **Consulta** para abrir la consulta de esa mascota.

### 8.2 Consulta

En **Consultas** aparecen las consultas abiertas con mascota, propietario,
motivo y fecha. Desde cada fila:

- **Cargo:** añade un concepto y un monto a la cuenta del cliente.
- **Receta:** emite una receta con una o varias líneas (medicamento, dosis,
  frecuencia, duración, cantidad y precio). Si se marca descontar stock, el
  inventario baja al emitir.
- **Cerrar:** guarda diagnóstico, tratamiento y examen, y cierra la consulta.

El precio de una receta se congela al emitirla: cambiar el catálogo después no
reescribe recetas ya dispensadas.

### 8.3 Laboratorio

1. Pulsa **Solicitar laboratorio**.
2. Elige mascota, consulta asociada (opcional), examen, prioridad y notas.
3. Pulsa **Solicitar**.

La lista muestra el examen, la prioridad, el resultado y su estado. Las
acciones son **Iniciar**, **Resultado** (escribe el texto del resultado) y
**Entregar**.

### 8.4 Hospitalización

La pestaña **Hospitalización** lista los pacientes ingresados con motivo, área,
ingreso y estado. En esta versión no hay un botón para ingresar un paciente
desde la interfaz: el alta de hospitalización solo está disponible en el
servicio de dominio. Si ya existe una hospitalización **Activa**, puedes
registrar su evolución o darla de alta:

- **Evolución:** añade una nota con peso, temperatura y frecuencia cardiaca.
- **Alta:** cierra la hospitalización con el estado indicado.

### 8.5 Cirugía

1. Pulsa **Programar cirugía**.
2. Indica mascota, consulta y hospitalización (opcionales), procedimiento,
   fecha y anestesia.
3. Pulsa **Programar**.

La lista muestra procedimiento, anestesia, fecha y estado. **Iniciar** marca
la cirugía en curso; **Cerrar** abre el cierre y **Completar** guarda el
diagnóstico postoperatorio y las notas.

### 8.6 Estética

1. Pulsa **Registrar servicio**.
2. Elige mascota, cita (opcional), servicio, tipo, precio y observaciones.
3. Marca **Crear cargo pendiente** si el servicio se cobra después.
4. Pulsa **Registrar**.

El servicio queda en la lista con mascota, tipo, observaciones, precio y
fecha. Si se marca el cargo, aparece en **Caja → Cargos pendientes**.

### 8.7 Caja

La pestaña **Caja** lista los **Cargos pendientes** con concepto, mascota,
propietario, monto y origen. **Facturar** abre la facturación con esos cargos.
**Cerrar caja** registra el cierre del día (ver §10.3).

### 8.8 Catálogos

**Catálogos** mantiene los **Servicios** (código, nombre, precio y estado) y
los **Exámenes de laboratorio** (código, nombre y estado). Solo el Admin
puede modificarlos. Los precios que se cambian aquí afectan a las facturas
nuevas, no a las ya emitidas.

---

## 9. Inventario

**Inventario** muestra productos con categoría, stock, precio de venta y
caducidad. La cabecera y las pestañas permiten filtrar por estado.

### 9.1 Crear un producto

1. Pulsa **Nuevo producto**.
2. Completa nombre comercial, principio activo, código, categoría, unidad,
   stock inicial, stock mínimo, precio de compra, precio de venta y caducidad.
3. Pulsa **Crear**.

### 9.2 Ajustar stock

Selecciona un producto y usa **−10** o **+10**. El ajuste queda registrado
como movimiento con el motivo **Ajuste manual** (bajada) o **Recepción**
(subida). El stock también baja al emitir una receta con descuento o una
factura con productos.

### 9.3 Pedidos y edición

- **Marcar pedido en curso** / **Marcar pedido recibido** cambia el aviso de
  reposición del producto.
- **Editar producto** permite corregir precios, mínimo, caducidad, proveedor
  y stock.

Un producto por debajo del mínimo aparece en **Panel → Alertas de
inventario**.

---

## 10. Facturación y cierre de caja

### 10.1 Emitir una factura

1. Pulsa **Nueva factura**.
2. Elige el cliente o deja **Consumidor final**.
3. Elige un servicio o producto y pulsa **Añadir** tantas veces como haga
   falta. Cada línea se puede quitar con **×**.
4. Ajusta **Descuento** e **Impuesto** si corresponde.
5. Elige el método: **Sin pago**, **Efectivo**, **Tarjeta**,
   **Transferencia** o **Credito**.
6. Pulsa **Emitir**.

La factura nace **Emitida** si no se indicó método de pago, o **Pagada** si se
indicó uno. Al emitir se descuenta el stock de los productos.

### 10.2 Cobrar o devolver a pendiente

En el detalle de una factura:

- **Marcar como pagada** la pasa a **Pagada**.
- **Devolver a pendiente** la vuelve a **Pendiente** si no estaba pagada.

Los estados posibles son **Emitida**, **Pendiente** y **Pagada**.

### 10.3 Cerrar la caja

1. Pulsa **Cerrar caja** en **Facturación** o en **Clínico → Caja**.
2. Revisa el total **Calculado** (facturas pagadas del día) y los desgloses
   por **Efectivo**, **Tarjeta** y **Transferencia**.
3. Escribe los **Ingresos reales** contados y una observación si hay
   diferencia.
4. Confirma el cierre.

El cierre guarda los ingresos calculados, los reales, el desfase y las
observaciones. Se calcula sobre el **día natural en curso** del equipo.

---

## 11. Vínculos y portal del dueño

**Vínculos** conecta una ficha de propietario con la app móvil.

### 11.1 Aprobar un vínculo

1. Pulsa **Aprobar vínculo**.
2. Elige la ficha del propietario.
3. Escribe el **Correo** del dueño y una **Contraseña inicial** de al menos 8
   caracteres.
4. Pulsa **Aprobar y emitir**.

La aplicación crea la cuenta del dueño, el portal y muestra una sola vez:

- El **token** del portal.
- La **huella** de 6 caracteres hexadecimales.
- El **QR** de emparejamiento.

Entrega el QR al dueño y confirma en voz alta que la huella que aparece en su
teléfono coincide con la de la pantalla.

> **Importante:** en esta versión el QR apunta a una dirección local del nodo
> (`local://portal/…`). La app móvil solo puede descargar portales publicados
> en Turso por HTTP. El escaneo, la huella y el rechazo de un QR alterado
> funcionan, pero la descarga inicial falla. Ver
> [limitaciones](#19-limitaciones-conocidas).

### 11.2 Revocar o eliminar

- **Revocar** deja el portal en estado revocado: el token deja de abrir la
  base y el teléfono no vuelve a sincronizar. No borra lo ya descargado en el
  teléfono.
- **Eliminar** revoca el portal, lo marca como borrado y elimina su base
  local.

La lista muestra si el dueño ya está **Emparejado** y la fecha del último
emparejamiento.

---

## 12. Ajustes de la clínica y del equipo

Solo el Admin ve **Ajustes**.

### 12.1 Datos de la clínica

Edita **Nombre**, **Ciudad**, **Teléfono** y **Dirección**, y pulsa **Guardar
cambios**. Estos datos aparecen en la cabecera y en los documentos internos.

### 12.2 Equipo

La tabla muestra persona, rol, último acceso y estado.

- **Invitar miembro:** pide nombre, correo y rol. Genera una contraseña
  temporal de 12 caracteres válida 72 horas, que se muestra una sola vez.
- **Clave:** restablece la contraseña de esa persona con otra temporal.
- **Editar:** cambia nombre, rol y si la cuenta está activa.

Reglas que la aplicación hace cumplir:

- El correo es único dentro de la clínica.
- El último Admin activo no se puede desactivar ni degradar.
- No hay auto-registro ni invitaciones por correo: la clave se entrega en
  mano.

---

## 13. Móvil: instalación y clave

### 13.1 Crear la clave

La primera vez, la app muestra **Protege tus datos**.

1. Escribe una **Clave** de al menos 8 caracteres.
2. Repítela en **Repite la clave**.
3. Pulsa **Crear clave**.

La clave no se guarda: se deriva y se usa para cifrar la caché, los tokens de
los portales y el registro local. **Si se pierde, no hay recuperación**; hay
que borrar los datos de la app y volver a emparejar cada portal.

### 13.2 Desbloquear y bloquear

- Al abrir la app de nuevo, escribe la clave y pulsa **Desbloquear**.
- **Ajustes → Seguridad → Bloquear ahora** borra la clave de la memoria.
- **Cerrar sesión y borrar todo** elimina portales, caché, cola, altas,
  registro local y la propia clave. El teléfono queda como recién instalado.

---

## 14. Móvil: inicio, perfil y mascotas

**Inicio** es la pantalla principal.

### 14.1 Completar el perfil

Si no hay perfil, la tarjeta muestra **Completar**. Pulsa y rellena **Nombre
completo**, **Correo**, **Teléfono**, **Dirección** y **Documento**. Guarda con
**Guardar**. El perfil se puede editar después con **Editar**.

### 14.2 Mis mascotas

La lista muestra las mascotas de todos los portales emparejados, con nombre,
especie, raza y clínica. **Ver** abre la ficha con la historia visible.

### 14.3 Registrar una mascota en el teléfono

1. Pulsa **Registrar mi mascota aquí**.
2. Completa **Nombre**, **Especie**, **Raza**, **Sexo** y **Peso en kg**.
3. Pulsa **Guardar**.

La mascota queda en **Pendiente de registrar en la clínica** hasta que se
empareje un portal y la clínica importe los datos. La importación compara el
perfil por correo, DNI o teléfono, y la mascota por nombre y especie.

### 14.4 Primera visita y emparejamiento

Los botones **Primera visita** y **Emparejar portal** llevan a los flujos de
§17 y §18.

---

## 15. Móvil: citas

**Citas** tiene dos bloques: **Mis citas** (confirmadas por la clínica) y
**Solicitudes enviadas** (pendientes de respuesta).

### 15.1 Pedir una cita

1. Pulsa **Pedir cita**.
2. Elige la **Mascota**.
3. Escribe el **Tipo de servicio** (por ejemplo, consulta, vacunación, baño).
4. Indica la **Fecha preferida** (`AAAA-MM-DD`) y la **Hora preferida**
   (`HH:MM`).
5. Añade el **Motivo**.
6. Pulsa **Enviar**.

La solicitud queda en la cola local y se envía al sincronizar. La clínica
decide si la convierte en cita; el resultado aparece en **Solicitudes
enviadas**.

### 15.2 Sin conexión

Si no hay red o el **Modo avión** está activo, la solicitud se guarda como
pendiente y la cabecera muestra **N cambio(s) por enviar**. No se pierde: se
reenvía al sincronizar. Reintentar no duplica la solicitud.

---

## 16. Móvil: historia visible

**Historial** muestra la historia que la clínica ha publicado en el portal:

- Visitas y consultas cerradas.
- Vacunas.
- Pesos.
- Resultados de laboratorio.
- Recetas.

Se puede filtrar por mascota y **Ver todo** quita el filtro. Cada entrada
indica tipo, fecha, título, detalle y mascota. Si no hay registros, aparece
**Nada que mostrar** con una explicación.

> La historia visible no es la historia clínica completa del escritorio:
> solo incluye lo que el nodo proyecta al portal del dueño.

---

## 17. Móvil: portales y emparejamiento

**Portales** lista las clínicas emparejadas. Cada tarjeta muestra el nombre de
la clínica, el estado (**Activo**, **Revocado** o **Inaccesible**), los
cambios pendientes y las acciones **Consultar** y **Olvidar**.

- **Consultar** selecciona ese portal como activo.
- **Olvidar** borra la caché y el token de **ese** portal; los demás no se
  tocan.

### 17.1 Emparejar un portal

1. En el escritorio, el operador abre **Vínculos** y genera el QR.
2. En el teléfono, abre **Portales → Emparejar un portal**.
3. Pega el contenido del QR en el cuadro de texto. La app no usa la cámara:
   el contenido se pega como texto.
4. Pulsa **Leer código**.
5. La app muestra **Confirma con el operador que la huella es XXXX** y el
   nombre de la clínica.
6. Compara la huella con la del escritorio. Si coincide, pulsa **Coincide**;
   si no, **No coincide**.

Al confirmar, la app guarda el portal, descarga sus tablas e importa el
registro local. Si la descarga falla, el alta se deshace para no dejar un
portal a medio emparejar.

> En la versión actual, el QR del escritorio contiene una URL `local://` y la
> descarga contra Turso falla. El flujo de lectura y confirmación de huella sí
> se puede probar; la descarga completa queda pendiente.

### 17.2 Un portal por clínica

Si el dueño tiene mascotas en dos clínicas, tendrá **dos portales** y dos
cuentas. La app los mantiene aislados: la caché, el token y la revocación de
uno no afectan al otro.

---

## 18. Móvil: directorio y primera visita

**Directorio** sirve para pedir una primera cita en una clínica que todavía no
tiene el portal del dueño.

1. Abre **Directorio** desde **Inicio → Primera visita**.
2. Busca por nombre, ciudad o dirección.
3. Pulsa la clínica elegida.
4. Completa **Tu nombre**, **Correo**, **Teléfono**, **Mascota**, **Motivo** y
   **Franja preferida**.
5. Pulsa **Enviar**.

La solicitud se envía al buzón de la clínica. Si no hay red, queda pendiente y
se puede reintentar con **Reintentar solicitudes pendientes**. El estado se ve
en **Solicitudes enviadas**.

> El directorio embebido en esta versión contiene clínicas de staging. En
> producción se publica el directorio real de la sede.

---

## 19. Limitaciones conocidas

Estas son las limitaciones de la versión actual. No son fallos de uso: están
pendientes en el producto.

1. **Emparejamiento escritorio–teléfono incompleto.** El escritorio crea
   portales con URL `local://portal/…`; la app móvil solo habla HTTP con
   Turso. Se puede leer el QR y confirmar la huella, pero la descarga inicial
   no completa.
2. **Sin notificaciones push.** Las citas y los cambios se ven al abrir la app
   y sincronizar.
3. **Sin impresión ni pasarela de pago.** La factura es un registro interno;
   no se imprime ni cobra con tarjeta desde la app.
4. **Sin descarga de adjuntos en el móvil.** La historia visible incluye
   metadatos, no los archivos clínicos.
5. **Sin recuperación de contraseña por correo.** El Admin restablece claves
   del personal; el último Admin se recupera con `fuzzy-admin`.
6. **Sin recuperación de la clave del teléfono.** Si se pierde, hay que borrar
   los datos de la app y volver a emparejar.
7. **Sin identidad global.** Una persona con cuentas en dos clínicas tiene dos
   cuentas y dos portales.
8. **El cierre de caja es por día natural** del equipo, no por turno.
9. **El botón Exportar de Propietarios no exporta** todavía; solo refresca la
   lista.
10. **El botón En espera del detalle de la cita no cambia el estado.** Usa
    **Clínico → Recepción → Recibir**.
11. **No hay pantalla de auditoría** ni de carga de fotos o galería clínica,
    aunque el modelo de datos las contemple.
12. **No hay botón para ingresar una hospitalización** desde la interfaz; solo
    se pueden evolucionar o dar de alta las hospitalizaciones existentes.

---

## 20. Problemas frecuentes

| Síntoma | Qué revisar |
|---|---|
| **“correo o contraseña incorrectos”** | Mayúsculas del correo (no importan), que la cuenta esté activa y que la contraseña temporal no haya vencido |
| **“la contraseña temporal venció”** | El Admin debe restablecerla en **Ajustes → Equipo → Clave** |
| **No veo un módulo en el menú** | El rol no tiene el permiso; consulta la tabla de §3 |
| **No puedo desactivar a un usuario** | Es el último Admin activo; la aplicación lo impide |
| **La cita no cambia a En espera** | Usa **Clínico → Recepción → Recibir**; el botón del detalle no lo hace en esta versión |
| **El stock no coincide** | Revisa los movimientos y ajusta con un motivo; las recetas y facturas también descuentan |
| **La app móvil dice “Sin conexión”** | Comprueba la red, que **Modo avión** esté apagado y que el portal no esté revocado |
| **La app móvil dice “Sin clínica emparejada”** | No hay portal activo; empareja uno en **Portales** |
| **La descarga del portal falla** | Limitación conocida: el QR del escritorio usa `local://` y el móvil solo habla HTTP con Turso |
| **Olvidé la clave del teléfono** | No hay recuperación; borra los datos de la app y vuelve a emparejar |
| **Una solicitud de cita sigue pendiente** | Pulsa **Sincronizar** o espera a tener red; la cola se reintenta sin duplicar |
| **El cierre de caja muestra un desfase** | Compara **Calculado** con **Ingresos reales** y deja una observación |

---

## 21. Glosario

| Término | Significado |
|---|---|
| **Nodo** | El equipo de la clínica que guarda la base local y ejecuta el escritorio |
| **Portal** | La base de datos de un dueño, con la copia de sus datos visible desde el teléfono |
| **Emparejamiento** | El acto de conectar el teléfono con el portal de una clínica mediante el QR y la huella |
| **Huella** | Los 6 caracteres hexadecimales que se comparan en voz alta al emparejar |
| **Outbox / cola** | Los cambios del teléfono que aún no se han enviado |
| **LWW** | “Last write wins”: en un conflicto gana la fila con `updated_at` más reciente |
| **Tombstone** | Marca de fila borrada que se conserva para que el borrado se propague |
| **Proyección** | Copia de los datos de una ficha desde la clínica al portal del dueño |
| **Modo avión** | Ajuste del teléfono que impide intentar la red y deja todo en cola |
| **Caché** | Copia cifrada de lo descargado del portal, con caducidad de 30 días |

---

Para el detalle técnico, la arquitectura y los procedimientos de operación,
consulta el [manual técnico](manual-tecnico.md).
