-- 0002 — Datos de la sede que la app necesita antes de emparejar.
--
-- El directorio sigue siendo público y comercial: no añade identidades,
-- correos de clientes ni tokens de clínica (D22/D27).

ALTER TABLE clinicas_publicas ADD COLUMN zona_horaria TEXT NOT NULL
    DEFAULT 'America/Bogota';
