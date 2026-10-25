#include "ui_arbol.h"

#include <stdio.h>

#define VERIFICAR(condicion)                         \
    do {                                             \
        if (!(condicion)) {                          \
            fprintf(stderr,                         \
                    "ERROR: %s (linea %d)\n",        \
                    #condicion, __LINE__);           \
            return 1;                                \
        }                                            \
    } while (0)

int main(void)
{
    UINodo nodos[8] = {0};
    UIArbol arbol = {0};

    size_t raiz, fila, guardar, cancelar;
    size_t adicional;

    VERIFICAR(
        ui_arbol_inicializar(&arbol, nodos, 8)
        == UI_ARBOL_OK
    );

    VERIFICAR(
        ui_arbol_agregar(
            &arbol, UI_NODO_NULO,
            UI_NODO_COLUMNA, -1, 12.0f, &raiz
        ) == UI_ARBOL_OK
    );

    VERIFICAR(
        ui_arbol_agregar(
            &arbol, raiz,
            UI_NODO_FILA, -1, 8.0f, &fila
        ) == UI_ARBOL_OK
    );

    VERIFICAR(
        ui_arbol_agregar(
            &arbol, fila,
            UI_NODO_WIDGET, 50, 0.0f, &guardar
        ) == UI_ARBOL_OK
    );

    VERIFICAR(
        ui_arbol_agregar(
            &arbol, fila,
            UI_NODO_WIDGET, 51, 0.0f, &cancelar
        ) == UI_ARBOL_OK
    );

    VERIFICAR(arbol.cantidad == 4);
    VERIFICAR(raiz == 0);
    VERIFICAR(nodos[fila].padre == raiz);
    VERIFICAR(nodos[fila].primer_hijo == guardar);
    VERIFICAR(nodos[guardar].siguiente_hermano == cancelar);
    VERIFICAR(nodos[cancelar].padre == fila);
    VERIFICAR(nodos[cancelar].id_widget == 51);

    /* Un widget no puede contener otro widget. */
    VERIFICAR(
        ui_arbol_agregar(
            &arbol, guardar,
            UI_NODO_WIDGET, 52, 0.0f, &adicional
        ) == UI_ARBOL_PADRE_INVALIDO
    );

    /* No permitimos insertar nodos con padres inexistentes. */
    VERIFICAR(
        ui_arbol_agregar(
            &arbol, 99,
            UI_NODO_WIDGET, 53, 0.0f, &adicional
        ) == UI_ARBOL_PADRE_INVALIDO
    );

    puts("OK: arbol jerarquico basico");
    return 0;
}
