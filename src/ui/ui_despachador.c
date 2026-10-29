#define _POSIX_C_SOURCE 200809L
#include "ui_despachador.h"
#include <math.h>
#include <string.h>
#include <time.h>

bool ui_despachador_reloj_monotonico(void *usuario, double *milisegundos)
{
    (void)usuario;
    if (!milisegundos) return false;
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return false;
    *milisegundos = (double)ts.tv_sec * 1000.0 +
                    (double)ts.tv_nsec / 1000000.0;
    return true;
}

UIDespachoResultado ui_despachador_inicializar(
    UIDespachador *desp, UIColaEventos *cola,
    UIDespachoCallback callback, void *usuario_callback)
{
    if (!desp || !cola || !callback || !cola->datos || !cola->capacidad ||
        cola->cantidad > cola->capacidad || cola->primero >= cola->capacidad)
        return UI_DESP_ERR_ARGUMENTO;
    memset(desp, 0, sizeof(*desp));
    desp->cola = cola;
    desp->callback = callback;
    desp->usuario_callback = usuario_callback;
    desp->reloj = ui_despachador_reloj_monotonico;
    desp->estado_vm = UI_VM_SANA;
    desp->estado_despachador = UI_DESP_LIBRE;
    desp->max_eventos_frame = UI_MAX_EVENTOS_POR_FRAME_DEF;
    desp->presupuesto_ms = UI_PRESUPUESTO_EVENTOS_MS_DEF;
    return UI_DESP_OK;
}

void ui_despachador_configurar_reloj(
    UIDespachador *desp, UIDespachoReloj reloj, void *usuario_reloj)
{
    if (!desp || desp->estado_despachador == UI_DESP_EJECUTANDO) return;
    desp->reloj = reloj ? reloj : ui_despachador_reloj_monotonico;
    desp->usuario_reloj = usuario_reloj;
}

UIDespachoResultado ui_despachador_encolar(UIDespachador *desp, UIEvento evento)
{
    if (!desp || !desp->cola) return UI_DESP_ERR_ARGUMENTO;
    if (desp->estado_vm == UI_VM_COMPROMETIDA) return UI_DESP_ERR_VM_COMPROMETIDA;
    if (ui_eventos_encolar(desp->cola, evento) == UI_EVENTOS_OK) {
        desp->metricas.eventos_encolados++;
        return UI_DESP_OK;
    }
    if (desp->cola->cantidad >= desp->cola->capacidad) {
        desp->metricas.eventos_rechazados_cola_llena++;
        return UI_DESP_ERR_COLA_LLENA;
    }
    return UI_DESP_ERR_ARGUMENTO;
}

static bool leer_reloj(UIDespachador *d, double *t)
{
    return d->reloj && d->reloj(d->usuario_reloj, t) && isfinite(*t);
}

UIDespachoResultado ui_despachador_procesar_frame(UIDespachador *d)
{
    if (!d || !d->cola || (!d->callback && !d->callback_evento) || !d->reloj ||
        !d->max_eventos_frame || !isfinite(d->presupuesto_ms) ||
        d->presupuesto_ms <= 0.0) return UI_DESP_ERR_ARGUMENTO;
    if (d->estado_vm == UI_VM_COMPROMETIDA) return UI_DESP_ERR_VM_COMPROMETIDA;
    if (d->estado_despachador == UI_DESP_EJECUTANDO) return UI_DESP_ERR_REENTRADA;

    double inicio = 0.0, ahora = 0.0;
    if (!leer_reloj(d, &inicio)) return UI_DESP_ERR_RELOJ;
    d->estado_despachador = UI_DESP_EJECUTANDO;
    UIDespachoResultado resultado = UI_DESP_OK;
    size_t procesados = 0;
    while (ui_eventos_cantidad(d->cola) != 0) {
        if (procesados >= d->max_eventos_frame) {
            d->metricas.cortes_por_limite++;
            resultado = UI_DESP_LIMITE_EVENTOS;
            break;
        }
        if (!leer_reloj(d, &ahora) || ahora < inicio) {
            resultado = UI_DESP_ERR_RELOJ;
            break;
        }
        if (procesados != 0 && ahora - inicio >= d->presupuesto_ms) {
            d->metricas.cortes_por_presupuesto++;
            resultado = UI_DESP_PRESUPUESTO_AGOTADO;
            break;
        }
        UIEvento evento;
        if (ui_eventos_desencolar(d->cola, &evento) != UI_EVENTOS_OK) {
            resultado = UI_DESP_ERR_ARGUMENTO;
            break;
        }
        d->metricas.eventos_consumidos++;
        procesados++;
        if (!d->callback_evento && evento.tipo != UI_EVENTO_CLIC) {
            d->metricas.errores_contrato++;
            resultado = UI_DESP_ERR_CONTRATO;
            break;
        }
        double inicio_callback=ahora;
        UIDespachoCallbackResultado r = d->callback_evento
            ? d->callback_evento(d->usuario_callback, evento)
            : d->callback(d->usuario_callback, evento.id_widget);
        double fin_callback;
        if (leer_reloj(d,&fin_callback) && fin_callback>=inicio_callback &&
            fin_callback-inicio_callback>d->metricas.max_callback_ms)
            d->metricas.max_callback_ms=fin_callback-inicio_callback;
        if (r == UI_CB_DESP_OK) {
            d->metricas.eventos_despachados++;
        } else if (r == UI_CB_DESP_CONTRATO) {
            d->metricas.errores_contrato++;
            resultado = UI_DESP_ERR_CONTRATO;
            break;
        } else {
            d->metricas.errores_vm++;
            d->estado_vm = UI_VM_COMPROMETIDA;
            resultado = UI_DESP_ERR_VM_COMPROMETIDA;
            break;
        }
    }
    if (leer_reloj(d, &ahora) && ahora >= inicio)
        d->metricas.ultimo_tiempo_frame_ms = ahora - inicio;
    d->metricas.pendientes_ultimo_frame = ui_eventos_cantidad(d->cola);
    d->estado_despachador = UI_DESP_LIBRE;
    return resultado;
}

static UIDespachoCallbackResultado sin_uso(void *u, int id) {
    (void)u; (void)id; return UI_CB_DESP_CONTRATO;
}
UIDespachoResultado ui_despachador_inicializar_eventos(
    UIDespachador *desp, UIColaEventos *cola,
    UIDespachoEventoCallback callback, void *usuario_callback) {
    if (!callback) return UI_DESP_ERR_ARGUMENTO;
    UIDespachoResultado r=ui_despachador_inicializar(desp,cola,sin_uso,usuario_callback);
    if (r==UI_DESP_OK) { desp->callback=NULL; desp->callback_evento=callback; }
    return r;
}
