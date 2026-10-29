#include "ui_arbol.h"

#include <math.h>
#include <stdio.h>

#define VERIFICAR(condicion) do {                        \
    if (!(condicion)) {                                 \
        fprintf(stderr, "ERROR linea %d: %s\n",         \
                __LINE__, #condicion);                  \
        return 1;                                       \
    }                                                   \
} while (0)

int main(void)
{
    UIArbol arbol = {0};
    UINodo nodos[5] = {0};

    size_t raiz, fila, columna, boton1, boton2;
    size_t extra = UI_NODO_NULO;

    /* 1. Validar argumentos de inicializacion. */
    VERIFICAR(ui_arbol_inicializar(
        NULL, nodos, 5) == UI_ARBOL_ARGUMENTO_INVALIDO);

    VERIFICAR(ui_arbol_inicializar(
        &arbol, nodos, 5) == UI_ARBOL_OK);

    /* 2. Construir cuatro niveles jerarquicos. */
    VERIFICAR(ui_arbol_agregar(
        &arbol, UI_NODO_NULO, UI_NODO_COLUMNA,
        -1, 12.0f, &raiz) == UI_ARBOL_OK);

    VERIFICAR(ui_arbol_agregar(
        &arbol, raiz, UI_NODO_FILA,
        -1, 8.0f, &fila) == UI_ARBOL_OK);

    VERIFICAR(ui_arbol_agregar(
        &arbol, fila, UI_NODO_COLUMNA,
        -1, 4.0f, &columna) == UI_ARBOL_OK);

    VERIFICAR(ui_arbol_agregar(
        &arbol, columna, UI_NODO_WIDGET,
        50, 0.0f, &boton1) == UI_ARBOL_OK);

    VERIFICAR(ui_arbol_agregar(
        &arbol, columna, UI_NODO_WIDGET,
        51, 0.0f, &boton2) == UI_ARBOL_OK);

    /* 3. Verificar relaciones y orden. */
    VERIFICAR(arbol.cantidad == 5);
    VERIFICAR(nodos[raiz].primer_hijo == fila);
    VERIFICAR(nodos[fila].primer_hijo == columna);
    VERIFICAR(nodos[columna].primer_hijo == boton1);
    VERIFICAR(nodos[columna].ultimo_hijo == boton2);
    VERIFICAR(nodos[boton1].siguiente_hermano == boton2);
    VERIFICAR(nodos[boton2].siguiente_hermano == UI_NODO_NULO);
    VERIFICAR(nodos[boton2].padre == columna);

    puts("[OK] Jerarquia de cuatro niveles y hermanos");

    /* 4. Verificar capacidad agotada. */
    VERIFICAR(ui_arbol_agregar(
        &arbol, fila, UI_NODO_WIDGET,
        52, 0.0f, &extra) == UI_ARBOL_CAPACIDAD_AGOTADA);

    VERIFICAR(arbol.cantidad == 5);
    VERIFICAR(nodos[columna].ultimo_hijo == boton2);

    puts("[OK] Capacidad agotada sin corrupcion");

    /* 5. Reinicializar y comprobar errores. */
    VERIFICAR(ui_arbol_inicializar(
        &arbol, nodos, 5) == UI_ARBOL_OK);

    VERIFICAR(arbol.cantidad == 0);

    VERIFICAR(ui_arbol_agregar(
        &arbol, UI_NODO_NULO, UI_NODO_COLUMNA,
        -1, 0.0f, &raiz) == UI_ARBOL_OK);

    VERIFICAR(ui_arbol_agregar(
        &arbol, 99, UI_NODO_WIDGET,
        10, 0.0f, &extra) == UI_ARBOL_PADRE_INVALIDO);

    VERIFICAR(ui_arbol_agregar(
        &arbol, raiz, UI_NODO_FILA,
        -1, -1.0f, &extra) == UI_ARBOL_ARGUMENTO_INVALIDO);

    VERIFICAR(ui_arbol_agregar(
        &arbol, raiz, UI_NODO_FILA,
        -1, INFINITY, &extra) == UI_ARBOL_ARGUMENTO_INVALIDO);

    VERIFICAR(arbol.cantidad == 1);
    VERIFICAR(nodos[raiz].primer_hijo == UI_NODO_NULO);

    puts("[OK] Reinicializacion y parametros invalidos");
    puts("OK: estabilidad del arbol jerarquico");

    return 0;
}
