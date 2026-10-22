cmake_minimum_required(VERSION 3.16)

project(LatinoVox
    VERSION 0.1.0
    LANGUAGES C
)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 1. Buscar Raylib en el sistema
find_package(raylib CONFIG REQUIRED)

# 2. Apuntar directamente a la ruta de cabeceras fuentes de latino-core
set(LATINO_INCLUDE_DIR "/home/tuxmatic/latino-core/include")

find_library(LATINO_LIBRARY
    NAMES latino liblatino
    PATHS /usr/local/lib
    REQUIRED
)

add_executable(motor
    src/main.c
)

target_include_directories(motor
    PRIVATE
    ${LATINO_INCLUDE_DIR}
)

target_link_libraries(motor
    PRIVATE
    raylib
    ${LATINO_LIBRARY}
    m
)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(motor PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
endif()