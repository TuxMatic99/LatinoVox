
#include "ui_layout.h"
#include <math.h>

UIResultado ui_distribuir(
    UIRect c,
    UIDireccion direccion,
    float espacio,
    float relleno,
    const UIElemento *elementos,
    size_t cantidad,
    UIRect *salida
) {
    if ((direccion != UI_FILA && direccion != UI_COLUMNA) ||
        !isfinite(c.x) || !isfinite(c.y) ||
        !isfinite(c.ancho) || !isfinite(c.alto) ||
        !isfinite(espacio) || !isfinite(relleno) ||
        c.ancho < 0 || c.alto < 0 ||
        espacio < 0 || relleno < 0 ||
        (cantidad > 0 && (!elementos || !salida))) {
        return UI_LAYOUT_ARGUMENTO_INVALIDO;
    }

    if (cantidad == 0) return UI_LAYOUT_OK;

    double disponible = direccion == UI_FILA
        ? c.ancho : c.alto;

    disponible -= 2.0 * relleno;
    disponible -= (double)(cantidad - 1) * espacio;

    double total_base = 0;
    double total_pesos = 0;

    for (size_t i = 0; i < cantidad; i++) {
        float base = elementos[i].base;
        float peso = elementos[i].crecimiento;

        if (!isfinite(base) || !isfinite(peso) ||
            base < 0 || peso < 0) {
            return UI_LAYOUT_ARGUMENTO_INVALIDO;
        }

        total_base += base;
        total_pesos += peso;
    }

    double transversal = direccion == UI_FILA
        ? (double)c.alto - 2.0 * relleno
        : (double)c.ancho - 2.0 * relleno;

    if (!isfinite(disponible) ||
        !isfinite(transversal) ||
        !isfinite(total_base) ||
        !isfinite(total_pesos)) {
        return UI_LAYOUT_ARGUMENTO_INVALIDO;
    }

    if (disponible < total_base || transversal < 0) {
        return UI_LAYOUT_SIN_ESPACIO;
    }

    double restante = disponible - total_base;
    double posicion = relleno;

    for (size_t i = 0; i < cantidad; i++) {
        double tamano = elementos[i].base;

        if (total_pesos > 0) {
            tamano += restante *
                elementos[i].crecimiento / total_pesos;
        }

        if (direccion == UI_FILA) {
            salida[i] = (UIRect) {
                c.x + (float)posicion,
                c.y + relleno,
                (float)tamano,
                (float)transversal
            };
        } else {
            salida[i] = (UIRect) {
                c.x + relleno,
                c.y + (float)posicion,
                (float)transversal,
                (float)tamano
            };
        }

        posicion += tamano + espacio;
    }

    return UI_LAYOUT_OK;
}
