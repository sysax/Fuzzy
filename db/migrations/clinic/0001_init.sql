-- 0001 — Núcleo de arranque de una instalación.
-- Fase 0: solo lo que sostiene el bootstrap (D6/D7). El dominio llega en 0002.

CREATE TABLE clinicas (
    id           INTEGER PRIMARY KEY,
    uuid         TEXT    NOT NULL UNIQUE,
    nombre       TEXT    NOT NULL,
    direccion    TEXT,
    ciudad       TEXT,
    telefono     TEXT,
    zona_horaria TEXT    NOT NULL DEFAULT 'America/Bogota',
    created_at   INTEGER NOT NULL,
    updated_at   INTEGER NOT NULL,
    deleted_at   INTEGER
);

-- Fila única: marca que la instalación ya fue provisionada y que el bootstrap
-- no debe volver a ofrecerse aunque se pierda el último administrador.
CREATE TABLE instalacion (
    id            INTEGER PRIMARY KEY CHECK (id = 1),
    clinica_uuid  TEXT    NOT NULL,
    bootstrap_at  INTEGER NOT NULL,
    bootstrap_por TEXT    NOT NULL,
    version       INTEGER NOT NULL DEFAULT 1,
    FOREIGN KEY (clinica_uuid) REFERENCES clinicas (uuid)
);
