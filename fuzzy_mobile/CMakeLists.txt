# Ruta fija del módulo: también se usa desde tests/, donde
# CMAKE_CURRENT_SOURCE_DIR apunta al subdirectorio.
set(FUZZY_MOBILE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

add_library(fuzzy_mobile_controllers STATIC
    app/MobileController.cpp
    app/MobileController.h
    style/Style.cpp
    style/Style.h
)
add_library(fuzzy::mobile_controllers ALIAS fuzzy_mobile_controllers)

target_include_directories(fuzzy_mobile_controllers PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(fuzzy_mobile_controllers PUBLIC
    fuzzy::core
    Qt6::Core
    Qt6::Sql
    Qt6::Network
)

file(GLOB FUZZY_MOBILE_QML_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/qml/*.qml"
)

# Registra el módulo QML, la fuente y el directorio público en un ejecutable.
function(fuzzy_add_mobile_qml target)
    foreach(qml_file IN LISTS FUZZY_MOBILE_QML_FILES)
        get_filename_component(qml_name "${qml_file}" NAME)
        set_source_files_properties("${qml_file}" PROPERTIES
            QT_RESOURCE_ALIAS "qml/${qml_name}"
        )
    endforeach()

    qt_add_qml_module(${target}
        URI FuzzyMovil
        VERSION 0.1
        QML_FILES
            ${FUZZY_MOBILE_QML_FILES}
    )

    qt_add_resources(${target} "fuzzy_mobile_fonts"
        PREFIX "/fonts"
        BASE "${FUZZY_MOBILE_DIR}/assets/fonts"
        FILES "${FUZZY_MOBILE_DIR}/assets/fonts/DejaVuSans.ttf"
    )

    # El directorio es público por diseño (D22): solo datos comerciales y el
    # token de inserción del buzón. Ningún token de portal vive aquí.
    qt_add_resources(${target} "fuzzy_mobile_directory"
        PREFIX "/fuzzy/directory"
        BASE "${CMAKE_SOURCE_DIR}/directory"
        FILES "${CMAKE_SOURCE_DIR}/directory/clinicas.json"
    )
endfunction()

qt_add_executable(fuzzy_mobile
    main.cpp
)

fuzzy_add_mobile_qml(fuzzy_mobile)

target_link_libraries(fuzzy_mobile PRIVATE
    fuzzy::mobile_controllers
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Sql
)

set_target_properties(fuzzy_mobile PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
