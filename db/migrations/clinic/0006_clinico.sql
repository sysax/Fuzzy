-- 0006 — Flujo clínico (fase 9): laboratorio, hospitalización, cirugía,
-- estética y movimientos de inventario.
--
-- Estas tablas son la fuente de verdad del nodo. Lo que el dueño ve se
-- proyecta a la base del portal (laboratorio) o se queda en el nodo
-- (hospitalización, cirugía y estética: información clínica interna).
--
-- Convenciones de siempre: id local + uuid global, timestamps en ms UTC,
-- borrado lógico y `sync_state` para el outbox.

-- ── Catálogo de exámenes ─────────────────────────────────────
CREATE TABLE tipos_examen_laboratorio (
    id         INTEGER PRIMARY KEY,
    uuid       TEXT    NOT NULL UNIQUE,
    codigo     TEXT    NOT NULL,
    nombre     TEXT    NOT NULL,
    activo     INTEGER NOT NULL DEFAULT 1 CHECK (activo IN (0, 1)),
    created_at INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    updated_at INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    deleted_at INTEGER,
    sync_state TEXT    NOT NULL DEFAULT 'pendiente'
        CHECK (sync_state IN ('pendiente', 'sincronizado'))
);

CREATE UNIQUE INDEX tipos_examen_laboratorio_codigo_idx
    ON tipos_examen_laboratorio (lower(codigo))
    WHERE deleted_at IS NULL;
CREATE UNIQUE INDEX tipos_examen_laboratorio_nombre_idx
    ON tipos_examen_laboratorio (lower(nombre))
    WHERE deleted_at IS NULL;

INSERT INTO tipos_examen_laboratorio (id, uuid, codigo, nombre) VALUES
    (1, '55555555-5555-4555-8555-555555555501', 'LAB-HEM', 'Hemograma completo'),
    (2, '55555555-5555-4555-8555-555555555502', 'LAB-QUI', 'Química sanguínea'),
    (3, '55555555-5555-4555-8555-555555555503', 'LAB-URI', 'Urianálisis'),
    (4, '55555555-5555-4555-8555-555555555504', 'LAB-COP', 'Coprológico'),
    (5, '55555555-5555-4555-8555-555555555505', 'LAB-CIT', 'Citología'),
    (6, '55555555-5555-4555-8555-555555555506', 'LAB-TIR', 'Perfil tiroideo');

-- ── Laboratorio ──────────────────────────────────────────────
CREATE TABLE laboratorio_solicitudes (
    id              INTEGER PRIMARY KEY,
    uuid            TEXT    NOT NULL UNIQUE,
    mascota_id      INTEGER NOT NULL REFERENCES mascotas (id) ON DELETE CASCADE,
    consulta_id     INTEGER REFERENCES consultas_medicas (id) ON DELETE SET NULL,
    veterinario_id  INTEGER REFERENCES usuarios (id) ON DELETE SET NULL,
    tipo_examen_id  INTEGER REFERENCES tipos_examen_laboratorio (id) ON DELETE SET NULL,
    tipo_examen     TEXT    NOT NULL,
    prioridad       TEXT    NOT NULL DEFAULT 'Normal'
        CHECK (prioridad IN ('Normal', 'Urgente')),
    estado          TEXT    NOT NULL DEFAULT 'Solicitado'
        CHECK (estado IN ('Solicitado', 'En proceso', 'Listo', 'Entregado')),
    fecha_solicitud INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    fecha_resultado INTEGER,
    resultado       TEXT,
    observaciones   TEXT,
    created_at      INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    updated_at      INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    deleted_at      INTEGER,
    sync_state      TEXT    NOT NULL DEFAULT 'pendiente'
        CHECK (sync_state IN ('pendiente', 'sincronizado'))
);

CREATE INDEX laboratorio_solicitudes_mascota_idx
    ON laboratorio_solicitudes (mascota_id, fecha_solicitud);
CREATE INDEX laboratorio_solicitudes_estado_idx
    ON laboratorio_solicitudes (estado);

-- ── Hospitalización ──────────────────────────────────────────
CREATE TABLE hospitalizaciones (
    id             INTEGER PRIMARY KEY,
    uuid           TEXT    NOT NULL UNIQUE,
    mascota_id     INTEGER NOT NULL REFERENCES mascotas (id) ON DELETE CASCADE,
    consulta_id    INTEGER REFERENCES consultas_medicas (id) ON DELETE SET NULL,
    veterinario_id INTEGER REFERENCES usuarios (id) ON DELETE SET NULL,
    fecha_ingreso  INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    fecha_alta     INTEGER,
    motivo         TEXT    NOT NULL,
    diagnostico    TEXT,
    area           TEXT,
    estado         TEXT    NOT NULL DEFAULT 'Activa'
        CHECK (estado IN ('Activa', 'Alta', 'Traslado', 'Fallecido')),
    notas          TEXT,
    created_at     INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    updated_at     INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    deleted_at     INTEGER,
    sync_state     TEXT    NOT NULL DEFAULT 'pendiente'
        CHECK (sync_state IN ('pendiente', 'sincronizado'))
);

CREATE INDEX hospitalizaciones_mascota_idx
    ON hospitalizaciones (mascota_id, fecha_ingreso);
CREATE INDEX hospitalizaciones_estado_idx
    ON hospitalizaciones (estado);

CREATE TABLE hospitalizacion_evoluciones (
    id                  INTEGER PRIMARY KEY,
    uuid                TEXT    NOT NULL UNIQUE,
    hospitalizacion_id  INTEGER NOT NULL
        REFERENCES hospitalizaciones (id) ON DELETE CASCADE,
    usuario_id          INTEGER REFERENCES usuarios (id) ON DELETE SET NULL,
    fecha                INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    nota                TEXT    NOT NULL CHECK (length(trim(nota)) > 0),
    peso_kg             NUMERIC(5, 2) CHECK (peso_kg IS NULL OR peso_kg > 0),
    temperatura_c       NUMERIC(4, 1),
    frecuencia_cardiaca INTEGER
        CHECK (frecuencia_cardiaca IS NULL OR frecuencia_cardiaca > 0),
    created_at          INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    updated_at          INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    deleted_at          INTEGER,
    sync_state          TEXT    NOT NULL DEFAULT 'pendiente'
        CHECK (sync_state IN ('pendiente', 'sincronizado'))
);

CREATE INDEX hospitalizacion_evoluciones_ingreso_idx
    ON hospitalizacion_evoluciones (hospitalizacion_id, fecha);

-- ── Cirugía ──────────────────────────────────────────────────
CREATE TABLE cirugias (
    id                 INTEGER PRIMARY KEY,
    uuid               TEXT    NOT NULL UNIQUE,
    mascota_id         INTEGER NOT NULL REFERENCES mascotas (id) ON DELETE CASCADE,
    consulta_id        INTEGER REFERENCES consultas_medicas (id) ON DELETE SET NULL,
    hospitalizacion_id INTEGER REFERENCES hospitalizaciones (id) ON DELETE SET NULL,
    veterinario_id     INTEGER REFERENCES usuarios (id) ON DELETE SET NULL,
    fecha_programada   INTEGER NOT NULL,
    fecha_inicio       INTEGER,
    fecha_fin          INTEGER,
    procedimiento      TEXT    NOT NULL,
    anestesia          TEXT,
    diagnostico_pre    TEXT,
    diagnostico_post   TEXT,
    estado             TEXT    NOT NULL DEFAULT 'Programada'
        CHECK (estado IN ('Programada', 'En curso', 'Completada', 'Cancelada')),
    notas              TEXT,
    created_at         INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    updated_at         INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    deleted_at         INTEGER,
    sync_state         TEXT    NOT NULL DEFAULT 'pendiente'
        CHECK (sync_state IN ('pendiente', 'sincronizado'))
);

CREATE INDEX cirugias_mascota_idx ON cirugias (mascota_id, fecha_programada);
CREATE INDEX cirugias_estado_idx ON cirugias (estado);

-- ── Estética ─────────────────────────────────────────────────
CREATE TABLE estetica_servicios (
    id            INTEGER PRIMARY KEY,
    uuid          TEXT    NOT NULL UNIQUE,
    mascota_id    INTEGER NOT NULL REFERENCES mascotas (id) ON DELETE CASCADE,
    cita_id       INTEGER REFERENCES citas (id) ON DELETE SET NULL,
    estilista_id  INTEGER REFERENCES usuarios (id) ON DELETE SET NULL,
    servicio_id   INTEGER REFERENCES servicios (id) ON DELETE SET NULL,
    fecha         INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    tipo          TEXT    NOT NULL DEFAULT 'Baño y corte'
        CHECK (tipo IN ('Baño', 'Corte', 'Baño y corte', 'Deslanado', 'Otro')),
    precio        NUMERIC(10, 2) NOT NULL DEFAULT 0 CHECK (precio >= 0),
    observaciones TEXT,
    estado        TEXT    NOT NULL DEFAULT 'Realizado'
        CHECK (estado IN ('Realizado', 'Cancelado')),
    created_at    INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    updated_at    INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    deleted_at    INTEGER,
    sync_state    TEXT    NOT NULL DEFAULT 'pendiente'
        CHECK (sync_state IN ('pendiente', 'sincronizado'))
);

CREATE INDEX estetica_servicios_mascota_idx
    ON estetica_servicios (mascota_id, fecha);

-- ── Movimientos de inventario ────────────────────────────────
-- `productos_insumos.stock_actual` es la foto; esta tabla es el rastro de
-- cada entrada, salida o ajuste. La fase 9 la usa al dispensar una receta y
-- al facturar un producto.
CREATE TABLE movimientos_inventario (
    id               INTEGER PRIMARY KEY,
    uuid             TEXT    NOT NULL UNIQUE,
    producto_id      INTEGER NOT NULL REFERENCES productos_insumos (id) ON DELETE CASCADE,
    usuario_id       INTEGER REFERENCES usuarios (id) ON DELETE SET NULL,
    tipo             TEXT    NOT NULL CHECK (tipo IN ('Entrada', 'Salida', 'Ajuste')),
    cantidad         INTEGER NOT NULL CHECK (cantidad != 0),
    motivo           TEXT,
    referencia_tipo  TEXT,
    referencia_id    INTEGER,
    created_at       INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    updated_at       INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    deleted_at       INTEGER,
    sync_state       TEXT    NOT NULL DEFAULT 'pendiente'
        CHECK (sync_state IN ('pendiente', 'sincronizado'))
);

CREATE INDEX movimientos_inventario_producto_idx
    ON movimientos_inventario (producto_id, created_at);

-- Los cargos nuevos pueden identificar su origen por uuid (estética,
-- laboratorio, cirugía) sin depender del id local. La unicidad se conserva
-- tanto para los cargos antiguos (solo id) como para los nuevos (uuid).
DROP INDEX cargos_origen_idx;
CREATE UNIQUE INDEX cargos_origen_idx
    ON cargos (origen_tipo, coalesce(origen_uuid, cast(origen_id AS TEXT)))
    WHERE deleted_at IS NULL;
