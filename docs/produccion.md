# Producción

Este documento describe cómo se prepara, despliega y recupera FuzzyCloud en
producción. El diseño de fondo está en [`PLAN.md`](PLAN.md); los entornos, en
[`entornos.md`](entornos.md); los procedimientos por sede, en
[`runbook-master.md`](runbook-master.md) y [`runbook-portal.md`](runbook-portal.md).

## 1. Principios

1. **Una clínica, una cuenta de Turso.** Cada sede es dueña de su grupo, sus
   bases y sus tokens (D18). El operador de FuzzyCloud no tiene una credencial
   global que abra los datos de todas las clínicas.
2. **Producción no se toca desde una laptop.** El esquema remoto se aplica con
   un workflow manual y protegido; el nodo se actualiza en la sede con su
   runbook. El token de producción nunca vive en la máquina de desarrollo.
3. **Todo despliegue es reversible.** Antes de cambiar esquema o restaurar se
   toma un respaldo verificado; toda restauración deja una copia del estado
   anterior.
4. **Cero secretos en Git o en los binarios.** La puerta
   [`tools/scan-secrets.sh`](../tools/scan-secrets.sh) corre en CI y en
   `tools/check.sh`.
5. **El alta es un procedimiento, no un botón.** La creación de bases se
   automatiza; el bootstrap, la verificación y el registro los firma una
   persona.

## 2. Entornos y protección

| Entorno | Uso | Datos | Quién despliega |
|---|---|---|---|
| Local | Desarrollo y pruebas | Fixtures, nunca datos reales | Cualquiera |
| Staging | Ensayo de runbooks y migraciones | Datos sintéticos | CI y operador |
| Producción | Sede real | Datos clínicos | Operador con aprobación |

En GitHub, el entorno `production` debe configurarse **antes del primer
despliegue** con estas reglas (Settings → Environments → production):

- **Required reviewers:** al menos una persona distinta de quien dispara el
  workflow. Sin autoaprobación.
- **Deployment branches:** solo `main`.
- **Environment secrets:** `TURSO_DATABASE_TOKEN` con el token de la base que
  se va a migrar. No se define a nivel de repositorio.
- **Wait timer:** opcional; si se usa, deja margen para cancelar.

El entorno `staging` puede ser más laxo, pero también debe tener su propio
secreto `TURSO_DATABASE_TOKEN`. Los secretos de un entorno no están
disponibles para el otro.

> La configuración de entornos y secretos es externa al repositorio. El
> workflow solo la exige; no la crea.

## 3. Secretos

| Secreto | Dónde vive | Quién lo usa |
|---|---|---|
| Token de grupo de la clínica | Keyring del nodo; archivo `0600` solo durante el alta | Nodo (sync y portales) |
| Token de base para Atlas | Environment de GitHub (`staging`/`production`) | Workflow de despliegue |
| Token de plataforma Turso | Keyring del nodo o gestor de secretos de la sede | Alta de portales |
| Token de inserción del buzón | `directory/clinicas.json` (dato comercial) | App móvil, solo escritura |

Reglas:

- El token del nodo **nunca** se pega en un issue, un PR, un chat ni un
  documento. `fuzzy-admin` no lo imprime.
- `tools/provision-production.sh` escribe el token en `secrets/` (ignorado por
  Git) con permisos `0600` y avisa de que hay que moverlo al keyring y borrar
  el archivo.
- Rotar un token de base es un incidente, no una tarea de mantenimiento
  rutinario: se rota si hay sospecha de fuga, si cambia el operador o al
  revocar un portal comprometido.
- Los binarios distribuidos no llevan tokens. El escaneo de la fase 8 lo
  comprueba para el móvil y la fase 10 lo extiende a escritorio y
  `fuzzy-admin`.

## 4. Alta de una clínica

### 4.1 Preparación

1. La sede confirma nombre, ciudad, dirección, teléfono, zona horaria y la
   persona Admin inicial.
2. Se verifica que la cuenta de Turso de la clínica existe y que el operador
   tiene acceso a ella (no a la de otra sede).
3. Se elige un `slug` estable: minúsculas, números y guiones, 2–32 caracteres.
   El `slug` no se cambia después: nombra el grupo y las bases.
4. Se ensaya el alta con `--dry-run` y se revisa el plan impreso.

### 4.2 Creación de infraestructura

```bash
tools/provision-production.sh \
  --slug aurora \
  --nombre "Clínica Aurora" \
  --ciudad "Bogotá" \
  --direccion "Calle 1 #2-3" \
  --telefono "+57 300 000 0000" \
  --dry-run

# Tras revisar el plan y con la aprobación de la sede:
tools/provision-production.sh \
  --slug aurora \
  --nombre "Clínica Aurora" \
  --ciudad "Bogotá" \
  --direccion "Calle 1 #2-3" \
  --telefono "+57 300 000 0000" \
  --confirmar-produccion
```

El script crea el grupo `fuzzy-clinic-<slug>`, las bases
`fuzzy-clinic-<slug>` y `fuzzy-inbox-<slug>`, emite el token de grupo, aplica
las migraciones de clínica y buzón, y deja el token en
`secrets/produccion-<slug>.token` con permisos `0600`. No crea portales ni
publica el directorio.

### 4.3 Puesta en marcha en la sede

1. Copiar el token al keyring del equipo del nodo y borrar
   `secrets/produccion-<slug>.token`.
2. Instalar el escritorio y abrirlo. El nodo aplica las migraciones embebidas y
   muestra el asistente de bootstrap.
3. Ejecutar el bootstrap con los datos de la sede y la cuenta Admin inicial.
4. Verificar con `fuzzy-admin estado` (ver §7) y con un inicio de sesión real.
5. Crear un cliente de prueba, aprobar su portal y comprobar el emparejamiento.
6. Borrar los datos de prueba antes de la apertura, o dejar constancia de que
   la sede los acepta.

### 4.4 Directorio y respaldos

1. Añadir la sede a `directory/clinicas.json` con el `inbox_url` y el
   `inbox_token` reales.
2. Publicar el directorio con `tools/publish-directory.sh` (o dejar que CI lo
   publique desde `main`).
3. Programar `tools/backup.sh --turso fuzzy-clinic-<slug>` y
   `tools/backup.sh --db <AppDataLocation>/FuzzyCloud/clinica.db`.
4. Registrar la sede en [`registro-clinicas.md`](registro-clinicas.md) con
   responsable, fecha, versión de esquema y frecuencia de respaldo.

## 5. Flujo de release

```
feature/* → PR → CI (schema, qt, portal, imagenes, desktop, movil, clinico,
                    produccion, contract) → revisión → merge a main
  → workflow "Desplegar esquema" (manual, dry-run por defecto)
  → aprobación del entorno production
  → aplicar esquema
  → runbook de la sede (nodo y portales)
  → verificación y registro
```

### 5.1 Esquema remoto

El workflow [`.github/workflows/deploy.yml`](../.github/workflows/deploy.yml)
es manual y recibe `entorno`, `componente`, `base` y `aplicar`:

```bash
gh workflow run deploy.yml \
  -f entorno=production \
  -f componente=clinic \
  -f base=fuzzy-clinic-aurora \
  -f aplicar=false
```

Pasos del workflow: valida el destino, muestra `migrate status`, ejecuta
`migrate apply --dry-run`, aplica solo si `aplicar=true` y vuelve a mostrar el
estado. La aprobación del entorno ocurre antes de ejecutar los pasos.

Reglas:

- `aplicar=false` es el valor por defecto y debe ser el primer disparo.
- Una base por ejecución; no se encadenan sedes en el mismo run.
- Si `migrate status` muestra migraciones pendientes inesperadas, se detiene y
  se investiga: puede haber otra persona aplicando el mismo esquema.
- El workflow **no** actualiza el nodo local. Eso lo hace el runbook del máster.

### 5.2 Nodo local

El escritorio aplica las migraciones embebidas al arrancar. Para un nodo
detenido o una base nueva:

```bash
fuzzy-admin migrar --db <ruta>/clinica.db
fuzzy-admin estado --db <ruta>/clinica.db
```

Orden obligatorio: respaldo verificado → ensayo en staging → actualización del
nodo → verificación → registro. Si la verificación falla, se restaura el
respaldo (§6.2) y se detiene el despliegue.

### 5.3 Portales

Los portales se migran de forma perezosa antes de proyectar
(`PortalProvisioner::migrar`). No hay un despliegue masivo de portales: cada
uno se actualiza cuando el nodo le proyecta datos. El runbook del portal
([`runbook-portal.md`](runbook-portal.md)) cubre creación, rotación,
revocación y borrado.

## 6. Respaldos y restauración

### 6.1 Política

| Activo | Herramienta | Frecuencia mínima | Retención |
|---|---|---|---|
| Base local del máster | `tools/backup.sh --db` | Diaria | 14 copias |
| Base de dominio en Turso | `tools/backup.sh --turso` | Diaria | 14 copias |
| Buzón en Turso | `tools/backup.sh --turso` | Diaria | 14 copias |
| Configuración del nodo | Copia del keyring y del archivo de entorno | Al cambiar | Última versión |

- Los respaldos locales usan `VACUUM INTO`, escriben `.sha256` y `.json`, y se
  verifican inmediatamente después de crearse.
- Los respaldos remotos son un `.dump` comprimido con `.sha256` y `.json`.
- `--keep` ajusta la retención; el valor por defecto es 14.
- La copia debe salir del equipo: disco externo, NAS o almacenamiento de la
  sede. `backups/` está ignorado por Git, pero no es un destino externo.
- Una copia no cuenta hasta que `fuzzy-admin verificar --respaldo` (local) o
  `gzip -t` + `sha256sum -c` (remoto) pasa.

Ejemplo de programación (systemd timer o cron, con rutas absolutas):

```cron
15 2 * * * /opt/fuzzycloud/tools/backup.sh --db /home/fuzzy/.local/share/FuzzyCloud/FuzzyCloud/clinica.db --out /mnt/respaldos/aurora --keep 30
30 2 * * * /opt/fuzzycloud/tools/backup.sh --turso fuzzy-clinic-aurora --out /mnt/respaldos/aurora --keep 30
```

La frecuencia y el destino se acuerdan con la sede y se anotan en
[`registro-clinicas.md`](registro-clinicas.md). El diseño no promete un RPO
concreto: el RPO es la antigüedad del último respaldo verificado.

### 6.2 Restauración

```bash
# 1. Detener el escritorio. La restauración no se hace en caliente.
# 2. Verificar el respaldo antes de tocar nada.
fuzzy-admin verificar --db <ruta>/clinica.db --respaldo /mnt/respaldos/aurora/clinica-<ts>.db

# 3. Restaurar. Deja <ruta>/clinica.db.antes-de-restaurar-<ts>-<id>.
fuzzy-admin restaurar --db <ruta>/clinica.db \
  --desde /mnt/respaldos/aurora/clinica-<ts>.db --confirmar

# 4. Verificar y abrir el escritorio.
fuzzy-admin estado --db <ruta>/clinica.db
```

- `restaurar` exige `--confirmar`; sin él no toca la base.
- Si la base restaurada no pasa `integrity_check` o `foreign_key_check`, la
  operación falla y deja la copia previa.
- La copia previa se conserva hasta que la sede confirme que los datos son los
  esperados. No se borra en el mismo día.
- Para Turso, la restauración es importar el `.dump` en una base nueva y
  cambiar el token del nodo; nunca se sobrescribe la base en caliente.

### 6.3 Simulacro de reversión

`tools/rollback-drill.sh` crea una base efímera con el esquema real, toma un
respaldo, introduce un cambio, restaura y comprueba que el cambio desaparece.
Corre en CI (`produccion`) y en `tools/check.sh`. No sustituye al ensayo en
staging con el runbook completo, pero detecta regresiones del mecanismo.

## 7. CLI `fuzzy-admin`

Se compila con `FUZZY_BUILD_ADMIN=ON` (por defecto) y queda en
`build/fuzzy_admin/fuzzy-admin`. No abre ventanas ni necesita servidor gráfico.

| Comando | Uso |
|---|---|
| `estado --db <ruta> [--json]` | Clínica, versión de esquema, integridad y conteos |
| `verificar --db <ruta>` | Comprueba la base actual |
| `verificar --db <ruta> --respaldo <archivo>` | Comprueba checksum e integridad de un respaldo |
| `respaldar --db <ruta> --salida <archivo>` | Crea respaldo + `.sha256` + `.json` |
| `restaurar --db <ruta> --desde <archivo> --confirmar` | Restaura y deja copia previa |
| `recuperar-admin --db <ruta> --correo <correo>` | Reactiva un Admin y emite contraseña temporal |
| `migrar --db <ruta>` | Aplica las migraciones embebidas de clínica |

Códigos de salida: `0` correcto, `1` uso incorrecto, `2` operación fallida.
La contraseña temporal de `recuperar-admin` se muestra una sola vez y caduca a
las 72 h; el usuario debe cambiarla al entrar. La operación queda en
`auditoria` con `usuario_id NULL` y acción `recuperacion_admin_cli`, porque no
hay una sesión de personal que la firme (D7).

## 8. Puerta de secretos

`tools/scan-secrets.sh` comprueba:

- archivos que nunca deben versionarse (`.env`, `secrets/`, `*.db`, `*.dump`);
- tokens de Turso, GitHub y AWS, claves privadas y URLs con `authToken=` en
  archivos versionados o listos para versionar;
- credenciales embebidas en los binarios de escritorio, móvil y `fuzzy-admin`.

Es una puerta enfocada, no un escáner completo. No detecta contraseñas
humanas, tokens con formato propietario ni secretos en el historial de Git. Si
se sospecha una fuga, se rota el secreto y se revisa el historial con una
herramienta dedicada.

## 9. Checklist de alta (staging y producción)

Copiar esta lista al registro de la sede y firmar cada punto. Los puntos de
staging se ejecutan **antes** del alta real; los de producción, en la sede.

**Antes del alta**

- [ ] PR integrado en `main` con CI verde, incluido el job `produccion`.
- [ ] `atlas migrate validate` limpio en los cuatro esquemas.
- [ ] `tools/scan-secrets.sh` limpio.
- [ ] `tools/rollback-drill.sh` correcto.
- [ ] Ensayo del runbook del máster en staging, con evidencia.
- [ ] Ensayo del runbook del portal en staging, con evidencia.
- [ ] Entorno `production` con revisores, rama `main` y secreto propio.

**Alta**

- [ ] `--dry-run` revisado y aprobado por la sede.
- [ ] Grupo, bases y token creados; token en el keyring y archivo borrado.
- [ ] Migraciones de clínica y buzón aplicadas.
- [ ] Bootstrap ejecutado; Admin inicial entra y cambia la contraseña.
- [ ] Cliente de prueba, portal, QR y emparejamiento verificados.
- [ ] Datos de prueba borrados o aceptados por escrito.
- [ ] Directorio actualizado y publicado.
- [ ] Respaldos programados y primera copia verificada.
- [ ] Sede registrada en `docs/registro-clinicas.md`.

**Después del alta**

- [ ] `fuzzy-admin estado` sin errores.
- [ ] Sync del nodo contra Turso comprobado.
- [ ] Buzón de primera visita probado desde la app.
- [ ] Responsable y contacto de la sede anotados.
- [ ] Fecha del próximo simulacro de restauración anotada.

## 10. Lo que no se automatiza

- La aprobación del entorno y la revisión humana del despliegue.
- El bootstrap y la verificación en la sede.
- La ejecución del runbook en staging con datos sintéticos.
- La rotación de tokens del nodo y de los portales.
- La decisión de restaurar y la comunicación a la clínica.
- La copia fuera del equipo y el control de retención física.

Estas tareas se registran con fecha, responsable y resultado. Un runbook sin
evidencia es un runbook no ejecutado.
