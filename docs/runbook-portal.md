# Runbook del portal

Procedimiento operativo de la **base de un dueño** (`portal`), creada y
gestionada desde el nodo. El alta de la clínica está en
[`produccion.md`](produccion.md); el nodo, en
[`runbook-master.md`](runbook-master.md).

**Audiencia:** Admin o Recepción de la sede, con soporte del operador.
**Duración típica:** alta 5–10 min; revocación 2 min; recuperación 20–40 min.

## 0. Modelo

- Un portal es una base Turso independiente por dueño. Su identidad estable es
  `portales.uuid`; el nombre local es `fuzzy-portal-<uuid>.db` y en Turso
  `fuzzy-portal-<uuid>`.
- El token del portal **solo** abre esa base. No abre la clínica, ni el buzón,
  ni otro portal.
- El nodo guarda solo el SHA-256 del token (D27). El token real vive en el QR
  de emparejamiento y, en el teléfono, cifrado con la contraseña del dueño.
- La base contiene la proyección de los datos del dueño y sus solicitudes de
  cita. No contiene tablas internas ni datos de otros clientes.
- El portal no se respalda como una copia de la clínica: se puede reconstruir
  proyectando de nuevo. Lo que no se reconstruye son las solicitudes de cita
  pendientes de recolectar.

## 1. Alta del portal

1. En el escritorio, abrir **Vínculos** y localizar la ficha del propietario.
2. Si la ficha no tiene cuenta, usar **Crear acceso a la app**:
   - correo del dueño;
   - contraseña inicial (mínimo 8 caracteres), entregada en el mostrador;
   - confirmar.
3. El nodo crea la cuenta, la base del portal, aplica el esquema del portal,
   escribe `portal_meta` y emite el token.
4. Se muestra el QR de emparejamiento con una huella de 6 caracteres.
5. Verificar con el dueño, en voz alta, que la huella del QR coincide con la
   que muestra la app.
6. El dueño escanea el QR, define su clave y descarga el portal.
7. Comprobar que la app muestra la mascota y que el nodo registra el
   emparejamiento (`pareado_at`).

Criterios de aceptación:

- [ ] La app abre el portal y muestra solo las mascotas de esa ficha.
- [ ] El nodo muestra el portal como activo y emparejado.
- [ ] El token no abre la base de la clínica ni otro portal.
- [ ] La huella del QR coincide con la de la app.
- [ ] El dueño puede cerrar sesión y volver a entrar con su clave.

## 2. Verificación

Desde el nodo:

```bash
DB="<ruta>/clinica.db"
fuzzy-admin estado --db "$DB"          # portales: N
sqlite3 "$DB" "SELECT uuid, estado, pareado_at, ultima_sync_at
               FROM portales WHERE deleted_at IS NULL ORDER BY id;"
```

Desde Turso:

```bash
turso db list | grep fuzzy-portal
turso db shell fuzzy-portal-<uuid> "SELECT * FROM portal_meta;"
turso db shell fuzzy-portal-<uuid> "PRAGMA integrity_check;"
```

Desde la app: abrir el portal, refrescar y comprobar que la última sync avanza.

## 3. Revocación

Se usa cuando el dueño pierde el teléfono, cambia de número, pide dejar de usar
la app o hay sospecha de acceso indebido.

1. **Vínculos** → portal → **Revocar**.
2. El estado pasa a `revocado`; el token deja de verificar de inmediato.
3. La app falla en la siguiente consulta y muestra “requiere re-emparejamiento”.
4. Lo ya descargado **no** se borra a distancia: la caché del teléfono caduca a
   los 30 días y se elimina al cerrar sesión (ver
   [`limites-cache.md`](limites-cache.md)).
5. Anotar fecha, motivo y quién autorizó.

La revocación no borra la base ni los datos del nodo. Para eso, §4.

## 4. Borrado

Se usa cuando la relación termina y la sede ha decidido eliminar el portal.

1. Confirmar por escrito con la sede y con el dueño cuando corresponda.
2. **Vínculos** → portal → **Eliminar**.
3. El nodo marca `deleted_at`, pone el estado en `revocado` y elimina la base
   del portal.
4. Verificar que el token ya no abre nada y que la base desapareció de Turso.
5. Conservar el respaldo si la sede lo exige; si no, borrarlo con el mismo
   criterio que el resto de datos clínicos.

El borrado es irreversible desde la interfaz. Si se ejecuta por error, hay que
volver a aprobar el acceso y re-emparejar; las solicitudes pendientes se
pierden.

## 5. Rotación de token

En el MVP la rotación no tiene un botón en la interfaz. El camino soportado es:

1. Revocar el portal (§3).
2. Eliminar el portal (§4) solo si la sede acepta perder la base y las
   solicitudes pendientes; si no, conservar la base y emitir un portal nuevo
   desde **Vínculos** con la misma ficha.
3. Entregar el QR nuevo y repetir el emparejamiento (§1).

El servicio `PortalTokenService::rotar` existe y está cubierto por pruebas para
automatizar la rotación en una tarea de mantenimiento, pero **no** se invoca
desde la interfaz ni desde `fuzzy-admin`. No improvisar un `UPDATE` manual
sobre `portales.token_hash`: el token real no se guarda y el portal quedaría
inaccesible.

## 6. Migración de esquema

Los portales se migran de forma perezosa antes de proyectar
(`PortalProvisioner::migrar`). No hay despliegue masivo.

1. Aplicar el esquema del portal en staging y ensayar la proyección.
2. Desplegar el nodo con la versión nueva.
3. Abrir un portal de prueba y comprobar `portal_meta.esquema_version`.
4. Repetir con un portal real de la sede.
5. Si la migración falla, el nodo no proyecta ese portal y registra el error;
   no se fuerza el esquema a mano.

## 7. Respaldo y recuperación

### 7.1 Respaldo

```bash
tools/backup.sh --turso fuzzy-portal-<uuid> --out /mnt/respaldos/<sede>/portales
```

- Es un `.dump` comprimido con `.sha256` y `.json`.
- La frecuencia se acuerda con la sede. Si el portal solo contiene datos
  derivados, el respaldo protege sobre todo las solicitudes pendientes.
- Verificar con `gzip -t` y `sha256sum -c`.

### 7.2 Recuperación

1. Verificar el `.dump`.
2. Crear una base nueva `fuzzy-portal-<uuid>-restore-<fecha>`.
3. Importar el `.dump` con `turso db shell`.
4. Comprobar `PRAGMA integrity_check` y `portal_meta`.
5. Actualizar la URL del portal en el nodo y emitir un token nuevo.
6. Re-emparejar la app con el QR nuevo.
7. Retirar la base anterior solo después de la confirmación de la sede.

Si no hay respaldo, el portal se puede reconstruir proyectando desde la
clínica; se pierden las solicitudes de cita que aún no se hubieran recolectado.

## 8. Incidentes

| Incidente | Acción inmediata | Siguiente paso |
|---|---|---|
| Token filtrado | Revocar §3 | Reemitir y re-emparejar |
| Portal muestra datos ajenos | Revocar y detener la proyección | Revisar el aislamiento con `tests/portal`; escalar |
| Base del portal corrupta | Revocar | Restaurar §7.2 o reconstruir por proyección |
| Portal revocado por error | Revisar estado | Reemitir y re-emparejar |
| Dueño pierde la clave | No hay recuperación de clave | Crear clave nueva y re-emparejar cada portal |
| App no refresca | Revisar token, URL y red | Verificar sync del nodo y estado del portal |
| Borrado accidental | Detener el nodo | Restaurar respaldo o reconstruir; informar a la sede |

Cualquier incidente con datos de un dueño se registra con hora, portal, acción,
resultado y responsable. No se copian datos de un portal a otro para “probar”.

## 9. Hoja de evidencia

| Campo | Valor |
|---|---|
| Sede / `slug` | |
| Portal (`uuid`) | |
| Dueño / ficha | |
| Acción | alta / verificación / revocación / borrado / rotación / migración / recuperación |
| Fecha y hora | |
| Token emitido (sí/no, sin valor) | |
| Respaldo usado (ruta y SHA-256) | |
| Resultado | correcto / con incidencias |
| Incidencias y acciones | |
| Responsable | |
| Confirmación del dueño o de la sede | |
