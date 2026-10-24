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
