-- 0001 — Metadatos del portal de un cliente.
-- Cada portal es una base independiente (D18) con el subconjunto de un dueño.

CREATE TABLE portal_meta (
    id              INTEGER PRIMARY KEY CHECK (id = 1),
    esquema_version INTEGER NOT NULL,
    cuenta_uuid     TEXT    NOT NULL,
    clinica_uuid    TEXT    NOT NULL,
    generado_at     INTEGER NOT NULL
);
