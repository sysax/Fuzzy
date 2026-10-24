-- 0002 — Proyección del portal de un dueño.
--
-- D24: no es una copia filtrada de la clínica, es un esquema reducido. No
-- hay notas internas, auditoría, costos, inventario, usuarios ni roles.
-- Cada base de portal pertenece a una sola cuenta y a una sola clínica; el
-- `clinica_uuid` de `portal_meta` es la única sede válida para esta base.
--
-- Las referencias entre tablas usan `uuid`, no ids locales, para que una
-- reconstrucción completa del portal sea idempotente (D23).

ALTER TABLE portal_meta ADD COLUMN clinica_nombre TEXT;
ALTER TABLE portal_meta ADD COLUMN proyeccion_version INTEGER NOT NULL DEFAULT 0;
ALTER TABLE portal_meta ADD COLUMN proyectado_at INTEGER;

-- ── Identidad del dueño ──────────────────────────────────────
-- Una sola fila. La app puede actualizar estos campos; el nodo los proyecta.
CREATE TABLE perfil (
    id             INTEGER PRIMARY KEY CHECK (id = 1),
    uuid           TEXT    NOT NULL UNIQUE,
    nombre_completo TEXT   NOT NULL,
    correo         TEXT,
    telefono       TEXT,
    direccion      TEXT,
    dni            TEXT,
    activo         INTEGER NOT NULL DEFAULT 1 CHECK (activo IN (0, 1)),
    created_at     INTEGER NOT NULL,
    updated_at     INTEGER NOT NULL,
    deleted_at     INTEGER
);

-- ── Mascotas ─────────────────────────────────────────────────
CREATE TABLE mascotas (
    id                INTEGER PRIMARY KEY,
    uuid              TEXT    NOT NULL UNIQUE,
    nombre            TEXT    NOT NULL,
    especie           TEXT    NOT NULL,
    raza              TEXT,
    sexo              TEXT    NOT NULL DEFAULT 'Desconocido'
        CHECK (sexo IN ('Desconocido', 'Macho', 'Hembra')),
    peso_kg           REAL    NOT NULL DEFAULT 0 CHECK (peso_kg >= 0),
    numero_chip       TEXT,
    fecha_nacimiento  TEXT,
    activo            INTEGER NOT NULL DEFAULT 1 CHECK (activo IN (0, 1)),
    castrado          INTEGER NOT NULL DEFAULT 0 CHECK (castrado IN (0, 1)),
    alergias          TEXT,
    atencion_especial TEXT,
    created_at        INTEGER NOT NULL,
    updated_at        INTEGER NOT NULL,
    deleted_at        INTEGER
);

CREATE INDEX portal_mascotas_nombre_idx ON mascotas (nombre);

-- Una sola fila por mascota: la foto se reemplaza completa (D2).
CREATE TABLE mascota_fotos (
    id             INTEGER PRIMARY KEY,
    uuid           TEXT    NOT NULL UNIQUE,
    mascota_uuid   TEXT    NOT NULL UNIQUE
        REFERENCES mascotas (uuid) ON DELETE CASCADE,
    foto           BLOB,
    mime           TEXT    NOT NULL DEFAULT 'image/jpeg',
    sha256         TEXT,
    ancho          INTEGER CHECK (ancho IS NULL OR ancho > 0),
    alto           INTEGER CHECK (alto IS NULL OR alto > 0),
    actualizada_at INTEGER NOT NULL,
    created_at     INTEGER NOT NULL,
    updated_at     INTEGER NOT NULL,
    deleted_at     INTEGER,
    CHECK (foto IS NULL OR length(foto) <= 2097152),
    CHECK (sha256 IS NULL OR length(sha256) = 64)
);

-- ── Historia clínica visible ─────────────────────────────────
CREATE TABLE visitas (
    id            INTEGER PRIMARY KEY,
    uuid          TEXT    NOT NULL UNIQUE,
    mascota_uuid  TEXT    NOT NULL REFERENCES mascotas (uuid) ON DELETE CASCADE,
    fecha         INTEGER NOT NULL,
    motivo        TEXT,
    diagnostico   TEXT,
    anamnesis     TEXT,
    examen_fisico TEXT,
    tratamiento   TEXT,
    veterinario   TEXT,
    es_emergencia INTEGER NOT NULL DEFAULT 0
        CHECK (es_emergencia IN (0, 1)),
    estado        TEXT    NOT NULL DEFAULT 'Cerrada'
        CHECK (estado IN ('En curso', 'Cerrada')),
    cerrada_at    INTEGER,
    created_at    INTEGER NOT NULL,
    updated_at    INTEGER NOT NULL,
    deleted_at    INTEGER
);

CREATE INDEX portal_visitas_mascota_idx ON visitas (mascota_uuid, fecha);

CREATE TABLE recetas (
    id            INTEGER PRIMARY KEY,
    uuid          TEXT    NOT NULL UNIQUE,
    mascota_uuid  TEXT    NOT NULL REFERENCES mascotas (uuid) ON DELETE CASCADE,
    visita_uuid   TEXT    REFERENCES visitas (uuid) ON DELETE SET NULL,
    fecha_emision INTEGER NOT NULL,
    indicaciones  TEXT,
    veterinario   TEXT,
    created_at    INTEGER NOT NULL,
    updated_at    INTEGER NOT NULL,
    deleted_at    INTEGER
);

CREATE INDEX portal_recetas_mascota_idx ON recetas (mascota_uuid, fecha_emision);

CREATE TABLE receta_items (
    id           INTEGER PRIMARY KEY,
    uuid         TEXT    NOT NULL UNIQUE,
    receta_uuid  TEXT    NOT NULL REFERENCES recetas (uuid) ON DELETE CASCADE,
    medicamento  TEXT    NOT NULL,
    dosis        TEXT    NOT NULL,
    frecuencia   TEXT    NOT NULL,
    duracion     TEXT,
    cantidad     INTEGER NOT NULL CHECK (cantidad > 0),
    created_at   INTEGER NOT NULL,
    updated_at   INTEGER NOT NULL,
    deleted_at   INTEGER
);

CREATE INDEX portal_receta_items_receta_idx ON receta_items (receta_uuid);

CREATE TABLE vacunas (
    id               INTEGER PRIMARY KEY,
    uuid             TEXT    NOT NULL UNIQUE,
    mascota_uuid     TEXT    NOT NULL REFERENCES mascotas (uuid) ON DELETE CASCADE,
    nombre_vacuna    TEXT    NOT NULL,
    fecha_aplicacion TEXT    NOT NULL,
    proxima_dosis    TEXT,
    lote             TEXT,
    notas            TEXT,
    veterinario      TEXT,
    created_at       INTEGER NOT NULL,
    updated_at       INTEGER NOT NULL,
    deleted_at       INTEGER
);

CREATE INDEX portal_vacunas_mascota_idx ON vacunas (mascota_uuid, fecha_aplicacion);

CREATE TABLE laboratorio (
    id               INTEGER PRIMARY KEY,
    uuid             TEXT    NOT NULL UNIQUE,
    mascota_uuid     TEXT    NOT NULL REFERENCES mascotas (uuid) ON DELETE CASCADE,
    fecha_solicitud  INTEGER NOT NULL,
    fecha_resultado  INTEGER,
    tipo_examen      TEXT    NOT NULL,
    resultado        TEXT,
    estado           TEXT    NOT NULL DEFAULT 'Solicitado'
        CHECK (estado IN ('Solicitado', 'En proceso', 'Listo', 'Entregado')),
    solicitado_por   TEXT,
    created_at       INTEGER NOT NULL,
    updated_at       INTEGER NOT NULL,
    deleted_at       INTEGER
);

CREATE INDEX portal_laboratorio_mascota_idx
    ON laboratorio (mascota_uuid, fecha_solicitud);

CREATE TABLE pesos (
    id           INTEGER PRIMARY KEY,
    uuid         TEXT    NOT NULL UNIQUE,
    mascota_uuid TEXT    NOT NULL REFERENCES mascotas (uuid) ON DELETE CASCADE,
    fecha        INTEGER NOT NULL,
    peso_kg      REAL    NOT NULL CHECK (peso_kg > 0),
    created_at   INTEGER NOT NULL,
    updated_at   INTEGER NOT NULL,
    deleted_at   INTEGER
);

CREATE INDEX portal_pesos_mascota_idx ON pesos (mascota_uuid, fecha);

-- ── Citas ────────────────────────────────────────────────────
CREATE TABLE citas (
    id           INTEGER PRIMARY KEY,
    uuid         TEXT    NOT NULL UNIQUE,
    mascota_uuid TEXT    NOT NULL REFERENCES mascotas (uuid) ON DELETE CASCADE,
    fecha_hora   INTEGER NOT NULL,
    tipo_servicio TEXT   NOT NULL,
    estado       TEXT    NOT NULL DEFAULT 'Agendada'
        CHECK (estado IN ('Agendada', 'Cancelada', 'Reprogramada',
                          'Completada', 'En espera', 'En curso')),
    direccion    TEXT,
    notas        TEXT,
    origen       TEXT    NOT NULL DEFAULT 'app'
        CHECK (origen IN ('mostrador', 'telefono', 'app', 'primera_visita')),
    created_at   INTEGER NOT NULL,
    updated_at   INTEGER NOT NULL,
    deleted_at   INTEGER
);

CREATE INDEX portal_citas_mascota_idx ON citas (mascota_uuid, fecha_hora);

-- La app inserta aquí; el nodo valida y escribe el resultado en la misma
-- fila. Es la única escritura de negocio que el token del portal permite,
-- además de perfil y mascota_fotos.
CREATE TABLE solicitudes_cita (
    id              INTEGER PRIMARY KEY,
    uuid            TEXT    NOT NULL UNIQUE,
    mascota_uuid    TEXT    NOT NULL REFERENCES mascotas (uuid) ON DELETE CASCADE,
    tipo_servicio   TEXT    NOT NULL,
    fecha_preferida TEXT    NOT NULL,
    hora_preferida  TEXT    NOT NULL,
    motivo          TEXT,
    estado          TEXT    NOT NULL DEFAULT 'Pendiente'
        CHECK (estado IN ('Pendiente', 'Aceptada', 'Rechazada', 'Caducada')),
    respuesta       TEXT,
    cita_uuid       TEXT    REFERENCES citas (uuid) ON DELETE SET NULL,
    created_at      INTEGER NOT NULL,
    updated_at      INTEGER NOT NULL,
    deleted_at      INTEGER
);

CREATE INDEX portal_solicitudes_estado_idx
    ON solicitudes_cita (estado, created_at);

-- ── Adjuntos ─────────────────────────────────────────────────
-- Solo metadatos. El binario nunca sale del nodo (D2).
CREATE TABLE adjuntos (
    id           INTEGER PRIMARY KEY,
    uuid         TEXT    NOT NULL UNIQUE,
    mascota_uuid TEXT    NOT NULL REFERENCES mascotas (uuid) ON DELETE CASCADE,
    visita_uuid  TEXT    REFERENCES visitas (uuid) ON DELETE SET NULL,
    nombre       TEXT    NOT NULL,
    mime         TEXT    NOT NULL,
    sha256       TEXT    NOT NULL CHECK (length(sha256) = 64),
    tamano       INTEGER NOT NULL CHECK (tamano >= 0),
    created_at   INTEGER NOT NULL,
    updated_at   INTEGER NOT NULL,
    deleted_at   INTEGER
);

CREATE INDEX portal_adjuntos_mascota_idx ON adjuntos (mascota_uuid, created_at);
