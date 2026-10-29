#include "ui_runtime.h"
#include "ui_utf8.h"
#include "ui_despachador.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define EXIGIR(c) do { if (!(c)) { fprintf(stderr,"FALLO %d: %s\n",__LINE__,#c); exit(2); } } while(0)
typedef struct { UIEvento recibidos[3]; int n; double reloj; } Registro;
static bool reloj(void *u,double *t) { *t=((Registro*)u)->reloj; return true; }
static UIDespachoCallbackResultado callback(void *u,UIEvento e) {
    Registro *r=u; EXIGIR(r->n<3); r->recibidos[r->n++]=e; r->reloj+=7.5; return UI_CB_DESP_OK;
}
int main(void) {
    const char *valido="aáñ€\xf0\x9f\x98\x80";
    const char *malos[]={"\xc0\xaf","\xed\xa0\x80","\xf4\x90\x80\x80","\xf5\x80\x80\x80","\x80","\xe2\x82","\xe2X"};
    size_t n=0; EXIGIR(ui_utf8_validar(valido,&n) && n==5);
    for(size_t i=0;i<sizeof malos/sizeof *malos;++i) EXIGIR(!ui_utf8_validar(malos[i],NULL));
    for(size_t cap=1;cap<18;++cap) {
        char buf[20]; memset(buf,0x55,sizeof buf);
        bool completa=ui_utf8_copiar_seguro(buf,cap,valido);
        EXIGIR(ui_utf8_validar(buf,NULL) && strlen(buf)<cap);
        EXIGIR(completa==(strlen(valido)<cap));
        EXIGIR((unsigned char)buf[cap]==0x55);
    }
    char limite[256]; memset(limite,'x',254); limite[254]='\0';
    EXIGIR(!ui_utf8_agregar(limite,sizeof limite,0xf1) && strlen(limite)==254);
    EXIGIR(ui_utf8_agregar(limite,sizeof limite,'a') && strlen(limite)==255);
    EXIGIR(!ui_utf8_agregar(limite,sizeof limite,'b'));
    char borrar[32]; EXIGIR(ui_utf8_copiar_seguro(borrar,sizeof borrar,valido));
    for(int i=5;i>0;--i) {EXIGIR(ui_utf8_validar(borrar,&n) && n==(size_t)i); EXIGIR(ui_utf8_borrar_ultimo(borrar));}
    EXIGIR(!ui_utf8_borrar_ultimo(borrar));
    UIRuntime ui; EXIGIR(lui_inicializar(&ui));
    EXIGIR(lui_iniciar_contenedor(&ui,UI_NODO_COLUMNA,8));
    EXIGIR(lui_agregar_control(&ui,1,UI_WIDGET_ETIQUETA,"Nombre"));
    EXIGIR(lui_agregar_control(&ui,2,UI_WIDGET_CAMPO_TEXTO,"María Ñáñez"));
    EXIGIR(lui_agregar_control(&ui,3,UI_WIDGET_BOTON,"Guardar"));
    EXIGIR(lui_fin_contenedor(&ui) && lui_validar(&ui));
    UIEvento e;
    EXIGIR(!lui_tecla(&ui,UI_TECLA_TAB,&e) && ui.nodos[ui.foco].id_widget==2);
    EXIGIR(lui_caracter(&ui,0x20ac));
    EXIGIR(!strcmp(lui_texto(&ui,2),"María Ñáñez€"));
    EXIGIR(lui_tecla(&ui,UI_TECLA_ESCAPE,&e) && e.tipo==UI_EVENTO_CANCELAR && e.id_widget==2);
    EXIGIR(!strcmp(lui_texto(&ui,2),"María Ñáñez"));
    EXIGIR(lui_tecla(&ui,UI_TECLA_ENTER,&e) && e.tipo==UI_EVENTO_SUBMIT);
    EXIGIR(!lui_tecla(&ui,UI_TECLA_RETROTAB,&e) && ui.nodos[ui.foco].id_widget==3);
    EXIGIR(lui_tecla(&ui,UI_TECLA_ENTER,&e) && e.tipo==UI_EVENTO_CLIC);
    EXIGIR(!lui_tecla(&ui,UI_TECLA_TAB,&e) && ui.nodos[ui.foco].id_widget==2);
    EXIGIR(!lui_establecer_texto(&ui,2,"\xff") && !strcmp(lui_texto(&ui,2),"María Ñáñez"));
    EXIGIR(!lui_establecer_texto(&ui,2,"dos\nlineas"));
    EXIGIR(lui_establecer_texto(&ui,2,"") && lui_caracter(&ui,0xf1));
    EXIGIR(!lui_tecla(&ui,UI_TECLA_BORRAR,&e) && !*lui_texto(&ui,2));
    EXIGIR(lui_habilitar(&ui,2,false));
    EXIGIR(!lui_enfocar(&ui,2) && !lui_caracter(&ui,'x'));
    EXIGIR(!lui_tecla(&ui,UI_TECLA_TAB,&e) && ui.nodos[ui.foco].id_widget==3);
    EXIGIR(!lui_tecla(&ui,UI_TECLA_TAB,&e) && ui.nodos[ui.foco].id_widget==3);
    EXIGIR(lui_tecla(&ui,UI_TECLA_ESCAPE,&e) && e.tipo==UI_EVENTO_CANCELAR);
    EXIGIR(lui_habilitar(&ui,2,true) && lui_enfocar(&ui,2));
    char invalido[8]="previo";
    EXIGIR(!ui_utf8_copiar_seguro(invalido,sizeof invalido,"\xf0\x9f") && !*invalido);
    EXIGIR(!ui_utf8_copiar_seguro(invalido,0,"a"));
    Registro registro={0}; UIEvento almacen[3]; UIColaEventos cola; UIDespachador d;
    EXIGIR(ui_eventos_inicializar(&cola,almacen,3)==UI_EVENTOS_OK);
    EXIGIR(ui_despachador_inicializar_eventos(&d,&cola,callback,&registro)==UI_DESP_OK);
    ui_despachador_configurar_reloj(&d,reloj,&registro);
    for(int i=1;i<=3;++i) EXIGIR(ui_despachador_encolar(&d,(UIEvento){(UITipoEvento)i,i})==UI_DESP_OK);
    EXIGIR(ui_despachador_procesar_frame(&d)==UI_DESP_PRESUPUESTO_AGOTADO && registro.n==1);
    EXIGIR(d.metricas.max_callback_ms==7.5);
    EXIGIR(ui_despachador_procesar_frame(&d)==UI_DESP_PRESUPUESTO_AGOTADO);
    EXIGIR(ui_despachador_procesar_frame(&d)==UI_DESP_OK && registro.n==3);
    for(int i=0;i<3;++i) EXIGIR(registro.recibidos[i].tipo==(UITipoEvento)(i+1));
    puts("OK: UTF-8 estricto, bordes, foco circular, edicion, eventos y latencia");
    return 0;
}
