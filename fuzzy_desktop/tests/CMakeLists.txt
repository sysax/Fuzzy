# Pruebas de los controladores de escritorio. Enlazan la lógica de presentación
# sin QML ni plataforma gráfica: son las que vigilan las reglas de negocio.
qt_add_executable(tst_controladores
    tst_controladores.cpp
)

target_link_libraries(tst_controladores PRIVATE
    fuzzy::desktop_controllers
    fuzzy::core
    Qt6::Sql
    Qt6::Test
)

add_test(NAME tst_controladores COMMAND tst_controladores)

# Arnés visual: carga el mismo Main.qml que la aplicación contra una base
# SQLite temporal sembrada con datos reales. Cada captura es una baseline.
qt_add_executable(tst_visual
    visual/tst_visual.cpp
)

fuzzy_add_desktop_qml(tst_visual)

target_link_libraries(tst_visual PRIVATE
    fuzzy::desktop_controllers
    fuzzy::core
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::Sql
    Qt6::Test
)

target_compile_definitions(tst_visual PRIVATE
    FUZZY_VISUAL_BASELINE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/visual/baselines"
)

add_test(NAME tst_visual COMMAND tst_visual)

set_tests_properties(tst_visual PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software;FUZZY_HOY=2026-03-09"
    TIMEOUT 300
)
