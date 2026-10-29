#ifndef UI_DESPACHADOR_H
#define UI_DESPACHADOR_H

#include <stdbool.h>
#include <stddef.h>
#include "ui_eventos.h"

#define UI_MAX_EVENTOS_POR_FRAME_DEF 16u
#define UI_PRESUPUESTO_EVENTOS_MS_DEF 2.0

typedef enum { UI_VM_SANA = 0, UI_VM_COMPROMETIDA } UIEstadoVM;
typedef enum { UI_DESP_LIBRE = 0, UI_DESP_EJECUTANDO } UIEstadoDespachador;
typedef enum {
    UI_DESP_OK = 0,
    UI_DESP_PRESUPUESTO_AGOTADO,
    UI_DESP_LIMITE_EVENTOS,
    UI_DESP_ERR_ARGUMENTO,
    UI_DESP_ERR_REENTRADA,
    UI_DESP_ERR_COLA_LLENA,
    UI_DESP_ERR_CONTRATO,
    UI_DESP_ERR_VM_COMPROMETIDA,
    UI_DESP_ERR_RELOJ
} UIDespachoResultado;

typedef enum {
    UI_CB_DESP_OK = 0,
    UI_CB_DESP_CONTRATO,
    UI_CB_DESP_VM_ERROR
} UIDespachoCallbackResultado;

typedef UIDespachoCallbackResultado (*UIDespachoCallback)(void *usuario, int id);
typedef UIDespachoCallbackResultado (*UIDespachoEventoCallback)(void *usuario, UIEvento evento);
typedef bool (*UIDespachoReloj)(void *usuario, double *milisegundos);

typedef struct {
    size_t eventos_encolados;
    size_t eventos_rechazados_cola_llena;
    size_t eventos_consumidos;
    size_t eventos_despachados;
    size_t errores_contrato;
    size_t errores_vm;
    size_t cortes_por_presupuesto;
    size_t cortes_por_limite;
    size_t pendientes_ultimo_frame;
    double ultimo_tiempo_frame_ms;
    double max_callback_ms;
} UIDespachoMetricas;

typedef struct {
    UIColaEventos *cola; /* prestada: pertenece al llamador */
    UIDespachoCallback callback;
    UIDespachoEventoCallback callback_evento;
    void *usuario_callback;
    UIDespachoReloj reloj;
    void *usuario_reloj;
    UIEstadoVM estado_vm;
    UIEstadoDespachador estado_despachador;
    size_t max_eventos_frame;
    double presupuesto_ms;
    UIDespachoMetricas metricas;
} UIDespachador;

bool ui_despachador_reloj_monotonico(void *usuario, double *milisegundos);

UIDespachoResultado ui_despachador_inicializar(
    UIDespachador *desp, UIColaEventos *cola,
    UIDespachoCallback callback, void *usuario_callback);

UIDespachoResultado ui_despachador_inicializar_eventos(
    UIDespachador *desp, UIColaEventos *cola,
    UIDespachoEventoCallback callback, void *usuario_callback);

void ui_despachador_configurar_reloj(
    UIDespachador *desp, UIDespachoReloj reloj, void *usuario_reloj);

UIDespachoResultado ui_despachador_encolar(UIDespachador *desp, UIEvento evento);
UIDespachoResultado ui_despachador_procesar_frame(UIDespachador *desp);

#endif
