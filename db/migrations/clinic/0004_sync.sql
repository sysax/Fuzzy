-- 0004 — Motor de sincronización entre nodos (fase 5A).
--
-- Cada nodo escribe primero en su SQLite local y deja el cambio en
-- `sync_outbox`. El push lo publica en `sync_change_log` de Turso; el pull
-- aplica los eventos de los demás nodos con LWW y tombstones.
--
-- Idempotencia: `event_uuid` es determinista para (nodo, entidad, uuid,
-- updated_at, operación). Reintentar un lote interrumpido vuelve a insertar
-- el mismo evento y `ON CONFLICT(event_uuid) DO NOTHING` lo descarta.
--
-- `sync_aplicado` es el ledger local de eventos ya aplicados; sin él un pull
-- repetido volvería a escribir filas ya convergidas.

CREATE TABLE sync_outbox (
    id           INTEGER PRIMARY KEY,
    event_uuid   TEXT    NOT NULL UNIQUE,
    entidad      TEXT    NOT NULL,
    entidad_uuid TEXT    NOT NULL,
    operacion    TEXT    NOT NULL
        CHECK (operacion IN ('upsert', 'delete')),
    payload      TEXT    NOT NULL,
    updated_at   INTEGER NOT NULL,
    creado_at    INTEGER NOT NULL,
    enviado_at   INTEGER,
    intentos     INTEGER NOT NULL DEFAULT 0,
    ultimo_error TEXT,
    UNIQUE (entidad, entidad_uuid)
);

CREATE INDEX sync_outbox_pendiente_idx
    ON sync_outbox (enviado_at, id);

CREATE TABLE sync_change_log (
    id           INTEGER PRIMARY KEY,
    event_uuid   TEXT    NOT NULL UNIQUE,
    entidad      TEXT    NOT NULL,
    entidad_uuid TEXT    NOT NULL,
    operacion    TEXT    NOT NULL
        CHECK (operacion IN ('upsert', 'delete')),
    payload      TEXT    NOT NULL,
    updated_at   INTEGER NOT NULL,
    origen_nodo  TEXT    NOT NULL,
    aplicado_at  INTEGER NOT NULL
);

CREATE INDEX sync_change_log_evento_idx
    ON sync_change_log (aplicado_at, id);
CREATE INDEX sync_change_log_entidad_idx
    ON sync_change_log (entidad, entidad_uuid);

CREATE TABLE sync_aplicado (
    event_uuid  TEXT    PRIMARY KEY,
    aplicado_at INTEGER NOT NULL
);

CREATE TABLE sync_meta (
    id             INTEGER PRIMARY KEY CHECK (id = 1),
    nodo_uuid      TEXT    NOT NULL,
    ultimo_push_at INTEGER,
    ultimo_pull_at INTEGER,
    ultimo_pull_id INTEGER NOT NULL DEFAULT 0,
    actualizado_at INTEGER NOT NULL
);
