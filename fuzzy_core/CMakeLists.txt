add_library(fuzzy_core
    src/Version.cpp
    src/SqliteExecutor.cpp
    src/TursoHttpExecutor.cpp
    src/Politicas.cpp
)
add_library(fuzzy::core ALIAS fuzzy_core)

target_include_directories(fuzzy_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(fuzzy_core PUBLIC
    Qt6::Core
    Qt6::Sql
    Qt6::Network
)

target_compile_features(fuzzy_core PUBLIC cxx_std_20)

target_compile_definitions(fuzzy_core PUBLIC
    FUZZY_VERSION="${PROJECT_VERSION}"
)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
