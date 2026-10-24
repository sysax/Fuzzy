-- 0002 — Dominio de la clínica.
--
-- Una base por clínica (D18/D28). Por eso las tablas de dominio NO repiten
-- `clinica_uuid`: el archivo es la frontera de tenant y `clinicas` tiene una
-- sola fila. Repetir la sede en cada tabla solo abriría la puerta a la
-- co-tenencia que D28 rechaza.
--
-- Convenciones (D12):
--   · `id` entero local + `uuid` global.
--   · timestamps en milisegundos UTC.
--   · borrado lógico con `deleted_at` y `sync_state` para el outbox.
--   · `auditoria` e `historial_cambios_cita` son append-only: solo created_at.
--
-- Las imágenes clínicas viven en disco (`mascota_galeria.storage_path`); la
-- foto de perfil que ve el dueño se proyecta como BLOB en el portal.

-- ── Sede ─────────────────────────────────────────────────────
-- 0001 crea la fila única de la instalación. Aquí se completa con los
-- campos que el panel necesita y con el estado de sincronización.
ALTER TABLE clinicas ADD COLUMN activa INTEGER NOT NULL DEFAULT 1
    CHECK (activa IN (0, 1));
ALTER TABLE clinicas ADD COLUMN sync_state TEXT NOT NULL DEFAULT 'pendiente'
    CHECK (sync_state IN ('pendiente', 'sincronizado'));

-- ── Personal ─────────────────────────────────────────────────
CREATE TABLE roles (
    id          INTEGER PRIMARY KEY,
    uuid        TEXT    NOT NULL UNIQUE,
    nombre_rol  TEXT    NOT NULL UNIQUE,
    descripcion TEXT,
    created_at  INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    updated_at  INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    deleted_at  INTEGER,
    sync_state  TEXT    NOT NULL DEFAULT 'pendiente'
        CHECK (sync_state IN ('pendiente', 'sincronizado'))
);

CREATE TABLE usuarios (
    id                    INTEGER PRIMARY KEY,
    uuid                  TEXT    NOT NULL UNIQUE,
    nombre_completo       TEXT    NOT NULL,
    correo                TEXT    NOT NULL,
    password_hash         TEXT    NOT NULL,
    rol_id                INTEGER NOT NULL REFERENCES roles (id) ON DELETE RESTRICT,
    fecha_registro        INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    activo                INTEGER NOT NULL DEFAULT 1 CHECK (activo IN (0, 1)),
    debe_cambiar_password INTEGER NOT NULL DEFAULT 1
        CHECK (debe_cambiar_password IN (0, 1)),
    password_expira_at    INTEGER,
    ultimo_acceso         INTEGER,
    created_at            INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    updated_at            INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    deleted_at            INTEGER,
    sync_state            TEXT    NOT NULL DEFAULT 'pendiente'
        CHECK (sync_state IN ('pendiente', 'sincronizado'))
);

-- D25: el correo es único dentro de la clínica, no entre clínicas.
CREATE UNIQUE INDEX usuarios_correo_idx
    ON usuarios (lower(correo))
    WHERE deleted_at IS NULL;
CREATE INDEX usuarios_rol_idx ON usuarios (rol_id);

-- D3: catálogo fijo. Los ids son estables porque la app los usa en sesión.
INSERT INTO roles (id, uuid, nombre_rol, descripcion) VALUES
    (1, '11111111-1111-4111-8111-111111111101', 'Admin',
     'Acceso total a configuraciones, personal y reportes financieros.'),
    (2, '11111111-1111-4111-8111-111111111102', 'Veterinario',
     'Expedientes médicos, consultas y solicitudes de laboratorio.'),
    (3, '11111111-1111-4111-8111-111111111103', 'Recepcion',
     'Recepción de clientes, identificación de mascotas, cobros y cierre de caja.'),
    (4, '11111111-1111-4111-8111-111111111104', 'Estilista',
     'Agenda de estética y reportes de pelo/piel.');

-- ── Clientes y cuentas del portal ────────────────────────────
-- `clientes` es la ficha asistencial. `cuentas` es el plano de identidad
-- del dueño (D9): existe solo si la clínica le da acceso a la app. La
-- aprobación del vínculo ocurre antes de crear el portal.
CREATE TABLE clientes (
    id             INTEGER PRIMARY KEY,
    uuid           TEXT    NOT NULL UNIQUE,
    nombre_completo TEXT   NOT NULL,
    telefono       TEXT,
    correo         TEXT,
    direccion      TEXT,
    dni            TEXT,
    activo         INTEGER NOT NULL DEFAULT 1 CHECK (activo IN (0, 1)),
    created_at     INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    updated_at     INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    deleted_at     INTEGER,
    sync_state     TEXT    NOT NULL DEFAULT 'pendiente'
        CHECK (sync_state IN ('pendiente', 'sincronizado'))
);

CREATE UNIQUE INDEX clientes_correo_idx
    ON clientes (lower(correo))
    WHERE correo IS NOT NULL AND deleted_at IS NULL;
CREATE UNIQUE INDEX clientes_dni_idx
    ON clientes (lower(dni))
    WHERE dni IS NOT NULL AND deleted_at IS NULL;

CREATE TABLE cuentas (
    id                    INTEGER PRIMARY KEY,
    uuid                  TEXT    NOT NULL UNIQUE,
    cliente_id            INTEGER NOT NULL UNIQUE
        REFERENCES clientes (id) ON DELETE CASCADE,
    correo                TEXT    NOT NULL,
    password_hash         TEXT    NOT NULL,
    estado                TEXT    NOT NULL DEFAULT 'Pendiente'
        CHECK (estado IN ('Pendiente', 'Activa', 'Rechazada', 'Suspendida')),
    activo                INTEGER NOT NULL DEFAULT 1 CHECK (activo IN (0, 1)),
    debe_cambiar_password INTEGER NOT NULL DEFAULT 0
        CHECK (debe_cambiar_password IN (0, 1)),
    password_expira_at    INTEGER,
    verificado_at         INTEGER,
    verificado_por        INTEGER REFERENCES usuarios (id) ON DELETE SET NULL,
    ultimo_acceso         INTEGER,
    created_at            INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    updated_at            INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    deleted_at            INTEGER,
    sync_state            TEXT    NOT NULL DEFAULT 'pendiente'
        CHECK (sync_state IN ('pendiente', 'sincronizado'))
);

CREATE UNIQUE INDEX cuentas_correo_idx
    ON cuentas (lower(correo))
    WHERE deleted_at IS NULL;
CREATE INDEX cuentas_cliente_idx ON cuentas (cliente_id);

CREATE TABLE mascotas (
    id                 INTEGER PRIMARY KEY,
    uuid               TEXT    NOT NULL UNIQUE,
    cliente_id         INTEGER NOT NULL REFERENCES clientes (id) ON DELETE CASCADE,
    nombre             TEXT    NOT NULL,
    especie            TEXT    NOT NULL,
    raza               TEXT,
    sexo               TEXT    NOT NULL DEFAULT 'Desconocido'
        CHECK (sexo IN ('Desconocido', 'Macho', 'Hembra')),
    peso_kg            REAL    NOT NULL DEFAULT 0 CHECK (peso_kg >= 0),
    numero_chip        TEXT,
    fecha_nacimiento   TEXT,
    activo             INTEGER NOT NULL DEFAULT 1 CHECK (activo IN (0, 1)),
    castrado           INTEGER NOT NULL DEFAULT 0 CHECK (castrado IN (0, 1)),
    alergias           TEXT,
    atencion_especial  TEXT,
    created_at         INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    updated_at         INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    deleted_at         INTEGER,
    sync_state         TEXT    NOT NULL DEFAULT 'pendiente'
        CHECK (sync_state IN ('pendiente', 'sincronizado'))
);

CREATE INDEX mascotas_cliente_idx ON mascotas (cliente_id);
CREATE UNIQUE INDEX mascotas_chip_idx
    ON mascotas (numero_chip)
    WHERE numero_chip IS NOT NULL AND deleted_at IS NULL;

-- ── Catálogos ────────────────────────────────────────────────
CREATE TABLE categorias_inventario (
    id               INTEGER PRIMARY KEY,
    uuid             TEXT    NOT NULL UNIQUE,
    nombre_categoria TEXT    NOT NULL,
    created_at       INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    updated_at       INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    deleted_at       INTEGER,
    sync_state       TEXT    NOT NULL DEFAULT 'pendiente'
        CHECK (sync_state IN ('pendiente', 'sincronizado'))
);

CREATE UNIQUE INDEX categorias_inventario_nombre_idx
    ON categorias_inventario (lower(nombre_categoria))
    WHERE deleted_at IS NULL;

CREATE TABLE tipos_servicio_cita (
    id             INTEGER PRIMARY KEY,
    uuid           TEXT    NOT NULL UNIQUE,
    nombre_servicio TEXT   NOT NULL,
    created_at     INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    updated_at     INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    deleted_at     INTEGER,
    sync_state     TEXT    NOT NULL DEFAULT 'pendiente'
        CHECK (sync_state IN ('pendiente', 'sincronizado'))
);

CREATE UNIQUE INDEX tipos_servicio_cita_nombre_idx
    ON tipos_servicio_cita (lower(nombre_servicio))
    WHERE deleted_at IS NULL;

CREATE TABLE estados_cita (
    id            INTEGER PRIMARY KEY,
    uuid          TEXT    NOT NULL UNIQUE,
    nombre_estado TEXT    NOT NULL,
    created_at    INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    updated_at    INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    deleted_at    INTEGER,
    sync_state    TEXT    NOT NULL DEFAULT 'pendiente'
        CHECK (sync_state IN ('pendiente', 'sincronizado'))
);

CREATE UNIQUE INDEX estados_cita_nombre_idx
    ON estados_cita (lower(nombre_estado))
    WHERE deleted_at IS NULL;

CREATE TABLE servicios (
    id         INTEGER PRIMARY KEY,
    uuid       TEXT    NOT NULL UNIQUE,
    codigo     TEXT    NOT NULL,
    nombre     TEXT    NOT NULL,
    precio     NUMERIC(10, 2) NOT NULL CHECK (precio >= 0),
    activo     INTEGER NOT NULL DEFAULT 1 CHECK (activo IN (0, 1)),
    created_at INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    updated_at INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    deleted_at INTEGER,
    sync_state TEXT    NOT NULL DEFAULT 'pendiente'
        CHECK (sync_state IN ('pendiente', 'sincronizado'))
);

CREATE UNIQUE INDEX servicios_codigo_idx
    ON servicios (lower(codigo))
    WHERE deleted_at IS NULL;

INSERT INTO categorias_inventario (id, uuid, nombre_categoria) VALUES
    (1, '44444444-4444-4444-8444-444444444401', 'Medicamentos'),
    (2, '44444444-4444-4444-8444-444444444402', 'Vacunas'),
    (3, '44444444-4444-4444-8444-444444444403', 'Insumos'),
    (4, '44444444-4444-4444-8444-444444444404', 'Alimentos'),
    (5, '44444444-4444-4444-8444-444444444405', 'Otros');

INSERT INTO tipos_servicio_cita (id, uuid, nombre_servicio) VALUES
    (1, '33333333-3333-4333-8333-333333333301', 'Médica'),
    (2, '33333333-3333-4333-8333-333333333302', 'Peluquería / Estética'),
    (3, '33333333-3333-4333-8333-333333333303', 'Domicilio');

INSERT INTO estados_cita (id, uuid, nombre_estado) VALUES
    (1, '22222222-2222-4222-8222-222222222201', 'Agendada'),
    (2, '22222222-2222-4222-8222-222222222202', 'Cancelada'),
    (3, '22222222-2222-4222-8222-222222222203', 'Reprogramada'),
    (4, '22222222-2222-4222-8222-222222222204', 'Completada'),
    (5, '22222222-2222-4222-8222-222222222205', 'En espera'),
    (6, '22222222-2222-4222-8222-222222222206', 'En curso');

-- ── Agenda ───────────────────────────────────────────────────
CREATE TABLE citas (
    id                    INTEGER PRIMARY KEY,
    uuid                  TEXT    NOT NULL UNIQUE,
    mascota_id            INTEGER NOT NULL REFERENCES mascotas (id) ON DELETE CASCADE,
    tipo_servicio_id      INTEGER NOT NULL
        REFERENCES tipos_servicio_cita (id) ON DELETE RESTRICT,
    estado_cita_id        INTEGER NOT NULL
        REFERENCES estados_cita (id) ON DELETE RESTRICT,
    empleado_asignado_id  INTEGER REFERENCES usuarios (id) ON DELETE SET NULL,
    fecha_hora_programada INTEGER NOT NULL,
    fecha_llegada         INTEGER,
    prioridad             TEXT    NOT NULL DEFAULT 'Normal'
        CHECK (prioridad IN ('Normal', 'Emergencia')),
    es_emergencia         INTEGER NOT NULL DEFAULT 0
        CHECK (es_emergencia IN (0, 1)),
    origen                TEXT    NOT NULL DEFAULT 'mostrador'
        CHECK (origen IN ('mostrador', 'telefono', 'app', 'primera_visita')),
    direccion_domicilio   TEXT,
    notas_cliente         TEXT,
    fecha_creacion        INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    fecha_actualizacion   INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    created_at            INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    updated_at            INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    deleted_at            INTEGER,
    sync_state            TEXT    NOT NULL DEFAULT 'pendiente'
        CHECK (sync_state IN ('pendiente', 'sincronizado'))
);

CREATE INDEX citas_fecha_idx ON citas (fecha_hora_programada);
CREATE INDEX citas_mascota_idx ON citas (mascota_id);
CREATE INDEX citas_estado_idx ON citas (estado_cita_id);
CREATE INDEX citas_prioridad_idx ON citas (prioridad);

CREATE TABLE historial_cambios_cita (
    id                  INTEGER PRIMARY KEY,
    uuid                TEXT    NOT NULL UNIQUE,
    cita_id             INTEGER NOT NULL REFERENCES citas (id) ON DELETE CASCADE,
    usuario_cambio_id   INTEGER REFERENCES usuarios (id) ON DELETE SET NULL,
    estado_anterior_id  INTEGER REFERENCES estados_cita (id) ON DELETE SET NULL,
    estado_nuevo_id     INTEGER REFERENCES estados_cita (id) ON DELETE SET NULL,
    fecha_hora_anterior INTEGER,
    fecha_hora_nueva    INTEGER,
    motivo_cambio       TEXT,
    created_at          INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    sync_state          TEXT    NOT NULL DEFAULT 'pendiente'
        CHECK (sync_state IN ('pendiente', 'sincronizado'))
);

CREATE INDEX historial_cambios_cita_cita_idx
    ON historial_cambios_cita (cita_id, created_at);

-- ── Historia clínica ─────────────────────────────────────────
CREATE TABLE consultas_medicas (
    id                   INTEGER PRIMARY KEY,
    uuid                 TEXT    NOT NULL UNIQUE,
    mascota_id           INTEGER NOT NULL REFERENCES mascotas (id) ON DELETE CASCADE,
    veterinario_id       INTEGER REFERENCES usuarios (id) ON DELETE SET NULL,
    cita_id              INTEGER UNIQUE REFERENCES citas (id) ON DELETE SET NULL,
    fecha_consulta       INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    motivo_visita        TEXT,
    es_emergencia        INTEGER NOT NULL DEFAULT 0
        CHECK (es_emergencia IN (0, 1)),
    diagnostico          TEXT,
    tratamiento_indicado TEXT,
    anamnesis            TEXT,
    examen_fisico        TEXT,
    estado               TEXT    NOT NULL DEFAULT 'En curso'
        CHECK (estado IN ('En curso', 'Cerrada')),
    fecha_cierre         INTEGER,
    triage_notas         TEXT,
    created_at           INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    updated_at           INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    deleted_at           INTEGER,
    sync_state           TEXT    NOT NULL DEFAULT 'pendiente'
        CHECK (sync_state IN ('pendiente', 'sincronizado'))
);

CREATE INDEX consultas_medicas_mascota_idx
    ON consultas_medicas (mascota_id, fecha_consulta);
CREATE INDEX consultas_medicas_veterinario_idx
    ON consultas_medicas (veterinario_id);
CREATE INDEX consultas_medicas_estado_idx
    ON consultas_medicas (estado);

CREATE TABLE signos_vitales (
    id                    INTEGER PRIMARY KEY,
    uuid                  TEXT    NOT NULL UNIQUE,
    mascota_id            INTEGER NOT NULL REFERENCES mascotas (id) ON DELETE CASCADE,
    consulta_id           INTEGER REFERENCES consultas_medicas (id) ON DELETE SET NULL,
    fecha                 INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    peso_kg               NUMERIC(5, 2) CHECK (peso_kg IS NULL OR peso_kg > 0),
    temperatura_c         NUMERIC(4, 1),
    frecuencia_cardiaca   INTEGER CHECK (frecuencia_cardiaca IS NULL OR frecuencia_cardiaca > 0),
    frecuencia_respiratoria INTEGER
        CHECK (frecuencia_respiratoria IS NULL OR frecuencia_respiratoria > 0),
    created_at            INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    updated_at            INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    deleted_at            INTEGER,
    sync_state            TEXT    NOT NULL DEFAULT 'pendiente'
        CHECK (sync_state IN ('pendiente', 'sincronizado'))
);

CREATE INDEX signos_vitales_mascota_idx
    ON signos_vitales (mascota_id, fecha);

CREATE TABLE vacunas (
    id               INTEGER PRIMARY KEY,
    uuid             TEXT    NOT NULL UNIQUE,
    mascota_id       INTEGER NOT NULL REFERENCES mascotas (id) ON DELETE CASCADE,
    veterinario_id   INTEGER REFERENCES usuarios (id) ON DELETE SET NULL,
    nombre_vacuna    TEXT    NOT NULL,
    fecha_aplicacion TEXT    NOT NULL,
    proxima_dosis    TEXT,
    lote             TEXT,
    notas            TEXT,
    created_at       INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    updated_at       INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    deleted_at       INTEGER,
    sync_state       TEXT    NOT NULL DEFAULT 'pendiente'
        CHECK (sync_state IN ('pendiente', 'sincronizado'))
);

CREATE INDEX vacunas_mascota_idx ON vacunas (mascota_id, fecha_aplicacion);

CREATE TABLE recetas (
    id             INTEGER PRIMARY KEY,
    uuid           TEXT    NOT NULL UNIQUE,
    consulta_id    INTEGER NOT NULL UNIQUE
        REFERENCES consultas_medicas (id) ON DELETE CASCADE,
    mascota_id     INTEGER NOT NULL REFERENCES mascotas (id) ON DELETE CASCADE,
    veterinario_id INTEGER REFERENCES usuarios (id) ON DELETE SET NULL,
    fecha_emision  INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    indicaciones   TEXT,
    created_at     INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    updated_at     INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    deleted_at     INTEGER,
    sync_state     TEXT    NOT NULL DEFAULT 'pendiente'
        CHECK (sync_state IN ('pendiente', 'sincronizado'))
);

CREATE INDEX recetas_mascota_idx ON recetas (mascota_id, fecha_emision);

CREATE TABLE receta_detalles (
    id              INTEGER PRIMARY KEY,
    uuid            TEXT    NOT NULL UNIQUE,
    receta_id       INTEGER NOT NULL REFERENCES recetas (id) ON DELETE CASCADE,
    producto_id     INTEGER REFERENCES productos_insumos (id) ON DELETE SET NULL,
    medicamento     TEXT    NOT NULL,
    dosis           TEXT    NOT NULL,
    frecuencia      TEXT    NOT NULL,
    duracion        TEXT,
    cantidad        INTEGER NOT NULL CHECK (cantidad > 0),
    precio_unitario NUMERIC(10, 2) NOT NULL CHECK (precio_unitario >= 0),
    created_at      INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    updated_at      INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    deleted_at      INTEGER,
    sync_state      TEXT    NOT NULL DEFAULT 'pendiente'
        CHECK (sync_state IN ('pendiente', 'sincronizado'))
);

CREATE INDEX receta_detalles_receta_idx ON receta_detalles (receta_id);

CREATE TABLE mascota_galeria (
    id           INTEGER PRIMARY KEY,
    uuid         TEXT    NOT NULL UNIQUE,
    mascota_id   INTEGER NOT NULL REFERENCES mascotas (id) ON DELETE CASCADE,
    titulo       TEXT    NOT NULL CHECK (length(trim(titulo)) > 0),
    tipo         TEXT    NOT NULL DEFAULT 'Foto'
        CHECK (tipo IN ('Foto', 'Radiografia', 'Ecografia', 'Laboratorio', 'Otro')),
    storage_path TEXT    NOT NULL,
    usuario_id   INTEGER REFERENCES usuarios (id) ON DELETE SET NULL,
    created_at   INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    updated_at   INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    deleted_at   INTEGER,
    sync_state   TEXT    NOT NULL DEFAULT 'pendiente'
        CHECK (sync_state IN ('pendiente', 'sincronizado'))
);

CREATE UNIQUE INDEX mascota_galeria_path_idx
    ON mascota_galeria (storage_path)
    WHERE deleted_at IS NULL;
CREATE INDEX mascota_galeria_mascota_idx
    ON mascota_galeria (mascota_id, created_at);

-- ── Inventario ───────────────────────────────────────────────
CREATE TABLE productos_insumos (
    id               INTEGER PRIMARY KEY,
    uuid             TEXT    NOT NULL UNIQUE,
    nombre_comercial TEXT    NOT NULL,
    principio_activo TEXT,
    categoria_id     INTEGER REFERENCES categorias_inventario (id) ON DELETE SET NULL,
    codigo           TEXT,
    unidad           TEXT    NOT NULL DEFAULT 'unidades',
    stock_actual     INTEGER NOT NULL DEFAULT 0 CHECK (stock_actual >= 0),
    stock_minimo     INTEGER NOT NULL DEFAULT 5 CHECK (stock_minimo >= 0),
    precio_compra    NUMERIC(10, 2) NOT NULL CHECK (precio_compra >= 0),
    precio_venta     NUMERIC(10, 2) NOT NULL CHECK (precio_venta >= 0),
    fecha_caducidad  TEXT,
    proveedor_nombre TEXT,
    proveedor_correo TEXT,
    pedido_en_curso  INTEGER NOT NULL DEFAULT 0
        CHECK (pedido_en_curso IN (0, 1)),
    created_at       INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    updated_at       INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    deleted_at       INTEGER,
    sync_state       TEXT    NOT NULL DEFAULT 'pendiente'
        CHECK (sync_state IN ('pendiente', 'sincronizado'))
);

CREATE INDEX productos_insumos_categoria_idx
    ON productos_insumos (categoria_id);
CREATE UNIQUE INDEX productos_insumos_codigo_idx
    ON productos_insumos (lower(codigo))
    WHERE codigo IS NOT NULL AND deleted_at IS NULL;
CREATE INDEX productos_insumos_caducidad_idx
    ON productos_insumos (fecha_caducidad);

-- ── Facturación y caja ───────────────────────────────────────
CREATE TABLE facturas (
    id              INTEGER PRIMARY KEY,
    uuid            TEXT    NOT NULL UNIQUE,
    cliente_id      INTEGER REFERENCES clientes (id) ON DELETE SET NULL,
    cajero_id       INTEGER REFERENCES usuarios (id) ON DELETE SET NULL,
    numero_factura  TEXT,
    fecha_emision   INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    subtotal        NUMERIC(10, 2) NOT NULL DEFAULT 0 CHECK (subtotal >= 0),
    impuesto        NUMERIC(10, 2) NOT NULL DEFAULT 0 CHECK (impuesto >= 0),
    descuento       NUMERIC(10, 2) NOT NULL DEFAULT 0 CHECK (descuento >= 0),
    monto_total     NUMERIC(10, 2) NOT NULL DEFAULT 0 CHECK (monto_total >= 0),
    metodo_pago     TEXT
        CHECK (metodo_pago IS NULL OR metodo_pago IN
               ('Efectivo', 'Tarjeta', 'Transferencia', 'Credito')),
    estado          TEXT    NOT NULL DEFAULT 'Borrador'
        CHECK (estado IN ('Borrador', 'Emitida', 'Pagada', 'Pendiente')),
    nota_interna    TEXT,
    enviar_correo   INTEGER NOT NULL DEFAULT 1 CHECK (enviar_correo IN (0, 1)),
    imprimir_ticket INTEGER NOT NULL DEFAULT 0
        CHECK (imprimir_ticket IN (0, 1)),
    created_at      INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    updated_at      INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    deleted_at      INTEGER,
    sync_state      TEXT    NOT NULL DEFAULT 'pendiente'
        CHECK (sync_state IN ('pendiente', 'sincronizado'))
);

CREATE INDEX facturas_cliente_idx ON facturas (cliente_id, fecha_emision);
CREATE INDEX facturas_estado_idx ON facturas (estado);
CREATE UNIQUE INDEX facturas_numero_idx
    ON facturas (numero_factura)
    WHERE numero_factura IS NOT NULL AND deleted_at IS NULL;

CREATE TABLE detalles_factura (
    id              INTEGER PRIMARY KEY,
    uuid            TEXT    NOT NULL UNIQUE,
    factura_id      INTEGER NOT NULL REFERENCES facturas (id) ON DELETE CASCADE,
    producto_id     INTEGER REFERENCES productos_insumos (id) ON DELETE SET NULL,
    consulta_id     INTEGER REFERENCES consultas_medicas (id) ON DELETE SET NULL,
    servicio_id     INTEGER REFERENCES servicios (id) ON DELETE SET NULL,
    codigo          TEXT,
    nombre          TEXT,
    tipo            TEXT CHECK (tipo IS NULL OR tipo IN ('Servicio', 'Producto')),
    cantidad        INTEGER NOT NULL DEFAULT 1 CHECK (cantidad > 0),
    precio_unitario NUMERIC(10, 2) NOT NULL CHECK (precio_unitario >= 0),
    created_at      INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    updated_at      INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    deleted_at      INTEGER,
    sync_state      TEXT    NOT NULL DEFAULT 'pendiente'
        CHECK (sync_state IN ('pendiente', 'sincronizado'))
);

CREATE INDEX detalles_factura_factura_idx ON detalles_factura (factura_id);
CREATE INDEX detalles_factura_servicio_idx ON detalles_factura (servicio_id);

CREATE TABLE cierres_caja (
    id                  INTEGER PRIMARY KEY,
    uuid                TEXT    NOT NULL UNIQUE,
    usuario_cierre_id   INTEGER REFERENCES usuarios (id) ON DELETE SET NULL,
    fecha_cierre        INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    ingresos_calculados NUMERIC(10, 2) NOT NULL DEFAULT 0,
    ingresos_reales     NUMERIC(10, 2) NOT NULL DEFAULT 0,
    desfase             NUMERIC(10, 2) NOT NULL DEFAULT 0,
    observaciones       TEXT,
    created_at          INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    updated_at          INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    deleted_at          INTEGER,
    sync_state          TEXT    NOT NULL DEFAULT 'pendiente'
        CHECK (sync_state IN ('pendiente', 'sincronizado'))
);

CREATE INDEX cierres_caja_fecha_idx ON cierres_caja (fecha_cierre);

CREATE TABLE cargos (
    id          INTEGER PRIMARY KEY,
    uuid        TEXT    NOT NULL UNIQUE,
    origen_tipo TEXT    NOT NULL
        CHECK (origen_tipo IN ('Consulta', 'Receta', 'Laboratorio', 'Cirugia',
                               'Hospitalizacion', 'Producto', 'Servicio', 'Otro')),
    origen_id   INTEGER NOT NULL,
    origen_uuid TEXT,
    concepto    TEXT    NOT NULL,
    monto       NUMERIC(10, 2) NOT NULL DEFAULT 0 CHECK (monto >= 0),
    estado      TEXT    NOT NULL DEFAULT 'Pendiente'
        CHECK (estado IN ('Pendiente', 'Facturado', 'Pagado', 'Anulado')),
    factura_id  INTEGER REFERENCES facturas (id) ON DELETE SET NULL,
    created_at  INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    updated_at  INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    deleted_at  INTEGER,
    sync_state  TEXT    NOT NULL DEFAULT 'pendiente'
        CHECK (sync_state IN ('pendiente', 'sincronizado'))
);

CREATE INDEX cargos_estado_idx ON cargos (estado);
CREATE INDEX cargos_factura_idx ON cargos (factura_id);
CREATE UNIQUE INDEX cargos_origen_idx
    ON cargos (origen_tipo, origen_id)
    WHERE deleted_at IS NULL;

-- ── Registro de portales (solo nodo) ─────────────────────────
-- D27: aquí nunca se guarda un token. El token de cada portal vive en el
-- keyring del sistema con la clave `portal:<uuid>`.
CREATE TABLE portales (
    id            INTEGER PRIMARY KEY,
    uuid          TEXT    NOT NULL UNIQUE,
    cuenta_id     INTEGER NOT NULL UNIQUE REFERENCES cuentas (id) ON DELETE CASCADE,
    cliente_id    INTEGER NOT NULL REFERENCES clientes (id) ON DELETE CASCADE,
    url           TEXT    NOT NULL,
    estado        TEXT    NOT NULL DEFAULT 'activo'
        CHECK (estado IN ('activo', 'revocado', 'borrado')),
    pareado_at    INTEGER,
    ultima_sync_at INTEGER,
    created_at    INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    updated_at    INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    deleted_at    INTEGER
);

CREATE INDEX portales_cliente_idx ON portales (cliente_id);

-- ── Auditoría ────────────────────────────────────────────────
-- Append-only: no se actualiza ni se borra. Solo created_at.
CREATE TABLE auditoria (
    id           INTEGER PRIMARY KEY,
    uuid         TEXT    NOT NULL UNIQUE,
    usuario_id   INTEGER REFERENCES usuarios (id) ON DELETE SET NULL,
    accion       TEXT    NOT NULL,
    entidad      TEXT    NOT NULL,
    entidad_uuid TEXT,
    detalle      TEXT,
    created_at   INTEGER NOT NULL DEFAULT (strftime('%s', 'now') * 1000),
    sync_state   TEXT    NOT NULL DEFAULT 'pendiente'
        CHECK (sync_state IN ('pendiente', 'sincronizado'))
);

CREATE INDEX auditoria_entidad_idx ON auditoria (entidad, entidad_uuid);
CREATE INDEX auditoria_fecha_idx ON auditoria (created_at);
