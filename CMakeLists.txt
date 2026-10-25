cmake_minimum_required(VERSION 3.18)

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

# 2. Detectar un conjunto completo y coherente de cabeceras Latino-Core.
# Si el usuario proporciona -DLATINO_INCLUDE_DIR, respetar esa selección,
# pero comprobar que no falten las dependencias incluidas por latino.h.
set(_LATINO_HEADERS latino.h latclass.h latast.h latmv.h latobj.h)
set(_LATINO_CANDIDATES
    "${LATINO_INCLUDE_DIR}"
    "$ENV{HOME}/latino-core/include"
    "/usr/local/include"
    "/usr/include"
)
set(_LATINO_HEADERS_FOUND FALSE)
foreach(_candidate IN LISTS _LATINO_CANDIDATES)
    if(NOT IS_DIRECTORY "${_candidate}")
        continue()
    endif()
    set(_complete TRUE)
    foreach(_header IN LISTS _LATINO_HEADERS)
        if(NOT EXISTS "${_candidate}/${_header}")
            set(_complete FALSE)
            break()
        endif()
    endforeach()
    if(_complete)
        set(LATINO_INCLUDE_DIR "${_candidate}" CACHE PATH
            "Directorio con cabeceras completas de Latino-Core" FORCE)
        set(_LATINO_HEADERS_FOUND TRUE)
        break()
    endif()
endforeach()
if(NOT _LATINO_HEADERS_FOUND)
    message(FATAL_ERROR
        "No se encontraron las cabeceras completas de Latino-Core. "
        "Configura -DLATINO_INCLUDE_DIR=/ruta/latino-core/include")
endif()
message(STATUS "Latino-Core headers: ${LATINO_INCLUDE_DIR}")

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

# Motor geométrico independiente
add_library(latino_ui_layout STATIC
    src/ui/ui_layout.c
)

target_include_directories(latino_ui_layout
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(motor
    PRIVATE
        latino_ui_layout
)

# Pruebas automatizadas
include(CTest)

if(BUILD_TESTING)
    add_executable(test_ui_layout
        tests/test_ui_layout.c
    )

    target_link_libraries(test_ui_layout
        PRIVATE
            latino_ui_layout
            m
    )

    add_test(
        NAME latino_ui_layout_basico
        COMMAND test_ui_layout
    )
endif()

# Demostración gráfica del motor de maquetación
add_executable(demo_ui_layout
    tests/demo_ui_layout.c
)

target_link_libraries(demo_ui_layout
    PRIVATE
        latino_ui_layout
        raylib
        m
)