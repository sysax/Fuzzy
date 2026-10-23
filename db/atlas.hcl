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
