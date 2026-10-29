#include "ui_callbacks.h"
#include <string.h>
#include "latmv.h"
#include "latobj.h"

/* Registro intencionalmente único hasta disponer de userdata en la API FFI. */
static UICBContexto *registro = NULL;

static void ffi_id_actual(lat_mv *mv)
{
    if (!registro || registro->mv != mv || !registro->ejecutando) {
        /* Nunca se debe consultar fuera de un despacho válido. */
        latC_error(mv, "ui_cb.id_actual fuera de un evento");
        return;
    }
    latC_apilar_double(mv, (double)registro->evento_id);
}

static void ffi_tipo_actual(lat_mv *mv) {
    if (!registro || registro->mv != mv || !registro->ejecutando) {
        latC_error(mv, "ui_cb.tipo_actual fuera de un evento"); return;
    }
    latC_apilar_double(mv, (double)registro->evento_tipo);
}
static const lat_CReg lib_ui_cb[] = {
    {"tipo_actual", ffi_tipo_actual, 0},
    {"id_actual", ffi_id_actual, 0},
    {NULL, NULL, 0}
};

UICBResultado ui_cb_inicializar(UICBContexto *ctx, lat_mv *mv)
{
    if (!ctx || !mv) return UI_CB_ARGUMENTO_INVALIDO;
    if (registro) return UI_CB_OCUPADO;
    memset(ctx, 0, sizeof(*ctx));
    ctx->mv = mv;
    return UI_CB_OK;
}

UICBResultado ui_cb_registrar(UICBContexto *ctx)
{
    if (!ctx || !ctx->mv) return UI_CB_ARGUMENTO_INVALIDO;
    if (registro) return UI_CB_OCUPADO;
    if (ctx->ejecutando || ctx->inutilizada) return UI_CB_ESTADO_INVALIDO;
    if (ctx->mv->ptrpila != 0 || ctx->mv->ptrctx != 0)
        return UI_CB_ESTADO_INVALIDO;
    registro = ctx;
    latC_abrir_liblatino(ctx->mv, "ui_cb", lib_ui_cb);
    return UI_CB_OK;
}

UICBResultado ui_cb_despachar_evento(UICBContexto *ctx, const char *nombre, UIEvento evento)
{
    if (!ctx || !nombre || !*nombre || !ctx->mv || evento.id_widget < 0 ||
        evento.tipo < UI_EVENTO_CLIC || evento.tipo > UI_EVENTO_CANCELAR)
        return UI_CB_ARGUMENTO_INVALIDO;
    if (registro != ctx) return UI_CB_NO_REGISTRADO;
    if (ctx->inutilizada) return UI_CB_VM_NO_DISPONIBLE;
    if (ctx->ejecutando) return UI_CB_OCUPADO;

    lat_mv *mv = ctx->mv;
    if (mv->ptrpila != 0 || mv->ptrctx != 0 ||
        mv->tope != mv->pila ||
        mv->contexto_actual != mv->contexto[0]) {
        ctx->inutilizada = true;
        return UI_CB_ESTADO_INVALIDO;
    }

    lat_objeto *func = latO_obtener_contexto(mv, mv->contexto[0], nombre);
    if (!func) return UI_CB_FUNCION_INEXISTENTE;
    if (func->tipo != T_FUN || func->nparams != 0)
        return UI_CB_FIRMA_INVALIDA;

    ctx->evento_id = evento.id_widget;
    ctx->evento_tipo = evento.tipo;
    ctx->ejecutando = true;
    ctx->llamadas++;
    int estado = latC_llamar_funcion(mv, func);
    ctx->ejecutando = false;

    /* No intentar reconstruir una pila tras longjmp: bloquear la VM. */
    if (estado != 0 || mv->ptrpila != 0 || mv->ptrctx != 0 ||
        mv->tope != mv->pila ||
        mv->contexto_actual != mv->contexto[0]) {
        ctx->errores++;
        ctx->inutilizada = true;
        return estado != 0 ? UI_CB_ERROR_VM : UI_CB_ESTADO_INVALIDO;
    }

    ctx->exitos++;
    return UI_CB_OK;
}

void ui_cb_desregistrar(UICBContexto *ctx)
{
    if (ctx && registro == ctx && !ctx->ejecutando) {
        registro = NULL;
    }
}

UICBResultado ui_cb_despachar(UICBContexto *ctx, const char *nombre, int id) {
    return ui_cb_despachar_evento(ctx,nombre,(UIEvento){UI_EVENTO_CLIC,id});
}
