#ifndef UI_ARBOL_H
#define UI_ARBOL_H

#include <stddef.h>
#include <stdint.h>

#define UI_NODO_NULO SIZE_MAX

typedef enum {
    UI_NODO_FILA,
    UI_NODO_COLUMNA,
    UI_NODO_WIDGET
} UITipoNodo;

typedef enum {
    UI_ARBOL_OK,
    UI_ARBOL_ARGUMENTO_INVALIDO,
    UI_ARBOL_CAPACIDAD_AGOTADA,
    UI_ARBOL_PADRE_INVALIDO
} UIArbolResultado;

typedef struct {
    UITipoNodo tipo;

    size_t padre;
    size_t primer_hijo;
    size_t ultimo_hijo;
    size_t siguiente_hermano;

    int id_widget;
    float separacion;
} UINodo;

typedef struct {
    UINodo *nodos;
    size_t capacidad;
    size_t cantidad;
} UIArbol;

UIArbolResultado ui_arbol_inicializar(
    UIArbol *arbol,
    UINodo *almacenamiento,
    size_t capacidad
);

UIArbolResultado ui_arbol_agregar(
    UIArbol *arbol,
    size_t padre,
    UITipoNodo tipo,
    int id_widget,
    float separacion,
    size_t *indice_nuevo
);

#endif
