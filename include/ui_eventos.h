#ifndef UI_EVENTOS_H
#define UI_EVENTOS_H
#include <stddef.h>

typedef enum {
    UI_EVENTO_CLIC = 1
} UITipoEvento;

typedef struct {
    UITipoEvento tipo;
    int id_widget;
} UIEvento;

typedef enum {
    UI_EVENTOS_OK = 0,
    UI_EVENTOS_ARGUMENTO_INVALIDO,
    UI_EVENTOS_LLENO,
    UI_EVENTOS_VACIO
} UIEventosResultado;

typedef struct {
    UIEvento *datos;
    size_t capacidad;
    size_t primero;
    size_t cantidad;
} UIColaEventos;

UIEventosResultado ui_eventos_inicializar(UIColaEventos *cola,
                                         UIEvento *almacenamiento,
                                         size_t capacidad);
UIEventosResultado ui_eventos_encolar(UIColaEventos *cola, UIEvento evento);
UIEventosResultado ui_eventos_desencolar(UIColaEventos *cola, UIEvento *salida);
size_t ui_eventos_cantidad(const UIColaEventos *cola);
#endif
