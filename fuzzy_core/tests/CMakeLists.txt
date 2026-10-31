qt_add_executable(tst_version
    tst_version.cpp
)

target_link_libraries(tst_version PRIVATE
    fuzzy::core
    Qt6::Test
)

add_test(NAME tst_version COMMAND tst_version)

qt_add_executable(tst_sqlite_executor
    tst_sqlite_executor.cpp
)

target_link_libraries(tst_sqlite_executor PRIVATE
    fuzzy::core
    Qt6::Sql
    Qt6::Test
)

add_test(NAME tst_sqlite_executor COMMAND tst_sqlite_executor)

qt_add_executable(tst_politicas
    tst_politicas.cpp
)

target_link_libraries(tst_politicas PRIVATE
    fuzzy::core
    Qt6::Test
)

add_test(NAME tst_politicas COMMAND tst_politicas)

qt_add_executable(tst_portal_token_service
    tst_portal_token_service.cpp
)

target_link_libraries(tst_portal_token_service PRIVATE
    fuzzy::core
    Qt6::Sql
    Qt6::Test
)

add_test(NAME tst_portal_token_service COMMAND tst_portal_token_service)

qt_add_executable(tst_buzon_service
    tst_buzon_service.cpp
)

target_link_libraries(tst_buzon_service PRIVATE
    fuzzy::core
    Qt6::Sql
    Qt6::Test
)

add_test(NAME tst_buzon_service COMMAND tst_buzon_service)

qt_add_executable(tst_turso_http_executor
    tst_turso_http_executor.cpp
)

target_link_libraries(tst_turso_http_executor PRIVATE
    fuzzy::core
    Qt6::Network
    Qt6::Test
)

add_test(NAME tst_turso_http_executor COMMAND tst_turso_http_executor)

# Suites que aplican el esquema real desde db/migrations.
function(fuzzy_esquema_test nombre)
    qt_add_executable(${nombre} ${nombre}.cpp)
    target_link_libraries(${nombre} PRIVATE
        fuzzy::core
        Qt6::Sql
        Qt6::Test
    )
    target_compile_definitions(${nombre} PRIVATE
        FUZZY_MIGRATIONS_DIR="${CMAKE_SOURCE_DIR}/db/migrations"
    )
    add_test(NAME ${nombre} COMMAND ${nombre})
endfunction()

fuzzy_esquema_test(tst_migration_runner)
fuzzy_esquema_test(tst_portal_provisioner)
fuzzy_esquema_test(tst_portal_projector)
fuzzy_esquema_test(tst_portal_contract)
fuzzy_esquema_test(tst_buzon_recoleccion)
fuzzy_esquema_test(tst_solicitud_cita)
fuzzy_esquema_test(tst_sync_engine)

# Fase 6: imágenes. FileStore no necesita esquema; las otras dos sí.
qt_add_executable(tst_file_store tst_file_store.cpp)
target_link_libraries(tst_file_store PRIVATE fuzzy::core Qt6::Test)
add_test(NAME tst_file_store COMMAND tst_file_store)

fuzzy_esquema_test(tst_mascota_foto)
fuzzy_esquema_test(tst_adjuntos_clinicos)

# Los tests de imagen usan QImage, pero no necesitan plataforma gráfica: su
# main es QTEST_GUILESS_MAIN (QCoreApplication), así corren en CI sin display.
target_link_libraries(tst_mascota_foto PRIVATE Qt6::Gui)
target_link_libraries(tst_adjuntos_clinicos PRIVATE Qt6::Gui)

# Fase 8: cifrado de la caché del teléfono. Los vectores vienen de AES-256-GCM.
qt_add_executable(tst_cryptobox tst_cryptobox.cpp)
target_link_libraries(tst_cryptobox PRIVATE fuzzy::core Qt6::Test)
add_test(NAME tst_cryptobox COMMAND tst_cryptobox)

# Fase 8: flujo móvil completo sobre SQLite real (caché, buzón, portal, emparejamiento).
qt_add_executable(tst_movil_servicios tst_movil_servicios.cpp)
target_link_libraries(tst_movil_servicios PRIVATE fuzzy::core Qt6::Sql Qt6::Test)
add_test(NAME tst_movil_servicios COMMAND tst_movil_servicios)
