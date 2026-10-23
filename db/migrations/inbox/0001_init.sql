-- 0001 — Buzón de altas de primera visita.
-- La app escribe aquí antes de tener portal; el nodo consume y borra.
-- El token de inserción es compartido y público por diseño, así que las
-- restricciones CHECK son la única barrera contra basura (D21).

CREATE TABLE altas (
    id           INTEGER PRIMARY KEY,
    uuid         TEXT    NOT NULL UNIQUE,
    clinica_uuid TEXT    NOT NULL,
    nombre       TEXT    NOT NULL,
    correo       TEXT,
    telefono     TEXT    NOT NULL,
    mascota      TEXT,
    motivo       TEXT,
    franja       TEXT,
    creada_at    INTEGER NOT NULL,
    CHECK (length(nombre) BETWEEN 3 AND 120),
    CHECK (length(telefono) BETWEEN 7 AND 20),
    CHECK (correo IS NULL OR length(correo) BETWEEN 5 AND 254),
    CHECK (mascota IS NULL OR length(mascota) <= 120),
    CHECK (motivo IS NULL OR length(motivo) <= 500),
    CHECK (franja IS NULL OR length(franja) <= 40)
);

CREATE INDEX altas_clinica_idx ON altas (clinica_uuid, creada_at);
