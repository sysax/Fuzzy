# Ruta fija del módulo: la función también se llama desde tests/, donde
# CMAKE_CURRENT_SOURCE_DIR apunta al subdirectorio.
set(FUZZY_DESKTOP_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

set(FUZZY_CONTROLLERS
    agenda/AgendaController.cpp
    ajustes/AjustesController.cpp
    clinico/ClinicoController.cpp
    common/Consulta.cpp
    facturacion/FacturacionController.cpp
    inventario/InventarioController.cpp
    pacientes/PacientesController.cpp
    panel/PanelController.cpp
    propietarios/PropietariosController.cpp
    session/SessionController.cpp
    style/Style.cpp
    vinculos/VinculosController.cpp
)

# Lógica de presentación compartida por la aplicación y las pruebas. No
# contiene QML: así los tests de controladores pueden enlazarla sin arrastrar
# una plataforma gráfica.
add_library(fuzzy_desktop_controllers STATIC
    ${FUZZY_CONTROLLERS}
)
add_library(fuzzy::desktop_controllers ALIAS fuzzy_desktop_controllers)

target_include_directories(fuzzy_desktop_controllers PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(fuzzy_desktop_controllers PUBLIC
    fuzzy::core
    Qt6::Core
    Qt6::Sql
)

file(GLOB FUZZY_QML_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/qml/*.qml"
)

file(GLOB FUZZY_ICONS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/*.svg"
)

# Registra el módulo QML y los recursos gráficos en un ejecutable. Se usa para
# la app y para el arnés visual, que carga el mismo Main.qml.
function(fuzzy_add_desktop_qml target)
    # Los archivos QML se comparten entre la app y el arnés visual. El alias
    # mantiene la ruta qrc:/qt/qml/Fuzzy/qml/... aunque el consumidor esté en
    # otro directorio de CMake.
    foreach(qml_file IN LISTS FUZZY_QML_FILES)
        get_filename_component(qml_name "${qml_file}" NAME)
        set_source_files_properties("${qml_file}" PROPERTIES
            QT_RESOURCE_ALIAS "qml/${qml_name}"
        )
    endforeach()

    qt_add_qml_module(${target}
        URI Fuzzy
        VERSION 0.1
        QML_FILES
            ${FUZZY_QML_FILES}
    )

    qt_add_resources(${target} "fuzzy_icons"
        PREFIX "/icons"
        BASE "${FUZZY_DESKTOP_DIR}/assets/icons"
        FILES ${FUZZY_ICONS}
    )

    qt_add_resources(${target} "fuzzy_fonts"
        PREFIX "/fonts"
        BASE "${FUZZY_DESKTOP_DIR}/assets/fonts"
        FILES "${FUZZY_DESKTOP_DIR}/assets/fonts/DejaVuSans.ttf"
    )
endfunction()

qt_add_executable(fuzzy_desktop
    main.cpp
)

fuzzy_add_desktop_qml(fuzzy_desktop)

target_link_libraries(fuzzy_desktop PRIVATE
    fuzzy::desktop_controllers
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Sql
)

set_target_properties(fuzzy_desktop PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
