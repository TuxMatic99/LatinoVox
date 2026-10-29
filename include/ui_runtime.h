#ifndef UI_RUNTIME_H
#define UI_RUNTIME_H
#include "ui_layout_arbol.h"
#include "ui_eventos.h"
#include <stdbool.h>
#include <stddef.h>
#define LUI_MAX_NODOS 128
#define LUI_TEXTO_MAX 256

typedef enum { UI_WIDGET_ETIQUETA, UI_WIDGET_BOTON, UI_WIDGET_CAMPO_TEXTO } UITipoWidget;
typedef enum { UI_TECLA_TAB, UI_TECLA_RETROTAB, UI_TECLA_ENTER,
               UI_TECLA_ESCAPE, UI_TECLA_BORRAR } UITecla;
typedef struct {
    UIArbol arbol;
    UINodo nodos[LUI_MAX_NODOS];
    UIRect rectangulos[LUI_MAX_NODOS];
    char textos[LUI_MAX_NODOS][LUI_TEXTO_MAX];
    UITipoWidget tipos[LUI_MAX_NODOS];
    bool habilitados[LUI_MAX_NODOS];
    size_t foco;
    char respaldo[LUI_TEXTO_MAX];
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
bool lui_agregar_control(UIRuntime *ui, int id, UITipoWidget tipo, const char *texto);
bool lui_enfocar(UIRuntime *ui, int id);
bool lui_habilitar(UIRuntime *ui, int id, bool habilitado);
bool lui_habilitado(const UIRuntime *ui, int id);
const char *lui_texto(const UIRuntime *ui, int id);
bool lui_establecer_texto(UIRuntime *ui, int id, const char *texto);
bool lui_caracter(UIRuntime *ui, unsigned int punto);
/* Devuelve true solo si se ha producido un evento para encolar. */
bool lui_tecla(UIRuntime *ui, UITecla tecla, UIEvento *evento);
#endif
