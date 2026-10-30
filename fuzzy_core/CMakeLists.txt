add_library(fuzzy_core
    src/Version.cpp
    src/Uuid.cpp
    src/SqlScript.cpp
    src/MigrationRunner.cpp
    src/SqliteExecutor.cpp
    src/TursoHttpExecutor.cpp
    src/Politicas.cpp
    src/PortalBaseStore.cpp
    src/PortalTokenService.cpp
    src/PortalProvisioner.cpp
    src/PortalProjector.cpp
    src/BuzonService.cpp
    src/SolicitudCitaService.cpp
    src/SyncEntities.cpp
    src/SyncOutbox.cpp
    src/SyncEngine.cpp
    src/FileStore.cpp
    src/MascotaFotoService.cpp
    src/GaleriaClinicaService.cpp
    src/PasswordHasher.cpp
    src/AuthService.cpp
)
add_library(fuzzy::core ALIAS fuzzy_core)

# Migraciones del portal incrustadas: el runner las aplica al crear la base
# del dueño y en la migración perezosa (D14, fase 5B).
qt_add_resources(fuzzy_core "migraciones"
    PREFIX "/fuzzy/migrations"
    BASE "${CMAKE_SOURCE_DIR}/db/migrations"
    FILES
        "${CMAKE_SOURCE_DIR}/db/migrations/portal/0001_init.sql"
        "${CMAKE_SOURCE_DIR}/db/migrations/portal/0002_portal.sql"
        "${CMAKE_SOURCE_DIR}/db/migrations/clinic/0001_init.sql"
        "${CMAKE_SOURCE_DIR}/db/migrations/clinic/0002_dominio.sql"
        "${CMAKE_SOURCE_DIR}/db/migrations/clinic/0003_portal_token.sql"
        "${CMAKE_SOURCE_DIR}/db/migrations/clinic/0004_sync.sql"
        "${CMAKE_SOURCE_DIR}/db/migrations/clinic/0005_imagenes.sql"
)

target_include_directories(fuzzy_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(fuzzy_core PUBLIC
    Qt6::Core
    Qt6::Sql
    Qt6::Network
)

# Gui solo se usa dentro de fuzzy_core para decodificar y codificar imágenes.
# PRIVATE evita que los consumidores (y sus tests) hereden QT_GUI_LIB y
# terminen creando un QGuiApplication que necesita display.
target_link_libraries(fuzzy_core PRIVATE
    Qt6::Gui
)

target_compile_features(fuzzy_core PUBLIC cxx_std_20)

target_compile_definitions(fuzzy_core PUBLIC
    FUZZY_VERSION="${PROJECT_VERSION}"
)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
