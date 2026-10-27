#include "ui_layout_arbol.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static int rect_valido(UIRect r) {
    return isfinite(r.x) && isfinite(r.y) &&
           isfinite(r.ancho) && isfinite(r.alto) &&
           r.ancho >= 0.0f && r.alto >= 0.0f;
}

/* Recorre la estructura de arriba abajo. La profundidad siempre está
   limitada por el número de nodos, incluso ante referencias corruptas. */
static UILayoutArbolResultado distribuir_nodo(
    const UIArbol *a, size_t id, UIRect *temp,
    unsigned char *visitado, size_t profundidad)
{
    if (id >= a->cantidad || profundidad >= a->cantidad || visitado[id])
        return UI_LAYOUT_ARBOL_ESTRUCTURA_INVALIDA;
    visitado[id] = 1;
    const UINodo *nodo = &a->nodos[id];
    if (!rect_valido(temp[id])) return UI_LAYOUT_ARBOL_ARGUMENTO_INVALIDO;
    if (nodo->tipo == UI_NODO_WIDGET) {
        return nodo->primer_hijo == UI_NODO_NULO &&
               nodo->ultimo_hijo == UI_NODO_NULO
            ? UI_LAYOUT_ARBOL_OK : UI_LAYOUT_ARBOL_ESTRUCTURA_INVALIDA;
    }
    if (nodo->tipo != UI_NODO_FILA && nodo->tipo != UI_NODO_COLUMNA)
        return UI_LAYOUT_ARBOL_ESTRUCTURA_INVALIDA;
    if (!isfinite(nodo->separacion) || nodo->separacion < 0.0f)
        return UI_LAYOUT_ARBOL_ARGUMENTO_INVALIDO;

    size_t cantidad = 0, h = nodo->primer_hijo;
    while (h != UI_NODO_NULO) {
        if (h >= a->cantidad || ++cantidad > a->cantidad ||
            a->nodos[h].padre != id)
            return UI_LAYOUT_ARBOL_ESTRUCTURA_INVALIDA;
        h = a->nodos[h].siguiente_hermano;
    }
    if ((cantidad == 0) != (nodo->ultimo_hijo == UI_NODO_NULO))
        return UI_LAYOUT_ARBOL_ESTRUCTURA_INVALIDA;
    if (cantidad == 0) return UI_LAYOUT_ARBOL_OK;

    UIElemento *elementos = calloc(cantidad, sizeof(*elementos));
    UIRect *rects = calloc(cantidad, sizeof(*rects));
    size_t *ids = calloc(cantidad, sizeof(*ids));
    if (!elementos || !rects || !ids) {
        free(elementos); free(rects); free(ids);
        return UI_LAYOUT_ARBOL_SIN_MEMORIA;
    }
    h = nodo->primer_hijo;
    for (size_t j = 0; j < cantidad; ++j) {
        ids[j] = h;
        elementos[j] = (UIElemento){0.0f, 1.0f};
        h = a->nodos[h].siguiente_hermano;
    }
    if (ids[cantidad-1] != nodo->ultimo_hijo) {
        free(elementos); free(rects); free(ids);
        return UI_LAYOUT_ARBOL_ESTRUCTURA_INVALIDA;
    }
    UIResultado r = ui_distribuir(temp[id],
        nodo->tipo == UI_NODO_FILA ? UI_FILA : UI_COLUMNA,
        nodo->separacion, 0.0f, elementos, cantidad, rects);
    free(elementos);
    if (r != UI_LAYOUT_OK) {
        free(rects); free(ids);
        return r == UI_LAYOUT_SIN_ESPACIO ? UI_LAYOUT_ARBOL_SIN_ESPACIO
                                          : UI_LAYOUT_ARBOL_ARGUMENTO_INVALIDO;
    }
    UILayoutArbolResultado resultado = UI_LAYOUT_ARBOL_OK;
    for (size_t j = 0; j < cantidad; ++j) {
        if (!rect_valido(rects[j])) {
            resultado = UI_LAYOUT_ARBOL_ARGUMENTO_INVALIDO;
            break;
        }
        temp[ids[j]] = rects[j];
        resultado = distribuir_nodo(a, ids[j], temp, visitado, profundidad + 1);
        if (resultado != UI_LAYOUT_ARBOL_OK) break;
    }
    free(rects); free(ids);
    return resultado;
}

UILayoutArbolResultado ui_layout_arbol_calcular(
    const UIArbol *arbol, UIRect raiz, UIRect *salida, size_t capacidad_salida)
{
    if (!arbol || !arbol->nodos || !salida || !rect_valido(raiz) ||
        arbol->cantidad == 0 || arbol->cantidad > arbol->capacidad ||
        capacidad_salida < arbol->cantidad ||
        arbol->nodos[0].padre != UI_NODO_NULO ||
        arbol->nodos[0].tipo == UI_NODO_WIDGET)
        return UI_LAYOUT_ARBOL_ARGUMENTO_INVALIDO;
    size_t n = arbol->cantidad;
    UIRect *temp = calloc(n, sizeof(*temp));
    unsigned char *visitado = calloc(n, sizeof(*visitado));
    if (!temp || !visitado) {
        free(temp); free(visitado);
        return UI_LAYOUT_ARBOL_SIN_MEMORIA;
    }
    temp[0] = raiz;
    UILayoutArbolResultado resultado = distribuir_nodo(arbol, 0, temp, visitado, 0);
    if (resultado == UI_LAYOUT_ARBOL_OK) {
        for (size_t i = 0; i < n; ++i) {
            if (!visitado[i]) {
                resultado = UI_LAYOUT_ARBOL_ESTRUCTURA_INVALIDA;
                break;
            }
        }
    }
    if (resultado == UI_LAYOUT_ARBOL_OK)
        memcpy(salida, temp, n * sizeof(*salida));
    free(temp); free(visitado);
    return resultado;
}
