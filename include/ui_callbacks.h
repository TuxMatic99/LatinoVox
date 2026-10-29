#ifndef LATINOVOX_UI_CALLBACKS_H
#define LATINOVOX_UI_CALLBACKS_H

#include <stdbool.h>
#include <stddef.h>
#include "latino.h"
#include "ui_eventos.h"

typedef enum {
    UI_CB_OK = 0,
    UI_CB_ARGUMENTO_INVALIDO,
    UI_CB_NO_REGISTRADO,
    UI_CB_OCUPADO,
    UI_CB_VM_NO_DISPONIBLE,
    UI_CB_ESTADO_INVALIDO,
    UI_CB_FUNCION_INEXISTENTE,
    UI_CB_FIRMA_INVALIDA,
    UI_CB_ERROR_VM
} UICBResultado;

typedef struct {
    lat_mv *mv;
    int evento_id;
    UITipoEvento evento_tipo;
    bool ejecutando;
    bool inutilizada;
    size_t llamadas;
    size_t exitos;
    size_t errores;
} UICBContexto;

/* Versión experimental: un contexto/VM activo por proceso; no thread-safe. */
UICBResultado ui_cb_inicializar(UICBContexto *ctx, lat_mv *mv);
UICBResultado ui_cb_registrar(UICBContexto *ctx);
UICBResultado ui_cb_despachar(UICBContexto *ctx, const char *nombre,
                             int evento_id);
UICBResultado ui_cb_despachar_evento(UICBContexto *ctx, const char *nombre, UIEvento evento);
void ui_cb_desregistrar(UICBContexto *ctx);

#endif
