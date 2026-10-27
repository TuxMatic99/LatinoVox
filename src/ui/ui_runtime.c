#include "ui_runtime.h"
#include <stdio.h>
#include <string.h>

static bool falla(UIRuntime *ui, const char *msg) {
    if (ui && !ui->error[0]) snprintf(ui->error, sizeof(ui->error), "%s", msg);
    return false;
}

bool lui_inicializar(UIRuntime *ui) {
    if (!ui) return false;
    memset(ui, 0, sizeof(*ui));
    snprintf(ui->titulo, sizeof(ui->titulo), "LatinoUI");
    return ui_arbol_inicializar(&ui->arbol, ui->nodos, LUI_MAX_NODOS) == UI_ARBOL_OK;
}

bool lui_titulo(UIRuntime *ui, const char *texto) {
    if (!ui || !texto || !texto[0]) return falla(ui, "Titulo invalido");
    if (strlen(texto) >= LUI_TEXTO_MAX) return falla(ui, "Titulo demasiado largo");
    snprintf(ui->titulo, sizeof(ui->titulo), "%s", texto);
    return true;
}

bool lui_iniciar_contenedor(UIRuntime *ui, UITipoNodo tipo, float separacion) {
    if (!ui || ui->error[0]) return false;
    if (tipo != UI_NODO_FILA && tipo != UI_NODO_COLUMNA)
        return falla(ui, "Tipo de contenedor invalido");
    if (ui->profundidad >= LUI_MAX_NODOS)
        return falla(ui, "Profundidad maxima alcanzada");
    if (ui->profundidad == 0 && ui->arbol.cantidad != 0)
        return falla(ui, "Solo se permite un contenedor raiz");
    size_t padre = ui->profundidad ? ui->pila[ui->profundidad - 1] : UI_NODO_NULO;
    size_t indice = UI_NODO_NULO;
    if (ui_arbol_agregar(&ui->arbol, padre, tipo, -1, separacion, &indice) != UI_ARBOL_OK)
        return falla(ui, "No se pudo crear contenedor");
    ui->pila[ui->profundidad++] = indice;
    return true;
}

bool lui_fin_contenedor(UIRuntime *ui) {
    if (!ui || ui->error[0]) return false;
    if (!ui->profundidad) return falla(ui, "No hay contenedor abierto");
    ui->profundidad--;
    return true;
}

bool lui_agregar_widget(UIRuntime *ui, int id, const char *texto) {
    if (!ui || ui->error[0]) return false;
    if (!ui->profundidad) return falla(ui, "Widget fuera de contenedor");
    if (!texto || !texto[0] || strlen(texto) >= LUI_TEXTO_MAX)
        return falla(ui, "Texto de widget invalido");
    if (id < 0) return falla(ui, "Identificador negativo");
    for (size_t i = 0; i < ui->arbol.cantidad; i++)
        if (ui->nodos[i].tipo == UI_NODO_WIDGET && ui->nodos[i].id_widget == id)
            return falla(ui, "Identificador de widget duplicado");
    size_t indice = UI_NODO_NULO;
    if (ui_arbol_agregar(&ui->arbol, ui->pila[ui->profundidad - 1],
                        UI_NODO_WIDGET, id, 0.0f, &indice) != UI_ARBOL_OK)
        return falla(ui, "No se pudo crear widget");
    snprintf(ui->textos[indice], LUI_TEXTO_MAX, "%s", texto);
    return true;
}

bool lui_validar(UIRuntime *ui) {
    if (!ui || ui->error[0]) return false;
    if (ui->profundidad) return falla(ui, "Quedaron contenedores sin cerrar");
    if (!ui->arbol.cantidad) return falla(ui, "No existe contenedor raiz");
    return true;
}

bool lui_calcular(UIRuntime *ui, UIRect area) {
    if (!ui || ui->error[0]) return false;
    UILayoutArbolResultado r = ui_layout_arbol_calcular(&ui->arbol, area,
                                                         ui->rectangulos, LUI_MAX_NODOS);
    /* No convertir falta de espacio en error permanente: redimensionar puede resolverlo. */
    return r == UI_LAYOUT_ARBOL_OK;
}
