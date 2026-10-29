#include <stdio.h>
#include "ui_despachador.h"
static int llamadas=0;
static UIDespachoCallbackResultado falla(void *u, int id) {
    (void)u; (void)id; llamadas++; return UI_CB_DESP_VM_ERROR;
}
int main(void) {
    UIEvento mem[4]={{0}};
    UIColaEventos cola={0};
    UIDespachador d;
    if(ui_eventos_inicializar(&cola,mem,4)!=UI_EVENTOS_OK) return 1;
    if(ui_despachador_inicializar(&d,&cola,falla,NULL)!=UI_DESP_OK) return 2;
    if(ui_despachador_encolar(&d,(UIEvento){UI_EVENTO_CLIC,1})!=UI_DESP_OK) return 3;
    if(ui_despachador_encolar(&d,(UIEvento){UI_EVENTO_CLIC,2})!=UI_DESP_OK) return 4;
    if(ui_despachador_procesar_frame(&d)!=UI_DESP_ERR_VM_COMPROMETIDA) return 5;
    if(d.estado_vm!=UI_VM_COMPROMETIDA || llamadas!=1 || ui_eventos_cantidad(&cola)!=1) return 6;
    if(ui_despachador_procesar_frame(&d)!=UI_DESP_ERR_VM_COMPROMETIDA || llamadas!=1) return 7;
    puts("OK: cortacircuitos ante error del adaptador");
    return 0;
}
