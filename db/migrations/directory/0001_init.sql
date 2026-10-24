-- 0001 — Directorio público de clínicas (D22).
-- Lo publica CI; la app solo lee. `inbox_token` es una capacidad de solo
-- inserción sobre el buzón: es pública por diseño, no un secreto.

CREATE TABLE clinicas_publicas (
    id          INTEGER PRIMARY KEY,
    uuid        TEXT    NOT NULL UNIQUE,
    nombre      TEXT    NOT NULL,
    ciudad      TEXT,
    direccion   TEXT,
    telefono    TEXT,
    inbox_url   TEXT    NOT NULL,
    inbox_token TEXT    NOT NULL,
    activa      INTEGER NOT NULL DEFAULT 1,
    updated_at  INTEGER NOT NULL
);
