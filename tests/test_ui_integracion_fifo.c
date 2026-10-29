#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ui_despachador.h"
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FALLO %s:%d: %s\n",__FILE__,__LINE__,#x); return 1; } } while (0)
typedef struct { int ids[128]; size_t n; UIDespachador *desp; bool reentrada; } Registro;
static UIDespachoCallbackResultado callback(void *u, int id) {
    Registro *r = u;
    if (r->n >= 128) return UI_CB_DESP_VM_ERROR;
    r->ids[r->n++] = id;
    if (r->reentrada && ui_despachador_procesar_frame(r->desp) != UI_DESP_ERR_REENTRADA)
        return UI_CB_DESP_VM_ERROR;
    return UI_CB_DESP_OK;
}
typedef struct { double t; double paso; bool error; } Reloj;
static bool reloj(void *u, double *t) {
    Reloj *r = u;
    if (r->error) return false;
    *t = r->t;
    r->t += r->paso;
    return true;
}
int main(void) {
    UIEvento almacen[64] = {{0}};
    UIColaEventos cola = {0};
    UIDespachador desp;
    Registro r = {{0},0,&desp,true};
    Reloj tiempo = {0,0,false};
    CHECK(ui_eventos_inicializar(&cola,almacen,64)==UI_EVENTOS_OK);
    CHECK(ui_despachador_inicializar(&desp,&cola,callback,&r)==UI_DESP_OK);
    ui_despachador_configurar_reloj(&desp,reloj,&tiempo);
    int ids[]={1,2,99,1,404,2};
    for (size_t i=0;i<6;i++) CHECK(ui_despachador_encolar(&desp,(UIEvento){UI_EVENTO_CLIC,ids[i]})==UI_DESP_OK);
    CHECK(ui_despachador_procesar_frame(&desp)==UI_DESP_OK);
    CHECK(r.n==6 && memcmp(r.ids,ids,sizeof(ids))==0);
    CHECK(desp.estado_vm==UI_VM_SANA && ui_eventos_cantidad(&cola)==0);
    for (int i=0;i<40;i++) CHECK(ui_despachador_encolar(&desp,(UIEvento){UI_EVENTO_CLIC,i})==UI_DESP_OK);
    CHECK(ui_despachador_procesar_frame(&desp)==UI_DESP_LIMITE_EVENTOS);
    CHECK(r.n==22 && ui_eventos_cantidad(&cola)==24);
    CHECK(ui_despachador_procesar_frame(&desp)==UI_DESP_LIMITE_EVENTOS);
    CHECK(ui_eventos_cantidad(&cola)==8);
    CHECK(ui_despachador_procesar_frame(&desp)==UI_DESP_OK);
    CHECK(ui_eventos_cantidad(&cola)==0 && r.n==46);
    for(int i=0;i<64;i++) CHECK(ui_despachador_encolar(&desp,(UIEvento){UI_EVENTO_CLIC,i})==UI_DESP_OK);
    CHECK(ui_despachador_encolar(&desp,(UIEvento){UI_EVENTO_CLIC,100})==UI_DESP_ERR_COLA_LLENA);
    CHECK(desp.metricas.eventos_rechazados_cola_llena==1);
    while(ui_eventos_cantidad(&cola)>0) CHECK(ui_despachador_procesar_frame(&desp)==UI_DESP_LIMITE_EVENTOS || ui_eventos_cantidad(&cola)==0);
    CHECK(r.n==110);
    CHECK(ui_despachador_encolar(&desp,(UIEvento){UI_EVENTO_CLIC,7})==UI_DESP_OK);
    tiempo.t=0; tiempo.paso=3.0;
    CHECK(ui_despachador_encolar(&desp,(UIEvento){UI_EVENTO_CLIC,8})==UI_DESP_OK);
    CHECK(ui_despachador_procesar_frame(&desp)==UI_DESP_PRESUPUESTO_AGOTADO);
    CHECK(ui_eventos_cantidad(&cola)==1);
    tiempo.paso=0;
    CHECK(ui_despachador_procesar_frame(&desp)==UI_DESP_OK);
    CHECK(ui_despachador_encolar(&desp,(UIEvento){UI_EVENTO_CLIC,9})==UI_DESP_OK);
    tiempo.error=true;
    CHECK(ui_despachador_procesar_frame(&desp)==UI_DESP_ERR_RELOJ);
    CHECK(ui_eventos_cantidad(&cola)==1);
    tiempo.error=false;
    CHECK(ui_despachador_procesar_frame(&desp)==UI_DESP_OK);
    puts("OK: FIFO, limite, reloj, reentrada, cola llena y pendientes");
    return 0;
}
