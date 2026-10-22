#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include "raylib.h"

// Cabeceras de Latino-Core
#include "latino.h"
#include "latast.h"
#include "latmv.h"
#include "latobj.h"

#define TILE_SIZE 48
#define MAP_WIDTH 30
#define MAP_HEIGHT 30
#define MAX_MSG_LEN 128
#define ARCHIVO_GUARDADO "mundo.dat"

// Tipos de casilla:
// 0 = Pasto (transitable)
// 1 = Muro de piedra (bloquea)
// 2 = Agua profunda (bloquea)
// 3 = Cartel interactivo (transitable)
// 4 = Árbol / Bosque (bloquea)
// 5 = Camino de tierra / Puente (transitable)
static int mapa[MAP_HEIGHT][MAP_WIDTH];
static char carteles[MAP_HEIGHT][MAP_WIDTH][MAX_MSG_LEN];

static int jugador_x = 2;
static int jugador_y = 2;
static double ultimo_tiempo_latino_ms = 0.0;
static int contador_recargas = 0;

// Estado de construcción y persistencia
static int bloque_seleccionado = 5; // Por defecto: 5 (Camino/Puente)
static char estado_archivo[64] = "Listo";

// ============================================================================
// PUENTE FFI (C <-> LATINO): API "motor.*"
// ============================================================================

static void motor_poner_casilla(lat_mv *mv)
{
    lat_objeto *o_tipo = latC_desapilar(mv);
    lat_objeto *o_y    = latC_desapilar(mv);
    lat_objeto *o_x    = latC_desapilar(mv);

    int x    = (int)latC_checar_numerico(mv, o_x);
    int y    = (int)latC_checar_numerico(mv, o_y);
    int tipo = (int)latC_checar_numerico(mv, o_tipo);

    if (x >= 0 && x < MAP_WIDTH && y >= 0 && y < MAP_HEIGHT) {
        mapa[y][x] = tipo;
    }
}

static void motor_obtener_casilla(lat_mv *mv)
{
    lat_objeto *o_y = latC_desapilar(mv);
    lat_objeto *o_x = latC_desapilar(mv);

    int x = (int)latC_checar_numerico(mv, o_x);
    int y = (int)latC_checar_numerico(mv, o_y);

    int valor = 1;
    if (x >= 0 && x < MAP_WIDTH && y >= 0 && y < MAP_HEIGHT) {
        valor = mapa[y][x];
    }
    latC_apilar(mv, latC_crear_numerico(mv, (double)valor));
}

static void motor_colocar_jugador(lat_mv *mv)
{
    lat_objeto *o_y = latC_desapilar(mv);
    lat_objeto *o_x = latC_desapilar(mv);

    int x = (int)latC_checar_numerico(mv, o_x);
    int y = (int)latC_checar_numerico(mv, o_y);

    if (x >= 0 && x < MAP_WIDTH && y >= 0 && y < MAP_HEIGHT) {
        jugador_x = x;
        jugador_y = y;
    }
}

static void motor_poner_cartel(lat_mv *mv)
{
    lat_objeto *o_texto = latC_desapilar(mv);
    lat_objeto *o_y     = latC_desapilar(mv);
    lat_objeto *o_x     = latC_desapilar(mv);

    int x = (int)latC_checar_numerico(mv, o_x);
    int y = (int)latC_checar_numerico(mv, o_y);
    const char *texto = latC_checar_cadena(mv, o_texto);

    if (x >= 0 && x < MAP_WIDTH && y >= 0 && y < MAP_HEIGHT && texto != NULL) {
        mapa[y][x] = 3;
        snprintf(carteles[y][x], MAX_MSG_LEN, "%s", texto);
    }
}

static void motor_ruido(lat_mv *mv)
{
    lat_objeto *o_semilla = latC_desapilar(mv);
    lat_objeto *o_y       = latC_desapilar(mv);
    lat_objeto *o_x       = latC_desapilar(mv);

    unsigned int x = (unsigned int)latC_checar_numerico(mv, o_x);
    unsigned int y = (unsigned int)latC_checar_numerico(mv, o_y);
    unsigned int s = (unsigned int)latC_checar_numerico(mv, o_semilla);

    unsigned int h = x * 374761393U + y * 668265263U + s * 1442695041U;
    h = (h ^ (h >> 13)) * 1274126177U;
    int resultado = (int)((h ^ (h >> 16)) % 100U);

    latC_apilar(mv, latC_crear_numerico(mv, (double)resultado));
}

static const lat_CReg libmotor[] = {
    {"poner_casilla",   motor_poner_casilla,   3},
    {"obtener_casilla", motor_obtener_casilla, 2},
    {"colocar_jugador", motor_colocar_jugador, 2},
    {"poner_cartel",    motor_poner_cartel,    3},
    {"ruido",           motor_ruido,           3},
    {NULL, NULL, 0}
};

// ============================================================================
// PERSISTENCIA Y CARGA DE SCRIPTS
// ============================================================================

static void guardar_mundo_disco(void)
{
    FILE *f = fopen(ARCHIVO_GUARDADO, "wb");
    if (f != NULL) {
        fwrite(&jugador_x, sizeof(int), 1, f);
        fwrite(&jugador_y, sizeof(int), 1, f);
        fwrite(mapa, sizeof(mapa), 1, f);
        fwrite(carteles, sizeof(carteles), 1, f);
        fclose(f);
        snprintf(estado_archivo, sizeof(estado_archivo), "Mundo guardado en %s", ARCHIVO_GUARDADO);
    } else {
        snprintf(estado_archivo, sizeof(estado_archivo), "Error al guardar");
    }
}

static void cargar_mundo_disco(void)
{
    FILE *f = fopen(ARCHIVO_GUARDADO, "rb");
    if (f != NULL) {
        if (fread(&jugador_x, sizeof(int), 1, f) == 1 &&
            fread(&jugador_y, sizeof(int), 1, f) == 1 &&
            fread(mapa, sizeof(mapa), 1, f) == 1 &&
            fread(carteles, sizeof(carteles), 1, f) == 1) {
            snprintf(estado_archivo, sizeof(estado_archivo), "Mundo cargado desde %s", ARCHIVO_GUARDADO);
        }
        fclose(f);
    } else {
        snprintf(estado_archivo, sizeof(estado_archivo), "No existe %s aun", ARCHIVO_GUARDADO);
    }
}

static void cargar_mundo_desde_latino(void)
{
    double t_inicio = GetTime();

    memset(carteles, 0, sizeof(carteles));
    for (int y = 0; y < MAP_HEIGHT; y++) {
        for (int x = 0; x < MAP_WIDTH; x++) {
            if (x == 0 || x == MAP_WIDTH - 1 || y == 0 || y == MAP_HEIGHT - 1) {
                mapa[y][x] = 1;
            } else {
                mapa[y][x] = 0;
            }
        }
    }

    lat_mv *mv = latC_crear_mv();
    latC_abrir_liblatino(mv, "motor", libmotor);

    const char *ruta = NULL;
    if (FileExists("scripts/arranque.lat")) {
        ruta = "scripts/arranque.lat";
    } else if (FileExists("../scripts/arranque.lat")) {
        ruta = "../scripts/arranque.lat";
    }

    if (ruta != NULL) {
        int status = 0;
        mv->nombre_archivo = (char *)ruta;
        mv->global->REPL = false;

        ast *nodo = latA_analizar_arch((char *)ruta, &status);
        if (status == 0 && nodo != NULL) {
            lat_objeto *main_func = latC_analizar(mv, nodo);
            latC_llamar_funcion(mv, main_func);
            latO_destruir(mv, main_func);
            latA_destruir(nodo);
        }
    }

    latC_destruir_mv(mv);

    ultimo_tiempo_latino_ms = (GetTime() - t_inicio) * 1000.0;
    contador_recargas++;
    snprintf(estado_archivo, sizeof(estado_archivo), "Regenerado por Latino (#%d)", contador_recargas);
}

static bool casilla_es_transitable(int x, int y)
{
    if (x < 0 || x >= MAP_WIDTH || y < 0 || y >= MAP_HEIGHT) {
        return false;
    }
    int t = mapa[y][x];
    return (t == 0 || t == 3 || t == 5);
}

static const char *nombre_bloque(int tipo)
{
    switch (tipo) {
        case 1: return "1:Muro";
        case 2: return "2:Agua";
        case 4: return "4:Arbol";
        case 5: return "5:Camino";
        default: return "Pasto";
    }
}

int main(void)
{
    setlocale(LC_ALL, "C.UTF-8");
    setlocale(LC_NUMERIC, "C");

    // Solo FLAG_WINDOW_RESIZABLE para evitar la doble espera con SetTargetFPS(60)
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);

    InitWindow(1280, 800, "LatinoVox 2D - View Culling, Edicion y Persistencia");
    if (!IsWindowReady()) {
        return 1;
    }

    SetWindowMinSize(640, 480);
    SetTargetFPS(60);

    cargar_mundo_desde_latino();

    Camera2D camara = { 0 };
    camara.rotation = 0.0f;
    camara.zoom = 1.5f;

    while (!WindowShouldClose()) {
        int screenWidth = GetScreenWidth();
        int screenHeight = GetScreenHeight();

        // --- A. CONTROLES DE SISTEMA Y PERSISTENCIA ---
        if (IsKeyPressed(KEY_R)) cargar_mundo_desde_latino();
        if (IsKeyPressed(KEY_G)) guardar_mundo_disco();
        if (IsKeyPressed(KEY_C)) cargar_mundo_disco();
        if (IsKeyPressed(KEY_F11)) ToggleFullscreen();

        // Selección de bloque para construir (Teclas 1, 2, 4, 5)
        if (IsKeyPressed(KEY_ONE))   bloque_seleccionado = 1;
        if (IsKeyPressed(KEY_TWO))   bloque_seleccionado = 2;
        if (IsKeyPressed(KEY_FOUR))  bloque_seleccionado = 4;
        if (IsKeyPressed(KEY_FIVE))  bloque_seleccionado = 5;

        // Control de Zoom
        float rueda = GetMouseWheelMove();
        if (rueda != 0.0f) camara.zoom += rueda * 0.15f;
        if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD)) camara.zoom += 0.2f;
        if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT)) camara.zoom -= 0.2f;
        if (IsKeyPressed(KEY_ZERO)) camara.zoom = 1.5f;
        if (camara.zoom < 0.5f) camara.zoom = 0.5f;
        if (camara.zoom > 4.0f) camara.zoom = 4.0f;

        // --- B. MOVIMIENTO DEL JUGADOR ---
        int nx = jugador_x;
        int ny = jugador_y;

        if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP))    ny--;
        if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN))  ny++;
        if (IsKeyPressed(KEY_A) || IsKeyPressed(KEY_LEFT))  nx--;
        if (IsKeyPressed(KEY_D) || IsKeyPressed(KEY_RIGHT)) nx++;

        if (casilla_es_transitable(nx, ny)) {
            jugador_x = nx;
            jugador_y = ny;
        }

        camara.offset = (Vector2){ screenWidth / 2.0f, screenHeight / 2.0f };
        camara.target = (Vector2){
            jugador_x * TILE_SIZE + TILE_SIZE / 2.0f,
            jugador_y * TILE_SIZE + TILE_SIZE / 2.0f
        };

        // --- C. INTERACCIÓN CON EL RATÓN (COLOCAR / QUITAR BLOQUES) ---
        Vector2 pos_mundo_raton = GetScreenToWorld2D(GetMousePosition(), camara);
        int raton_tx = (int)(pos_mundo_raton.x / TILE_SIZE);
        int raton_ty = (int)(pos_mundo_raton.y / TILE_SIZE);

        // Permitir modificar cualquier casilla interior (respetando el borde exterior)
        if (raton_tx > 0 && raton_tx < MAP_WIDTH - 1 &&
            raton_ty > 0 && raton_ty < MAP_HEIGHT - 1) {
            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                mapa[raton_ty][raton_tx] = 0; // Quitar bloque -> Pasto
            } else if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
                // Evitar encerrar al propio jugador bajo un muro
                if (raton_tx != jugador_x || raton_ty != jugador_y) {
                    mapa[raton_ty][raton_tx] = bloque_seleccionado;
                }
            }
        }

        // --- D. CÁLCULO DE VIEW CULLING (SOLO DIBUJAR LO VISIBLE EN CÁMARA) ---
        Vector2 esq_sup_izq = GetScreenToWorld2D((Vector2){ 0, 0 }, camara);
        Vector2 esq_inf_der = GetScreenToWorld2D((Vector2){ (float)screenWidth, (float)screenHeight }, camara);

        int min_x = (int)(esq_sup_izq.x / TILE_SIZE) - 1;
        int min_y = (int)(esq_sup_izq.y / TILE_SIZE) - 1;
        int max_x = (int)(esq_inf_der.x / TILE_SIZE) + 1;
        int max_y = (int)(esq_inf_der.y / TILE_SIZE) + 1;

        if (min_x < 0) min_x = 0;
        if (min_y < 0) min_y = 0;
        if (max_x >= MAP_WIDTH)  max_x = MAP_WIDTH - 1;
        if (max_y >= MAP_HEIGHT) max_y = MAP_HEIGHT - 1;

        int casillas_dibujadas = 0;

        // --- E. RENDERIZADO OPTIMIZADO ---
        BeginDrawing();
        ClearBackground((Color){ 25, 30, 35, 255 });

        BeginMode2D(camara);

        for (int y = min_y; y <= max_y; y++) {
            for (int x = min_x; x <= max_x; x++) {
                int px = x * TILE_SIZE;
                int py = y * TILE_SIZE;
                int tipo = mapa[y][x];
                casillas_dibujadas++;

                if (tipo == 1) {
                    DrawRectangle(px, py, TILE_SIZE, TILE_SIZE, DARKGRAY);
                } else if (tipo == 2) {
                    DrawRectangle(px, py, TILE_SIZE, TILE_SIZE, (Color){ 65, 155, 225, 255 });
                } else if (tipo == 5) {
                    DrawRectangle(px, py, TILE_SIZE, TILE_SIZE, (Color){ 210, 180, 140, 255 });
                } else {
                    DrawRectangle(px, py, TILE_SIZE, TILE_SIZE, (Color){ 175, 215, 155, 255 });

                    if (tipo == 3) {
                        DrawRectangle(px + 12, py + 12, TILE_SIZE - 24, TILE_SIZE - 24, BROWN);
                    } else if (tipo == 4) {
                        // Árbol optimizado en GPU (rectángulo de copa + tronco)
                        DrawRectangle(px + 8, py + 8, TILE_SIZE - 16, TILE_SIZE - 16, DARKGREEN);
                    }
                }
            }
        }

        // Cursor selector del ratón
        if (raton_tx > 0 && raton_tx < MAP_WIDTH - 1 &&
            raton_ty > 0 && raton_ty < MAP_HEIGHT - 1) {
            DrawRectangleLines(raton_tx * TILE_SIZE, raton_ty * TILE_SIZE, TILE_SIZE, TILE_SIZE, YELLOW);
        }

        // Jugador
        DrawCircle(
            jugador_x * TILE_SIZE + TILE_SIZE / 2,
            jugador_y * TILE_SIZE + TILE_SIZE / 2,
            TILE_SIZE * 0.35f,
            RED
        );

        EndMode2D();

        // --- F. HUD DE TELEMETRÍA Y EDICIÓN ---
        DrawRectangle(0, 0, screenWidth, 95, (Color){ 0, 0, 0, 205 });
        DrawText(
            "Mover: WASD | Clic Izq: Quitar | Clic Der: Poner | Bloques: [1]Muro [2]Agua [4]Arbol [5]Camino",
            18, 10, 17, RAYWHITE
        );
        DrawText(
            TextFormat("FPS: %d | Culling: %d/900 casillas | VM Latino: %.2f ms | Bloque Activo: [%s]",
                       GetFPS(), casillas_dibujadas, ultimo_tiempo_latino_ms, nombre_bloque(bloque_seleccionado)),
            18, 38, 17, GREEN
        );
        DrawText(
            TextFormat("Persistencia: [G] Guardar | [C] Cargar | [R] Regenerar con Latino | Estado: %s",
                       estado_archivo),
            18, 66, 17, SKYBLUE
        );

        if (mapa[jugador_y][jugador_x] == 3) {
            int boxY = screenHeight - 90;
            DrawRectangle(40, boxY, screenWidth - 80, 68, (Color){ 20, 20, 20, 235 });
            DrawRectangleLines(40, boxY, screenWidth - 80, 68, GOLD);
            DrawText(carteles[jugador_y][jugador_x], 60, boxY + 24, 20, YELLOW);
        }

        EndDrawing();
    }

    CloseWindow();
    return 0;
}