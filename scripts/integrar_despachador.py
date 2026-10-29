from pathlib import Path

raiz = Path.cwd()
cmake = raiz / 'CMakeLists.txt'
if not cmake.exists():
    raise SystemExit('ERROR: ejecutar desde la raiz de LatinoVox')
contenido = cmake.read_text(encoding='utf-8')
marca = '# LatinoUI despachador independiente v0.1'
if marca in contenido:
    print('OK: CMake ya contiene el modulo')
    raise SystemExit(0)
fragmento = '''
# LatinoUI despachador independiente v0.1
add_library(latino_ui_despachador STATIC src/ui/ui_despachador.c)
target_include_directories(latino_ui_despachador PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(latino_ui_despachador PUBLIC latino_ui_eventos)

if(BUILD_TESTING)
    add_executable(test_ui_integracion_fifo tests/test_ui_integracion_fifo.c)
    target_link_libraries(test_ui_integracion_fifo PRIVATE latino_ui_despachador)
    add_test(NAME latino_ui_integracion_fifo COMMAND test_ui_integracion_fifo)
    set_tests_properties(latino_ui_integracion_fifo PROPERTIES TIMEOUT 10)

    add_executable(test_ui_despachador_error tests/test_ui_despachador_error.c)
    target_link_libraries(test_ui_despachador_error PRIVATE latino_ui_despachador)
    add_test(NAME latino_ui_despachador_error COMMAND test_ui_despachador_error)
    set_tests_properties(latino_ui_despachador_error PROPERTIES TIMEOUT 10)
endif()
'''
cmake.write_text(contenido.rstrip('\n') + '\n\n' + fragmento, encoding='utf-8')
print('OK: objetivos de despachador y dos pruebas CTest agregados')
