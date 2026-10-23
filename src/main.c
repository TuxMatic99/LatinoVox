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
#define MAX_WIDGETS_UI 48
#define MAX_PESTANAS_UI 6
#define ARCHIVO_GUARDADO "mundo_chunks.dat"

typedef enum {
    WIDGET_ETIQUETA = 0,
    WIDGET_BOTON,
    WIDGET_CAJA_TEXTO,
    WIDGET_DESLIZADOR,
    WIDGET_INTERRUPTOR,
    WIDGET_PROGRESO
} TipoWidget;

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

typedef struct {
    bool activa;
    int id;
    char titulo[32];
} PestanaUI;

typedef struct {
    bool activo;
    TipoWidget tipo;
    int pestana;
    int id;
    char etiqueta[48];
    char texto[64];
    char ayuda[128];
    Color color;
    int min_val;
    int max_val;
    int valor;
} WidgetUI;

typedef struct {
    Color fondo_panel;
    Color borde_panel;
    Color texto_titulo;
    Color texto_normal;
    Color texto_etiqueta;
    Color fondo_widget;
} TemaUI;

static DefNodo tabla_nodos[MAX_NODOS];
static Chunk mundo[WORLD_CHUNKS_Y][WORLD_CHUNKS_X];
static CartelMundo lista_carteles[MAX_CARTELES];

// Estado del Frontend GUI y Pestañas
static PestanaUI pestanas_ui[MAX_PESTANAS_UI];
static int total_pestanas_ui = 0;
static int pestana_actual = 0;
static int pestana_en_construccion = 0;

static WidgetUI widgets_ui[MAX_WIDGETS_UI];
static int total_widgets_ui = 0;
static char titulo_ui[48] = "LatinoUI Studio";
static bool mostrar_panel_ui = true;
static int ultimo_evento_id = 0;
static int id_caja_activa = -1;
static int id_slider_arrastrando = -1;
static float scroll_panel_y = 0.0f;
static int tema_actual = 0;

// Estado de Ventana Modal Emergente
static bool modal_activo = false;
static char modal_titulo[64] = "";
static char modal_mensaje[256] = "";

// Notificación Flotante (Toast UX)
static char texto_toast[128] = "";
static float tiempo_toast = 0.0f;

// Parámetros de comunicación C -> Latino
static int modo_ejecucion_latino = 0;
static int chunk_solicitado_x = 0;
static int chunk_solicitado_y = 0;
static int semilla_mundo = 2026;

// Estado del jugador, pincel, minimapa y movimiento
static int jugador_x = 36;
static int jugador_y = 36;
static int dir_mov_x = 0;
static int dir_mov_y = 0;
static bool modo_un_toque = true;
static float temporizador_paso = 0.0f;
static int tamano_pincel = 1;
static bool mostrar_minimapa = true;

// Estado de interfaz y Auto Hot-Reload
static int bloque_seleccionado = 5;
static bool mostrar_bordes_chunk = true;
static int ajuste_fuente = 0;
static long ultima_modificacion_script = 0;
static float temporizador_vigilancia = 0.0f;

static double ultimo_tiempo_vm_ms = 0.0;
static int total_chunks_generados = 0;
static long ram_actual_kb = 0;
static char estado_sistema[96] = "Listo";

static const TemaUI paleta_temas[3] = {
    // 0: Tema Oscuro (Studio)
    { {16, 20, 28, 242}, {102, 191, 255, 255}, {255, 203, 0, 255}, {245, 245, 245, 255}, {102, 191, 255, 255}, {28, 34, 46, 255} },
    // 1: Tema Claro (Papel)
    { {235, 238, 245, 245}, {50, 90, 150, 255}, {25, 55, 110, 255}, {25, 30, 38, 255}, {40, 100, 170, 255}, {210, 216, 228, 255} },
    // 2: Tema Esmeralda (Retro Terminal)
    { {10, 28, 22, 242}, {45, 220, 145, 255}, {120, 255, 180, 255}, {220, 255, 235, 255}, {80, 235, 160, 255}, {18, 48, 36, 255} }
};

// Declaraciones anticipadas
static void ejecutar_script_latino(int modo, int cx, int cy);
static void actualizar_chunks_cercanos(void);

// ============================================================================
// UTILIDADES DE MEMORIA, ARCHIVOS Y ACCESO ESPACIAL A CHUNKS
// ============================================================================

static const char *obtener_ruta_script(void)
{
    if (FileExists("scripts/arranque.lat")) return "scripts/arranque.lat";
    if (FileExists("../scripts/arranque.lat")) return "../scripts/arranque.lat";
    return NULL;
}

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

static void mostrar_notificacion_toast(const char *msg)
{
    if (msg == NULL) return;
    snprintf(texto_toast, sizeof(texto_toast), "%s", msg);
    snprintf(estado_sistema, sizeof(estado_sistema), "%s", msg);
    tiempo_toast = 3.5f;
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

static WidgetUI *buscar_widget_por_id(int id)
{
    for (int i = 0; i < total_widgets_ui; i++) {
        if (widgets_ui[i].activo && widgets_ui[i].id == id) {
            return &widgets_ui[i];
        }
    }
    return NULL;
}

// ============================================================================
// PUENTE FFI (C <-> LATINO): API "motor.*" (MUNDO + FRONTEND GUI + UX)
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
            (unsigned char)r, (unsigned char)g, (unsigned char)b, 255
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

static void motor_ui_pestana(lat_mv *mv)
{
    lat_objeto *o_titulo = latC_desapilar(mv);
    lat_objeto *o_id     = latC_desapilar(mv);

    int id = (int)latC_checar_numerico(mv, o_id);
    const char *titulo = latC_checar_cadena(mv, o_titulo);

    if (id >= 0 && id < MAX_PESTANAS_UI && titulo != NULL) {
        pestanas_ui[id].activa = true;
        pestanas_ui[id].id = id;
        snprintf(pestanas_ui[id].titulo, sizeof(pestanas_ui[id].titulo), "%s", titulo);
        if (id + 1 > total_pestanas_ui) {
            total_pestanas_ui = id + 1;
        }
        pestana_en_construccion = id;
    }
}

static void motor_ui_etiqueta(lat_mv *mv)
{
    lat_objeto *o_texto = latC_desapilar(mv);
    const char *texto = latC_checar_cadena(mv, o_texto);
    if (total_widgets_ui < MAX_WIDGETS_UI && texto != NULL) {
        WidgetUI *w = &widgets_ui[total_widgets_ui++];
        memset(w, 0, sizeof(WidgetUI));
        w->activo = true;
        w->tipo = WIDGET_ETIQUETA;
        w->pestana = pestana_en_construccion;
        w->id = -1;
        snprintf(w->etiqueta, sizeof(w->etiqueta), "%s", texto);
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

    if (total_widgets_ui < MAX_WIDGETS_UI && texto != NULL) {
        WidgetUI *w = &widgets_ui[total_widgets_ui++];
        memset(w, 0, sizeof(WidgetUI));
        w->activo = true;
        w->tipo = WIDGET_BOTON;
        w->pestana = pestana_en_construccion;
        w->id = id;
        snprintf(w->etiqueta, sizeof(w->etiqueta), "%s", texto);
        w->color = (Color){ (unsigned char)r, (unsigned char)g, (unsigned char)b, 255 };
    }
}

static void motor_ui_caja_texto(lat_mv *mv)
{
    lat_objeto *o_inicial  = latC_desapilar(mv);
    lat_objeto *o_etiqueta = latC_desapilar(mv);
    lat_objeto *o_id       = latC_desapilar(mv);

    int id = (int)latC_checar_numerico(mv, o_id);
    const char *etiqueta = latC_checar_cadena(mv, o_etiqueta);
    const char *inicial  = latC_checar_cadena(mv, o_inicial);

    if (total_widgets_ui < MAX_WIDGETS_UI && etiqueta != NULL && inicial != NULL) {
        WidgetUI *w = &widgets_ui[total_widgets_ui++];
        memset(w, 0, sizeof(WidgetUI));
        w->activo = true;
        w->tipo = WIDGET_CAJA_TEXTO;
        w->pestana = pestana_en_construccion;
        w->id = id;
        snprintf(w->etiqueta, sizeof(w->etiqueta), "%s", etiqueta);
        snprintf(w->texto, sizeof(w->texto), "%s", inicial);
    }
}

static void motor_ui_deslizador(lat_mv *mv)
{
    lat_objeto *o_val      = latC_desapilar(mv);
    lat_objeto *o_max      = latC_desapilar(mv);
    lat_objeto *o_min      = latC_desapilar(mv);
    lat_objeto *o_etiqueta = latC_desapilar(mv);
    lat_objeto *o_id       = latC_desapilar(mv);

    int id  = (int)latC_checar_numerico(mv, o_id);
    const char *etiqueta = latC_checar_cadena(mv, o_etiqueta);
    int min_v = (int)latC_checar_numerico(mv, o_min);
    int max_v = (int)latC_checar_numerico(mv, o_max);
    int val   = (int)latC_checar_numerico(mv, o_val);

    if (total_widgets_ui < MAX_WIDGETS_UI && etiqueta != NULL) {
        WidgetUI *w = &widgets_ui[total_widgets_ui++];
        memset(w, 0, sizeof(WidgetUI));
        w->activo = true;
        w->tipo = WIDGET_DESLIZADOR;
        w->pestana = pestana_en_construccion;
        w->id = id;
        snprintf(w->etiqueta, sizeof(w->etiqueta), "%s", etiqueta);
        w->min_val = min_v;
        w->max_val = (max_v > min_v) ? max_v : min_v + 1;
        w->valor = val;
    }
}

static void motor_ui_interruptor(lat_mv *mv)
{
    lat_objeto *o_estado   = latC_desapilar(mv);
    lat_objeto *o_etiqueta = latC_desapilar(mv);
    lat_objeto *o_id       = latC_desapilar(mv);

    int id = (int)latC_checar_numerico(mv, o_id);
    const char *etiqueta = latC_checar_cadena(mv, o_etiqueta);
    int estado = (int)latC_checar_numerico(mv, o_estado);

    if (total_widgets_ui < MAX_WIDGETS_UI && etiqueta != NULL) {
        WidgetUI *w = &widgets_ui[total_widgets_ui++];
        memset(w, 0, sizeof(WidgetUI));
        w->activo = true;
        w->tipo = WIDGET_INTERRUPTOR;
        w->pestana = pestana_en_construccion;
        w->id = id;
        snprintf(w->etiqueta, sizeof(w->etiqueta), "%s", etiqueta);
        w->valor = (estado != 0) ? 1 : 0;
    }
}

static void motor_ui_progreso(lat_mv *mv)
{
    lat_objeto *o_max      = latC_desapilar(mv);
    lat_objeto *o_val      = latC_desapilar(mv);
    lat_objeto *o_etiqueta = latC_desapilar(mv);
    lat_objeto *o_id       = latC_desapilar(mv);

    int id = (int)latC_checar_numerico(mv, o_id);
    const char *etiqueta = latC_checar_cadena(mv, o_etiqueta);
    int val = (int)latC_checar_numerico(mv, o_val);
    int max_v = (int)latC_checar_numerico(mv, o_max);

    if (total_widgets_ui < MAX_WIDGETS_UI && etiqueta != NULL) {
        WidgetUI *w = &widgets_ui[total_widgets_ui++];
        memset(w, 0, sizeof(WidgetUI));
        w->activo = true;
        w->tipo = WIDGET_PROGRESO;
        w->pestana = pestana_en_construccion;
        w->id = id;
        snprintf(w->etiqueta, sizeof(w->etiqueta), "%s", etiqueta);
        w->min_val = 0;
        w->max_val = (max_v > 0) ? max_v : 100;
        w->valor = val;
    }
}

static void motor_ui_tema(lat_mv *mv)
{
    lat_objeto *o_tema = latC_desapilar(mv);
    int t = (int)latC_checar_numerico(mv, o_tema);
    if (t >= 0 && t <= 2) {
        tema_actual = t;
    }
}

static void motor_ui_modal(lat_mv *mv)
{
    lat_objeto *o_msg    = latC_desapilar(mv);
    lat_objeto *o_titulo = latC_desapilar(mv);

    const char *titulo = latC_checar_cadena(mv, o_titulo);
    const char *msg    = latC_checar_cadena(mv, o_msg);

    if (titulo != NULL && msg != NULL) {
        snprintf(modal_titulo, sizeof(modal_titulo), "%s", titulo);
        snprintf(modal_mensaje, sizeof(modal_mensaje), "%s", msg);
        modal_activo = true;
    }
}

static void motor_ui_ayuda(lat_mv *mv)
{
    lat_objeto *o_ayuda = latC_desapilar(mv);
    lat_objeto *o_id    = latC_desapilar(mv);

    int id = (int)latC_checar_numerico(mv, o_id);
    const char *ayuda = latC_checar_cadena(mv, o_ayuda);

    WidgetUI *w = buscar_widget_por_id(id);
    if (w != NULL && ayuda != NULL) {
        snprintf(w->ayuda, sizeof(w->ayuda), "%s", ayuda);
    }
}

static void motor_ui_obtener_texto(lat_mv *mv)
{
    lat_objeto *o_id = latC_desapilar(mv);
    int id = (int)latC_checar_numerico(mv, o_id);
    WidgetUI *w = buscar_widget_por_id(id);
    const char *texto = (w != NULL) ? w->texto : "";
    latC_apilar(mv, latC_crear_cadena(mv, texto));
}

static void motor_ui_obtener_numero(lat_mv *mv)
{
    lat_objeto *o_id = latC_desapilar(mv);
    int id = (int)latC_checar_numerico(mv, o_id);
    WidgetUI *w = buscar_widget_por_id(id);
    double num = 0.0;
    if (w != NULL) {
        if (w->tipo == WIDGET_CAJA_TEXTO) num = atof(w->texto);
        else num = (double)w->valor;
    }
    latC_apilar(mv, latC_crear_numerico(mv, num));
}

static void motor_ui_obtener_valor(lat_mv *mv)
{
    lat_objeto *o_id = latC_desapilar(mv);
    int id = (int)latC_checar_numerico(mv, o_id);
    WidgetUI *w = buscar_widget_por_id(id);
    int val = (w != NULL) ? w->valor : 0;
    latC_apilar(mv, latC_crear_numerico(mv, (double)val));
}

static void motor_evento_id(lat_mv *mv)
{
    latC_apilar(mv, latC_crear_numerico(mv, (double)ultimo_evento_id));
}

static void motor_seleccionar_bloque(lat_mv *mv)
{
    lat_objeto *o_id = latC_desapilar(mv);
    int id = (int)latC_checar_numerico(mv, o_id);
    if (id >= 0 && id < MAX_NODOS && tabla_nodos[id].registrado) {
        bloque_seleccionado = id;
    }
}

static void motor_fijar_pincel(lat_mv *mv)
{
    lat_objeto *o_tam = latC_desapilar(mv);
    int tam = (int)latC_checar_numerico(mv, o_tam);
    if (tam < 1) tam = 1;
    if (tam > 5) tam = 5;
    tamano_pincel = tam;
}

static void motor_fijar_rejilla(lat_mv *mv)
{
    lat_objeto *o_est = latC_desapilar(mv);
    mostrar_bordes_chunk = ((int)latC_checar_numerico(mv, o_est) != 0);
}

static void motor_fijar_marcha(lat_mv *mv)
{
    lat_objeto *o_est = latC_desapilar(mv);
    modo_un_toque = ((int)latC_checar_numerico(mv, o_est) != 0);
    dir_mov_x = 0;
    dir_mov_y = 0;
}

static void motor_fijar_minimapa(lat_mv *mv)
{
    lat_objeto *o_est = latC_desapilar(mv);
    mostrar_minimapa = ((int)latC_checar_numerico(mv, o_est) != 0);
}

static void motor_semilla(lat_mv *mv)
{
    latC_apilar(mv, latC_crear_numerico(mv, (double)semilla_mundo));
}

static void motor_cambiar_semilla(lat_mv *mv)
{
    lat_objeto *o_sem = latC_desapilar(mv);
    semilla_mundo = (int)latC_checar_numerico(mv, o_sem);
    memset(mundo, 0, sizeof(mundo));
    memset(lista_carteles, 0, sizeof(lista_carteles));
    total_chunks_generados = 0;
}

static void motor_notificar(lat_mv *mv)
{
    lat_objeto *o_msg = latC_desapilar(mv);
    const char *msg = latC_checar_cadena(mv, o_msg);
    mostrar_notificacion_toast(msg);
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
    {"ui_pestana",         motor_ui_pestana,         2},
    {"ui_etiqueta",        motor_ui_etiqueta,        1},
    {"ui_boton",           motor_ui_boton,           5},
    {"ui_caja_texto",      motor_ui_caja_texto,      3},
    {"ui_deslizador",      motor_ui_deslizador,      5},
    {"ui_interruptor",     motor_ui_interruptor,     3},
    {"ui_progreso",        motor_ui_progreso,        4},
    {"ui_tema",            motor_ui_tema,            1},
    {"ui_modal",           motor_ui_modal,           2},
    {"ui_ayuda",           motor_ui_ayuda,           2},
    {"ui_obtener_texto",   motor_ui_obtener_texto,   1},
    {"ui_obtener_numero",  motor_ui_obtener_numero,  1},
    {"ui_obtener_valor",   motor_ui_obtener_valor,   1},
    {"evento_id",          motor_evento_id,          0},
    {"boton_clicado",      motor_evento_id,          0},
    {"seleccionar_bloque", motor_seleccionar_bloque, 1},
    {"fijar_pincel",       motor_fijar_pincel,       1},
    {"fijar_rejilla",      motor_fijar_rejilla,      1},
    {"fijar_marcha",       motor_fijar_marcha,       1},
    {"fijar_minimapa",     motor_fijar_minimapa,     1},
    {"semilla",            motor_semilla,            0},
    {"cambiar_semilla",    motor_cambiar_semilla,    1},
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
// EJECUCIÓN DE LATINO Y GENERACIÓN DE CHUNKS
// ============================================================================

static void ejecutar_script_latino(int modo, int cx, int cy)
{
    double t0 = GetTime();
    modo_ejecucion_latino = modo;
    chunk_solicitado_x = cx;
    chunk_solicitado_y = cy;

    const char *ruta = obtener_ruta_script();
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

    // Actualizar automáticamente cualquier barra de progreso con ID 400
    WidgetUI *w_prog = buscar_widget_por_id(400);
    if (w_prog != NULL && w_prog->tipo == WIDGET_PROGRESO) {
        w_prog->valor = total_chunks_generados;
    }
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

static void inicializar_mundo_con_latino(bool conservar_posicion)
{
    int prev_x = jugador_x;
    int prev_y = jugador_y;

    memset(mundo, 0, sizeof(mundo));
    memset(lista_carteles, 0, sizeof(lista_carteles));
    memset(tabla_nodos, 0, sizeof(tabla_nodos));
    memset(widgets_ui, 0, sizeof(widgets_ui));
    memset(pestanas_ui, 0, sizeof(pestanas_ui));
    total_widgets_ui = 0;
    total_pestanas_ui = 0;
    pestana_en_construccion = 0;
    total_chunks_generados = 0;
    id_caja_activa = -1;
    id_slider_arrastrando = -1;
    dir_mov_x = 0;
    dir_mov_y = 0;

    tabla_nodos[0] = (DefNodo){ true, "Pasto",  (Color){175, 215, 155, 255}, false };
    tabla_nodos[1] = (DefNodo){ true, "Piedra", DARKGRAY, true };

    const char *ruta = obtener_ruta_script();
    if (ruta != NULL) {
        ultima_modificacion_script = GetFileModTime(ruta);
    }

    ejecutar_script_latino(0, 0, 0);

    if (conservar_posicion) {
        jugador_x = prev_x;
        jugador_y = prev_y;
    }

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
        mostrar_notificacion_toast("Mundo guardado en disco correctamente");
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
            WidgetUI *w_prog = buscar_widget_por_id(400);
            if (w_prog != NULL) w_prog->valor = total_chunks_generados;
            mostrar_notificacion_toast("Mundo cargado desde disco correctamente");
        }
        fclose(f);
    } else {
        mostrar_notificacion_toast("Aun no existe un archivo guardado");
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

static void pintar_con_pincel(int centro_x, int centro_y, int tipo)
{
    int r_ini = -(tamano_pincel - 1) / 2;
    int r_fin = tamano_pincel / 2;

    for (int dy = r_ini; dy <= r_fin; dy++) {
        for (int dx = r_ini; dx <= r_fin; dx++) {
            int tx = centro_x + dx;
            int ty = centro_y + dy;
            if (tx > 0 && tx < MAP_WIDTH - 1 && ty > 0 && ty < MAP_HEIGHT - 1) {
                if (tipo != 0 && tx == jugador_x && ty == jugador_y &&
                    tabla_nodos[tipo].es_solido) {
                    continue;
                }
                establecer_casilla_mundo(tx, ty, tipo);
            }
        }
    }
}

int main(void)
{
    setlocale(LC_ALL, "C.UTF-8");
    setlocale(LC_NUMERIC, "C");

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);

    InitWindow(1500, 940, "LatinoVox 2D & LatinoUI Studio v0.4 - Pestanas, Temas, Hotbar y Minimapa");
    if (!IsWindowReady()) {
        return 1;
    }

    SetWindowMinSize(960, 680);
    SetTargetFPS(60);

    inicializar_mundo_con_latino(false);

    Camera2D camara = { 0 };
    camara.rotation = 0.0f;
    camara.zoom = 1.35f;

    while (!WindowShouldClose()) {
        int screenWidth = GetScreenWidth();
        int screenHeight = GetScreenHeight();
        float dt = GetFrameTime();
        TemaUI tema = paleta_temas[tema_actual];

        if (tiempo_toast > 0.0f) {
            tiempo_toast -= dt;
            if (tiempo_toast < 0.0f) tiempo_toast = 0.0f;
        }

        // --- VIGILANTE AUTO HOT-RELOAD ---
        temporizador_vigilancia += dt;
        if (temporizador_vigilancia >= 0.5f) {
            temporizador_vigilancia = 0.0f;
            const char *ruta = obtener_ruta_script();
            if (ruta != NULL) {
                long mod_actual = GetFileModTime(ruta);
                if (ultima_modificacion_script != 0 && mod_actual != ultima_modificacion_script) {
                    inicializar_mundo_con_latino(true);
                    mostrar_notificacion_toast("Auto Hot-Reload: arranque.lat actualizado en vivo!");
                }
            }
        }

        float escala_ui = (float)screenWidth / 1400.0f;
        if (escala_ui < 1.0f) escala_ui = 1.0f;
        if (escala_ui > 2.2f) escala_ui = 2.2f;

        int font_hud = (int)(22.0f * escala_ui) + ajuste_fuente;
        if (font_hud < 16) font_hud = 16;

        int sep_linea = font_hud + 8;
        int alto_hud = sep_linea * 3 + 18;

        int ancho_panel_ui = (int)(400.0f * escala_ui);
        Rectangle rect_panel_ui = {
            (float)(screenWidth - ancho_panel_ui - 14),
            (float)(alto_hud + 14),
            (float)ancho_panel_ui,
            (float)(screenHeight - alto_hud - 28)
        };

        // Geometría de la Hotbar Inferior (Estilo Terraria / Luanti)
        int slots_hotbar = 0;
        int ids_hotbar[8];
        for (int k = 1; k <= 7; k++) {
            if (tabla_nodos[k].registrado) {
                ids_hotbar[slots_hotbar++] = k;
            }
        }
        int lado_slot = (int)(56.0f * escala_ui);
        int ancho_hotbar = slots_hotbar * (lado_slot + 8) + 12;
        int alto_hotbar = lado_slot + 16;
        int area_libre_w = mostrar_panel_ui ? (int)rect_panel_ui.x : screenWidth;
        Rectangle rect_hotbar = {
            (float)((area_libre_w - ancho_hotbar) / 2),
            (float)(screenHeight - alto_hotbar - 14),
            (float)ancho_hotbar,
            (float)alto_hotbar
        };

        Vector2 pos_raton = GetMousePosition();
        bool raton_en_panel = mostrar_panel_ui && CheckCollisionPointRec(pos_raton, rect_panel_ui);
        bool raton_en_hotbar = CheckCollisionPointRec(pos_raton, rect_hotbar);
        bool raton_sobre_gui = modal_activo || raton_en_panel || raton_en_hotbar || (pos_raton.y <= (float)alto_hud);

        // --- A. GESTIÓN DE VENTANA MODAL EMERGENTE ---
        if (modal_activo) {
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) ||
                IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                modal_activo = false;
            }
        }

        // --- B. ENTRADA DE TECLADO EN CAJA DE TEXTO ACTIVA ---
        if (!modal_activo && id_caja_activa != -1) {
            WidgetUI *w_caja = buscar_widget_por_id(id_caja_activa);
            if (w_caja != NULL && w_caja->tipo == WIDGET_CAJA_TEXTO) {
                int tecla_char = GetCharPressed();
                while (tecla_char > 0) {
                    if (tecla_char >= 32 && tecla_char <= 126) {
                        size_t len = strlen(w_caja->texto);
                        if (len + 1 < sizeof(w_caja->texto)) {
                            w_caja->texto[len] = (char)tecla_char;
                            w_caja->texto[len + 1] = '\0';
                        }
                    }
                    tecla_char = GetCharPressed();
                }

                if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) {
                    size_t len = strlen(w_caja->texto);
                    if (len > 0) w_caja->texto[len - 1] = '\0';
                }

                if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
                    id_caja_activa = -1;
                }
            } else {
                id_caja_activa = -1;
            }
        }

        // --- C. ATAJOS DE SISTEMA ---
        if (!modal_activo && id_caja_activa == -1) {
            if (IsKeyPressed(KEY_R)) {
                inicializar_mundo_con_latino(false);
                mostrar_notificacion_toast("Script recargado manualmente [R]");
            }
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

            if (IsKeyPressed(KEY_F1) && ajuste_fuente > -6) ajuste_fuente -= 2;
            if (IsKeyPressed(KEY_F2) && ajuste_fuente < 18) ajuste_fuente += 2;

            for (int k = 1; k <= 7; k++) {
                if (IsKeyPressed(KEY_ZERO + k) && tabla_nodos[k].registrado) {
                    bloque_seleccionado = k;
                }
            }
        } else if (IsKeyPressed(KEY_TAB)) {
            id_caja_activa = -1;
        }

        // --- D. CLIC EN LA HOTBAR INFERIOR ---
        if (!modal_activo && raton_en_hotbar && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            for (int s = 0; s < slots_hotbar; s++) {
                Rectangle r_slot = {
                    rect_hotbar.x + 10.0f + (float)(s * (lado_slot + 8)),
                    rect_hotbar.y + 8.0f,
                    (float)lado_slot,
                    (float)lado_slot
                };
                if (CheckCollisionPointRec(pos_raton, r_slot)) {
                    bloque_seleccionado = ids_hotbar[s];
                    ultimo_evento_id = bloque_seleccionado;
                    ejecutar_script_latino(2, 0, 0);
                    break;
                }
            }
        }

        // --- E. RUEDA DEL RATÓN: SCROLL EN LA GUI O ZOOM EN EL MAPA ---
        float rueda = GetMouseWheelMove();
        if (!modal_activo && rueda != 0.0f) {
            if (raton_en_panel) {
                scroll_panel_y += rueda * 36.0f;
                if (scroll_panel_y > 0.0f) scroll_panel_y = 0.0f;
            } else {
                camara.zoom += rueda * 0.15f;
            }
        }

        if (!modal_activo && id_caja_activa == -1) {
            if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD)) camara.zoom += 0.2f;
            if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT)) camara.zoom -= 0.2f;
            if (IsKeyPressed(KEY_ZERO)) camara.zoom = 1.35f;
        }
        if (camara.zoom < 0.35f) camara.zoom = 0.35f;
        if (camara.zoom > 4.0f)  camara.zoom = 4.0f;

        // --- F. INTERACCIÓN CON PESTAÑAS Y WIDGETS DEL PANEL FRONTEND GUI ---
        const char *tooltip_activo = NULL;
        int alto_titulo_panel = font_hud + 18;
        int alto_barra_tabs = (total_pestanas_ui > 0) ? (font_hud + 18) : 0;
        int alto_cabecera_panel = alto_titulo_panel + alto_barra_tabs + 8;

        Rectangle area_scroll_ui = {
            rect_panel_ui.x,
            rect_panel_ui.y + (float)alto_cabecera_panel,
            rect_panel_ui.width,
            rect_panel_ui.height - (float)alto_cabecera_panel - 8.0f
        };

        if (!modal_activo && mostrar_panel_ui) {
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !raton_en_panel) {
                id_caja_activa = -1;
            }

            if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
                id_slider_arrastrando = -1;
            }

            // Clic en la barra de Pestañas (Tabs)
            if (total_pestanas_ui > 0 && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                float w_tab = (rect_panel_ui.width - 20.0f) / (float)total_pestanas_ui;
                for (int t = 0; t < total_pestanas_ui; t++) {
                    if (!pestanas_ui[t].activa) continue;
                    Rectangle r_tab = {
                        rect_panel_ui.x + 10.0f + (float)t * w_tab,
                        rect_panel_ui.y + (float)alto_titulo_panel,
                        w_tab - 4.0f,
                        (float)alto_barra_tabs
                    };
                    if (CheckCollisionPointRec(pos_raton, r_tab)) {
                        pestana_actual = t;
                        scroll_panel_y = 0.0f;
                        id_caja_activa = -1;
                        break;
                    }
                }
            }

            int cursor_y = (int)area_scroll_ui.y + (int)scroll_panel_y + 8;
            int font_w = font_hud - 2;

            for (int i = 0; i < total_widgets_ui; i++) {
                WidgetUI *w = &widgets_ui[i];
                if (!w->activo || w->pestana != pestana_actual) continue;

                int alto_item = font_w + 20;
                if (w->tipo == WIDGET_ETIQUETA)   alto_item = font_w + 10;
                if (w->tipo == WIDGET_CAJA_TEXTO) alto_item = (font_w * 2) + 24;
                if (w->tipo == WIDGET_DESLIZADOR) alto_item = (font_w * 2) + 22;
                if (w->tipo == WIDGET_PROGRESO)   alto_item = (font_w * 2) + 20;

                Rectangle rect_item = {
                    rect_panel_ui.x + 14.0f,
                    (float)cursor_y,
                    rect_panel_ui.width - 28.0f,
                    (float)alto_item
                };

                bool visible = (rect_item.y + rect_item.height >= area_scroll_ui.y) &&
                               (rect_item.y <= area_scroll_ui.y + area_scroll_ui.height);

                bool hover = visible && CheckCollisionPointRec(pos_raton, area_scroll_ui) &&
                             CheckCollisionPointRec(pos_raton, rect_item);

                if (hover && w->ayuda[0] != '\0') {
                    tooltip_activo = w->ayuda;
                }

                if (w->tipo == WIDGET_BOTON) {
                    if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                        id_caja_activa = -1;
                        ultimo_evento_id = w->id;
                        ejecutar_script_latino(2, 0, 0);
                        actualizar_chunks_cercanos();
                        break;
                    }
                } else if (w->tipo == WIDGET_CAJA_TEXTO) {
                    if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                        id_caja_activa = w->id;
                    }
                } else if (w->tipo == WIDGET_INTERRUPTOR) {
                    if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                        id_caja_activa = -1;
                        w->valor = (w->valor == 0) ? 1 : 0;
                        ultimo_evento_id = w->id;
                        ejecutar_script_latino(2, 0, 0);
                        break;
                    }
                } else if (w->tipo == WIDGET_DESLIZADOR) {
                    if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                        id_caja_activa = -1;
                        id_slider_arrastrando = w->id;
                    }
                    if (id_slider_arrastrando == w->id && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                        float barra_x = rect_item.x + 10.0f;
                        float barra_w = rect_item.width - 20.0f;
                        float ratio = (pos_raton.x - barra_x) / barra_w;
                        if (ratio < 0.0f) ratio = 0.0f;
                        if (ratio > 1.0f) ratio = 1.0f;
                        int nuevo_val = w->min_val + (int)(ratio * (float)(w->max_val - w->min_val) + 0.5f);
                        if (nuevo_val != w->valor) {
                            w->valor = nuevo_val;
                            ultimo_evento_id = w->id;
                            ejecutar_script_latino(2, 0, 0);
                        }
                    }
                }

                cursor_y += alto_item + 10;
            }
        }

        // --- G. MOVIMIENTO DEL JUGADOR ---
        if (!modal_activo && id_caja_activa == -1) {
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
        } else {
            dir_mov_x = 0;
            dir_mov_y = 0;
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

        // --- H. PINTADO EN EL MUNDO CON EL RATÓN ---
        Vector2 pos_mundo_raton = GetScreenToWorld2D(pos_raton, camara);
        int raton_tx = (int)(pos_mundo_raton.x / TILE_SIZE);
        int raton_ty = (int)(pos_mundo_raton.y / TILE_SIZE);

        if (!raton_sobre_gui && id_slider_arrastrando == -1 &&
            raton_tx > 0 && raton_tx < MAP_WIDTH - 1 &&
            raton_ty > 0 && raton_ty < MAP_HEIGHT - 1) {
            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                id_caja_activa = -1;
                pintar_con_pincel(raton_tx, raton_ty, 0);
            } else if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
                id_caja_activa = -1;
                pintar_con_pincel(raton_tx, raton_ty, bloque_seleccionado);
            }
        }

        // --- I. VIEW CULLING Y RENDERIZADO DEL MUNDO ---
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
            int r_ini = -(tamano_pincel - 1) / 2;
            int px_ini = (raton_tx + r_ini) * TILE_SIZE;
            int py_ini = (raton_ty + r_ini) * TILE_SIZE;
            int lado_px = tamano_pincel * TILE_SIZE;
            DrawRectangleLinesEx((Rectangle){ (float)px_ini, (float)py_ini, (float)lado_px, (float)lado_px }, 2.5f, YELLOW);
        }

        DrawCircle(
            jugador_x * TILE_SIZE + TILE_SIZE / 2,
            jugador_y * TILE_SIZE + TILE_SIZE / 2,
            TILE_SIZE * 0.35f,
            RED
        );

        EndMode2D();

        // --- J. HUD SUPERIOR ---
        int j_cx = jugador_x / CHUNK_SIZE;
        int j_cy = jugador_y / CHUNK_SIZE;

        DrawRectangle(0, 0, screenWidth, alto_hud, tema.fondo_panel);
        DrawLine(0, alto_hud, screenWidth, alto_hud, tema.borde_panel);

        DrawText(
            id_caja_activa != -1
                ? "MODO ESCRITURA ACTIVO: Escribe con tu teclado | [ENTER] o Clic fuera para terminar"
                : "WASD: Caminar | [ESPACIO] Parar | [TAB] Panel GUI | Clic en Pestanas o Hotbar | [G/C] Guardar",
            20, 10, font_hud, (id_caja_activa != -1) ? GOLD : tema.texto_normal
        );
        DrawText(
            TextFormat("FPS: %d | Pos: (%d,%d) Chunk: (%d,%d) | Semilla: %d | Chunks: %d/256 | VM: %.2f ms",
                       GetFPS(), jugador_x, jugador_y, j_cx, j_cy,
                       semilla_mundo, total_chunks_generados, ultimo_tiempo_vm_ms),
            20, 10 + sep_linea, font_hud, (tema_actual == 1) ? DARKGREEN : GREEN
        );
        DrawText(
            TextFormat("Bloque: [%d:%s] | Pincel: %dx%d | Estado: %s",
                       bloque_seleccionado, nombre_bloque(bloque_seleccionado),
                       tamano_pincel, tamano_pincel, estado_sistema),
            20, 10 + sep_linea * 2, font_hud, tema.texto_etiqueta
        );

        // --- K. MINIMAPA 2D EN VIVO ( ABAJO A LA IZQUIERDA ) ---
        if (mostrar_minimapa) {
            int map_px = 192; // 64x64 bloques alrededor o mundo 256x256 a escala
            int x_mm = 18;
            int y_mm = screenHeight - map_px - 18;

            DrawRectangle(x_mm - 4, y_mm - 28, map_px + 8, map_px + 32, tema.fondo_panel);
            DrawRectangleLines(x_mm - 4, y_mm - 28, map_px + 8, map_px + 32, tema.borde_panel);
            DrawText("Minimapa Chunks", x_mm + 6, y_mm - 24, 18, tema.texto_titulo);

            // Dibujar cada casilla generada (paso 2 para cubrir 256x256 en 128x128 o escala 3/4)
            for (int my = 0; my < 64; my++) {
                for (int mx = 0; mx < 64; mx++) {
                    int gx = mx * 4;
                    int gy = my * 4;
                    int cx = gx / CHUNK_SIZE;
                    int cy = gy / CHUNK_SIZE;
                    Color cm = (Color){ 30, 35, 45, 255 };
                    if (mundo[cy][cx].generado) {
                        int t = obtener_casilla_mundo(gx, gy);
                        cm = tabla_nodos[t].color;
                    }
                    DrawRectangle(x_mm + mx * 3, y_mm + my * 3, 3, 3, cm);
                }
            }
            // Posición del jugador en el minimapa
            int j_mx = x_mm + (jugador_x * 3) / 4;
            int j_my = y_mm + (jugador_y * 3) / 4;
            DrawCircle(j_mx, j_my, 4.0f, RED);
            DrawCircleLines(j_mx, j_my, 5.0f, WHITE);
        }

        // --- L. BARRA RÁPIDA DE HERRAMIENTAS (HOTBAR ESTILO TERRARIA / LUANTI) ---
        DrawRectangleRec(rect_hotbar, tema.fondo_panel);
        DrawRectangleLinesEx(rect_hotbar, 2.0f, tema.borde_panel);

        for (int s = 0; s < slots_hotbar; s++) {
            int id_b = ids_hotbar[s];
            Rectangle r_slot = {
                rect_hotbar.x + 10.0f + (float)(s * (lado_slot + 8)),
                rect_hotbar.y + 8.0f,
                (float)lado_slot,
                (float)lado_slot
            };
            bool activo = (bloque_seleccionado == id_b);
            bool hover_s = CheckCollisionPointRec(pos_raton, r_slot);

            DrawRectangleRec(r_slot, tabla_nodos[id_b].color);
            DrawRectangleLinesEx(r_slot, activo ? 3.5f : 1.5f,
                                 activo ? GOLD : (hover_s ? WHITE : DARKGRAY));
            DrawText(TextFormat("%d", id_b), (int)r_slot.x + 5, (int)r_slot.y + 4, 18, BLACK);
            DrawText(TextFormat("%d", id_b), (int)r_slot.x + 4, (int)r_slot.y + 3, 18, WHITE);
        }

        // --- M. PANEL LATERAL FRONTEND GUI CON PESTAÑAS Y SCROLL ---
        if (mostrar_panel_ui) {
            DrawRectangleRec(rect_panel_ui, tema.fondo_panel);
            DrawRectangleLinesEx(rect_panel_ui, 2.0f, tema.borde_panel);

            DrawText(titulo_ui, (int)rect_panel_ui.x + 16, (int)rect_panel_ui.y + 10, font_hud, tema.texto_titulo);

            // Barra de Pestañas (Tabs)
            if (total_pestanas_ui > 0) {
                float w_tab = (rect_panel_ui.width - 20.0f) / (float)total_pestanas_ui;
                int f_tab = font_hud - 4;
                if (f_tab < 15) f_tab = 15;

                for (int t = 0; t < total_pestanas_ui; t++) {
                    if (!pestanas_ui[t].activa) continue;
                    Rectangle r_tab = {
                        rect_panel_ui.x + 10.0f + (float)t * w_tab,
                        rect_panel_ui.y + (float)alto_titulo_panel,
                        w_tab - 4.0f,
                        (float)alto_barra_tabs
                    };
                    bool es_actual = (pestana_actual == t);
                    bool hover_t = CheckCollisionPointRec(pos_raton, r_tab);

                    DrawRectangleRec(r_tab, es_actual ? tema.borde_panel : tema.fondo_widget);
                    DrawRectangleLinesEx(r_tab, 1.5f, (es_actual || hover_t) ? GOLD : tema.borde_panel);
                    DrawText(
                        pestanas_ui[t].titulo,
                        (int)r_tab.x + 8,
                        (int)r_tab.y + (alto_barra_tabs - f_tab) / 2,
                        f_tab,
                        es_actual ? BLACK : tema.texto_normal
                    );
                }
            }

            BeginScissorMode(
                (int)area_scroll_ui.x,
                (int)area_scroll_ui.y,
                (int)area_scroll_ui.width,
                (int)area_scroll_ui.height
            );

            int cursor_y = (int)area_scroll_ui.y + (int)scroll_panel_y + 8;
            int font_w = font_hud - 2;

            for (int i = 0; i < total_widgets_ui; i++) {
                WidgetUI *w = &widgets_ui[i];
                if (!w->activo || w->pestana != pestana_actual) continue;

                int alto_item = font_w + 20;
                if (w->tipo == WIDGET_ETIQUETA)   alto_item = font_w + 10;
                if (w->tipo == WIDGET_CAJA_TEXTO) alto_item = (font_w * 2) + 24;
                if (w->tipo == WIDGET_DESLIZADOR) alto_item = (font_w * 2) + 22;
                if (w->tipo == WIDGET_PROGRESO)   alto_item = (font_w * 2) + 20;

                Rectangle rect_item = {
                    rect_panel_ui.x + 14.0f,
                    (float)cursor_y,
                    rect_panel_ui.width - 28.0f,
                    (float)alto_item
                };

                bool hover = CheckCollisionPointRec(pos_raton, area_scroll_ui) &&
                             CheckCollisionPointRec(pos_raton, rect_item);

                if (w->tipo == WIDGET_ETIQUETA) {
                    DrawText(w->etiqueta, (int)rect_item.x + 4, (int)rect_item.y + 4, font_w, tema.texto_etiqueta);
                } else if (w->tipo == WIDGET_BOTON) {
                    bool seleccionado = (w->id == bloque_seleccionado);
                    Color c_fondo = w->color;
                    if (hover) {
                        c_fondo.r = (unsigned char)((c_fondo.r + 40 > 255) ? 255 : c_fondo.r + 40);
                        c_fondo.g = (unsigned char)((c_fondo.g + 40 > 255) ? 255 : c_fondo.g + 40);
                        c_fondo.b = (unsigned char)((c_fondo.b + 40 > 255) ? 255 : c_fondo.b + 40);
                    }
                    DrawRectangleRec(rect_item, c_fondo);
                    DrawRectangleLinesEx(rect_item, seleccionado ? 3.0f : 1.5f,
                                         (hover || seleccionado) ? GOLD : RAYWHITE);
                    DrawText(w->etiqueta,
                             (int)rect_item.x + 12,
                             (int)rect_item.y + (alto_item - font_w) / 2,
                             font_w, RAYWHITE);
                } else if (w->tipo == WIDGET_CAJA_TEXTO) {
                    bool tiene_foco = (id_caja_activa == w->id);
                    DrawText(w->etiqueta, (int)rect_item.x + 4, (int)rect_item.y + 2, font_w - 2, tema.texto_normal);

                    Rectangle rect_caja = {
                        rect_item.x,
                        rect_item.y + (float)font_w + 4.0f,
                        rect_item.width,
                        (float)font_w + 16.0f
                    };
                    DrawRectangleRec(rect_caja, tema.fondo_widget);
                    DrawRectangleLinesEx(rect_caja, tiene_foco ? 2.5f : 1.5f,
                                         tiene_foco ? GOLD : (hover ? tema.borde_panel : GRAY));

                    bool parpadeo = tiene_foco && (((int)(GetTime() * 2.0)) % 2 == 0);
                    DrawText(
                        parpadeo ? TextFormat("%s|", w->texto) : w->texto,
                        (int)rect_caja.x + 10,
                        (int)rect_caja.y + 8,
                        font_w,
                        tiene_foco ? GOLD : tema.texto_normal
                    );
                } else if (w->tipo == WIDGET_DESLIZADOR) {
                    DrawText(
                        TextFormat("%s: %d", w->etiqueta, w->valor),
                        (int)rect_item.x + 4,
                        (int)rect_item.y + 2,
                        font_w - 2,
                        tema.texto_normal
                    );

                    float barra_x = rect_item.x + 10.0f;
                    float barra_y = rect_item.y + (float)font_w + 12.0f;
                    float barra_w = rect_item.width - 20.0f;
                    float ratio = (float)(w->valor - w->min_val) / (float)(w->max_val - w->min_val);

                    DrawRectangle((int)barra_x, (int)barra_y, (int)barra_w, 10, DARKGRAY);
                    DrawRectangle((int)barra_x, (int)barra_y, (int)(barra_w * ratio), 10, tema.borde_panel);
                    DrawCircle((int)(barra_x + barra_w * ratio), (int)barra_y + 5, 10.0f, hover ? GOLD : RAYWHITE);
                } else if (w->tipo == WIDGET_INTERRUPTOR) {
                    bool encendido = (w->valor != 0);
                    DrawRectangleRec(rect_item, tema.fondo_widget);
                    DrawRectangleLinesEx(rect_item, 1.5f, hover ? GOLD : tema.borde_panel);

                    DrawText(w->etiqueta,
                             (int)rect_item.x + 10,
                             (int)rect_item.y + (alto_item - font_w) / 2,
                             font_w - 2, tema.texto_normal);

                    Rectangle sw_box = {
                        rect_item.x + rect_item.width - 66.0f,
                        rect_item.y + (rect_item.height - 26.0f) / 2.0f,
                        54.0f,
                        26.0f
                    };
                    DrawRectangleRec(sw_box, encendido ? (Color){ 45, 165, 85, 255 } : (Color){ 90, 95, 105, 255 });
                    DrawCircle(
                        (int)(encendido ? sw_box.x + sw_box.width - 13.0f : sw_box.x + 13.0f),
                        (int)(sw_box.y + 13.0f),
                        10.0f, RAYWHITE
                    );
                } else if (w->tipo == WIDGET_PROGRESO) {
                    int pct = (w->valor * 100) / w->max_val;
                    DrawText(
                        TextFormat("%s: %d / %d (%d%%)", w->etiqueta, w->valor, w->max_val, pct),
                        (int)rect_item.x + 4,
                        (int)rect_item.y + 2,
                        font_w - 2,
                        tema.texto_normal
                    );
                    Rectangle r_bar = {
                        rect_item.x + 4.0f,
                        rect_item.y + (float)font_w + 6.0f,
                        rect_item.width - 8.0f,
                        18.0f
                    };
                    float ratio = (float)w->valor / (float)w->max_val;
                    if (ratio < 0.0f) ratio = 0.0f;
                    if (ratio > 1.0f) ratio = 1.0f;

                    DrawRectangleRec(r_bar, tema.fondo_widget);
                    DrawRectangle((int)r_bar.x, (int)r_bar.y, (int)(r_bar.width * ratio), (int)r_bar.height, (Color){ 50, 185, 95, 255 });
                    DrawRectangleLinesEx(r_bar, 1.5f, tema.borde_panel);
                }

                cursor_y += alto_item + 10;
            }

            EndScissorMode();
        }

        // --- N. NOTIFICACIÓN FLOTANTE (TOAST UX) ---
        if (tiempo_toast > 0.0f && texto_toast[0] != '\0') {
            float alpha = (tiempo_toast < 0.8f) ? (tiempo_toast / 0.8f) : 1.0f;
            unsigned char opacidad = (unsigned char)(235.0f * alpha);
            int ancho_t = MeasureText(texto_toast, font_hud) + 44;
            int x_t = (area_libre_w - ancho_t) / 2;
            if (x_t < 20) x_t = 20;
            int y_t = alto_hud + 18;

            DrawRectangle(x_t, y_t, ancho_t, font_hud + 24, (Color){ 20, 65, 95, opacidad });
            DrawRectangleLines(x_t, y_t, ancho_t, font_hud + 24, (Color){ 100, 220, 255, opacidad });
            DrawText(texto_toast, x_t + 22, y_t + 12, font_hud, (Color){ 255, 255, 255, opacidad });
        }

        // --- O. TOOLTIP UX ---
        if (!modal_activo && tooltip_activo != NULL) {
            int f_tip = font_hud - 4;
            if (f_tip < 15) f_tip = 15;
            int w_tip = MeasureText(tooltip_activo, f_tip) + 28;
            int h_tip = f_tip + 20;
            int x_tip = (int)pos_raton.x - w_tip - 16;
            int y_tip = (int)pos_raton.y + 12;
            if (x_tip < 10) x_tip = 10;
            if (y_tip + h_tip > screenHeight - 10) y_tip = screenHeight - h_tip - 10;

            DrawRectangle(x_tip, y_tip, w_tip, h_tip, (Color){ 10, 12, 16, 245 });
            DrawRectangleLines(x_tip, y_tip, w_tip, h_tip, GOLD);
            DrawText(tooltip_activo, x_tip + 14, y_tip + 10, f_tip, YELLOW);
        }

        // --- P. LECTURA DE CARTELES ---
        if (obtener_casilla_mundo(jugador_x, jugador_y) == 3) {
            const char *msg = buscar_texto_cartel(jugador_x, jugador_y);
            if (msg != NULL) {
                int alto_caja = font_hud + 28;
                int x_cartel = mostrar_minimapa ? 230 : 26;
                int y_cartel = (int)rect_hotbar.y - alto_caja - 12;
                int w_cartel = area_libre_w - x_cartel - 26;
                DrawRectangle(x_cartel, y_cartel, w_cartel, alto_caja, (Color){ 20, 20, 20, 240 });
                DrawRectangleLines(x_cartel, y_cartel, w_cartel, alto_caja, GOLD);
                DrawText(msg, x_cartel + 20, y_cartel + (alto_caja - font_hud) / 2, font_hud - 2, YELLOW);
            }
        }

        // --- Q. VENTANA MODAL EMERGENTE (CONTROLADA DESDE LATINO) ---
        if (modal_activo) {
            DrawRectangle(0, 0, screenWidth, screenHeight, (Color){ 0, 0, 0, 175 });
            int w_mod = (int)(760.0f * escala_ui);
            if (w_mod > screenWidth - 60) w_mod = screenWidth - 60;
            int h_mod = (int)(260.0f * escala_ui);
            int x_mod = (screenWidth - w_mod) / 2;
            int y_mod = (screenHeight - h_mod) / 2;

            DrawRectangle(x_mod, y_mod, w_mod, h_mod, tema.fondo_panel);
            DrawRectangleLinesEx((Rectangle){ (float)x_mod, (float)y_mod, (float)w_mod, (float)h_mod }, 3.0f, GOLD);

            DrawText(modal_titulo, x_mod + 28, y_mod + 24, font_hud + 2, tema.texto_titulo);
            DrawLine(x_mod + 24, y_mod + font_hud + 36, x_mod + w_mod - 24, y_mod + font_hud + 36, tema.borde_panel);
            DrawText(modal_mensaje, x_mod + 28, y_mod + font_hud + 58, font_hud - 2, tema.texto_normal);

            Rectangle btn_cerrar = {
                (float)(x_mod + w_mod / 2 - 140),
                (float)(y_mod + h_mod - 64),
                280.0f,
                46.0f
            };
            DrawRectangleRec(btn_cerrar, (Color){ 45, 135, 85, 255 });
            DrawRectangleLinesEx(btn_cerrar, 2.0f, WHITE);
            DrawText("Aceptar [Clic / Enter]", (int)btn_cerrar.x + 24, (int)btn_cerrar.y + 12, font_hud - 2, WHITE);
        }

        EndDrawing();
    }

    CloseWindow();
    return 0;
}