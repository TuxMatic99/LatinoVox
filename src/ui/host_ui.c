#include <math.h>
#include <stdio.h>
#include "raylib.h"
#include "latino.h"
#include "latast.h"
#include "latmv.h"
#include "latobj.h"
#include "ui_runtime.h"
#include "ui_eventos.h"

static UIRuntime ui;
static int siguiente_etiqueta = 10000;

static void n_titulo(lat_mv *mv) {
    const char *texto = latC_checar_cadena(mv, latC_desapilar(mv));
    (void)lui_titulo(&ui, texto);
}
static void n_etiqueta(lat_mv *mv) {
    const char *texto = latC_checar_cadena(mv, latC_desapilar(mv));
    (void)lui_agregar_widget(&ui, siguiente_etiqueta++, texto);
}
static void n_boton(lat_mv *mv) {
    lat_objeto *o_texto = latC_desapilar(mv);
    lat_objeto *o_id = latC_desapilar(mv);
    const char *texto = latC_checar_cadena(mv, o_texto);
    double n = latC_checar_numerico(mv, o_id);
    if (!isfinite(n) || n < 0 || n >= 10000 || floor(n) != n) {
        fprintf(stderr, "LatinoUI: identificador de boton invalido\n");
        (void)lui_agregar_widget(&ui, -1, texto); /* establece error */
        return;
    }
    (void)lui_agregar_widget(&ui, (int)n, texto);
}
static void n_fila(lat_mv *mv) {
    double espacio = latC_checar_numerico(mv, latC_desapilar(mv));
    (void)lui_iniciar_contenedor(&ui, UI_NODO_FILA, (float)espacio);
}
static void n_columna(lat_mv *mv) {
    double espacio = latC_checar_numerico(mv, latC_desapilar(mv));
    (void)lui_iniciar_contenedor(&ui, UI_NODO_COLUMNA, (float)espacio);
}
static void n_fin(lat_mv *mv) { (void)mv; (void)lui_fin_contenedor(&ui); }

static const lat_CReg libui[] = {
    {"titulo", n_titulo, 1},
    {"etiqueta", n_etiqueta, 1},
    {"boton", n_boton, 2},
    {"iniciar_fila", n_fila, 1},
    {"iniciar_columna", n_columna, 1},
    {"fin_contenedor", n_fin, 0},
    {NULL, NULL, 0}
};

int main(void) {
    const char *ruta = "scripts/app_ui.lat";
    if (!lui_inicializar(&ui)) return 1;

    UIEvento memoria_eventos[64] = {0};
    UIColaEventos cola_eventos = {0};

    if (ui_eventos_inicializar(
            &cola_eventos, memoria_eventos, 64)
            != UI_EVENTOS_OK) {
        fprintf(stderr, "Error al iniciar la cola\n");
        return 1;
    }
    lat_mv *mv = latC_crear_mv();
    if (!mv) { fprintf(stderr, "No se pudo crear VM\n"); return 1; }
    latC_abrir_liblatino(mv, "ui", libui);
    mv->nombre_archivo = (char *)ruta;
    mv->global->REPL = false;
    int estado = 0;
    ast *nodo = latA_analizar_arch((char *)ruta, &estado);
    if (estado || !nodo) {
        fprintf(stderr, "No se pudo analizar %s\n", ruta);
        if (nodo) latA_destruir(nodo);
        latC_destruir_mv(mv);
        return 1;
    }
    lat_objeto *funcion = latC_analizar(mv, nodo);
    if (!funcion) {
        fprintf(stderr, "No se pudo preparar script\n");
        latA_destruir(nodo);
        latC_destruir_mv(mv);
        return 1;
    }
    int resultado = latC_llamar_funcion(mv, funcion);
    latO_destruir(mv, funcion);
    latA_destruir(nodo);
    if (resultado || !lui_validar(&ui)) {
        fprintf(stderr, "LatinoUI: fallo en script o estructura: %s\n", ui.error);
        latC_destruir_mv(mv);
        return 1;
    }

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(900, 600, ui.titulo);
    SetWindowMinSize(600, 400);
    SetTargetFPS(60);
    int ultimo_clic = -1;
    while (!WindowShouldClose()) {
        UIRect area = {24.0f, 70.0f, (float)GetScreenWidth() - 48.0f,
                       (float)GetScreenHeight() - 110.0f};
        bool valido = lui_calcular(&ui, area);
        if (valido && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            Vector2 p = GetMousePosition();
            for (size_t i = 0; i < ui.arbol.cantidad; i++) {
                if (ui.nodos[i].tipo != UI_NODO_WIDGET || ui.nodos[i].id_widget >= 10000) continue;
                UIRect b = ui.rectangulos[i];
                Rectangle r = {b.x, b.y, b.ancho, b.alto};
                if (CheckCollisionPointRec(p, r)) {
                   UIEvento nuevo = {
                       UI_EVENTO_CLIC,
                       ui.nodos[i].id_widget
                   };

                   UIEventosResultado estado_cola =
                       ui_eventos_encolar(&cola_eventos, nuevo);

                   if (estado_cola != UI_EVENTOS_OK) {
                       fprintf(stderr,
                           "No se pudo encolar evento %d (codigo %d)\n",
                           nuevo.id_widget, (int)estado_cola);
                   }
                    break;
                }
            }
        }
       UIEvento pendiente;

       while (ui_eventos_desencolar(
                  &cola_eventos, &pendiente)
                  == UI_EVENTOS_OK) {
           if (pendiente.tipo == UI_EVENTO_CLIC) {
               ultimo_clic = pendiente.id_widget;
               printf("LatinoUI: evento CLIC, boton %d\n",
                      ultimo_clic);
           }
       }

       BeginDrawing();
        ClearBackground(RAYWHITE);
        DrawText(ui.titulo, 24, 19, 28, DARKBLUE);
        if (!valido) {
            DrawText("Espacio insuficiente para layout", 24, 85, 24, MAROON);
        } else {
            for (size_t i = 0; i < ui.arbol.cantidad; i++) {
                UIRect b = ui.rectangulos[i];
                Rectangle r = {b.x, b.y, b.ancho, b.alto};
                if (ui.nodos[i].tipo == UI_NODO_WIDGET) {
                    bool boton = ui.nodos[i].id_widget < 10000;
                    DrawRectangleRec(r, boton ? SKYBLUE : (Color){231, 239, 247, 255});
                    DrawRectangleLinesEx(r, 1.0f, GRAY);
                    BeginScissorMode((int)r.x, (int)r.y, (int)r.width, (int)r.height);
                    DrawText(ui.textos[i], (int)b.x + 9, (int)b.y + 8, 24, DARKBLUE);
                    EndScissorMode();
                } else {
                    DrawRectangleLinesEx(r, 1.0f, LIGHTGRAY);
                }
            }
        }
        DrawText(TextFormat("Ultimo clic: %d", ultimo_clic), 24, GetScreenHeight() - 31, 20, DARKGRAY);
        EndDrawing();
    }
    CloseWindow();
    latC_destruir_mv(mv);
    return 0;
}
