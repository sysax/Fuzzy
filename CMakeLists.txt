cmake_minimum_required(VERSION 3.24)

project(FuzzyCloud VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Test Sql Network)

qt_standard_project_setup(REQUIRES 6.11)

include(CTest)

add_subdirectory(fuzzy_core)

option(FUZZY_BUILD_DESKTOP "Compila el esqueleto del escritorio" ON)
if(FUZZY_BUILD_DESKTOP)
    find_package(Qt6 6.11 REQUIRED COMPONENTS Quick QuickControls2)
    add_subdirectory(fuzzy_desktop)
endif()
