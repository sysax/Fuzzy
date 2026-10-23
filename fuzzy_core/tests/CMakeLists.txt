qt_add_executable(tst_version
    tst_version.cpp
)

target_link_libraries(tst_version PRIVATE
    fuzzy::core
    Qt6::Test
)

add_test(NAME tst_version COMMAND tst_version)
