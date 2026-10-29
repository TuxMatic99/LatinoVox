
#include "ui_layout.h"

#include <math.h>
#include <stdio.h>

static int fallos = 0;

static void comprobar(
    int condicion,
    const char *descripcion
) {
    if (condicion) {
        printf("[OK] %s\n", descripcion);
    } else {
        fprintf(stderr, "[ERROR] %s\n", descripcion);
        fallos++;
    }
}

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

    UIRect salida[2] = {0};
    UIRect contenedor = {0, 0, 600, 200};

    UIResultado estado = ui_distribuir(
        contenedor,
        UI_FILA,
        20.0f,
        10.0f,
        elementos,
        2,
        salida
    );

    comprobar(
        estado == UI_LAYOUT_OK,
        "Fila calculada correctamente"
    );

    comprobar(
        aproximadamente(salida[0].x, 10.0f) &&
        aproximadamente(salida[0].ancho, 280.0f) &&
        aproximadamente(salida[1].x, 310.0f) &&
        aproximadamente(salida[1].ancho, 280.0f),
        "Distribucion horizontal proporcional"
    );

    contenedor = (UIRect){0, 0, 300, 600};

    estado = ui_distribuir(
        contenedor,
        UI_COLUMNA,
        20.0f,
        10.0f,
        elementos,
        2,
        salida
    );

    comprobar(
        estado == UI_LAYOUT_OK &&
        aproximadamente(salida[0].y, 10.0f) &&
        aproximadamente(salida[0].alto, 280.0f) &&
        aproximadamente(salida[1].y, 310.0f) &&
        aproximadamente(salida[1].alto, 280.0f),
        "Distribucion vertical proporcional"
    );

    contenedor = (UIRect){0, 0, 150, 100};

    UIRect antes[2] = {salida[0], salida[1]};

    estado = ui_distribuir(
        contenedor,
        UI_FILA,
        20.0f,
        10.0f,
        elementos,
        2,
        salida
    );

    comprobar(
        estado == UI_LAYOUT_SIN_ESPACIO,
        "Deteccion de espacio insuficiente"
    );

    comprobar(
        aproximadamente(salida[0].x, antes[0].x) &&
        aproximadamente(salida[1].y, antes[1].y),
        "No modifica resultados al faltar espacio"
    );

    elementos[0].crecimiento = -1.0f;

    estado = ui_distribuir(
        contenedor,
        UI_FILA,
        20.0f,
        10.0f,
        elementos,
        2,
        salida
    );

    comprobar(
        estado == UI_LAYOUT_ARGUMENTO_INVALIDO,
        "Rechazo de factores de crecimiento negativos"
    );

    elementos[0].crecimiento = NAN;

    estado = ui_distribuir(
        contenedor,
        UI_FILA,
        20.0f,
        10.0f,
        elementos,
        2,
        salida
    );

    comprobar(
        estado == UI_LAYOUT_ARGUMENTO_INVALIDO,
        "Rechazo de valores no finitos"
    );

    comprobar(
        ui_distribuir(
            contenedor,
            UI_FILA,
            10.0f,
            0.0f,
            NULL,
            0,
            NULL
        ) == UI_LAYOUT_OK,
        "Contenedor sin elementos"
    );

    if (fallos == 0) {
        puts("\nTodas las pruebas de LatinoUI Layout pasaron.");
        return 0;
    }

    fprintf(stderr,
        "\nFallaron %d comprobaciones.\n",
        fallos
    );

    return 1;
}
