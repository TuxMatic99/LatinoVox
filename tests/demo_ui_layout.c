
#include "raylib.h"
#include "ui_layout.h"

int main(void)
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(900, 550, "LatinoUI - Demo de Layout");
    SetTargetFPS(60);

    UIDireccion direccion = UI_FILA;

    const UIElemento elementos[2] = {
        {120.0f, 1.0f},
        {120.0f, 1.0f}
    };

    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_SPACE)) {
            direccion = direccion == UI_FILA
                ? UI_COLUMNA : UI_FILA;
        }

        int ancho = GetScreenWidth();
        int alto = GetScreenHeight();

        UIRect contenedor = {
            30.0f,
            100.0f,
            (float)ancho - 60.0f,
            (float)alto - 140.0f
        };

        UIRect posiciones[2];

        UIResultado resultado = ui_distribuir(
            contenedor,
            direccion,
            15.0f,
            15.0f,
            elementos,
            2,
            posiciones
        );

        BeginDrawing();
        ClearBackground(RAYWHITE);

        DrawText("LatinoUI - Motor de Maquetacion",
                 30, 25, 24, DARKBLUE);

        DrawText("ESPACIO: alternar fila / columna",
                 30, 60, 18, DARKGRAY);

        if (resultado == UI_LAYOUT_OK) {
            Color colores[2] = { SKYBLUE, LIME };

            for (int i = 0; i < 2; i++) {
                DrawRectangleRec(
                    (Rectangle){
                        posiciones[i].x,
                        posiciones[i].y,
                        posiciones[i].ancho,
                        posiciones[i].alto
                    },
                    colores[i]
                );
            }

        } else {
            DrawText("Espacio insuficiente para el layout",
                     30, 120, 20, RED);
        }

        EndDrawing();
    }

    CloseWindow();
    return 0;
}
