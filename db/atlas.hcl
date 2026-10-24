# Entornos locales de Atlas.
#
# Los entornos remotos (staging y producción) se añaden en la fase 2 leyendo la
# URL y el token de variables de entorno. Aquí no se escriben credenciales.
#
# Uso, desde la raíz del repositorio:
#   atlas migrate apply --config file://db/atlas.hcl --env clinic
#   atlas migrate validate --dir file://db/migrations/clinic

env "clinic" {
  url = "sqlite://db/local/clinic.db"
  migration {
    dir = "file://db/migrations/clinic"
  }
}

env "portal" {
  url = "sqlite://db/local/portal.db"
  migration {
    dir = "file://db/migrations/portal"
  }
}

env "inbox" {
  url = "sqlite://db/local/inbox.db"
  migration {
    dir = "file://db/migrations/inbox"
  }
}

env "directory" {
  url = "sqlite://db/local/directory.db"
  migration {
    dir = "file://db/migrations/directory"
  }
}

# Entornos remotos (Fase 2). La URL y el token se inyectan por CLI
# (`--url "libsql://<db>?authToken=<token>"` con TURSO_DATABASE_TOKEN_*), de
# modo que aquí no se escriben credenciales.
# Producción se incorpora en la fase 10 (ver docs/PLAN.md §10).

env "staging-clinic" {
  url = "libsql://fuzzy-staging-clinic"
  migration {
    dir = "file://db/migrations/clinic"
  }
}

env "staging-portal" {
  url = "libsql://fuzzy-staging-portal"
  migration {
    dir = "file://db/migrations/portal"
  }
}

env "staging-inbox" {
  url = "libsql://fuzzy-staging-inbox"
  migration {
    dir = "file://db/migrations/inbox"
  }
}

env "staging-directory" {
  url = "libsql://fuzzy-staging-directory"
  migration {
    dir = "file://db/migrations/directory"
  }
}
