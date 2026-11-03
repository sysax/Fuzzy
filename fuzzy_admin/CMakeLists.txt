# CLI de operador (fase 10). Solo depende de fuzzy_core y Qt Core: no abre
# ventanas ni necesita plataforma gráfica, así corre en el servidor del nodo.
qt_add_executable(fuzzy_admin
    main.cpp
)

set_target_properties(fuzzy_admin PROPERTIES
    OUTPUT_NAME fuzzy-admin
)

target_link_libraries(fuzzy_admin PRIVATE
    fuzzy::core
    Qt6::Core
)
