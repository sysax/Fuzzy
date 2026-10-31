qt_add_executable(tst_mobile
    tst_mobile.cpp
)

target_link_libraries(tst_mobile PRIVATE
    fuzzy::mobile_controllers
    fuzzy::core
    Qt6::Sql
    Qt6::Test
)

add_test(NAME tst_mobile COMMAND tst_mobile)

# Carga real del módulo QML (Main, vistas y diálogos) sin pantalla, más la
# comprobación de que el directorio embebido no lleva tokens de portal.
qt_add_executable(tst_mobile_qml
    tst_mobile_qml.cpp
)

fuzzy_add_mobile_qml(tst_mobile_qml)

target_link_libraries(tst_mobile_qml PRIVATE
    fuzzy::mobile_controllers
    fuzzy::core
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Sql
    Qt6::Test
)

add_test(NAME tst_mobile_qml COMMAND tst_mobile_qml)

set_tests_properties(tst_mobile_qml PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
    TIMEOUT 300
)
