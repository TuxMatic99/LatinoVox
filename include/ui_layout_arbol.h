#ifndef UI_LAYOUT_ARBOL_H
#define UI_LAYOUT_ARBOL_H
#include "ui_arbol.h"
#include "ui_layout.h"

typedef enum {
    UI_LAYOUT_ARBOL_OK = 0,
    UI_LAYOUT_ARBOL_ARGUMENTO_INVALIDO,
    UI_LAYOUT_ARBOL_SIN_ESPACIO,
    UI_LAYOUT_ARBOL_SIN_MEMORIA,
    UI_LAYOUT_ARBOL_ESTRUCTURA_INVALIDA
} UILayoutArbolResultado;

/* salida[i] corresponde a arbol->nodos[i].
   Ante cualquier error, salida permanece sin cambios.
   La versión inicial distribuye cada grupo de hermanos con peso 1. */
UILayoutArbolResultado ui_layout_arbol_calcular(
    const UIArbol *arbol, UIRect raiz, UIRect *salida, size_t capacidad_salida);
#endif
