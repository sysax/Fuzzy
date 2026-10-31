# Límites de la caché del teléfono

Este documento describe qué protege la caché local de `fuzzy_mobile`, qué deja
deliberadamente en claro y qué riesgos **no** cubre. El diseño nace de §4.6 de
[`PLAN.md`](./PLAN.md) y de las decisiones D18, D22 y D26.

## 1. Modelo de amenaza

| Escenario | ¿Protegido? |
|---|---|
| Robo del teléfono con la app bloqueada | Sí: sin la clave no se descifra la caché ni los tokens |
| Copia del archivo `movil.db` (backup, root, extracción forense) | Sí: el contenido de negocio es cifrado; solo quedan ids y fechas |
| QR manipulado o fotografiado por un tercero | Sí: la huella de 6 hex se confirma en voz alta con el operador y el emparejamiento comprueba la clínica |
| Portal revocado | Parcial: la app deja de sincronizar, pero no puede borrar a distancia lo ya descargado |
| Dueño que olvida su clave | Datos irrecuperables por diseño: no hay recuperación ni escrow |
| Malware o root **mientras la app está desbloqueada** | No: la clave derivada está en memoria |
| Capturas de pantalla, miniaturas del sistema, backups del SO | Fuera del alcance de la app |

## 2. Qué se cifra

Cada fila de `cache_filas`, cada token de `portal_conocidos`, cada alta del
buzón y cada entrada del registro local se guarda como un sobre:

```
[versión 1 byte][nonce 12][tag 16][ciphertext]
```

- Algoritmo: AES-256-GCM, nonce aleatorio de 96 bits por sobre, tag de 128 bits.
- Clave: PBKDF2-HMAC-SHA256 con **600 000** iteraciones (configurable solo en
  pruebas con `MovilStore::setIteraciones()`), salt aleatorio de 16 bytes por
  instalación.
- AAD de una fila de caché: `portal_uuid/tabla/uuid`.
- AAD del token de un portal: `portal_uuid/token/portal_uuid`.
- Mover un sobre a otra fila, tabla o portal hace fallar la autenticación; no
  hay forma de “reutilizar” un cifrado válido en otro sitio.

La contraseña nunca se guarda. Se guardan salt, número de iteraciones y un
testigo (`clave_check`) cifrado con la clave derivada; al desbloquear se deriva
de nuevo y se compara en tiempo constante. La clave derivada se borra de forma
explícita (`fill('\0')`) al bloquear.

## 3. Qué queda en claro

La base local conserva lo mínimo para poder listar y sincronizar sin descifrar:

- `portal_conocidos`: `portal_uuid`, `clinica_uuid`, `clinica_nombre`, URL,
  huella, estado, `emparejado_at`, `ultima_sync`. El token va cifrado.
- `cache_filas`: `portal_uuid`, `tabla`, `uuid`, `updated_at`, `deleted_at`,
  `pendiente` y el sobre. **No** se guardan columnas de negocio en claro.
- `outbox`: portal, tabla, uuid y timestamps; el payload va cifrado.
- `altas` y `registro_local`: uuid, tipo, estado y timestamps; los datos van
  cifrados.
- `ajustes`: `kdf_salt`, `kdf_iteraciones`, `clave_check`.

Consecuencias de ese residuo: quien copie el archivo sabe **cuántos** portales
hay, a qué clínica pertenecen, cuántas filas y de qué tipo, y cuándo se
sincronizó por última vez. No sabe nombres, contactos, diagnósticos, recetas,
fotos ni tokens.

## 4. Ciclo de vida

| Acción | Efecto |
|---|---|
| `limpiarCache()` | Borra las filas descargadas; conserva la cola, los portales, los tokens y la clave. Vuelve a descargarse al sincronizar |
| `olvidarPortal(uuid)` | Borra la caché y el token de **ese** portal; los demás no se tocan |
| `purgarVencida()` | Borra la caché de los portales cuya `ultima_sync` tiene más de **30 días**; el portal sigue emparejado |
| `bloquear()` | Borra la clave de memoria; los datos siguen cifrados en disco |
| `cerrarSesion()` | `borrarTodo()`: portales, caché, cola, altas, registro local y claves KDF. El teléfono queda como nuevo |
| Revocación en el nodo | La sonda devuelve `sin_acceso`; el portal queda `revocado` y no se descarga más |

La purga de 30 días se evalúa contra el reloj del teléfono. Un teléfono con la
fecha retrasada puede conservar la caché más tiempo; no es un control de
seguridad, es higiene de datos.

## 5. Límites conocidos

1. **Sin recuperación de clave.** Es una decisión, no una carencia: no hay
   servidor que custodie la clave ni correo de recuperación. Perderla obliga a
   crear clave nueva y volver a emparejar cada portal.
2. **La revocación no borra lo descargado.** Un portal revocado deja de abrir la
   base, pero el teléfono conserva la última copia hasta que el dueño cierre
   sesión, olvide el portal o venza la caché.
3. **La memoria del proceso no está protegida.** Con la app desbloqueada, la
   clave y los datos descifrados están en RAM. Frente a malware con los mismos
   permisos no hay defensa dentro de la app.
4. **El sistema operativo ve la pantalla.** Capturas, conmutador de apps y
   backups del SO quedan fuera del modelo. Se recomienda bloqueo de pantalla del
   dispositivo y desactivar backups de la base.
5. **`CryptoBox` es propio.** Se implementó AES-256-GCM y PBKDF2 en casa porque
   el entorno no tenía cabeceras de OpenSSL; está validado contra vectores de
   `cryptography` (Python), pero no ha pasado una auditoría criptográfica
   externa. Si se añade OpenSSL o libsodium al despliegue, conviene sustituir la
   implementación sin cambiar el formato del sobre.
6. **Metadatos en claro.** Como se detalla en §3, el archivo revela estructura y
   volúmenes, no contenido.
7. **Reloj y orden de conflictos.** LWW depende de `updated_at` generado por los
   nodos. Un reloj muy desviado puede hacer que una edición vieja gane; el
   empate se resuelve a favor del remoto y las filas pendientes están
   protegidas.

## 6. Verificación

`tst_movil_servicios::tokenYCamposClinicosNoEstanEnClaro` escribe una base real,
la cierra y busca en el archivo:

- el token de la clínica,
- el prefijo `token-clinica`,
- el nombre del dueño,
- el nombre de la mascota.

Ninguno aparece. Además reabre la base y comprueba que sin la contraseña
correcta el token no se descifra y que con ella sí. `tst_cryptobox` valida el
cifrado contra vectores externos, y el job `movil` de CI escanea el binario para
que ningún token de portal llegue a él.
