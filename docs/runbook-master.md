# Runbook del máster de clínica

Procedimiento operativo del **nodo de la sede** (el escritorio que guarda la
base `clinic`). El alta de infraestructura está en
[`produccion.md`](produccion.md); los portales, en
[`runbook-portal.md`](runbook-portal.md).

**Audiencia:** operador de la sede o soporte con acceso físico al equipo.
**Duración típica:** alta 60–90 min; actualización 20–30 min; restauración
15–30 min.

## 0. Antes de empezar

### 0.1 Acceso y material

- [ ] Acceso físico o remoto al equipo del nodo.
- [ ] Usuario del sistema con permiso sobre el directorio de datos.
- [ ] Binario `fuzzy-admin` de la misma versión que el escritorio.
- [ ] `sqlite3` instalado.
- [ ] Token de la clínica en el keyring o en el gestor de secretos de la sede.
- [ ] Destino de respaldo externo montado y con espacio.
- [ ] Contacto de la sede y ventana de mantenimiento acordada.

### 0.2 Ruta de la base

El escritorio guarda la base en `QStandardPaths::AppDataLocation`:

| Sistema | Ruta típica |
|---|---|
| Linux | `~/.local/share/FuzzyCloud/FuzzyCloud/clinica.db` |
| Windows | `%LOCALAPPDATA%\FuzzyCloud\FuzzyCloud\clinica.db` |
| macOS | `~/Library/Application Support/FuzzyCloud/FuzzyCloud/clinica.db` |

Confirmar la ruta real antes de cualquier operación:

```bash
fuzzy-admin estado --db "<ruta>/clinica.db"
```

Si el comando falla porque no existe la base, **no** crear una nueva: puede ser
un equipo equivocado o un directorio de datos distinto. Verificar con la sede.

### 0.3 Ensayo en staging

Antes de ejecutar este runbook en producción, se ensaya completo en staging con
datos sintéticos. La hoja de evidencia (§9) se firma en staging y se repite en
producción. Un runbook no ensayado no se aplica a una sede real.

## 1. Alta de la sede

1. Ejecutar el alta de infraestructura de [`produccion.md`](produccion.md) §4.
2. Copiar el token al keyring del nodo y borrar
   `secrets/produccion-<slug>.token`.
3. Instalar el escritorio y abrirlo. El nodo aplica las migraciones embebidas.
4. Completar el bootstrap:
   - nombre de la clínica, ciudad, dirección y teléfono;
   - nombre, correo y contraseña del Admin inicial (mínimo 8 caracteres).
5. Verificar que el Admin entra y que se le exige cambiar la contraseña si es
   temporal.
6. Ejecutar la verificación de §2.
7. Registrar la sede en [`registro-clinicas.md`](registro-clinicas.md).

> El bootstrap solo se ofrece cuando no hay instalación ni clínica. Si la sede
> ya existe, **no** se reabre: el último Admin se recupera con §5.

## 2. Verificación después del alta

```bash
DB="<ruta>/clinica.db"

fuzzy-admin estado --db "$DB"
fuzzy-admin estado --db "$DB" --json
fuzzy-admin verificar --db "$DB"
```

Criterios de aceptación:

- [ ] `Integridad: ok`.
- [ ] `Esquema` igual a la versión esperada del release.
- [ ] `Clínica` y `clinica_uuid` presentes.
- [ ] `Usuarios` coincide con el equipo dado de alta.
- [ ] Inicio de sesión real de Admin y de un rol no Admin.
- [ ] Un cliente de prueba crea portal, muestra QR y empareja.
- [ ] El nodo sincroniza contra Turso sin errores.
- [ ] El buzón de primera visita devuelve una solicitud de prueba.

Si algo falla: detener el alta, no borrar datos y escalar con la salida de
`estado --json`.

## 3. Rutina

### 3.1 Diaria

- [ ] El escritorio abre y permite iniciar sesión.
- [ ] `fuzzy-admin estado --db "$DB"` sin errores.
- [ ] El respaldo programado de la última noche existe y tiene `.sha256`.
- [ ] No hay errores de sync repetidos en la interfaz.

### 3.2 Semanal

- [ ] Verificar el último respaldo local:
      `fuzzy-admin verificar --db "$DB" --respaldo <archivo>`.
- [ ] Verificar el último `.dump` remoto con `gzip -t` y `sha256sum -c`.
- [ ] Confirmar que la copia externa se actualizó (no solo la local).
- [ ] Revisar espacio en disco del nodo y del destino.

### 3.3 Mensual

- [ ] Simulacro de restauración en un equipo o directorio de prueba
      (`tools/rollback-drill.sh` o una copia real).
- [ ] Revisar usuarios activos y roles en Ajustes → Equipo.
- [ ] Revisar que el último Admin sigue activo y con correo válido.
- [ ] Anotar el resultado en el registro de la sede.

## 4. Actualización del nodo

### 4.1 Preparación

1. Leer las notas del release y confirmar la versión de esquema esperada.
2. Avisar a la sede y acordar la ventana.
3. Tomar un respaldo y verificarlo:

   ```bash
   fuzzy-admin respaldar --db "$DB" --salida /mnt/respaldos/<sede>/clinica-$(date -u +%Y%m%dT%H%M%SZ).db
   fuzzy-admin verificar --db "$DB" --respaldo <archivo>
   ```

4. Confirmar que el respaldo remoto de Turso también está al día.

### 4.2 Ensayo en staging

1. Aplicar el esquema con el workflow `deploy.yml` en `staging`,
   `aplicar=false` y después `aplicar=true`.
2. Abrir un nodo de staging con la versión nueva.
3. Ejecutar la verificación de §2 y los flujos clínicos de humo.

### 4.3 Producción

1. Aplicar el esquema remoto con `deploy.yml` en `production` (aprobación del
   entorno, `aplicar=false` y luego `aplicar=true`).
2. Cerrar el escritorio del nodo.
3. Instalar la versión nueva.
4. Abrir el escritorio: aplica las migraciones embebidas.
5. Si el escritorio no arranca o la migración falla:

   ```bash
   fuzzy-admin migrar --db "$DB"
   fuzzy-admin estado --db "$DB"
   ```

6. Verificar §2.
7. Registrar versión, fecha, responsable y resultado.

### 4.4 Reversión

Si la versión nueva no es aceptable:

1. Cerrar el escritorio.
2. Restaurar el respaldo previo (§6).
3. Reinstalar la versión anterior.
4. Verificar §2 y comunicar a la sede.

El esquema remoto no se “desmigra”: si el release cambió el esquema de Turso,
la reversión del nodo debe ser compatible con la versión aplicada. Por eso el
ensayo en staging y la compatibilidad de esquema son parte del gate.

## 5. Recuperación del último Admin (D7)

Se usa cuando no queda ningún Admin activo, cuando se perdió la contraseña o
cuando la cuenta quedó desactivada. **No** se reabre el bootstrap.

```bash
fuzzy-admin recuperar-admin --db "$DB" --correo admin@clinica.test
```

Resultado:

- la cuenta se reactiva (`activo = 1`);
- se emite una contraseña temporal de 12 caracteres, válida 72 h;
- se exige cambio al entrar;
- se escribe una fila en `auditoria` con acción `recuperacion_admin_cli` y
  `usuario_id NULL`.

Pasos:

1. Confirmar la identidad de la persona con la sede (presencial o llamada).
2. Ejecutar el comando en el equipo del nodo.
3. Entregar la contraseña por un canal acordado; no por correo masivo ni chat
   grupal.
4. Pedir que inicie sesión y cambie la contraseña de inmediato.
5. Verificar que la cuenta aparece activa y con rol Admin.
6. Anotar fecha, correo y motivo en el registro. No anotar la contraseña.

Si el correo no existe o no es Admin, el comando falla y no modifica nada.
Si hay varios Admin, la recuperación de uno no afecta a los demás.

## 6. Restauración de la base

### 6.1 Cuándo

- Corrupción detectada por `integrity_check`.
- Error de migración o actualización que no se puede corregir hacia adelante.
- Pérdida de datos confirmada.
- Equipo reemplazado.

### 6.2 Procedimiento

1. Detener el escritorio. No restaurar con la base abierta.
2. Verificar el respaldo elegido:

   ```bash
   fuzzy-admin verificar --db "$DB" --respaldo <archivo>
   ```

3. Restaurar:

   ```bash
   fuzzy-admin restaurar --db "$DB" --desde <archivo> --confirmar
   ```

4. Anotar la ruta de la copia previa que imprime el comando
   (`clinica.db.antes-de-restaurar-<ts>-<id>`).
5. Verificar:

   ```bash
   fuzzy-admin estado --db "$DB"
   fuzzy-admin verificar --db "$DB"
   ```

6. Abrir el escritorio, iniciar sesión y comprobar los datos que la sede
   reportó como perdidos.
7. Confirmar con la sede antes de borrar la copia previa. Conservarla al menos
   24 h.

### 6.3 Restauración remota (Turso)

1. Descargar el `.dump` y verificar `sha256`.
2. Crear una base nueva `fuzzy-clinic-<slug>-restore-<fecha>`.
3. Importar el `.dump` con `turso db shell`.
4. Verificar integridad y conteos.
5. Emitir un token nuevo, actualizar el keyring del nodo y probar el sync.
6. Solo entonces retirar la base anterior, con la sede informada.

Nunca se importa un `.dump` sobre la base en caliente de una sede.

## 7. Incidentes

| Incidente | Acción inmediata | Siguiente paso |
|---|---|---|
| Disco lleno | Cerrar el escritorio; liberar espacio; no borrar `clinica.db` ni `-wal` | Respaldo, verificación y reapertura |
| Base corrupta | Cerrar; copiar el archivo tal cual para análisis | Restaurar §6 y reportar |
| Token comprometido | Revocar el token en Turso; emitir uno nuevo | Actualizar keyring; revisar accesos |
| Equipo perdido o robado | Revocar tokens de la clínica y de portales | Alta en equipo nuevo y restauración |
| Sync detenido | Revisar red y token; no reinstalar a ciegas | Si persiste, restaurar respaldo remoto |
| Admin perdido | §5 | Verificar auditoría |
| Buzón sin altas | Revisar URL y token del directorio | Reproyectar y probar |
| Portal revocado por error | Revisar [`runbook-portal.md`](runbook-portal.md) §3 | Reemitir token y re-emparejar |

Para cualquier incidente con datos clínicos: no borrar evidencia, no
sobrescribir la base y registrar hora, síntoma, acción y resultado.

## 8. Baja de la sede

1. Confirmar por escrito la baja y el destino de los datos.
2. Tomar un respaldo final y verificarlo.
3. Exportar los datos que la sede conserve por obligación legal.
4. Revocar todos los portales y tokens.
5. Detener el nodo y retirar la base del equipo.
6. Eliminar las bases de Turso solo después de la confirmación escrita.
7. Archivar el registro de la sede con fecha y responsable.

## 9. Hoja de evidencia

Copiar por cada ejecución. Sin esta hoja, el runbook se considera no
ejecutado.

| Campo | Valor |
|---|---|
| Sede / `slug` | |
| Entorno | staging / producción |
| Fecha y hora | |
| Versión de esquema | |
| Versión de escritorio | |
| Respaldo usado (ruta y SHA-256) | |
| Pasos ejecutados | alta / verificación / actualización / recuperación / restauración / baja |
| Resultado | correcto / con incidencias |
| Incidencias y acciones | |
| Responsable | |
| Confirmación de la sede | |
