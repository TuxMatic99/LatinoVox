/* Demostracion visual del layout jerarquico de LatinoUI.
 * Independiente de LatinoVox y de Latino-Core.
 */
#include "raylib.h"
#include "ui_arbol.h"
#include "ui_layout_arbol.h"
#include <stdio.h>

#define NUM_NODOS 7

static Rectangle convertir(UIRect r) {
    return (Rectangle){r.x, r.y, r.ancho, r.alto};
}

static void panel(UIRect r, Color color, const char *texto) {
    Rectangle rec = convertir(r);
    DrawRectangleRec(rec, color);
    DrawRectangleLinesEx(rec, 2, DARKGRAY);
    DrawText(texto, (int)rec.x + 12, (int)rec.y + 12, 24, BLACK);
}

int main(void) {
    UINodo nodos[NUM_NODOS] = {0};
    UIArbol arbol = {0};
    size_t raiz, cabecera, fila, guardar, cancelar, contenido, pie;
    if (ui_arbol_inicializar(&arbol, nodos, NUM_NODOS) != UI_ARBOL_OK ||
        ui_arbol_agregar(&arbol, UI_NODO_NULO, UI_NODO_COLUMNA, -1, 12, &raiz) != UI_ARBOL_OK ||
        ui_arbol_agregar(&arbol, raiz, UI_NODO_WIDGET, 1, 0, &cabecera) != UI_ARBOL_OK ||
        ui_arbol_agregar(&arbol, raiz, UI_NODO_FILA, -1, 12, &fila) != UI_ARBOL_OK ||
        ui_arbol_agregar(&arbol, fila, UI_NODO_WIDGET, 2, 0, &guardar) != UI_ARBOL_OK ||
        ui_arbol_agregar(&arbol, fila, UI_NODO_WIDGET, 3, 0, &cancelar) != UI_ARBOL_OK ||
        ui_arbol_agregar(&arbol, raiz, UI_NODO_WIDGET, 4, 0, &contenido) != UI_ARBOL_OK ||
        ui_arbol_agregar(&arbol, raiz, UI_NODO_WIDGET, 5, 0, &pie) != UI_ARBOL_OK) {
        fprintf(stderr, "No se pudo construir el arbol de demostracion\n");
        return 1;
    }

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(900, 600, "LatinoUI Tree | Demo independiente");
    SetWindowMinSize(760, 480);
    SetTargetFPS(60);
    UIRect rects[NUM_NODOS] = {0};
    const char *estado = "Prueba los botones y cambia el tamano de ventana";

    while (!WindowShouldClose()) {
        float ancho = (float)GetScreenWidth();
        float alto = (float)GetScreenHeight();
        UIRect area = {20, 20, ancho - 40, alto - 40};
        UILayoutArbolResultado r = ui_layout_arbol_calcular(&arbol, area, rects, NUM_NODOS);
        if (r == UI_LAYOUT_ARBOL_OK && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            Vector2 mouse = GetMousePosition();
            if (CheckCollisionPointRec(mouse, convertir(rects[guardar]))) estado = "Accion: Guardar";
            else if (CheckCollisionPointRec(mouse, convertir(rects[cancelar]))) estado = "Accion: Cancelar";
        }
        BeginDrawing();
        ClearBackground(RAYWHITE);
        if (r == UI_LAYOUT_ARBOL_OK) {
            panel(rects[cabecera], (Color){211, 226, 242, 255}, "LatinoUI: columna principal");
            panel(rects[guardar], (Color){178, 224, 193, 255}, "Guardar");
            panel(rects[cancelar], (Color){244, 204, 204, 255}, "Cancelar");
            panel(rects[contenido], (Color){234, 228, 250, 255}, "Contenido adaptable");
            panel(rects[pie], (Color){230, 231, 233, 255}, estado);
            DrawText(TextFormat("%dx%d | 7 nodos", GetScreenWidth(), GetScreenHeight()), 32, 24, 14, DARKGRAY);
        } else {
            DrawText(TextFormat("Error de layout: %d", (int)r), 30, 30, 20, MAROON);
        }
        EndDrawing();
    }
    CloseWindow();
    return 0;
}
