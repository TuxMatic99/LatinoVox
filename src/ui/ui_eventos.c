#include "ui_eventos.h"
#include <stdint.h>

static int cola_valida(const UIColaEventos *cola)
{
    return cola && cola->datos && cola->capacidad > 0 &&
           cola->capacidad <= SIZE_MAX / sizeof(UIEvento) &&
           cola->primero < cola->capacidad &&
           cola->cantidad <= cola->capacidad;
}

UIEventosResultado ui_eventos_inicializar(UIColaEventos *cola,
                                         UIEvento *almacenamiento,
                                         size_t capacidad)
{
    if (!cola || !almacenamiento || capacidad == 0 ||
        capacidad > SIZE_MAX / sizeof(UIEvento))
        return UI_EVENTOS_ARGUMENTO_INVALIDO;
    cola->datos = almacenamiento;
    cola->capacidad = capacidad;
    cola->primero = 0;
    cola->cantidad = 0;
    return UI_EVENTOS_OK;
}

UIEventosResultado ui_eventos_encolar(UIColaEventos *cola, UIEvento evento)
{
    if (!cola_valida(cola) || evento.tipo != UI_EVENTO_CLIC || evento.id_widget < 0)
        return UI_EVENTOS_ARGUMENTO_INVALIDO;
    if (cola->cantidad == cola->capacidad)
        return UI_EVENTOS_LLENO;
    /* No sumamos indices directamente: evitamos overflow de size_t. */
    size_t restante = cola->capacidad - cola->primero;
    size_t posicion = cola->cantidad >= restante
        ? cola->cantidad - restante : cola->primero + cola->cantidad;
    cola->datos[posicion] = evento;
    cola->cantidad++;
    return UI_EVENTOS_OK;
}

UIEventosResultado ui_eventos_desencolar(UIColaEventos *cola, UIEvento *salida)
{
    if (!cola_valida(cola) || !salida)
        return UI_EVENTOS_ARGUMENTO_INVALIDO;
    if (cola->cantidad == 0)
        return UI_EVENTOS_VACIO;
    *salida = cola->datos[cola->primero];
    cola->primero = cola->primero + 1 == cola->capacidad ? 0 : cola->primero + 1;
    cola->cantidad--;
    return UI_EVENTOS_OK;
}

size_t ui_eventos_cantidad(const UIColaEventos *cola)
{
    return cola_valida(cola) ? cola->cantidad : 0;
}
