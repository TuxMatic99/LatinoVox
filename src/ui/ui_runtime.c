#include "ui_runtime.h"
#include "ui_utf8.h"
#include <stdio.h>
#include <string.h>

static bool campo_valido(const char *texto) {
    for (const unsigned char *p=(const unsigned char *)texto;*p;++p)
        if (*p<32 || *p==127) return false;
    return true;
}

static bool falla(UIRuntime *ui, const char *msg) {
    if (ui && !ui->error[0]) snprintf(ui->error, sizeof(ui->error), "%s", msg);
    return false;
}

bool lui_inicializar(UIRuntime *ui) {
    if (!ui) return false;
    memset(ui, 0, sizeof(*ui));
    ui->foco = UI_NODO_NULO;
    snprintf(ui->titulo, sizeof(ui->titulo), "LatinoUI");
    return ui_arbol_inicializar(&ui->arbol, ui->nodos, LUI_MAX_NODOS) == UI_ARBOL_OK;
}

bool lui_titulo(UIRuntime *ui, const char *texto) {
    if (!ui || !texto || !texto[0]) return falla(ui, "Titulo invalido");
    if (!ui_utf8_validar(texto, NULL)) return falla(ui, "Titulo UTF-8 invalido");
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

bool lui_agregar_control(UIRuntime *ui, int id, UITipoWidget tipo, const char *texto) {
    if (!ui || ui->error[0]) return false;
    if (!ui->profundidad) return falla(ui, "Widget fuera de contenedor");
    if (tipo < UI_WIDGET_ETIQUETA || tipo > UI_WIDGET_CAMPO_TEXTO)
        return falla(ui, "Tipo de widget invalido");
    if (!texto || (!texto[0] && tipo != UI_WIDGET_CAMPO_TEXTO) ||
        !ui_utf8_validar(texto, NULL) || strlen(texto) >= LUI_TEXTO_MAX ||
        (tipo == UI_WIDGET_CAMPO_TEXTO && !campo_valido(texto)))
        return falla(ui, "Texto de widget invalido");
    if (id < 0) return falla(ui, "Identificador negativo");
    for (size_t i = 0; i < ui->arbol.cantidad; i++)
        if (ui->nodos[i].tipo == UI_NODO_WIDGET && ui->nodos[i].id_widget == id)
            return falla(ui, "Identificador de widget duplicado");
    size_t indice = UI_NODO_NULO;
    if (ui_arbol_agregar(&ui->arbol, ui->pila[ui->profundidad - 1],
                        UI_NODO_WIDGET, id, 0.0f, &indice) != UI_ARBOL_OK)
        return falla(ui, "No se pudo crear widget");
    (void)ui_utf8_copiar_seguro(ui->textos[indice], LUI_TEXTO_MAX, texto);
    ui->tipos[indice] = tipo;
    ui->habilitados[indice] = true;
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

/* Compatibilidad con el runtime inicial: los consumidores nuevos declaran tipo. */
bool lui_agregar_widget(UIRuntime *ui, int id, const char *texto) {
    return lui_agregar_control(ui, id, id < 10000 ? UI_WIDGET_BOTON : UI_WIDGET_ETIQUETA, texto);
}
static size_t indice_id(const UIRuntime *ui, int id) {
    if (ui) for (size_t i=0;i<ui->arbol.cantidad;++i)
        if (ui->nodos[i].tipo==UI_NODO_WIDGET && ui->nodos[i].id_widget==id) return i;
    return UI_NODO_NULO;
}
static bool enfocable(const UIRuntime *ui, size_t i) {
    return ui && i<ui->arbol.cantidad && ui->nodos[i].tipo==UI_NODO_WIDGET &&
        ui->tipos[i]!=UI_WIDGET_ETIQUETA && ui->habilitados[i];
}
bool lui_enfocar(UIRuntime *ui, int id) {
    size_t i=indice_id(ui,id);
    if (!enfocable(ui,i)) return false;
    if (ui->foco != i) {
        ui->foco=i;
        (void)ui_utf8_copiar_seguro(ui->respaldo,sizeof ui->respaldo,ui->textos[i]);
    }
    return true;
}
const char *lui_texto(const UIRuntime *ui, int id) {
    size_t i=indice_id(ui,id);
    return i!=UI_NODO_NULO ? ui->textos[i] : NULL;
}
bool lui_establecer_texto(UIRuntime *ui, int id, const char *texto) {
    size_t i=indice_id(ui,id);
    if (i==UI_NODO_NULO || !ui_utf8_validar(texto,NULL) || strlen(texto)>=LUI_TEXTO_MAX) return false;
    if (ui->tipos[i]==UI_WIDGET_CAMPO_TEXTO && !campo_valido(texto)) return false;
    (void)ui_utf8_copiar_seguro(ui->textos[i],LUI_TEXTO_MAX,texto);
    if (ui->foco==i) (void)ui_utf8_copiar_seguro(ui->respaldo,sizeof ui->respaldo,texto);
    return true;
}
bool lui_caracter(UIRuntime *ui, unsigned int punto) {
    if (!ui || !enfocable(ui,ui->foco) || ui->tipos[ui->foco]!=UI_WIDGET_CAMPO_TEXTO ||
        punto<32 || (punto>=127 && punto<160)) return false;
    return ui_utf8_agregar(ui->textos[ui->foco],LUI_TEXTO_MAX,punto);
}
bool lui_tecla(UIRuntime *ui, UITecla tecla, UIEvento *evento) {
    if (!ui || !evento) return false;
    if (tecla==UI_TECLA_TAB || tecla==UI_TECLA_RETROTAB) {
        size_t n=ui->arbol.cantidad;
        if (!n) return false;
        size_t i=ui->foco==UI_NODO_NULO ? (tecla==UI_TECLA_TAB ? n-1 : 0) : ui->foco;
        for(size_t j=0;j<n;++j) {
            i=tecla==UI_TECLA_TAB ? (i+1)%n : (i ? i-1 : n-1);
            if (enfocable(ui,i)) { lui_enfocar(ui,ui->nodos[i].id_widget); break; }
        }
        return false;
    }
    size_t i=ui->foco;
    if (!enfocable(ui,i)) return false;
    bool campo=ui->tipos[i]==UI_WIDGET_CAMPO_TEXTO;
    if (tecla==UI_TECLA_BORRAR && campo) { ui_utf8_borrar_ultimo(ui->textos[i]); return false; }
    if (tecla==UI_TECLA_ESCAPE) {
        if (campo) (void)ui_utf8_copiar_seguro(ui->textos[i],LUI_TEXTO_MAX,ui->respaldo);
        *evento=(UIEvento){UI_EVENTO_CANCELAR,ui->nodos[i].id_widget}; return true;
    }
    if (tecla==UI_TECLA_ENTER) {
        *evento=(UIEvento){campo ? UI_EVENTO_SUBMIT : UI_EVENTO_CLIC,ui->nodos[i].id_widget}; return true;
    }
    return false;
}

bool lui_habilitar(UIRuntime *ui, int id, bool habilitado) {
    size_t i=indice_id(ui,id);
    if (i==UI_NODO_NULO) return false;
    ui->habilitados[i]=habilitado;
    if (!habilitado && ui->foco==i) ui->foco=UI_NODO_NULO;
    return true;
}
bool lui_habilitado(const UIRuntime *ui, int id) {
    size_t i=indice_id(ui,id);
    return i!=UI_NODO_NULO && ui->habilitados[i];
}
