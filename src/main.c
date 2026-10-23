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
#define CHUNK_SIZE 16
#define WORLD_CHUNKS_X 16
#define WORLD_CHUNKS_Y 16
#define MAP_WIDTH  (CHUNK_SIZE * WORLD_CHUNKS_X)
#define MAP_HEIGHT (CHUNK_SIZE * WORLD_CHUNKS_Y)

#define MAX_NODOS 32
#define MAX_MSG_LEN 128
#define MAX_CARTELES 64
#define MAX_BOTONES_UI 16
#define ARCHIVO_GUARDADO "mundo_chunks.dat"

typedef struct {
    bool registrado;
    char nombre[32];
    Color color;
    bool es_solido;
} DefNodo;

typedef struct {
    bool generado;
    unsigned char casillas[CHUNK_SIZE][CHUNK_SIZE];
} Chunk;

typedef struct {
    bool activo;
    int x;
    int y;
    char texto[MAX_MSG_LEN];
} CartelMundo;

// Estructura de un Botón de la GUI creado desde Latino
typedef struct {
    bool activo;
    int id;
    char texto[48];
    Color color;
} BotonUI;

static DefNodo tabla_nodos[MAX_NODOS];
static Chunk mundo[WORLD_CHUNKS_Y][WORLD_CHUNKS_X];
static CartelMundo lista_carteles[MAX_CARTELES];

// Estado de la GUI controlada por Latino
static BotonUI botones_ui[MAX_BOTONES_UI];
static int total_botones_ui = 0;
static char titulo_ui[48] = "Panel LatinoUI";
static bool mostrar_panel_ui = true;
static int ultimo_boton_clicado = 0;

// Parámetros de comunicación C -> Latino
static int modo_ejecucion_latino = 0; // 0 = Init, 1 = Chunk, 2 = Evento GUI
static int chunk_solicitado_x = 0;
static int chunk_solicitado_y = 0;

// Estado del jugador y movimiento continuo
static int jugador_x = 36;
static int jugador_y = 36;
static int dir_mov_x = 0;
static int dir_mov_y = 0;
static bool modo_un_toque = true;
static float temporizador_paso = 0.0f;

// Estado de interfaz y telemetría
static int bloque_seleccionado = 5;
static bool mostrar_bordes_chunk = true;
static int ajuste_fuente = 0;

static double ultimo_tiempo_vm_ms = 0.0;
static int total_chunks_generados = 0;
static long ram_actual_kb = 0;
static char estado_sistema[96] = "Listo";

// ============================================================================
// UTILIDADES DE MEMORIA Y ACCESO ESPACIAL A CHUNKS
// ============================================================================

static long obtener_ram_kb(void)
{
    FILE *f = fopen("/proc/self/status", "r");
    if (f == NULL) return 0;

    char linea[256];
    long rss_kb = 0;
    while (fgets(linea, sizeof(linea), f) != NULL) {
        if (strncmp(linea, "VmRSS:", 6) == 0) {
            sscanf(linea + 6, "%ld", &rss_kb);
            break;
        }
    }
    fclose(f);
    return rss_kb;
}

static void establecer_casilla_mundo(int gx, int gy, int tipo)
{
    if (gx < 0 || gx >= MAP_WIDTH || gy < 0 || gy >= MAP_HEIGHT) return;
    if (tipo < 0 || tipo >= MAX_NODOS) return;

    int cx = gx / CHUNK_SIZE;
    int cy = gy / CHUNK_SIZE;
    int lx = gx % CHUNK_SIZE;
    int ly = gy % CHUNK_SIZE;

    mundo[cy][cx].casillas[ly][lx] = (unsigned char)tipo;
}

static int obtener_casilla_mundo(int gx, int gy)
{
    if (gx < 0 || gx >= MAP_WIDTH || gy < 0 || gy >= MAP_HEIGHT) return 1;

    int cx = gx / CHUNK_SIZE;
    int cy = gy / CHUNK_SIZE;
    int lx = gx % CHUNK_SIZE;
    int ly = gy % CHUNK_SIZE;

    if (!mundo[cy][cx].generado) return 0;
    return (int)mundo[cy][cx].casillas[ly][lx];
}

static const char *buscar_texto_cartel(int gx, int gy)
{
    for (int i = 0; i < MAX_CARTELES; i++) {
        if (lista_carteles[i].activo &&
            lista_carteles[i].x == gx &&
            lista_carteles[i].y == gy) {
            return lista_carteles[i].texto;
        }
    }
    return NULL;
}

// ============================================================================
// PUENTE FFI (C <-> LATINO): API "motor.*" (MUNDO + GUI)
// ============================================================================

static void motor_registrar_nodo(lat_mv *mv)
{
    lat_objeto *o_solido = latC_desapilar(mv);
    lat_objeto *o_b      = latC_desapilar(mv);
    lat_objeto *o_g      = latC_desapilar(mv);
    lat_objeto *o_r      = latC_desapilar(mv);
    lat_objeto *o_nombre = latC_desapilar(mv);
    lat_objeto *o_id     = latC_desapilar(mv);

    int id = (int)latC_checar_numerico(mv, o_id);
    const char *nombre = latC_checar_cadena(mv, o_nombre);
    int r = (int)latC_checar_numerico(mv, o_r);
    int g = (int)latC_checar_numerico(mv, o_g);
    int b = (int)latC_checar_numerico(mv, o_b);
    int solido = (int)latC_checar_numerico(mv, o_solido);

    if (id >= 0 && id < MAX_NODOS && nombre != NULL) {
        tabla_nodos[id].registrado = true;
        snprintf(tabla_nodos[id].nombre, sizeof(tabla_nodos[id].nombre), "%s", nombre);
        tabla_nodos[id].color = (Color){
            (unsigned char)r,
            (unsigned char)g,
            (unsigned char)b,
            255
        };
        tabla_nodos[id].es_solido = (solido != 0);
    }
}

static void motor_ui_titulo(lat_mv *mv)
{
    lat_objeto *o_texto = latC_desapilar(mv);
    const char *texto = latC_checar_cadena(mv, o_texto);
    if (texto != NULL) {
        snprintf(titulo_ui, sizeof(titulo_ui), "%s", texto);
    }
}

static void motor_ui_boton(lat_mv *mv)
{
    lat_objeto *o_b     = latC_desapilar(mv);
    lat_objeto *o_g     = latC_desapilar(mv);
    lat_objeto *o_r     = latC_desapilar(mv);
    lat_objeto *o_texto = latC_desapilar(mv);
    lat_objeto *o_id    = latC_desapilar(mv);

    int id = (int)latC_checar_numerico(mv, o_id);
    const char *texto = latC_checar_cadena(mv, o_texto);
    int r = (int)latC_checar_numerico(mv, o_r);
    int g = (int)latC_checar_numerico(mv, o_g);
    int b = (int)latC_checar_numerico(mv, o_b);

    if (total_botones_ui < MAX_BOTONES_UI && texto != NULL) {
        botones_ui[total_botones_ui].activo = true;
        botones_ui[total_botones_ui].id = id;
        snprintf(botones_ui[total_botones_ui].texto,
                 sizeof(botones_ui[total_botones_ui].texto), "%s", texto);
        botones_ui[total_botones_ui].color = (Color){
            (unsigned char)r,
            (unsigned char)g,
            (unsigned char)b,
            255
        };
        total_botones_ui++;
    }
}

static void motor_boton_clicado(lat_mv *mv)
{
    latC_apilar(mv, latC_crear_numerico(mv, (double)ultimo_boton_clicado));
}

static void motor_seleccionar_bloque(lat_mv *mv)
{
    lat_objeto *o_id = latC_desapilar(mv);
    int id = (int)latC_checar_numerico(mv, o_id);
    if (id >= 0 && id < MAX_NODOS && tabla_nodos[id].registrado) {
        bloque_seleccionado = id;
    }
}

static void motor_notificar(lat_mv *mv)
{
    lat_objeto *o_msg = latC_desapilar(mv);
    const char *msg = latC_checar_cadena(mv, o_msg);
    if (msg != NULL) {
        snprintf(estado_sistema, sizeof(estado_sistema), "%s", msg);
    }
}

static void motor_modo_carga(lat_mv *mv)
{
    latC_apilar(mv, latC_crear_numerico(mv, (double)modo_ejecucion_latino));
}

static void motor_chunk_x(lat_mv *mv)
{
    latC_apilar(mv, latC_crear_numerico(mv, (double)chunk_solicitado_x));
}

static void motor_chunk_y(lat_mv *mv)
{
    latC_apilar(mv, latC_crear_numerico(mv, (double)chunk_solicitado_y));
}

static void motor_poner_casilla(lat_mv *mv)
{
    lat_objeto *o_tipo = latC_desapilar(mv);
    lat_objeto *o_y    = latC_desapilar(mv);
    lat_objeto *o_x    = latC_desapilar(mv);

    int gx   = (int)latC_checar_numerico(mv, o_x);
    int gy   = (int)latC_checar_numerico(mv, o_y);
    int tipo = (int)latC_checar_numerico(mv, o_tipo);

    establecer_casilla_mundo(gx, gy, tipo);
}

static void motor_obtener_casilla(lat_mv *mv)
{
    lat_objeto *o_y = latC_desapilar(mv);
    lat_objeto *o_x = latC_desapilar(mv);

    int gx = (int)latC_checar_numerico(mv, o_x);
    int gy = (int)latC_checar_numerico(mv, o_y);

    int valor = obtener_casilla_mundo(gx, gy);
    latC_apilar(mv, latC_crear_numerico(mv, (double)valor));
}

static void motor_colocar_jugador(lat_mv *mv)
{
    lat_objeto *o_y = latC_desapilar(mv);
    lat_objeto *o_x = latC_desapilar(mv);

    int gx = (int)latC_checar_numerico(mv, o_x);
    int gy = (int)latC_checar_numerico(mv, o_y);

    if (gx >= 0 && gx < MAP_WIDTH && gy >= 0 && gy < MAP_HEIGHT) {
        jugador_x = gx;
        jugador_y = gy;
        dir_mov_x = 0;
        dir_mov_y = 0;
    }
}

static void motor_jugador_x(lat_mv *mv)
{
    latC_apilar(mv, latC_crear_numerico(mv, (double)jugador_x));
}

static void motor_jugador_y(lat_mv *mv)
{
    latC_apilar(mv, latC_crear_numerico(mv, (double)jugador_y));
}

static void motor_poner_cartel(lat_mv *mv)
{
    lat_objeto *o_texto = latC_desapilar(mv);
    lat_objeto *o_y     = latC_desapilar(mv);
    lat_objeto *o_x     = latC_desapilar(mv);

    int gx = (int)latC_checar_numerico(mv, o_x);
    int gy = (int)latC_checar_numerico(mv, o_y);
    const char *texto = latC_checar_cadena(mv, o_texto);

    if (gx >= 0 && gx < MAP_WIDTH && gy >= 0 && gy < MAP_HEIGHT && texto != NULL) {
        establecer_casilla_mundo(gx, gy, 3);
        for (int i = 0; i < MAX_CARTELES; i++) {
            if (!lista_carteles[i].activo ||
                (lista_carteles[i].x == gx && lista_carteles[i].y == gy)) {
                lista_carteles[i].activo = true;
                lista_carteles[i].x = gx;
                lista_carteles[i].y = gy;
                snprintf(lista_carteles[i].texto, MAX_MSG_LEN, "%s", texto);
                break;
            }
        }
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
    {"registrar_nodo",     motor_registrar_nodo,     6},
    {"ui_titulo",          motor_ui_titulo,          1},
    {"ui_boton",           motor_ui_boton,           5},
    {"boton_clicado",      motor_boton_clicado,      0},
    {"seleccionar_bloque", motor_seleccionar_bloque, 1},
    {"notificar",          motor_notificar,          1},
    {"modo_carga",         motor_modo_carga,         0},
    {"chunk_x",            motor_chunk_x,            0},
    {"chunk_y",            motor_chunk_y,            0},
    {"poner_casilla",      motor_poner_casilla,      3},
    {"obtener_casilla",    motor_obtener_casilla,    2},
    {"colocar_jugador",    motor_colocar_jugador,    2},
    {"jugador_x",          motor_jugador_x,          0},
    {"jugador_y",          motor_jugador_y,          0},
    {"poner_cartel",       motor_poner_cartel,       3},
    {"ruido",              motor_ruido,              3},
    {NULL, NULL, 0}
};

// ============================================================================
// EJECUCIÓN DE LATINO (INICIALIZACIÓN, CHUNKS Y EVENTOS GUI)
// ============================================================================

static void ejecutar_script_latino(int modo, int cx, int cy)
{
    double t0 = GetTime();
    modo_ejecucion_latino = modo;
    chunk_solicitado_x = cx;
    chunk_solicitado_y = cy;

    const char *ruta = NULL;
    if (FileExists("scripts/arranque.lat")) {
        ruta = "scripts/arranque.lat";
    } else if (FileExists("../scripts/arranque.lat")) {
        ruta = "../scripts/arranque.lat";
    }

    if (ruta == NULL) return;

    lat_mv *mv = latC_crear_mv();
    latC_abrir_liblatino(mv, "motor", libmotor);

    int status = 0;
    mv->nombre_archivo = (char *)ruta;
    mv->global->REPL = false;

    ast *nodo = latA_analizar_arch((char *)ruta, &status);
    if (status == 0 && nodo != NULL) {
        lat_objeto *main_func = latC_analizar(mv, nodo);
        latC_llamar_funcion(mv, main_func);
        latO_destruir(mv, main_func);
        latA_destruir(nodo);
    } else {
        printf("[Motor-C] Error al analizar %s\n", ruta);
    }

    latC_destruir_mv(mv);
    fflush(stdout);

    ultimo_tiempo_vm_ms = (GetTime() - t0) * 1000.0;
    ram_actual_kb = obtener_ram_kb();
}

static void generar_chunk_bajo_demanda(int cx, int cy)
{
    if (cx < 0 || cx >= WORLD_CHUNKS_X || cy < 0 || cy >= WORLD_CHUNKS_Y) return;
    if (mundo[cy][cx].generado) return;

    mundo[cy][cx].generado = true;
    memset(mundo[cy][cx].casillas, 0, sizeof(mundo[cy][cx].casillas));
    ejecutar_script_latino(1, cx, cy);
    total_chunks_generados++;
}

static void actualizar_chunks_cercanos(void)
{
    int jugador_cx = jugador_x / CHUNK_SIZE;
    int jugador_cy = jugador_y / CHUNK_SIZE;

    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            generar_chunk_bajo_demanda(jugador_cx + dx, jugador_cy + dy);
        }
    }
}

static void inicializar_mundo_con_latino(void)
{
    memset(mundo, 0, sizeof(mundo));
    memset(lista_carteles, 0, sizeof(lista_carteles));
    memset(tabla_nodos, 0, sizeof(tabla_nodos));
    memset(botones_ui, 0, sizeof(botones_ui));
    total_botones_ui = 0;
    total_chunks_generados = 0;
    dir_mov_x = 0;
    dir_mov_y = 0;

    tabla_nodos[0] = (DefNodo){ true, "Pasto",  (Color){175, 215, 155, 255}, false };
    tabla_nodos[1] = (DefNodo){ true, "Piedra", DARKGRAY, true };

    ejecutar_script_latino(0, 0, 0);
    actualizar_chunks_cercanos();
}

// ============================================================================
// PERSISTENCIA EN DISCO
// ============================================================================

static void guardar_mundo_disco(void)
{
    FILE *f = fopen(ARCHIVO_GUARDADO, "wb");
    if (f != NULL) {
        fwrite(&jugador_x, sizeof(int), 1, f);
        fwrite(&jugador_y, sizeof(int), 1, f);
        fwrite(&total_chunks_generados, sizeof(int), 1, f);
        fwrite(mundo, sizeof(mundo), 1, f);
        fwrite(lista_carteles, sizeof(lista_carteles), 1, f);
        fclose(f);
        snprintf(estado_sistema, sizeof(estado_sistema), "Mundo guardado en disco OK");
    }
}

static void cargar_mundo_disco(void)
{
    FILE *f = fopen(ARCHIVO_GUARDADO, "rb");
    if (f != NULL) {
        if (fread(&jugador_x, sizeof(int), 1, f) == 1 &&
            fread(&jugador_y, sizeof(int), 1, f) == 1 &&
            fread(&total_chunks_generados, sizeof(int), 1, f) == 1 &&
            fread(mundo, sizeof(mundo), 1, f) == 1 &&
            fread(lista_carteles, sizeof(lista_carteles), 1, f) == 1) {
            dir_mov_x = 0;
            dir_mov_y = 0;
            snprintf(estado_sistema, sizeof(estado_sistema), "Mundo cargado de disco OK");
        }
        fclose(f);
    } else {
        snprintf(estado_sistema, sizeof(estado_sistema), "Sin archivo previo");
    }
}

static bool casilla_es_transitable(int gx, int gy)
{
    if (gx < 0 || gx >= MAP_WIDTH || gy < 0 || gy >= MAP_HEIGHT) return false;
    int tipo = obtener_casilla_mundo(gx, gy);
    if (tipo >= 0 && tipo < MAX_NODOS && tabla_nodos[tipo].registrado) {
        return !tabla_nodos[tipo].es_solido;
    }
    return false;
}

static const char *nombre_bloque(int tipo)
{
    if (tipo >= 0 && tipo < MAX_NODOS && tabla_nodos[tipo].registrado) {
        return tabla_nodos[tipo].nombre;
    }
    return "Desconocido";
}

int main(void)
{
    setlocale(LC_ALL, "C.UTF-8");
    setlocale(LC_NUMERIC, "C");

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);

    InitWindow(1440, 900, "LatinoVox 2D - PoC Frontend GUI Interactiva desde Latino");
    if (!IsWindowReady()) {
        return 1;
    }

    SetWindowMinSize(800, 600);
    SetTargetFPS(60);

    inicializar_mundo_con_latino();

    Camera2D camara = { 0 };
    camara.rotation = 0.0f;
    camara.zoom = 1.35f;

    while (!WindowShouldClose()) {
        int screenWidth = GetScreenWidth();
        int screenHeight = GetScreenHeight();
        float dt = GetFrameTime();

        // Escalado dinámico de tipografía y dimensiones del Panel GUI
        float escala_ui = (float)screenWidth / 1360.0f;
        if (escala_ui < 1.0f) escala_ui = 1.0f;
        if (escala_ui > 2.2f) escala_ui = 2.2f;

        int font_hud = (int)(24.0f * escala_ui) + ajuste_fuente;
        if (font_hud < 18) font_hud = 18;

        int sep_linea = font_hud + 10;
        int alto_hud = sep_linea * 3 + 20;

        int ancho_panel_ui = (int)(340.0f * escala_ui);
        Rectangle rect_panel_ui = {
            (float)(screenWidth - ancho_panel_ui - 16),
            (float)(alto_hud + 16),
            (float)ancho_panel_ui,
            (float)(screenHeight - alto_hud - 32)
        };

        Vector2 pos_raton = GetMousePosition();
        bool raton_sobre_gui = (mostrar_panel_ui && CheckCollisionPointRec(pos_raton, rect_panel_ui)) ||
                               (pos_raton.y <= (float)alto_hud);

        // --- A. CONTROLES DE SISTEMA, FUENTE Y TECLA [TAB] PARA GUI ---
        if (IsKeyPressed(KEY_R)) inicializar_mundo_con_latino();
        if (IsKeyPressed(KEY_G)) guardar_mundo_disco();
        if (IsKeyPressed(KEY_C)) cargar_mundo_disco();
        if (IsKeyPressed(KEY_B)) mostrar_bordes_chunk = !mostrar_bordes_chunk;
        if (IsKeyPressed(KEY_TAB)) mostrar_panel_ui = !mostrar_panel_ui;
        if (IsKeyPressed(KEY_F11)) ToggleFullscreen();

        if (IsKeyPressed(KEY_M)) {
            modo_un_toque = !modo_un_toque;
            dir_mov_x = 0;
            dir_mov_y = 0;
        }

        if (IsKeyPressed(KEY_F1) && ajuste_fuente > -8) ajuste_fuente -= 2;
        if (IsKeyPressed(KEY_F2) && ajuste_fuente < 20) ajuste_fuente += 2;

        for (int k = 1; k <= 7; k++) {
            if (IsKeyPressed(KEY_ZERO + k) && tabla_nodos[k].registrado) {
                bloque_seleccionado = k;
            }
        }

        // --- B. GESTIÓN DE EVENTOS DE CLIC EN LA GUI (MODO 2 DE LATINO) ---
        if (mostrar_panel_ui && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            int btn_y = (int)rect_panel_ui.y + font_hud + 32;
            int alto_btn = font_hud + 22;

            for (int i = 0; i < total_botones_ui; i++) {
                if (!botones_ui[i].activo) continue;
                Rectangle rect_btn = {
                    rect_panel_ui.x + 14.0f,
                    (float)btn_y,
                    rect_panel_ui.width - 28.0f,
                    (float)alto_btn
                };
                if (CheckCollisionPointRec(pos_raton, rect_btn)) {
                    ultimo_boton_clicado = botones_ui[i].id;
                    ejecutar_script_latino(2, 0, 0);
                    actualizar_chunks_cercanos();
                    break;
                }
                btn_y += alto_btn + 10;
            }
        }

        // Control de Zoom
        float rueda = GetMouseWheelMove();
        if (rueda != 0.0f) camara.zoom += rueda * 0.15f;
        if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD)) camara.zoom += 0.2f;
        if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT)) camara.zoom -= 0.2f;
        if (IsKeyPressed(KEY_ZERO)) camara.zoom = 1.35f;
        if (camara.zoom < 0.35f) camara.zoom = 0.35f;
        if (camara.zoom > 4.0f)  camara.zoom = 4.0f;

        // --- C. MOVIMIENTO CONTINUO DEL JUGADOR ---
        if (modo_un_toque) {
            if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP)) {
                dir_mov_x = 0; dir_mov_y = -1; temporizador_paso = 1.0f;
            } else if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN)) {
                dir_mov_x = 0; dir_mov_y = 1;  temporizador_paso = 1.0f;
            } else if (IsKeyPressed(KEY_A) || IsKeyPressed(KEY_LEFT)) {
                dir_mov_x = -1; dir_mov_y = 0; temporizador_paso = 1.0f;
            } else if (IsKeyPressed(KEY_D) || IsKeyPressed(KEY_RIGHT)) {
                dir_mov_x = 1; dir_mov_y = 0;  temporizador_paso = 1.0f;
            } else if (IsKeyPressed(KEY_SPACE)) {
                dir_mov_x = 0; dir_mov_y = 0;
            }
        } else {
            dir_mov_x = 0; dir_mov_y = 0;
            if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))         dir_mov_y = -1;
            else if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN))  dir_mov_y = 1;
            else if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))  dir_mov_x = -1;
            else if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) dir_mov_x = 1;
        }

        float intervalo_paso = (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) ? 0.055f : 0.11f;

        if (dir_mov_x != 0 || dir_mov_y != 0) {
            temporizador_paso += dt;
            if (temporizador_paso >= intervalo_paso) {
                temporizador_paso = 0.0f;
                int nx = jugador_x + dir_mov_x;
                int ny = jugador_y + dir_mov_y;

                generar_chunk_bajo_demanda(nx / CHUNK_SIZE, ny / CHUNK_SIZE);

                if (casilla_es_transitable(nx, ny)) {
                    jugador_x = nx;
                    jugador_y = ny;
                    actualizar_chunks_cercanos();
                } else if (modo_un_toque) {
                    dir_mov_x = 0;
                    dir_mov_y = 0;
                }
            }
        } else {
            temporizador_paso = intervalo_paso;
        }

        camara.offset = (Vector2){ screenWidth / 2.0f, screenHeight / 2.0f };
        camara.target = (Vector2){
            jugador_x * TILE_SIZE + TILE_SIZE / 2.0f,
            jugador_y * TILE_SIZE + TILE_SIZE / 2.0f
        };

        // --- D. INTERACCIÓN CON EL MUNDO (SOLO SI EL RATÓN NO ESTÁ SOBRE LA GUI) ---
        Vector2 pos_mundo_raton = GetScreenToWorld2D(pos_raton, camara);
        int raton_tx = (int)(pos_mundo_raton.x / TILE_SIZE);
        int raton_ty = (int)(pos_mundo_raton.y / TILE_SIZE);

        if (!raton_sobre_gui &&
            raton_tx > 0 && raton_tx < MAP_WIDTH - 1 &&
            raton_ty > 0 && raton_ty < MAP_HEIGHT - 1) {
            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                establecer_casilla_mundo(raton_tx, raton_ty, 0);
            } else if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
                if (raton_tx != jugador_x || raton_ty != jugador_y) {
                    establecer_casilla_mundo(raton_tx, raton_ty, bloque_seleccionado);
                }
            }
        }

        // --- E. VIEW CULLING Y RENDERIZADO DEL MUNDO ---
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

        BeginDrawing();
        ClearBackground((Color){ 18, 22, 28, 255 });

        BeginMode2D(camara);

        for (int gy = min_y; gy <= max_y; gy++) {
            for (int gx = min_x; gx <= max_x; gx++) {
                int cx = gx / CHUNK_SIZE;
                int cy = gy / CHUNK_SIZE;
                int px = gx * TILE_SIZE;
                int py = gy * TILE_SIZE;

                if (!mundo[cy][cx].generado) {
                    DrawRectangle(px, py, TILE_SIZE, TILE_SIZE, (Color){ 30, 35, 45, 255 });
                    continue;
                }

                int tipo = obtener_casilla_mundo(gx, gy);
                Color color_base = tabla_nodos[0].color;
                if (tipo >= 0 && tipo < MAX_NODOS && tabla_nodos[tipo].registrado) {
                    color_base = tabla_nodos[tipo].color;
                }

                if (tipo == 3 || tipo == 4) {
                    DrawRectangle(px, py, TILE_SIZE, TILE_SIZE, tabla_nodos[0].color);
                    DrawRectangle(px + 10, py + 10, TILE_SIZE - 20, TILE_SIZE - 20, color_base);
                } else {
                    DrawRectangle(px, py, TILE_SIZE, TILE_SIZE, color_base);
                }
            }
        }

        if (mostrar_bordes_chunk) {
            int min_cx = min_x / CHUNK_SIZE;
            int max_cx = max_x / CHUNK_SIZE;
            int min_cy = min_y / CHUNK_SIZE;
            int max_cy = max_y / CHUNK_SIZE;
            int px_chunk = CHUNK_SIZE * TILE_SIZE;

            for (int cy = min_cy; cy <= max_cy; cy++) {
                for (int cx = min_cx; cx <= max_cx; cx++) {
                    Color c_borde = mundo[cy][cx].generado ? SKYBLUE : DARKGRAY;
                    DrawRectangleLines(cx * px_chunk, cy * px_chunk, px_chunk, px_chunk, c_borde);
                }
            }
        }

        if (!raton_sobre_gui &&
            raton_tx > 0 && raton_tx < MAP_WIDTH - 1 &&
            raton_ty > 0 && raton_ty < MAP_HEIGHT - 1) {
            DrawRectangleLines(raton_tx * TILE_SIZE, raton_ty * TILE_SIZE, TILE_SIZE, TILE_SIZE, YELLOW);
        }

        DrawCircle(
            jugador_x * TILE_SIZE + TILE_SIZE / 2,
            jugador_y * TILE_SIZE + TILE_SIZE / 2,
            TILE_SIZE * 0.35f,
            RED
        );

        EndMode2D();

        // --- F. HUD SUPERIOR ---
        int j_cx = jugador_x / CHUNK_SIZE;
        int j_cy = jugador_y / CHUNK_SIZE;

        DrawRectangle(0, 0, screenWidth, alto_hud, (Color){ 0, 0, 0, 225 });
        DrawText(
            "WASD: Caminar | [ESPACIO] Parar | [TAB] Ocultar/Ver GUI | Clic Der: Poner | Clic Izq: Quitar",
            20, 12, font_hud, RAYWHITE
        );
        DrawText(
            TextFormat("FPS: %d | Pos: (%d,%d) Chunk: (%d,%d) | Chunks: %d/256 | VM Evento: %.2f ms",
                       GetFPS(), jugador_x, jugador_y, j_cx, j_cy,
                       total_chunks_generados, ultimo_tiempo_vm_ms),
            20, 12 + sep_linea, font_hud, GREEN
        );
        DrawText(
            TextFormat("Activo: [%d:%s] | %s",
                       bloque_seleccionado, nombre_bloque(bloque_seleccionado), estado_sistema),
            20, 12 + sep_linea * 2, font_hud, SKYBLUE
        );

        // --- G. RENDERIZADO DEL PANEL LATERAL GUI (DEFINIDO EN LATINO) ---
        if (mostrar_panel_ui) {
            DrawRectangleRec(rect_panel_ui, (Color){ 15, 20, 28, 235 });
            DrawRectangleLinesEx(rect_panel_ui, 2.0f, SKYBLUE);

            DrawText(titulo_ui, (int)rect_panel_ui.x + 18, (int)rect_panel_ui.y + 12, font_hud, GOLD);

            int btn_y = (int)rect_panel_ui.y + font_hud + 32;
            int alto_btn = font_hud + 22;

            for (int i = 0; i < total_botones_ui; i++) {
                if (!botones_ui[i].activo) continue;

                Rectangle rect_btn = {
                    rect_panel_ui.x + 14.0f,
                    (float)btn_y,
                    rect_panel_ui.width - 28.0f,
                    (float)alto_btn
                };

                bool hover = CheckCollisionPointRec(pos_raton, rect_btn);
                bool seleccionado = (botones_ui[i].id == bloque_seleccionado);

                Color c_fondo = botones_ui[i].color;
                if (hover) {
                    // Iluminar el botón al pasar el cursor encima
                    c_fondo.r = (unsigned char)((c_fondo.r + 45 > 255) ? 255 : c_fondo.r + 45);
                    c_fondo.g = (unsigned char)((c_fondo.g + 45 > 255) ? 255 : c_fondo.g + 45);
                    c_fondo.b = (unsigned char)((c_fondo.b + 45 > 255) ? 255 : c_fondo.b + 45);
                }

                DrawRectangleRec(rect_btn, c_fondo);
                DrawRectangleLinesEx(rect_btn, seleccionado ? 3.0f : 1.5f,
                                     (hover || seleccionado) ? YELLOW : RAYWHITE);

                DrawText(botones_ui[i].texto,
                         (int)rect_btn.x + 14,
                         (int)rect_btn.y + (alto_btn - font_hud) / 2,
                         font_hud - 2,
                         RAYWHITE);

                btn_y += alto_btn + 10;
            }
        }

        // Cuadro de lectura de carteles en la parte inferior
        if (obtener_casilla_mundo(jugador_x, jugador_y) == 3) {
            const char *msg = buscar_texto_cartel(jugador_x, jugador_y);
            if (msg != NULL) {
                int alto_caja = font_hud * 2 + 36;
                int boxY = screenHeight - alto_caja - 20;
                int ancho_caja = mostrar_panel_ui ? (int)rect_panel_ui.x - 45 : screenWidth - 60;
                DrawRectangle(30, boxY, ancho_caja, alto_caja, (Color){ 20, 20, 20, 240 });
                DrawRectangleLines(30, boxY, ancho_caja, alto_caja, GOLD);
                DrawText(msg, 50, boxY + (alto_caja - font_hud) / 2, font_hud, YELLOW);
            }
        }

        EndDrawing();
    }

    CloseWindow();
    return 0;
}