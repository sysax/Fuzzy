# Imágenes (fase 6)

Este documento describe las dos rutas de imagen del producto y el límite que
las separa: la foto del dueño vive en el portal; el material clínico vive en
el disco del nodo y **nunca** viaja. El diseño de origen está en
[`PLAN.md`](./PLAN.md) §4.5, §4.1 y la fase 6 de §6.

## 1. Foto de la mascota (portal)

La foto de perfil es una fila de `mascota_fotos` en la base del dueño (D2).
`MascotaFotoService::reemplazar()` la valida y reencoda antes de escribir:

| Regla | Valor |
|---|---|
| Tamaño máximo de entrada | 2 MB |
| Lado mayor | 1024 px (sin ampliar imágenes pequeñas) |
| Píxeles máximos de entrada | 40 MP |
| Formatos aceptados | cualquier imagen que Qt pueda decodificar (JPEG, PNG, WebP, …) |
| Formato de salida | JPEG, calidad 85→45 si hiciera falta |
| Metadatos EXIF | se aplican (orientación) y se descartan al recodificar |
| Transparencia | se compone sobre fondo blanco |

La fila es **una sola por mascota** (`mascota_uuid UNIQUE`). El reemplazo es
un `UPSERT` sobre esa fila que conserva el `uuid`; no se crean versiones ni
filas huérfanas. El servicio comprueba al terminar que queda exactamente una
fila y que su `sha256` y `length(foto)` coinciden con lo reencodado.

Una foto de más de 2 MB se rechaza antes de decodificar, de modo que no se
gasta memoria en ella. La app borra su copia temporal y la caché de esa foto
al recibir la respuesta (fase 8).

`PortalProjector` no escribe en `mascota_fotos`: es una tabla de la app.

## 2. Galería clínica (solo nodo)

Radiografías, ecografías y resultados de laboratorio se guardan con
`GaleriaClinicaService`, que combina dos piezas:

- `FileStore`: escribe el binario bajo una raíz del nodo con `QSaveFile`
  (temporal + rename), permisos `0600` y una lista blanca estricta de rutas
  relativas. Rechaza rutas absolutas, `..`, segmentos vacíos y separadores de
  Windows, para que un uuid o un título manipulado no escape del directorio.
- `mascota_galeria`: conserva `storage_path`, `mime`, `sha256`, `tamano` y la
  visita asociada. La ruta es relativa a la raíz del `FileStore`.

Formatos permitidos (detectados por contenido, no por extensión):

```
image/jpeg · image/png · image/webp · image/tiff · application/pdf
```

Límite: 10 MB por archivo. El binario se guarda tal cual: a diferencia de la
foto de perfil, el material clínico no se reencoda para no perder fidelidad
diagnóstica.

El `uuid` de la fila es determinista a partir de mascota, contenido, título,
tipo y visita. Reintentar la misma carga es idempotente y reutiliza la misma
ruta. Al borrar, la transacción elimina la fila y el archivo juntos; si el
borrado del archivo falla, se revierte la fila.

`mascota_galeria` **no está en `entidadesSync`**: ni el binario ni
`storage_path` salen del nodo. El servicio marca la fila como
`sync_state = 'sincronizado'` para que el outbox no la considere aunque en el
futuro se añada otra entidad.

## 3. Lo que ve el dueño

El proyector lee `mascota_galeria` y escribe en `adjuntos` del portal **solo
metadatos**:

```
uuid · mascota_uuid · visita_uuid · nombre · mime · sha256 · tamano
```

`adjuntos` no tiene ninguna columna `BLOB`. Tampoco se copia `storage_path`:
la ruta es un detalle interno del nodo. Una fila sin `sha256` y `tamano`
(por ejemplo, las anteriores a la migración `0005`) no se proyecta.

## 4. Gate de la fase

| Suite | Qué prueba |
|---|---|
| `tst_file_store` | Escritura atómica, permisos, reemplazo sin temporales y rechazo de rutas inseguras |
| `tst_mascota_foto` | Reencodado a 1024 px/JPEG, reemplazo sobre la misma fila, rechazo de >2 MB y de contenido no imagen |
| `tst_adjuntos_clinicos` | La galería no entra al sync; el change log y el portal no contienen el binario, su base64, su hex ni `storage_path`; el portal solo recibe `adjuntos` |

`tst_adjuntos_clinicos` escanea **todas** las tablas y valores de la base
remota y del portal, no solo las columnas que el proyector conoce. Si un byte
clínico aparece en Turso, el test rompe el build.

## 5. Límites conocidos

- La raíz del `FileStore` la decide la aplicación de escritorio (fase 7); el
  core no elige rutas bajo el directorio del usuario.
- La descarga del adjunto desde el móvil no existe todavía: el portal solo
  publica metadatos. El binario se sirve desde el nodo en una fase posterior.
- La foto se valida al escribirla en el portal; si la app escribe directamente
  contra Turso sin pasar por el nodo, la validación debe ejecutarse también en
  el cliente (fase 8).
