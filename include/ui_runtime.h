#ifndef UI_RUNTIME_H
#define UI_RUNTIME_H
#include "ui_layout_arbol.h"
#include <stdbool.h>
#include <stddef.h>
#define LUI_MAX_NODOS 128
#define LUI_TEXTO_MAX 256

typedef struct {
    UIArbol arbol;
    UINodo nodos[LUI_MAX_NODOS];
    UIRect rectangulos[LUI_MAX_NODOS];
    char textos[LUI_MAX_NODOS][LUI_TEXTO_MAX];
    size_t pila[LUI_MAX_NODOS];
    size_t profundidad;
    char titulo[LUI_TEXTO_MAX];
    char error[160];
} UIRuntime;

bool lui_inicializar(UIRuntime *ui);
bool lui_iniciar_contenedor(UIRuntime *ui, UITipoNodo tipo, float separacion);
bool lui_fin_contenedor(UIRuntime *ui);
bool lui_agregar_widget(UIRuntime *ui, int id, const char *texto);
bool lui_titulo(UIRuntime *ui, const char *texto);
bool lui_calcular(UIRuntime *ui, UIRect area);
bool lui_validar(UIRuntime *ui);
#endif
