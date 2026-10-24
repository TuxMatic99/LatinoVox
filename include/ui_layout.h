
#ifndef UI_LAYOUT_H
#define UI_LAYOUT_H

#include <stddef.h>

typedef struct {
    float x, y;
    float ancho, alto;
} UIRect;

typedef enum {
    UI_FILA,
    UI_COLUMNA
} UIDireccion;

typedef struct {
    float base;
    float crecimiento;
} UIElemento;

typedef enum {
    UI_LAYOUT_OK = 0,
    UI_LAYOUT_SIN_ESPACIO,
    UI_LAYOUT_ARGUMENTO_INVALIDO
} UIResultado;

/*
 * Calcula la posición de elementos consecutivos.
 *
 * Los elementos utilizan 'base' como tamaño mínimo en el
 * eje principal y 'crecimiento' como peso para repartir
 * espacio sobrante.
 *
 * Si falta espacio, devuelve UI_LAYOUT_SIN_ESPACIO
 * sin modificar el arreglo de salida.
 */
UIResultado ui_distribuir(
    UIRect contenedor,
    UIDireccion direccion,
    float espacio,
    float relleno,
    const UIElemento *elementos,
    size_t cantidad,
    UIRect *salida
);

#endif
