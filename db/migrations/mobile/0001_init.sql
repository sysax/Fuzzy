-- 0001 — Base local del teléfono (fase 8).
--
-- El teléfono no es una copia de la clínica: guarda la lista de portales
-- emparejados, una caché cifrada por portal, la cola de escrituras pendientes
-- y las altas de primera visita que viajan al buzón.
--
-- Lo único legible en claro son los identificadores y los tiempos, necesarios
-- para ordenar y resolver conflictos. Todo contenido de negocio y el token de
-- cada portal van cifrados con AES-256-GCM (CryptoBox), con la clave derivada
-- de la contraseña del dueño.

CREATE TABLE ajustes (
    clave      TEXT PRIMARY KEY,
    valor      TEXT NOT NULL,
    updated_at INTEGER NOT NULL
);

-- ── Portales emparejados (uno por clínica) ───────────────────
-- D26: la misma persona puede tener un portal en cada clínica. Cada portal se
-- empareja por separado y se descifra con la contraseña usada en ese acto.
CREATE TABLE portales_conocidos (
    portal_uuid    TEXT PRIMARY KEY,
    clinica_uuid   TEXT NOT NULL UNIQUE,
    clinica_nombre TEXT NOT NULL,
    url            TEXT NOT NULL,
    token_cifrado  BLOB NOT NULL,
    huella         TEXT NOT NULL CHECK (length(huella) = 6),
    emparejado_at  INTEGER NOT NULL,
    ultima_sync    INTEGER,
    estado         TEXT NOT NULL DEFAULT 'activo'
        CHECK (estado IN ('activo', 'revocado', 'inaccesible'))
);

-- ── Caché cifrada por portal ─────────────────────────────────
CREATE TABLE cache_filas (
    portal_uuid TEXT    NOT NULL
        REFERENCES portales_conocidos (portal_uuid) ON DELETE CASCADE,
    tabla       TEXT    NOT NULL,
    uuid        TEXT    NOT NULL,
    fecha       INTEGER,
    updated_at  INTEGER NOT NULL,
    deleted_at  INTEGER,
    pendiente   INTEGER NOT NULL DEFAULT 0 CHECK (pendiente IN (0, 1)),
    payload     BLOB    NOT NULL,
    PRIMARY KEY (portal_uuid, tabla, uuid)
);

CREATE INDEX cache_filas_orden_idx ON cache_filas (portal_uuid, tabla, fecha);

-- ── Escrituras pendientes ────────────────────────────────────
-- Una fila por operación. `uuid` es la clave de idempotencia: reenviar tras un
-- corte no duplica la solicitud ni el cambio de perfil.
CREATE TABLE outbox (
    id           INTEGER PRIMARY KEY,
    portal_uuid  TEXT    NOT NULL
        REFERENCES portales_conocidos (portal_uuid) ON DELETE CASCADE,
    uuid         TEXT    NOT NULL,
    tabla        TEXT    NOT NULL,
    payload      BLOB    NOT NULL,
    creado_at    INTEGER NOT NULL,
    intentos     INTEGER NOT NULL DEFAULT 0,
    ultimo_error TEXT,
    UNIQUE (portal_uuid, uuid)
);

-- ── Altas de primera visita (buzón) ──────────────────────────
-- El buzón es de un solo sentido: la app escribe y la clínica consume. Nunca
-- se lee, así que aquí no existe ninguna tabla de solicitudes ajenas.
CREATE TABLE altas (
    uuid           TEXT PRIMARY KEY,
    clinica_uuid   TEXT NOT NULL,
    clinica_nombre TEXT NOT NULL,
    payload        BLOB NOT NULL,
    estado         TEXT NOT NULL DEFAULT 'pendiente'
        CHECK (estado IN ('pendiente', 'enviada', 'rechazada')),
    creado_at      INTEGER NOT NULL,
    enviado_at     INTEGER,
    error          TEXT
);

CREATE INDEX altas_estado_idx ON altas (estado, creado_at);

-- ── Registro local antes de emparejar ────────────────────────
-- La app funciona sin portal: el dueño declara sus datos y su mascota. Al
-- emparejar se importan al portal de la clínica y la fila se marca.
CREATE TABLE registro_local (
    uuid       TEXT PRIMARY KEY,
    tipo       TEXT NOT NULL CHECK (tipo IN ('perfil', 'mascota')),
    payload    BLOB NOT NULL,
    created_at INTEGER NOT NULL,
    updated_at INTEGER NOT NULL,
    importado  INTEGER NOT NULL DEFAULT 0 CHECK (importado IN (0, 1))
);

CREATE INDEX registro_local_tipo_idx ON registro_local (tipo, importado);
