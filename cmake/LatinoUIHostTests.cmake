if(BUILD_TESTING)
    # Compilar el mismo host con main renombrado y una ventana simulada.
    # El ejecutable host_ui mantiene su enlace con Raylib real.
    add_library(host_ui_para_pruebas OBJECT src/ui/host_ui.c)
    target_compile_definitions(host_ui_para_pruebas PRIVATE main=latino_ui_host_main)
    target_include_directories(host_ui_para_pruebas PRIVATE
        "${PROJECT_SOURCE_DIR}/tests/soporte_host")
    target_link_libraries(host_ui_para_pruebas PRIVATE
        latino_ui_runtime latino_ui_callbacks latino_ui_despachador)

    add_executable(test_ui_host tests/test_ui_host.c)
    target_include_directories(test_ui_host PRIVATE
        "${PROJECT_SOURCE_DIR}/tests/soporte_host")
    target_link_libraries(test_ui_host PRIVATE
        host_ui_para_pruebas
        latino_ui_runtime latino_ui_callbacks latino_ui_despachador m)
    if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(host_ui_para_pruebas PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_options(test_ui_host PRIVATE -Wall -Wextra -Wpedantic)
    endif()
    add_test(NAME latino_ui_host_ciclo_vida COMMAND test_ui_host
        "${PROJECT_SOURCE_DIR}/tests/datos/host_ui_valido.lat"
        "${PROJECT_SOURCE_DIR}/tests/datos/host_ui_invalido.lat")
    set_tests_properties(latino_ui_host_ciclo_vida PROPERTIES TIMEOUT 30)
endif()

if(BUILD_TESTING)
    add_executable(test_ui_texto tests/test_ui_texto.c)
    target_link_libraries(test_ui_texto PRIVATE latino_ui_runtime latino_ui_despachador m)
    add_test(NAME latino_ui_texto_utf8 COMMAND test_ui_texto)
    add_test(NAME latino_ui_formulario_nombre COMMAND test_ui_host --formulario
        "${PROJECT_SOURCE_DIR}/scripts/formulario_nombre.lat")
    set_tests_properties(latino_ui_texto_utf8 latino_ui_formulario_nombre PROPERTIES TIMEOUT 30)
endif()

if(BUILD_TESTING AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_test(NAME latino_ui_persistencia_nombre COMMAND test_ui_host --persistencia
        "${PROJECT_SOURCE_DIR}/scripts/formulario_nombre_persistente.lat")
    set_tests_properties(latino_ui_persistencia_nombre PROPERTIES TIMEOUT 30)
endif()
