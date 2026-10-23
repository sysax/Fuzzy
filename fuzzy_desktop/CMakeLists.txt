qt_add_executable(fuzzy_desktop
    main.cpp
)

qt_add_qml_module(fuzzy_desktop
    URI Fuzzy
    VERSION 0.1
    QML_FILES
        Main.qml
)

target_link_libraries(fuzzy_desktop PRIVATE
    fuzzy::core
    Qt6::Quick
    Qt6::QuickControls2
)

set_target_properties(fuzzy_desktop PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)
