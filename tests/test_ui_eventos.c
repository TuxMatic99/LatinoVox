#include "ui_eventos.h"
#include <stdio.h>

#define EXIGIR(x) do { if (!(x)) { \
    fprintf(stderr, "Fallo linea %d: %s\n", __LINE__, #x); return 1; \
} } while (0)

int main(void)
{
    UIColaEventos cola = {0};
    UIEvento datos[3] = {{0}};
    UIEvento evento = {UI_EVENTO_CLIC, 777};
    EXIGIR(ui_eventos_inicializar(NULL, datos, 3) == UI_EVENTOS_ARGUMENTO_INVALIDO);
    EXIGIR(ui_eventos_inicializar(&cola, NULL, 3) == UI_EVENTOS_ARGUMENTO_INVALIDO);
    EXIGIR(ui_eventos_inicializar(&cola, datos, 0) == UI_EVENTOS_ARGUMENTO_INVALIDO);
    EXIGIR(ui_eventos_inicializar(&cola, datos, 3) == UI_EVENTOS_OK);
    EXIGIR(ui_eventos_desencolar(&cola, &evento) == UI_EVENTOS_VACIO);
    EXIGIR(evento.id_widget == 777);
    EXIGIR(ui_eventos_encolar(&cola, (UIEvento){UI_EVENTO_CLIC, 1}) == UI_EVENTOS_OK);
    EXIGIR(ui_eventos_encolar(&cola, (UIEvento){UI_EVENTO_CLIC, 2}) == UI_EVENTOS_OK);
    EXIGIR(ui_eventos_encolar(&cola, (UIEvento){UI_EVENTO_CLIC, 3}) == UI_EVENTOS_OK);
    EXIGIR(ui_eventos_encolar(&cola, (UIEvento){UI_EVENTO_CLIC, 4}) == UI_EVENTOS_LLENO);
    EXIGIR(ui_eventos_cantidad(&cola) == 3);
    EXIGIR(ui_eventos_desencolar(&cola, &evento) == UI_EVENTOS_OK && evento.id_widget == 1);
    EXIGIR(ui_eventos_encolar(&cola, (UIEvento){UI_EVENTO_CLIC, 4}) == UI_EVENTOS_OK);
    EXIGIR(ui_eventos_desencolar(&cola, &evento) == UI_EVENTOS_OK && evento.id_widget == 2);
    EXIGIR(ui_eventos_desencolar(&cola, &evento) == UI_EVENTOS_OK && evento.id_widget == 3);
    EXIGIR(ui_eventos_desencolar(&cola, &evento) == UI_EVENTOS_OK && evento.id_widget == 4);
    EXIGIR(ui_eventos_desencolar(&cola, &evento) == UI_EVENTOS_VACIO);
    EXIGIR(ui_eventos_encolar(&cola, (UIEvento){(UITipoEvento)99, 10}) == UI_EVENTOS_ARGUMENTO_INVALIDO);
    EXIGIR(ui_eventos_encolar(&cola, (UIEvento){UI_EVENTO_CLIC, -1}) == UI_EVENTOS_ARGUMENTO_INVALIDO);
    EXIGIR(ui_eventos_inicializar(&cola, datos, 3) == UI_EVENTOS_OK);
    EXIGIR(ui_eventos_cantidad(&cola) == 0);
    puts("OK: cola FIFO, overflow, wraparound, errores y reinicializacion");
    return 0;
}
