-- 0005 — Metadatos de imágenes clínicas (fase 6).
--
-- El binario vive en el disco del nodo (`storage_path`); la base solo guarda
-- metadatos verificables. `sha256` y `tamano` permiten comprobar el archivo y
-- proyectar `adjuntos` en el portal sin transportar un solo byte.
--
-- `mascota_galeria` NO entra en `entidadesSync`: la ruta apunta al disco de
-- este nodo y el binario nunca sale de él. Lo que viaja al portal es solo la
-- fila de `adjuntos` (nombre, mime, sha256 y tamaño).

ALTER TABLE mascota_galeria ADD COLUMN mime TEXT NOT NULL
    DEFAULT 'application/octet-stream';
ALTER TABLE mascota_galeria ADD COLUMN sha256 TEXT
    CHECK (sha256 IS NULL OR length(sha256) = 64);
ALTER TABLE mascota_galeria ADD COLUMN tamano INTEGER NOT NULL DEFAULT 0
    CHECK (tamano >= 0);
ALTER TABLE mascota_galeria ADD COLUMN visita_id INTEGER
    REFERENCES consultas_medicas (id) ON DELETE SET NULL;

CREATE INDEX mascota_galeria_visita_idx
    ON mascota_galeria (visita_id)
    WHERE visita_id IS NOT NULL;

-- Las filas anteriores a esta migración no tienen metadatos verificables.
-- El proyector las ignora hasta que se vuelvan a registrar con el servicio.
