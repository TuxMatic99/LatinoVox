#include "ui_arbol.h"

#include <math.h>

UIArbolResultado ui_arbol_inicializar(
    UIArbol *arbol,
    UINodo *almacenamiento,
    size_t capacidad
)
{
    if (!arbol || !almacenamiento || capacidad == 0) {
        return UI_ARBOL_ARGUMENTO_INVALIDO;
    }

    arbol->nodos = almacenamiento;
    arbol->capacidad = capacidad;
    arbol->cantidad = 0;

    return UI_ARBOL_OK;
}

UIArbolResultado ui_arbol_agregar(
    UIArbol *arbol,
    size_t padre,
    UITipoNodo tipo,
    int id_widget,
    float separacion,
    size_t *indice_nuevo
)
{
    if (!arbol || !arbol->nodos || !indice_nuevo ||
        arbol->capacidad == 0 ||
        arbol->cantidad > arbol->capacidad ||
        !isfinite(separacion) || separacion < 0.0f ||
        (tipo != UI_NODO_FILA &&
         tipo != UI_NODO_COLUMNA &&
         tipo != UI_NODO_WIDGET)) {
        return UI_ARBOL_ARGUMENTO_INVALIDO;
    }

    if (arbol->cantidad >= arbol->capacidad) {
        return UI_ARBOL_CAPACIDAD_AGOTADA;
    }

    /* El primer nodo debe ser un contenedor raíz. */
    if (arbol->cantidad == 0) {
        if (padre != UI_NODO_NULO ||
            tipo == UI_NODO_WIDGET) {
            return UI_ARBOL_PADRE_INVALIDO;
        }
    } else {
        if (padre >= arbol->cantidad ||
            arbol->nodos[padre].tipo == UI_NODO_WIDGET) {
            return UI_ARBOL_PADRE_INVALIDO;
        }
    }

    size_t indice = arbol->cantidad;

    UINodo *nodo = &arbol->nodos[indice];

    *nodo = (UINodo){
        .tipo = tipo,
        .padre = padre,
        .primer_hijo = UI_NODO_NULO,
        .ultimo_hijo = UI_NODO_NULO,
        .siguiente_hermano = UI_NODO_NULO,
        .id_widget = id_widget,
        .separacion = separacion
    };

    if (padre != UI_NODO_NULO) {
        UINodo *contenedor = &arbol->nodos[padre];

        if (contenedor->primer_hijo == UI_NODO_NULO) {
            contenedor->primer_hijo = indice;
        } else {
            arbol->nodos[contenedor->ultimo_hijo]
                .siguiente_hermano = indice;
        }

        contenedor->ultimo_hijo = indice;
    }

    arbol->cantidad++;
    *indice_nuevo = indice;

    return UI_ARBOL_OK;
}
