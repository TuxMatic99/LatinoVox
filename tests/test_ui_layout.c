
#include "ui_layout.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

static int aproximadamente(float a, float b)
{
    return fabsf(a - b) < 0.01f;
}

int main(void)
{
    UIElemento elementos[2] = {
        {100.0f, 1.0f},
        {100.0f, 1.0f}
    };

    UIRect resultado[2] = {0};

    // Fila: 600 px, relleno 10 por lado y
    // separación de 20 px. Cada hijo obtiene 280 px.
    UIRect ventana = {0, 0, 600, 200};

    UIResultado estado = ui_distribuir(
        ventana, UI_FILA, 20, 10,
        elementos, 2, resultado
    );

    assert(estado == UI_LAYOUT_OK);
    assert(aproximadamente(resultado[0].x, 10));
    assert(aproximadamente(resultado[0].ancho, 280));
    assert(aproximadamente(resultado[1].x, 310));
    assert(aproximadamente(resultado[1].ancho, 280));

    // Columna: los mismos elementos distribuidos
    // verticalmente.
    ventana = (UIRect){0, 0, 300, 600};

    estado = ui_distribuir(
        ventana, UI_COLUMNA, 20, 10,
        elementos, 2, resultado
    );

    assert(estado == UI_LAYOUT_OK);
    assert(aproximadamente(resultado[0].y, 10));
    assert(aproximadamente(resultado[0].alto, 280));
    assert(aproximadamente(resultado[1].y, 310));

    // Espacio insuficiente.
    ventana = (UIRect){0, 0, 150, 100};

    estado = ui_distribuir(
        ventana, UI_FILA, 20, 10,
        elementos, 2, resultado
    );

    assert(estado == UI_LAYOUT_SIN_ESPACIO);

    // Entrada inválida.
    elementos[0].crecimiento = -1;

    estado = ui_distribuir(
        ventana, UI_FILA, 20, 10,
        elementos, 2, resultado
    );

    assert(estado == UI_LAYOUT_ARGUMENTO_INVALIDO);

    puts("OK: pruebas iniciales de LatinoUI Layout");
    return 0;
}
