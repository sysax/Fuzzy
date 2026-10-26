-- 0003 — Credencial del portal: solo su huella, nunca el secreto (D27).
--
-- El token real lo emite Turso y vive en el keyring del sistema (o cifrado en
-- el teléfono, D23). La base de la clínica guarda únicamente el SHA-256 para
-- poder verificar, rotar y revocar sin custodiar la credencial.
--
-- `portales.uuid` es la identidad estable del portal: rotar el token no la
-- cambia y la base del portal conserva su nombre.

ALTER TABLE portales ADD COLUMN token_hash TEXT
    CHECK (token_hash IS NULL OR length(token_hash) = 64);

CREATE UNIQUE INDEX portales_token_hash_idx
    ON portales (token_hash)
    WHERE token_hash IS NOT NULL;
