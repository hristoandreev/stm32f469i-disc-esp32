#THIS FILE IS AUTO GENERATED FROM THE TEMPLATE! DO NOT CHANGE!
cmake_minimum_required(VERSION 3.17)

set(PROJECT_NAME stm32f469i-disc-esp32)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_C_STANDARD 11)

set(CMAKE_VERBOSE_MAKEFILE FALSE CACHE BOOL "" FORCE)

if (NOT TARGET)
    set(TARGET "CROSS")
endif()

if(${TARGET} STREQUAL "NATIVE")
    include(cmake/native.cmake)
elseif(${TARGET} STREQUAL "GOOGLETESTS")
    add_subdirectory(tests/Google_tests)
elseif(${TARGET} STREQUAL "CROSSTEST")
    include(cmake/crossTest.cmake)
elseif(${TARGET} STREQUAL "CROSSGPROF")
    include(cmake/crossGprof.cmake)
else()
    # project settings
    project(${PROJECT_NAME} C CXX ASM)
    include(cmake/cross.cmake)
endif()
