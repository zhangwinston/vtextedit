cmake_minimum_required (VERSION 3.21)
project(VTextEdit VERSION 12.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
include(GNUInstallDirs)
# Fork keeps the Qt 5.15 / Windows 7 build of VNote working: Qt 5 remains
# selectable for the whole dependency tree. Qt 6 builds still require 6.9+.
set(QT_DEFAULT_MAJOR_VERSION 6 CACHE STRING "Qt version to use (5 or 6), defaults to 6")
if(QT_DEFAULT_MAJOR_VERSION GREATER 5)
    find_package(Qt6 6.9 REQUIRED COMPONENTS Core)
endif()
option(VTEXTEDIT_ENABLE_TYPST "Build the native Typst editor and compiler library" OFF)
option(VTEXTEDIT_BUILD_DEMOS "Build editor demo applications" ON)
option(VTEXTEDIT_BUILD_TESTS "Build editor tests" ON)

if(VTEXTEDIT_ENABLE_TYPST)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    add_subdirectory(tools/typst-backend)
endif()

add_subdirectory(libs)
add_subdirectory(src)
if(VTEXTEDIT_BUILD_DEMOS)
    add_subdirectory(demo)
    if(VTEXTEDIT_ENABLE_TYPST)
        install(TARGETS VTextEditDemo VTypstEditorDemo
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
            BUNDLE DESTINATION .
        )
    endif()
endif()
enable_testing()
if(VTEXTEDIT_BUILD_TESTS)
    add_subdirectory(tests)
endif()
