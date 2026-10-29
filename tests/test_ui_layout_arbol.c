#include "ui_layout_arbol.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x) do {if (!(x)) {fprintf(stderr,"Fallo en linea %d: %s\n", __LINE__,#x);return 1;}}while(0)
static int cerca(float a,float b){return fabsf(a-b)<0.01f;}
int main(void) {
    UINodo nodos[5]={0}; UIArbol a={0};
    size_t raiz,fila,uno,dos,tres;
    CHECK(ui_arbol_inicializar(&a,nodos,5)==UI_ARBOL_OK);
    CHECK(ui_arbol_agregar(&a,UI_NODO_NULO,UI_NODO_COLUMNA,-1,10,&raiz)==UI_ARBOL_OK);
    CHECK(ui_arbol_agregar(&a,raiz,UI_NODO_FILA,-1,20,&fila)==UI_ARBOL_OK);
    CHECK(ui_arbol_agregar(&a,fila,UI_NODO_WIDGET,50,0,&uno)==UI_ARBOL_OK);
    CHECK(ui_arbol_agregar(&a,fila,UI_NODO_WIDGET,51,0,&dos)==UI_ARBOL_OK);
    CHECK(ui_arbol_agregar(&a,raiz,UI_NODO_WIDGET,52,0,&tres)==UI_ARBOL_OK);
    UIRect salida[5]={{-1,-1,-1,-1}};
    CHECK(ui_layout_arbol_calcular(&a,(UIRect){0,0,600,300},salida,5)==UI_LAYOUT_ARBOL_OK);
    CHECK(cerca(salida[fila].alto,145) && cerca(salida[tres].y,155));
    CHECK(cerca(salida[uno].ancho,290) && cerca(salida[dos].x,310));
    CHECK(cerca(salida[tres].ancho,600) && cerca(salida[tres].alto,145));
    UIRect antes=salida[dos];
    CHECK(ui_layout_arbol_calcular(&a,(UIRect){0,0,10,300},salida,5)==UI_LAYOUT_ARBOL_SIN_ESPACIO);
    CHECK(memcmp(&antes,&salida[dos],sizeof(antes))==0);
    CHECK(ui_layout_arbol_calcular(&a,(UIRect){0,0,600,300},salida,4)==UI_LAYOUT_ARBOL_ARGUMENTO_INVALIDO);
    printf("OK: layout jerarquico y atomicidad\n");
    return 0;
}
