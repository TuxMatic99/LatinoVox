
#include <stdio.h>
#include <string.h>

#include "raylib.h"
#include "latino.h"
#include "latast.h"
#include "latmv.h"
#include "latobj.h"

#define UI_TEXTO_MAX 256

static char titulo[UI_TEXTO_MAX] = "LatinoUI";
static char etiqueta[UI_TEXTO_MAX] = "Bienvenido";

static void ui_titulo(lat_mv *mv)
{
    lat_objeto *obj = latC_desapilar(mv);
    const char *texto = latC_checar_cadena(mv, obj);

    if (texto != NULL) {
        snprintf(titulo, sizeof(titulo), "%s", texto);
    }
}

static void ui_etiqueta(lat_mv *mv)
{
    lat_objeto *obj = latC_desapilar(mv);
    const char *texto = latC_checar_cadena(mv, obj);

    if (texto != NULL) {
        snprintf(etiqueta, sizeof(etiqueta), "%s", texto);
    }
}

static const lat_CReg libui[] = {
    {"titulo", ui_titulo, 1},
    {"etiqueta", ui_etiqueta, 1},
    {NULL, NULL, 0}
};

int main(void)
{
    const char *ruta = "scripts/app_ui.lat";

    lat_mv *mv = latC_crear_mv();

    if (mv == NULL) {
        fprintf(stderr, "Error al crear la VM de Latino\n");
        return 1;
    }

    latC_abrir_liblatino(mv, "ui", libui);

    mv->nombre_archivo = (char *)ruta;
    mv->global->REPL = false;

    int estado = 0;
    ast *nodo = latA_analizar_arch((char *)ruta, &estado);

    if (estado != 0 || nodo == NULL) {
        fprintf(stderr, "Error al analizar %s\n", ruta);
        if (nodo != NULL) {
            latA_destruir(nodo);
        }
        latC_destruir_mv(mv);
        return 1;
    }

    lat_objeto *funcion = latC_analizar(mv, nodo);

    if (funcion == NULL) {
        fprintf(stderr, "Error al preparar el script\n");
        latA_destruir(nodo);
        latC_destruir_mv(mv);
        return 1;
    }

    int resultado = latC_llamar_funcion(mv, funcion);

    latO_destruir(mv, funcion);
    latA_destruir(nodo);

    if (resultado != 0) {
        fprintf(stderr, "Error al ejecutar el script\n");
        latC_destruir_mv(mv);
        return 1;
    }

    InitWindow(900, 600, titulo);
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(RAYWHITE);
        DrawText(etiqueta, 60, 100, 28, DARKBLUE);
        EndDrawing();
    }

    CloseWindow();
    latC_destruir_mv(mv);

    return 0;
}
