#include "ui_utf8.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <locale.h>
#include "raylib.h"
#include "ui_layout.h"
#include "ui_contenedores.h"

// Cabeceras de Latino-Core
#include "latino.h"
#include "latast.h"
#include "latmv.h"
#include "latobj.h"

#define TILE_SIZE 48
#define CHUNK_SIZE 16
#define WORLD_CHUNKS_X 64
#define WORLD_CHUNKS_Y 64
#define MAP_WIDTH  (CHUNK_SIZE * WORLD_CHUNKS_X)
#define MAP_HEIGHT (CHUNK_SIZE * WORLD_CHUNKS_Y)

#define MAX_NODOS 32
#define MAX_MSG_LEN 192
#define MAX_CARTELES 64
#define MAX_WIDGETS_UI 128  // arranque.lat registra actualmente 72 widgets
#define MAX_PESTANAS_UI 6
#define MAX_LUCES_VISIBLES 128
#define MAX_LOGS_CONSOLA 10
#define MAX_SEMILLAS_ACTIVAS 128
#define MAX_TNT_ACTIVAS 64
#define MAX_GOLEMS 512
#define MAX_ENEMIGOS 256
#define ARCHIVO_GUARDADO "mundo_chunks_v08.dat"
#define ARCHIVO_TEMP_CONSOLA "/tmp/latino_consola_eval.lat"

typedef enum {
    WIDGET_ETIQUETA = 0,
    WIDGET_BOTON,
    WIDGET_CAJA_TEXTO,
    WIDGET_DESLIZADOR,
    WIDGET_INTERRUPTOR,
    WIDGET_PROGRESO,
    WIDGET_INVENTARIO
} TipoWidget;

typedef enum {
    SONIDO_CLIC = 0,
    SONIDO_CONSTRUIR = 1,
    SONIDO_MINAR = 2,
    SONIDO_CRAFT = 3,
    SONIDO_PUERTA = 4,
    SONIDO_ALERTA = 5,
    SONIDO_ESPADA = 6,
    SONIDO_EXPLOSION = 7,
    TOTAL_SONIDOS = 8
} TipoSonido;

typedef enum {
    BIOMA_BOSQUE = 0,
    BIOMA_DESIERTO = 1,
    BIOMA_NIEVE = 2,
    BIOMA_VOLCAN = 3,
    BIOMA_PANTANO = 4
} TipoBioma;

typedef struct {
    bool registrado;
    char nombre[32];
    Color color;
    bool es_solido;
    int radio_luz;
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
    int gx;
    int gy;
    float tiempo_restante;
} SemillaActiva;

typedef struct {
    bool activa;
    int gx;
    int gy;
    float tiempo_restante;
} TntActiva;

typedef struct {
    bool activo;
    int gx;
    int gy;
    int dir_x;
    int dir_y;
    int tarea; // 0=Espera, 1=Escolta/Combate, 2=Minero, 3=Ingeniero Puentes
} EntidadGolem;

typedef struct {
    bool activo;
    int tipo; // 0=Slime, 1=Esqueleto, 2=Demonio Magma
    int gx;
    int gy;
    int hp;
    int max_hp;
} EntidadEnemigo;

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
    char etiqueta[64];
    char texto[128];
    char ayuda[160];
    Color color;
    int min_val;
    int max_val;
    int valor;
    // Rectángulo final compartido por dibujo e interacción.
    Rectangle rect_calculado;
    int fila_id; // 0 = columna tradicional
} WidgetUI;

typedef struct {
    Color fondo_panel;
    Color borde_panel;
    Color texto_titulo;
    Color texto_normal;
    Color texto_etiqueta;
    Color fondo_widget;
} TemaUI;

typedef struct {
    int gx;
    int gy;
    int radio;
} FuenteLuz;

static DefNodo tabla_nodos[MAX_NODOS];
static int inventario[MAX_NODOS];
static Chunk mundo[WORLD_CHUNKS_Y][WORLD_CHUNKS_X];
static CartelMundo lista_carteles[MAX_CARTELES];
static SemillaActiva lista_semillas[MAX_SEMILLAS_ACTIVAS];
static TntActiva lista_tnt[MAX_TNT_ACTIVAS];
static EntidadGolem lista_golems[MAX_GOLEMS];
static EntidadEnemigo lista_enemigos[MAX_ENEMIGOS];

// Efecto visual de explosión y ataque de espada
static float tiempo_onda_explosion = 0.0f;
static int explosion_vis_x = 0;
static int explosion_vis_y = 0;
static float tiempo_ataque_espada = 0.0f;

// Fuente Tipográfica UTF-8 y Audio Procedural (raudio)
static Font fuente_ui;
static bool fuente_ttf_cargada = false;
static Sound sonidos_motor[TOTAL_SONIDOS];
static bool audio_inicializado = false;
static bool audio_activo = true;

// Estado del Frontend GUI, Pestañas y Cursor de Edición UTF-8
static PestanaUI pestanas_ui[MAX_PESTANAS_UI];
static int total_pestanas_ui = 0;
static int pestana_actual = 0;
static int pestana_en_construccion = 0;

static WidgetUI widgets_ui[MAX_WIDGETS_UI];
static int total_widgets_ui = 0;
static float separacion_fila_ui[MAX_WIDGETS_UI + 1];
static UIContContexto contenedores_ui;
static UIContElemento miembros_contenedor_ui[MAX_WIDGETS_UI];
static char titulo_ui[48] = "LatinoUI Studio";
static bool mostrar_panel_ui = true;
static int ultimo_evento_id = 0;
static int id_caja_activa = -1;
static int cursor_caja_bytes = 0;
static int acento_pendiente = 0;
static int id_slider_arrastrando = -1;
static float scroll_panel_y = 0.0f;
static int tema_actual = 0;

// Estado de Ventana Modal Emergente
static bool modal_activo = false;
static char modal_titulo[64] = "";
static char modal_mensaje[384] = "";

// Estado de la Consola .lat Interactiva In-Game [F3]
static bool consola_activa = false;
static char entrada_consola[160] = "motor.golem_crear(motor.jugador_x()+1, motor.jugador_y(), 3)";
static int cursor_consola_bytes = 0;
static char historial_consola[MAX_LOGS_CONSOLA][160];
static int total_logs_consola = 0;

// Notificación Flotante (Toast UX)
static char texto_toast[160] = "";
static float tiempo_toast = 0.0f;

// Parámetros de comunicación C -> Latino
static int modo_ejecucion_latino = 0;
static int chunk_solicitado_x = 0;
static int chunk_solicitado_y = 0;
static int semilla_mundo = 2026;

// Estado del Jugador, Salud, Combate y Gólems
static int jugador_x = 512;
static int jugador_y = 512;
static int jugador_hp = 100;
static int jugador_max_hp = 100;
static int enemigos_derrotados = 0;
static int dir_mov_x = 0;
static int dir_mov_y = 0;
static int ultima_dir_x = 1;
static int ultima_dir_y = 0;
static bool modo_un_toque = true;
static float temporizador_paso = 0.0f;
static int tamano_pincel = 1;
static bool mostrar_minimapa = true;
static bool modo_creativo = false;
static bool spawner_enemigos_activo = true;

static int golem_tarea_global = 1; // 1=Escolta/Combate, 2=Minar, 3=Puentes
static float temporizador_ia_tick = 0.0f;
static float temporizador_spawn_enemigo = 0.0f;

// Ciclo Día/Noche (0.0h a 24.0h)
static float hora_dia = 10.0f;
static bool ciclo_luz_auto = true;

// Estado de interfaz y Auto Hot-Reload
static int bloque_seleccionado = 8;
static bool mostrar_bordes_chunk = false;
static int ajuste_fuente = 0;
static long ultima_modificacion_script = 0;
static float temporizador_vigilancia = 0.0f;

static double ultimo_tiempo_vm_ms = 0.0;
static int total_chunks_generados = 0;
static long ram_actual_kb = 0;
static char estado_sistema[128] = "Listo";

static const TemaUI paleta_temas[3] = {
    { {16, 20, 28, 244}, {102, 191, 255, 255}, {255, 203, 0, 255}, {245, 245, 245, 255}, {102, 191, 255, 255}, {28, 34, 46, 255} },
    { {235, 238, 245, 248}, {50, 90, 150, 255}, {25, 55, 110, 255}, {20, 25, 32, 255}, {35, 90, 160, 255}, {210, 216, 228, 255} },
    { {10, 28, 22, 244}, {45, 220, 145, 255}, {120, 255, 180, 255}, {220, 255, 235, 255}, {80, 235, 160, 255}, {18, 48, 36, 255} }
};

static void ejecutar_script_latino(int modo, int cx, int cy);
static void actualizar_chunks_cercanos(void);
static void reproducir_sonido_motor(int id);
static void mostrar_notificacion_toast(const char *msg);

// ============================================================================
// CODIFICADOR Y EDITOR DE CADENAS UTF-8 CON SOPORTE DE CURSOR MÓVIL
// ============================================================================

static int utf8_codificar(int cp, char out[5])
{
    if (cp < 0x80) {
        out[0] = (char)cp;
        out[1] = '\0';
        return 1;
    } else if (cp < 0x800) {
        out[0] = (char)(0xC0 | ((cp >> 6) & 0x1F));
        out[1] = (char)(0x80 | (cp & 0x3F));
        out[2] = '\0';
        return 2;
    } else if (cp < 0x10000) {
        out[0] = (char)(0xE0 | ((cp >> 12) & 0x0F));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3F));
        out[2] = (char)(0x80 | (cp & 0x3F));
        out[3] = '\0';
        return 3;
    }
    out[0] = '\0';
    return 0;
}

static int utf8_prev_char(const char *s, int pos)
{
    if (pos <= 0) return 0;
    pos--;
    while (pos > 0 && (((unsigned char)s[pos] & 0xC0) == 0x80)) {
        pos--;
    }
    return pos;
}

static int utf8_next_char(const char *s, int pos)
{
    int len = (int)strlen(s);
    if (pos >= len) return len;
    pos++;
    while (pos < len && (((unsigned char)s[pos] & 0xC0) == 0x80)) {
        pos++;
    }
    return pos;
}

static void utf8_insertar_en_cursor(char *buf, size_t cap, int *cursor, const char *ins)
{
    size_t len_buf = strlen(buf);
    size_t len_ins = strlen(ins);
    if (len_ins == 0 || len_buf + len_ins + 1 >= cap) return;

    if (*cursor < 0) *cursor = 0;
    if (*cursor > (int)len_buf) *cursor = (int)len_buf;

    memmove(buf + *cursor + len_ins, buf + *cursor, len_buf - (size_t)(*cursor) + 1);
    memcpy(buf + *cursor, ins, len_ins);
    *cursor += (int)len_ins;
}

static void utf8_borrar_atras(char *buf, int *cursor)
{
    int len = (int)strlen(buf);
    if (*cursor <= 0 || len <= 0) return;
    if (*cursor > len) *cursor = len;

    int prev = utf8_prev_char(buf, *cursor);
    memmove(buf + prev, buf + *cursor, (size_t)(len - *cursor + 1));
    *cursor = prev;
}

static void utf8_borrar_adelante(char *buf, int *cursor)
{
    int len = (int)strlen(buf);
    if (*cursor < 0 || *cursor >= len) return;

    int next = utf8_next_char(buf, *cursor);
    memmove(buf + *cursor, buf + next, (size_t)(len - next + 1));
}

static void construir_texto_con_cursor(const char *texto, int cursor, bool mostrar_barra, char *salida, size_t cap_salida)
{
    int len = (int)strlen(texto);
    if (cursor < 0) cursor = 0;
    if (cursor > len) cursor = len;

    if (!mostrar_barra) {
        snprintf(salida, cap_salida, "%s", texto);
        return;
    }
    snprintf(salida, cap_salida, "%.*s|%s", cursor, texto, texto + cursor);
}

static void procesar_edicion_utf8(char *buf, size_t cap, int *cursor)
{
    int len = (int)strlen(buf);
    if (*cursor < 0) *cursor = 0;
    if (*cursor > len) *cursor = len;

    if (IsKeyPressed(KEY_LEFT) || IsKeyPressedRepeat(KEY_LEFT)) {
        *cursor = utf8_prev_char(buf, *cursor);
        acento_pendiente = 0;
    }
    if (IsKeyPressed(KEY_RIGHT) || IsKeyPressedRepeat(KEY_RIGHT)) {
        *cursor = utf8_next_char(buf, *cursor);
        acento_pendiente = 0;
    }
    if (IsKeyPressed(KEY_HOME)) {
        *cursor = 0;
        acento_pendiente = 0;
    }
    if (IsKeyPressed(KEY_END)) {
        *cursor = (int)strlen(buf);
        acento_pendiente = 0;
    }

    if (IsKeyPressed(KEY_BACKSPACE) || IsKeyPressedRepeat(KEY_BACKSPACE)) {
        if (acento_pendiente != 0) {
            acento_pendiente = 0;
        } else {
            utf8_borrar_atras(buf, cursor);
        }
    }
    if (IsKeyPressed(KEY_DELETE) || IsKeyPressedRepeat(KEY_DELETE)) {
        utf8_borrar_adelante(buf, cursor);
    }

    int cp = GetCharPressed();
    while (cp > 0) {
        if (cp == 180) {
            acento_pendiente = 1;
            cp = GetCharPressed();
            continue;
        }
        if (cp == '~' && acento_pendiente == 0) {
            acento_pendiente = 2;
            cp = GetCharPressed();
            continue;
        }

        int cp_final = cp;
        if (acento_pendiente == 1) {
            if      (cp == 'a') cp_final = 225;
            else if (cp == 'e') cp_final = 233;
            else if (cp == 'i') cp_final = 237;
            else if (cp == 'o') cp_final = 243;
            else if (cp == 'u') cp_final = 250;
            else if (cp == 'A') cp_final = 193;
            else if (cp == 'E') cp_final = 201;
            else if (cp == 'I') cp_final = 205;
            else if (cp == 'O') cp_final = 211;
            else if (cp == 'U') cp_final = 218;
            else if (cp == 'n') cp_final = 241;
            else if (cp == 'N') cp_final = 209;
            acento_pendiente = 0;
        } else if (acento_pendiente == 2) {
            if      (cp == 'n') cp_final = 241;
            else if (cp == 'N') cp_final = 209;
            else {
                utf8_insertar_en_cursor(buf, cap, cursor, "~");
            }
            acento_pendiente = 0;
        }

        if (cp_final >= 32) {
            char utf8_seq[5];
            if (utf8_codificar(cp_final, utf8_seq) > 0) {
                utf8_insertar_en_cursor(buf, cap, cursor, utf8_seq);
            }
        }

        cp = GetCharPressed();
    }
}

// ============================================================================
// SISTEMA DE AUDIO PROCEDURAL EN MEMORIA (RAUDIO)
// ============================================================================

static Sound generar_sonido_sintetico(float freq_ini, float freq_fin, float duracion_seg, int tipo_onda)
{
    int sample_rate = 44100;
    int total_muestras = (int)(sample_rate * duracion_seg);
    short *datos = (short *)malloc((size_t)total_muestras * sizeof(short));

    float fase = 0.0f;
    for (int i = 0; i < total_muestras; i++) {
        float progreso = (float)i / (float)total_muestras;
        float freq = freq_ini + (freq_fin - freq_ini) * progreso;
        float envolvente = 1.0f - progreso;

        if (tipo_onda == 2 && progreso > 0.5f) {
            freq = freq_fin * 1.25f;
        }

        fase += (2.0f * 3.14159265f * freq) / (float)sample_rate;
        float muestra = sinf(fase);

        if (tipo_onda == 1) {
            muestra = (muestra >= 0.0f ? 0.8f : -0.8f);
        } else if (tipo_onda == 3) {
            // Ruido de explosión sísmica
            float r = (float)((i * 1103515245 + 12345) & 0x7fff) / 16384.0f - 1.0f;
            muestra = (muestra * 0.4f + r * 0.6f);
        }

        datos[i] = (short)(muestra * envolvente * 9500.0f);
    }

    Wave onda = { 0 };
    onda.frameCount = (unsigned int)total_muestras;
    onda.sampleRate = (unsigned int)sample_rate;
    onda.sampleSize = 16;
    onda.channels = 1;
    onda.data = datos;

    Sound s = LoadSoundFromWave(onda);
    free(datos);
    return s;
}

static void inicializar_audio_procedural(void)
{
    InitAudioDevice();
    if (!IsAudioDeviceReady()) return;

    sonidos_motor[SONIDO_CLIC]      = generar_sonido_sintetico(680.0f,  920.0f, 0.045f, 0);
    sonidos_motor[SONIDO_CONSTRUIR] = generar_sonido_sintetico(240.0f,  160.0f, 0.075f, 0);
    sonidos_motor[SONIDO_MINAR]     = generar_sonido_sintetico(420.0f,  190.0f, 0.065f, 1);
    sonidos_motor[SONIDO_CRAFT]     = generar_sonido_sintetico(440.0f,  660.0f, 0.180f, 2);
    sonidos_motor[SONIDO_PUERTA]    = generar_sonido_sintetico(300.0f,  460.0f, 0.110f, 1);
    sonidos_motor[SONIDO_ALERTA]    = generar_sonido_sintetico(220.0f,  140.0f, 0.160f, 1);
    sonidos_motor[SONIDO_ESPADA]    = generar_sonido_sintetico(580.0f,  210.0f, 0.095f, 0);
    sonidos_motor[SONIDO_EXPLOSION] = generar_sonido_sintetico(140.0f,   45.0f, 0.340f, 3);

    audio_inicializado = true;
}

static void reproducir_sonido_motor(int id)
{
    if (!audio_inicializado || !audio_activo) return;
    if (id >= 0 && id < TOTAL_SONIDOS) {
        PlaySound(sonidos_motor[id]);
    }
}

static void cerrar_audio_procedural(void)
{
    if (!audio_inicializado) return;
    for (int i = 0; i < TOTAL_SONIDOS; i++) {
        UnloadSound(sonidos_motor[i]);
    }
    CloseAudioDevice();
}

// ============================================================================
// MOTOR TIPOGRÁFICO UTF-8 CON AUTOAJUSTE Y SALTO DE LÍNEA
// ============================================================================

static void inicializar_fuente_utf8(void)
{
    const char *rutas_ttf[] = {
        "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        NULL
    };

    int codepoints[224];
    for (int i = 0; i < 224; i++) {
        codepoints[i] = 32 + i;
    }

    for (int r = 0; rutas_ttf[r] != NULL; r++) {
        if (FileExists(rutas_ttf[r])) {
            fuente_ui = LoadFontEx(rutas_ttf[r], 32, codepoints, 224);
            SetTextureFilter(fuente_ui.texture, TEXTURE_FILTER_BILINEAR);
            fuente_ttf_cargada = true;
            break;
        }
    }

    if (!fuente_ttf_cargada) {
        fuente_ui = GetFontDefault();
    }
}

static int medir_texto_ui(const char *texto, int font_size)
{
    if (texto == NULL || texto[0] == '\0') return 0;
    Vector2 dim = MeasureTextEx(fuente_ui, texto, (float)font_size, 1.0f);
    return (int)dim.x;
}

static void dibujar_texto_ui(const char *texto, int x, int y, int font_size, Color color)
{
    if (texto == NULL || texto[0] == '\0') return;
    DrawTextEx(fuente_ui, texto, (Vector2){ (float)x, (float)y }, (float)font_size, 1.0f, color);
}

static void dibujar_texto_ajustado(const char *texto, Rectangle caja, int font_max, bool centrar, Color color)
{
    if (texto == NULL || texto[0] == '\0') return;

    int font_actual = font_max;
    int max_w = (int)caja.width - 12;
    if (max_w < 20) max_w = 20;

    while (font_actual > 12 && medir_texto_ui(texto, font_actual) > max_w) {
        font_actual--;
    }

    int ancho_txt = medir_texto_ui(texto, font_actual);
    int pos_x = centrar ? (int)caja.x + ((int)caja.width - ancho_txt) / 2 : (int)caja.x + 8;
    if (pos_x < (int)caja.x + 4) pos_x = (int)caja.x + 4;
    int pos_y = (int)caja.y + ((int)caja.height - font_actual) / 2;

    if (ancho_txt > max_w) {
        const char *vista = texto;
        while (*vista != '\0' && medir_texto_ui(vista, font_actual) > max_w) {
            vista = texto + utf8_next_char(texto, (int)(vista - texto));
        }
        dibujar_texto_ui(vista, (int)caja.x + 6, pos_y, font_actual, color);
    } else {
        dibujar_texto_ui(texto, pos_x, pos_y, font_actual, color);
    }
}

static int procesar_texto_multilinea(const char *texto, Rectangle area, int font_size, Color color, bool dibujar)
{
    if (texto == NULL || texto[0] == '\0') return font_size + 8;

    int max_w = (int)area.width;
    if (max_w < 60) max_w = 60;

    char copia[512];
    snprintf(copia, sizeof(copia), "%s", texto);

    char linea_actual[256] = "";
    char linea_prueba[640] = "";
    int cursor_y = (int)area.y;
    int salto_y = font_size + 6;

    const char *p = copia;
    while (*p != '\0') {
        if (*p == '\n') {
            if (dibujar && linea_actual[0] != '\0') {
                dibujar_texto_ui(linea_actual, (int)area.x, cursor_y, font_size, color);
            }
            linea_actual[0] = '\0';
            cursor_y += salto_y;
            p++;
            continue;
        }

        char palabra[128];
        int wi = 0;
        while (*p != '\0' && *p != ' ' && *p != '\n' && wi < (int)sizeof(palabra) - 1) {
            palabra[wi++] = *p++;
        }
        palabra[wi] = '\0';
        while (*p == ' ') p++;

        if (linea_actual[0] == '\0') {
            snprintf(linea_prueba, sizeof(linea_prueba), "%s", palabra);
        } else {
            snprintf(linea_prueba, sizeof(linea_prueba), "%.250s %.120s", linea_actual, palabra);
        }

        if (medir_texto_ui(linea_prueba, font_size) <= max_w) {
            snprintf(linea_actual, sizeof(linea_actual), "%.254s", linea_prueba);
        } else {
            if (dibujar && linea_actual[0] != '\0') {
                dibujar_texto_ui(linea_actual, (int)area.x, cursor_y, font_size, color);
            }
            snprintf(linea_actual, sizeof(linea_actual), "%s", palabra);
            cursor_y += salto_y;
        }
    }

    if (linea_actual[0] != '\0') {
        if (dibujar) {
            dibujar_texto_ui(linea_actual, (int)area.x, cursor_y, font_size, color);
        }
        cursor_y += salto_y;
    }

    return (cursor_y - (int)area.y);
}

// ============================================================================
// GENERADOR PROCEDURAL DE BIOMAS, TERRENO Y ACCESO ESPACIAL (64x64 CHUNKS)
// ============================================================================

static int calcular_ruido_hash(int x, int y, int s)
{
    unsigned int ux = (unsigned int)x;
    unsigned int uy = (unsigned int)y;
    unsigned int us = (unsigned int)s;
    unsigned int h = ux * 374761393U + uy * 668265263U + us * 1442695041U;
    h = (h ^ (h >> 13)) * 1274126177U;
    return (int)((h ^ (h >> 16)) % 100U);
}

// Determina cuál de los 5 Biomas corresponde a la coordenada global (gx, gy)
static TipoBioma obtener_bioma_en(int gx, int gy)
{
    int dx = gx - 512;
    int dy = gy - 512;

    // El centro (alrededor de 512,512) es siempre Bosque Templado seguro
    if (abs(dx) < 90 && abs(dy) < 90) {
        return BIOMA_BOSQUE;
    }

    // Distribución cardinal + perturbación suave por región
    int rx = gx / 48;
    int ry = gy / 48;
    int jitter = (calcular_ruido_hash(rx, ry, semilla_mundo + 77) % 40) - 20;

    if (dx + jitter > 90 && abs(dy) < 220)  return BIOMA_DESIERTO; // Este: Desierto
    if (dx + jitter < -90 && abs(dy) < 220) return BIOMA_VOLCAN;   // Oeste: Volcán
    if (dy + jitter < -90)                  return BIOMA_NIEVE;    // Norte: Tundra Nevada
    if (dy + jitter > 90)                   return BIOMA_PANTANO;  // Sur: Pantano Místico

    int n_bioma = calcular_ruido_hash(rx, ry, semilla_mundo + 333);
    if (n_bioma < 25) return BIOMA_BOSQUE;
    if (n_bioma < 45) return BIOMA_DESIERTO;
    if (n_bioma < 65) return BIOMA_NIEVE;
    if (n_bioma < 82) return BIOMA_PANTANO;
    return BIOMA_VOLCAN;
}

static const char *nombre_bioma(TipoBioma b)
{
    switch (b) {
        case BIOMA_BOSQUE:   return "Bosque Templado";
        case BIOMA_DESIERTO: return "Desierto Árido";
        case BIOMA_NIEVE:    return "Tundra Nevada";
        case BIOMA_VOLCAN:   return "Tierras Volcánicas";
        case BIOMA_PANTANO:  return "Pantano Místico";
        default:             return "Bosque";
    }
}

static void agregar_log_consola(const char *texto)
{
    if (texto == NULL) return;
    if (total_logs_consola < MAX_LOGS_CONSOLA) {
        snprintf(historial_consola[total_logs_consola++], sizeof(historial_consola[0]), "%s", texto);
    } else {
        memmove(historial_consola[0],
                historial_consola[1],
                (size_t)(MAX_LOGS_CONSOLA - 1) * sizeof(historial_consola[0]));
        snprintf(historial_consola[MAX_LOGS_CONSOLA - 1], sizeof(historial_consola[0]), "%s", texto);
    }
}

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
    (void)ui_utf8_copiar_seguro(texto_toast, sizeof texto_toast, msg);
    (void)ui_utf8_copiar_seguro(estado_sistema, sizeof estado_sistema, msg);
    tiempo_toast = 3.5f;
}

static void registrar_semilla_activa(int gx, int gy)
{
    for (int i = 0; i < MAX_SEMILLAS_ACTIVAS; i++) {
        if (!lista_semillas[i].activa || (lista_semillas[i].gx == gx && lista_semillas[i].gy == gy)) {
            lista_semillas[i].activa = true;
            lista_semillas[i].gx = gx;
            lista_semillas[i].gy = gy;
            lista_semillas[i].tiempo_restante = 5.0f;
            return;
        }
    }
}

static void registrar_tnt_activa(int gx, int gy)
{
    for (int i = 0; i < MAX_TNT_ACTIVAS; i++) {
        if (!lista_tnt[i].activa || (lista_tnt[i].gx == gx && lista_tnt[i].gy == gy)) {
            lista_tnt[i].activa = true;
            lista_tnt[i].gx = gx;
            lista_tnt[i].gy = gy;
            lista_tnt[i].tiempo_restante = 3.0f; // Mecha automática de 3 segundos
            return;
        }
    }
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
    if (tipo == 13) registrar_semilla_activa(gx, gy);
    if (tipo == 14) registrar_tnt_activa(gx, gy);
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
// GESTIÓN DE EJÉRCITO DE GÓLEMS ILIMITADOS, ENEMIGOS, TNT Y PUENTES
// ============================================================================

static int contar_golems_activos(void)
{
    int c = 0;
    for (int i = 0; i < MAX_GOLEMS; i++) {
        if (lista_golems[i].activo) c++;
    }
    return c;
}

static int contar_enemigos_activos(void)
{
    int c = 0;
    for (int i = 0; i < MAX_ENEMIGOS; i++) {
        if (lista_enemigos[i].activo) c++;
    }
    return c;
}

static void crear_nuevo_golem(int gx, int gy, int tarea)
{
    if (gx <= 1) gx = 2;
    if (gy <= 1) gy = 2;
    if (gx >= MAP_WIDTH - 2)  gx = MAP_WIDTH - 3;
    if (gy >= MAP_HEIGHT - 2) gy = MAP_HEIGHT - 3;

    for (int i = 0; i < MAX_GOLEMS; i++) {
        if (!lista_golems[i].activo) {
            lista_golems[i].activo = true;
            lista_golems[i].gx = gx;
            lista_golems[i].gy = gy;
            lista_golems[i].tarea = (tarea >= 0 && tarea <= 3) ? tarea : golem_tarea_global;
            // Asignar dirección inicial variada para cubrir terreno
            const int dirs[4][2] = { {1,0}, {-1,0}, {0,1}, {0,-1} };
            int d = i % 4;
            lista_golems[i].dir_x = dirs[d][0];
            lista_golems[i].dir_y = dirs[d][1];
            return;
        }
    }
}

static void crear_enemigo_en(int gx, int gy, int tipo)
{
    if (gx <= 1 || gx >= MAP_WIDTH - 2 || gy <= 1 || gy >= MAP_HEIGHT - 2) return;
    for (int i = 0; i < MAX_ENEMIGOS; i++) {
        if (!lista_enemigos[i].activo) {
            lista_enemigos[i].activo = true;
            lista_enemigos[i].tipo = tipo % 3;
            lista_enemigos[i].gx = gx;
            lista_enemigos[i].gy = gy;
            int hp_base = (tipo == 0) ? 30 : ((tipo == 1) ? 45 : 75);
            lista_enemigos[i].hp = hp_base;
            lista_enemigos[i].max_hp = hp_base;
            return;
        }
    }
}

static void invocar_oleada_enemigos(int cantidad)
{
    for (int k = 0; k < cantidad; k++) {
        int ang_x = ((k % 2 == 0) ? 1 : -1) * (6 + (k % 5));
        int ang_y = ((k % 3 == 0) ? 1 : -1) * (5 + (k % 4));
        int ex = jugador_x + ang_x;
        int ey = jugador_y + ang_y;
        TipoBioma b = obtener_bioma_en(ex, ey);
        int tipo = (b == BIOMA_VOLCAN) ? 2 : ((b == BIOMA_DESIERTO || b == BIOMA_NIEVE) ? 1 : 0);
        crear_enemigo_en(ex, ey, tipo);
    }
}

// Detona un bloque de TNT en (bx, by) limpiando 3x3, recolectando recursos y dañando enemigos
static void detonar_tnt_en(int bx, int by)
{
    int recolectados = 0;
    int enemigos_eliminados = 0;

    for (int dy = -2; dy <= 2; dy++) {
        for (int dx = -2; dx <= 2; dx++) {
            if (abs(dx) <= 1 && abs(dy) <= 1) {
                int tx = bx + dx;
                int ty = by + dy;
                if (tx <= 0 || tx >= MAP_WIDTH - 1 || ty <= 0 || ty >= MAP_HEIGHT - 1) continue;

                int actual = obtener_casilla_mundo(tx, ty);
                if (actual == 1 || actual == 4 || actual == 6 || actual == 16 || actual == 19) {
                    inventario[actual]++;
                    recolectados++;
                }
                establecer_casilla_mundo(tx, ty, 5); // Deja cráter transitable
            }
        }
    }

    // Dañar enemigos en un radio de 4 casillas de la explosión
    for (int e = 0; e < MAX_ENEMIGOS; e++) {
        if (!lista_enemigos[e].activo) continue;
        if (abs(lista_enemigos[e].gx - bx) <= 3 && abs(lista_enemigos[e].gy - by) <= 3) {
            lista_enemigos[e].activo = false;
            inventario[19]++; // Botín: Cristal Rubí
            enemigos_derrotados++;
            enemigos_eliminados++;
        }
    }

    explosion_vis_x = bx;
    explosion_vis_y = by;
    tiempo_onda_explosion = 0.45f;
    reproducir_sonido_motor(SONIDO_EXPLOSION);

    char msg[160];
    snprintf(msg, sizeof(msg), "💥 ¡BOOM! Explosión TNT (+%d bloques minados, %d enemigos abatidos)",
             recolectados, enemigos_eliminados);
    mostrar_notificacion_toast(msg);
}

// Construye un puente de madera (8) sobre agua (2) o lava (7) alrededor del jugador o en su dirección
static bool tender_puente_cercano(void)
{
    int puestos = 0;

    // 1. Tender calzada de 4 casillas en la dirección en la que mira el jugador
    for (int paso = 1; paso <= 4; paso++) {
        for (int ancho = -1; ancho <= 1; ancho++) {
            int tx = jugador_x + ultima_dir_x * paso + (ultima_dir_y != 0 ? ancho : 0);
            int ty = jugador_y + ultima_dir_y * paso + (ultima_dir_x != 0 ? ancho : 0);
            int t = obtener_casilla_mundo(tx, ty);
            if (t == 2 || t == 7) {
                establecer_casilla_mundo(tx, ty, 8);
                puestos++;
            }
        }
    }

    // 2. Además revisar un radio de 2 casillas alrededor del jugador por si hay agua/lava adyacente
    for (int dy = -2; dy <= 2; dy++) {
        for (int dx = -2; dx <= 2; dx++) {
            int tx = jugador_x + dx;
            int ty = jugador_y + dy;
            int t = obtener_casilla_mundo(tx, ty);
            if (t == 2 || t == 7) {
                establecer_casilla_mundo(tx, ty, 8);
                puestos++;
            }
        }
    }

    if (puestos > 0) {
        reproducir_sonido_motor(SONIDO_CONSTRUIR);
        mostrar_notificacion_toast("🌉 ¡Puente de Madera tendido sobre el agua/lava!");
        return true;
    }
    return false;
}

// Ataque con Espada del Jugador [Tecla F]
static void ejecutar_ataque_espada_jugador(void)
{
    tiempo_ataque_espada = 0.28f;
    reproducir_sonido_motor(SONIDO_ESPADA);

    int golpeados = 0;
    int abatidos = 0;

    for (int e = 0; e < MAX_ENEMIGOS; e++) {
        if (!lista_enemigos[e].activo) continue;
        int dx = lista_enemigos[e].gx - jugador_x;
        int dy = lista_enemigos[e].gy - jugador_y;
        if (abs(dx) <= 2 && abs(dy) <= 2) {
            lista_enemigos[e].hp -= 25;
            golpeados++;
            // Empuje (Knockback)
            int kx = lista_enemigos[e].gx + (dx > 0 ? 1 : (dx < 0 ? -1 : ultima_dir_x));
            int ky = lista_enemigos[e].gy + (dy > 0 ? 1 : (dy < 0 ? -1 : ultima_dir_y));
            if (kx > 1 && kx < MAP_WIDTH - 2 && ky > 1 && ky < MAP_HEIGHT - 2) {
                lista_enemigos[e].gx = kx;
                lista_enemigos[e].gy = ky;
            }
            if (lista_enemigos[e].hp <= 0) {
                lista_enemigos[e].activo = false;
                inventario[19]++; // Botín: +1 Cristal Rubí
                inventario[14]++; // Botín: +1 TNT
                enemigos_derrotados++;
                abatidos++;
            }
        }
    }

    if (abatidos > 0) {
        reproducir_sonido_motor(SONIDO_CRAFT);
        mostrar_notificacion_toast(TextFormat("⚔️ ¡Enemigo derrotado! (+1 Cristal Rubí, +1 TNT | Total: %d)", enemigos_derrotados));
    } else if (golpeados > 0) {
        mostrar_notificacion_toast(TextFormat("⚔️ ¡Golpe de espada a %d enemigo(s)!", golpeados));
    }
}

// ============================================================================
// PUENTE FFI (C <-> LATINO): API "motor.*"
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

static void motor_nodo_luz(lat_mv *mv)
{
    lat_objeto *o_radio = latC_desapilar(mv);
    lat_objeto *o_id    = latC_desapilar(mv);

    int id = (int)latC_checar_numerico(mv, o_id);
    int radio = (int)latC_checar_numerico(mv, o_radio);

    if (id >= 0 && id < MAX_NODOS) {
        tabla_nodos[id].radio_luz = radio;
    }
}

static void motor_sonido(lat_mv *mv)
{
    lat_objeto *o_id = latC_desapilar(mv);
    int id = (int)latC_checar_numerico(mv, o_id);
    reproducir_sonido_motor(id);
}

static void motor_fijar_audio(lat_mv *mv)
{
    lat_objeto *o_est = latC_desapilar(mv);
    audio_activo = ((int)latC_checar_numerico(mv, o_est) != 0);
}

static void motor_abrir_consola(lat_mv *mv)
{
    (void)mv;
    consola_activa = !consola_activa;
    cursor_consola_bytes = (int)strlen(entrada_consola);
    id_caja_activa = -1;
}

static void motor_inv_cantidad(lat_mv *mv)
{
    lat_objeto *o_id = latC_desapilar(mv);
    int id = (int)latC_checar_numerico(mv, o_id);
    int cant = (id >= 0 && id < MAX_NODOS) ? inventario[id] : 0;
    latC_apilar(mv, latC_crear_numerico(mv, (double)cant));
}

static void motor_inv_agregar(lat_mv *mv)
{
    lat_objeto *o_cant = latC_desapilar(mv);
    lat_objeto *o_id   = latC_desapilar(mv);

    int id   = (int)latC_checar_numerico(mv, o_id);
    int cant = (int)latC_checar_numerico(mv, o_cant);

    if (id >= 0 && id < MAX_NODOS) {
        inventario[id] += cant;
        if (inventario[id] < 0) inventario[id] = 0;
        if (inventario[id] > 999) inventario[id] = 999;
    }
}

// Funciones FFI de Gólems Ilimitados y Enemigos
static void motor_golem_crear(lat_mv *mv)
{
    lat_objeto *o_tarea = latC_desapilar(mv);
    lat_objeto *o_y     = latC_desapilar(mv);
    lat_objeto *o_x     = latC_desapilar(mv);
    int gx = (int)latC_checar_numerico(mv, o_x);
    int gy = (int)latC_checar_numerico(mv, o_y);
    int tarea = (int)latC_checar_numerico(mv, o_tarea);
    crear_nuevo_golem(gx, gy, tarea);
}

static void motor_golem_colocar(lat_mv *mv)
{
    lat_objeto *o_y = latC_desapilar(mv);
    lat_objeto *o_x = latC_desapilar(mv);
    int gx = (int)latC_checar_numerico(mv, o_x);
    int gy = (int)latC_checar_numerico(mv, o_y);
    crear_nuevo_golem(gx, gy, golem_tarea_global);
}

static void motor_golem_total(lat_mv *mv)
{
    latC_apilar(mv, latC_crear_numerico(mv, (double)contar_golems_activos()));
}

static void motor_golem_reunir(lat_mv *mv)
{
    (void)mv;
    int idx = 0;
    for (int i = 0; i < MAX_GOLEMS; i++) {
        if (lista_golems[i].activo) {
            int ox = (idx % 5) - 2;
            int oy = ((idx / 5) % 5) - 2;
            if (ox == 0 && oy == 0) ox = 1;
            lista_golems[i].gx = jugador_x + ox;
            lista_golems[i].gy = jugador_y + oy;
            idx++;
        }
    }
}

static void motor_golem_limpiar(lat_mv *mv)
{
    (void)mv;
    memset(lista_golems, 0, sizeof(lista_golems));
}

static void motor_golem_tarea(lat_mv *mv)
{
    latC_apilar(mv, latC_crear_numerico(mv, (double)golem_tarea_global));
}

static void motor_golem_fijar_tarea(lat_mv *mv)
{
    lat_objeto *o_t = latC_desapilar(mv);
    golem_tarea_global = (int)latC_checar_numerico(mv, o_t);
    for (int i = 0; i < MAX_GOLEMS; i++) {
        if (lista_golems[i].activo) {
            lista_golems[i].tarea = golem_tarea_global;
        }
    }
}

static void motor_enemigo_oleada(lat_mv *mv)
{
    lat_objeto *o_cant = latC_desapilar(mv);
    int cant = (int)latC_checar_numerico(mv, o_cant);
    if (cant < 1) cant = 1;
    if (cant > 30) cant = 30;
    invocar_oleada_enemigos(cant);
}

static void motor_enemigo_limpiar(lat_mv *mv)
{
    (void)mv;
    memset(lista_enemigos, 0, sizeof(lista_enemigos));
}

static void motor_fijar_enemigos(lat_mv *mv)
{
    lat_objeto *o_est = latC_desapilar(mv);
    spawner_enemigos_activo = ((int)latC_checar_numerico(mv, o_est) != 0);
}

static void motor_curar_jugador(lat_mv *mv)
{
    lat_objeto *o_hp = latC_desapilar(mv);
    int curar = (int)latC_checar_numerico(mv, o_hp);
    jugador_hp += curar;
    if (jugador_hp > jugador_max_hp) jugador_hp = jugador_max_hp;
}

static void motor_construir_puente(lat_mv *mv)
{
    (void)mv;
    if (!tender_puente_cercano()) {
        // Si no hay agua al lado, coloca 3 bloques de puente de madera frente al jugador
        for (int p = 1; p <= 3; p++) {
            establecer_casilla_mundo(jugador_x + ultima_dir_x * p, jugador_y + ultima_dir_y * p, 8);
        }
        reproducir_sonido_motor(SONIDO_CONSTRUIR);
        mostrar_notificacion_toast("🌉 ¡Pasarela de Madera de 3 bloques construida frente a ti!");
    }
}

static void motor_ui_resumen_inv(lat_mv *mv)
{
    (void)mv;
    if (total_widgets_ui < MAX_WIDGETS_UI) {
        WidgetUI *w = &widgets_ui[total_widgets_ui++];
        memset(w, 0, sizeof(WidgetUI));
        w->activo = true;
        w->tipo = WIDGET_INVENTARIO;
        w->pestana = pestana_en_construccion;
        w->fila_id = 0; // solo los botones participan en filas por ahora
        w->id = -2;
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
        if (contenedores_ui.grupo_actual != 0) {
            fprintf(stderr, "LatinoUI: cerrando fila pendiente al cambiar de pestana\n");
            (void)ui_cont_fin(&contenedores_ui);
        }
    }
}

// Primera API declarativa: una sola fila abierta a la vez.
// Solo deben agruparse botones en esta versión experimental.
static void motor_ui_iniciar_fila(lat_mv *mv)
{
    lat_objeto *o = latC_desapilar(mv);
    double espacio = latC_checar_numerico(mv, o);
    UIContResultado resultado = ui_cont_iniciar_fila(&contenedores_ui, (float)espacio);
    if (resultado != UI_CONT_OK) {
        fprintf(stderr, "LatinoUI: no se pudo iniciar fila (codigo %d)\n", (int)resultado);
        return;
    }
}

static void motor_ui_fin_contenedor(lat_mv *mv)
{
    (void)mv;
    UIContResultado resultado = ui_cont_fin(&contenedores_ui);
    if (resultado != UI_CONT_OK) {
        fprintf(stderr, "LatinoUI: cierre de fila invalido (codigo %d)\n", (int)resultado);
        return;
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
        w->fila_id = 0; // solo los botones participan en filas por ahora
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
        UIContResultado registro = ui_cont_agregar(&contenedores_ui, id);
        if (registro != UI_CONT_OK) {
            fprintf(stderr, "LatinoUI: error al registrar boton %d (codigo %d)\n", id, (int)registro);
            total_widgets_ui--;
            memset(w, 0, sizeof(*w));
            return;
        }
        w->fila_id = (int)ui_cont_grupo_de(&contenedores_ui, contenedores_ui.cantidad - 1);
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
        w->fila_id = 0; // solo los botones participan en filas por ahora
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
        w->fila_id = 0; // solo los botones participan en filas por ahora
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
        w->fila_id = 0; // solo los botones participan en filas por ahora
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
        w->fila_id = 0; // solo los botones participan en filas por ahora
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

static void motor_fijar_creativo(lat_mv *mv)
{
    lat_objeto *o_est = latC_desapilar(mv);
    modo_creativo = ((int)latC_checar_numerico(mv, o_est) != 0);
}

static void motor_fijar_ciclo_luz(lat_mv *mv)
{
    lat_objeto *o_est = latC_desapilar(mv);
    ciclo_luz_auto = ((int)latC_checar_numerico(mv, o_est) != 0);
}

static void motor_fijar_hora(lat_mv *mv)
{
    lat_objeto *o_hora = latC_desapilar(mv);
    float h = (float)latC_checar_numerico(mv, o_hora);
    while (h < 0.0f) h += 24.0f;
    while (h >= 24.0f) h -= 24.0f;
    hora_dia = h;

    WidgetUI *w_hora = buscar_widget_por_id(306);
    if (w_hora != NULL) w_hora->valor = (int)hora_dia;
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
    memset(lista_semillas, 0, sizeof(lista_semillas));
    memset(lista_tnt, 0, sizeof(lista_tnt));
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

    int x = (int)latC_checar_numerico(mv, o_x);
    int y = (int)latC_checar_numerico(mv, o_y);
    int s = (int)latC_checar_numerico(mv, o_semilla);

    int resultado = calcular_ruido_hash(x, y, s);
    latC_apilar(mv, latC_crear_numerico(mv, (double)resultado));
}

static const lat_CReg libmotor[] = {
    {"registrar_nodo",     motor_registrar_nodo,     6},
    {"nodo_luz",           motor_nodo_luz,           2},
    {"sonido",             motor_sonido,             1},
    {"fijar_audio",        motor_fijar_audio,        1},
    {"abrir_consola",      motor_abrir_consola,      0},
    {"inv_cantidad",       motor_inv_cantidad,       1},
    {"inv_agregar",        motor_inv_agregar,        2},
    {"golem_crear",        motor_golem_crear,        3},
    {"golem_colocar",      motor_golem_colocar,      2},
    {"golem_total",        motor_golem_total,        0},
    {"golem_reunir",       motor_golem_reunir,       0},
    {"golem_limpiar",      motor_golem_limpiar,      0},
    {"golem_tarea",        motor_golem_tarea,        0},
    {"golem_fijar_tarea",  motor_golem_fijar_tarea,  1},
    {"enemigo_oleada",     motor_enemigo_oleada,     1},
    {"enemigo_limpiar",    motor_enemigo_limpiar,    0},
    {"fijar_enemigos",     motor_fijar_enemigos,     1},
    {"curar_jugador",      motor_curar_jugador,      1},
    {"construir_puente",   motor_construir_puente,   0},
    {"ui_resumen_inv",     motor_ui_resumen_inv,     0},
    {"ui_titulo",          motor_ui_titulo,          1},
    {"ui_pestana",         motor_ui_pestana,         2},
    {"ui_etiqueta",        motor_ui_etiqueta,        1},
    {"ui_boton",           motor_ui_boton,           5},
    {"ui_iniciar_fila",    motor_ui_iniciar_fila,    1},
    {"ui_fin_contenedor",  motor_ui_fin_contenedor,  0},
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
    {"fijar_creativo",     motor_fijar_creativo,     1},
    {"fijar_ciclo_luz",    motor_fijar_ciclo_luz,    1},
    {"fijar_hora",         motor_fijar_hora,         1},
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
// EJECUCIÓN DE SCRIPTS LATINO Y GENERACIÓN MULTI-BIOMA DE CHUNKS (16x16)
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

static void evaluar_comando_consola_latino(const char *codigo)
{
    if (codigo == NULL || codigo[0] == '\0') return;

    char reg_cmd[160];
    snprintf(reg_cmd, sizeof(reg_cmd), "latino> %.145s", codigo);
    agregar_log_consola(reg_cmd);

    FILE *f = fopen(ARCHIVO_TEMP_CONSOLA, "w");
    if (f == NULL) {
        agregar_log_consola("[Error] No se pudo crear el buffer temporal.");
        return;
    }
    fprintf(f, "%s\n", codigo);
    fclose(f);

    double t0 = GetTime();
    lat_mv *mv = latC_crear_mv();
    latC_abrir_liblatino(mv, "motor", libmotor);

    int status = 0;
    mv->nombre_archivo = (char *)ARCHIVO_TEMP_CONSOLA;
    mv->global->REPL = false;

    ast *nodo = latA_analizar_arch((char *)ARCHIVO_TEMP_CONSOLA, &status);
    if (status == 0 && nodo != NULL) {
        lat_objeto *main_func = latC_analizar(mv, nodo);
        latC_llamar_funcion(mv, main_func);
        latO_destruir(mv, main_func);
        latA_destruir(nodo);
        ultimo_tiempo_vm_ms = (GetTime() - t0) * 1000.0;

        char ok_msg[160];
        snprintf(ok_msg, sizeof(ok_msg), "  [OK en %.2f ms] -> %.110s", ultimo_tiempo_vm_ms, estado_sistema);
        agregar_log_consola(ok_msg);
        reproducir_sonido_motor(SONIDO_CLIC);
    } else {
        agregar_log_consola("  [Error de Sintaxis] Revisa la instrucción en español.");
        reproducir_sonido_motor(SONIDO_ALERTA);
    }

    latC_destruir_mv(mv);
    actualizar_chunks_cercanos();
}

// Genera un Chunk de 16x16 según su Bioma, colocando puentes automáticos en las avenidas centrales
static void generar_chunk_bajo_demanda(int cx, int cy)
{
    if (cx < 0 || cx >= WORLD_CHUNKS_X || cy < 0 || cy >= WORLD_CHUNKS_Y) return;
    if (mundo[cy][cx].generado) return;

    mundo[cy][cx].generado = true;
    int base_x = cx * CHUNK_SIZE;
    int base_y = cy * CHUNK_SIZE;

    for (int ly = 0; ly < CHUNK_SIZE; ly++) {
        for (int lx = 0; lx < CHUNK_SIZE; lx++) {
            int gx = base_x + lx;
            int gy = base_y + ly;

            // Bordes del universo 1024x1024
            if (gx == 0 || gy == 0 || gx == MAP_WIDTH - 1 || gy == MAP_HEIGHT - 1) {
                mundo[cy][cx].casillas[ly][lx] = 1;
                continue;
            }

            TipoBioma bioma = obtener_bioma_en(gx, gy);
            int v = calcular_ruido_hash(gx, gy, semilla_mundo);
            int tipo = 0;

            if (bioma == BIOMA_BOSQUE) {
                tipo = 0; // Pasto
                if (v < 9)       tipo = 2; // Agua
                else if (v < 14) tipo = 6; // Costa de Arena
                else if (v > 82) tipo = 4; // Árbol
                else if (v == 80) tipo = 1; // Roca
            } else if (bioma == BIOMA_DESIERTO) {
                tipo = 6; // Arena
                if (v < 3)       tipo = 2;  // Oasis de Agua
                else if (v > 90) tipo = 16; // Cactus
                else if (v > 85) tipo = 1;  // Roca del desierto
            } else if (bioma == BIOMA_NIEVE) {
                tipo = 15; // Nieve
                if (v < 14)      tipo = 17; // Lago de Hielo transitable
                else if (v > 84) tipo = 4;  // Pino
                else if (v > 80) tipo = 1;  // Roca helada
            } else if (bioma == BIOMA_VOLCAN) {
                tipo = 18; // Basalto oscuro
                if (v < 14)      tipo = 7;  // Fosa de Lava
                else if (v > 94) tipo = 19; // Cristal Rubí luminoso
                else if (v > 80) tipo = 1;  // Roca volcánica
            } else {
                // BIOMA_PANTANO
                tipo = 0;
                if (v < 18)      tipo = 2;  // Ciénaga de Agua
                else if (v > 76) tipo = 4;  // Árbol de pantano
                else if (v == 75) tipo = 19; // Cristal místico
            }

            // Grandes carreteras reales que cruzan el mundo en X=512 y Y=512
            // Si cruzan agua (2) o lava (7), tienden automáticamente Puente de Madera (8)
            if (gx == 512 || gy == 512) {
                if (tipo == 2 || tipo == 7) {
                    tipo = 8; // Puente de Madera sobre el agua/lava
                } else {
                    tipo = 5; // Camino Real
                }
            }

            mundo[cy][cx].casillas[ly][lx] = (unsigned char)tipo;
        }
    }

    total_chunks_generados++;
    WidgetUI *w_prog = buscar_widget_por_id(400);
    if (w_prog != NULL && w_prog->tipo == WIDGET_PROGRESO) {
        w_prog->valor = total_chunks_generados;
    }

    // Probabilidad de generar un enemigo natural en chunks nuevos alejados del centro
    if (spawner_enemigos_activo && (abs(cx - 32) > 1 || abs(cy - 32) > 1)) {
        if ((calcular_ruido_hash(cx, cy, semilla_mundo + 99) % 100) < 45) {
            int ex = base_x + 8;
            int ey = base_y + 8;
            if (obtener_casilla_mundo(ex, ey) != 2 && obtener_casilla_mundo(ex, ey) != 7) {
                TipoBioma b = obtener_bioma_en(ex, ey);
                int t_enem = (b == BIOMA_VOLCAN) ? 2 : ((b == BIOMA_DESIERTO || b == BIOMA_NIEVE) ? 1 : 0);
                crear_enemigo_en(ex, ey, t_enem);
            }
        }
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

// ============================================================================
// SIMULACIÓN EN TIEMPO REAL DEL EJÉRCITO DE GÓLEMS Y DE LOS ENEMIGOS
// ============================================================================

static bool casilla_es_transitable(int gx, int gy)
{
    if (gx < 0 || gx >= MAP_WIDTH || gy < 0 || gy >= MAP_HEIGHT) return false;
    int tipo = obtener_casilla_mundo(gx, gy);
    if (tipo >= 0 && tipo < MAX_NODOS && tabla_nodos[tipo].registrado) {
        return !tabla_nodos[tipo].es_solido;
    }
    return false;
}

static void actualizar_ia_golems_y_enemigos(void)
{
    // 1. Actualizar todos los Gólems activos (hasta 512 Gólems simultáneos)
    for (int i = 0; i < MAX_GOLEMS; i++) {
        if (!lista_golems[i].activo) continue;
        EntidadGolem *g = &lista_golems[i];
        if (g->tarea == 0) continue;

        generar_chunk_bajo_demanda(g->gx / CHUNK_SIZE, g->gy / CHUNK_SIZE);

        // Prioridad universal del Gólem: si tiene un enemigo al lado (distancia <= 2), ¡lo ataca!
        bool ataco_enemigo = false;
        for (int e = 0; e < MAX_ENEMIGOS; e++) {
            if (!lista_enemigos[e].activo) continue;
            int edx = lista_enemigos[e].gx - g->gx;
            int edy = lista_enemigos[e].gy - g->gy;
            if (abs(edx) <= 2 && abs(edy) <= 2) {
                lista_enemigos[e].hp -= 15;
                if (lista_enemigos[e].hp <= 0) {
                    lista_enemigos[e].activo = false;
                    inventario[19]++;
                    enemigos_derrotados++;
                }
                ataco_enemigo = true;
                break;
            }
        }
        if (ataco_enemigo) continue;

        if (g->tarea == 1) {
            // TAREA 1: ESCOLTA Y CAZADOR DE ENEMIGOS
            int obj_x = jugador_x;
            int obj_y = jugador_y;
            int dist_min = 999;

            // Buscar si hay algún enemigo en un radio de 8 casillas para interceptarlo
            for (int e = 0; e < MAX_ENEMIGOS; e++) {
                if (!lista_enemigos[e].activo) continue;
                int d = abs(lista_enemigos[e].gx - g->gx) + abs(lista_enemigos[e].gy - g->gy);
                if (d < 14 && d < dist_min) {
                    dist_min = d;
                    obj_x = lista_enemigos[e].gx;
                    obj_y = lista_enemigos[e].gy;
                }
            }

            int nx = g->gx;
            int ny = g->gy;
            int margen = (dist_min < 999) ? 0 : (1 + (i % 3));
            if (obj_x > g->gx + margen)      nx++;
            else if (obj_x < g->gx - margen) nx--;
            if (obj_y > g->gy + margen)      ny++;
            else if (obj_y < g->gy - margen) ny--;

            if (casilla_es_transitable(nx, ny)) {
                g->gx = nx;
                g->gy = ny;
            }
        } else if (g->tarea == 2) {
            // TAREA 2: MINERO Y LEÑADOR AUTÓNOMO
            bool recolecto = false;
            for (int dy = -2; dy <= 2 && !recolecto; dy++) {
                for (int dx = -2; dx <= 2 && !recolecto; dx++) {
                    int tx = g->gx + dx;
                    int ty = g->gy + dy;
                    if (tx <= 1 || tx >= MAP_WIDTH - 2 || ty <= 1 || ty >= MAP_HEIGHT - 2) continue;
                    int t = obtener_casilla_mundo(tx, ty);
                    if (t == 4 || t == 1 || t == 16 || t == 19) {
                        inventario[t]++;
                        establecer_casilla_mundo(tx, ty, 0);
                        g->gx = tx;
                        g->gy = ty;
                        recolecto = true;
                    }
                }
            }
            if (!recolecto) {
                int nx = g->gx + g->dir_x;
                int ny = g->gy + g->dir_y;
                if (nx <= 2 || nx >= MAP_WIDTH - 3 || ny <= 2 || ny >= MAP_HEIGHT - 3 ||
                    obtener_casilla_mundo(nx, ny) == 2 || obtener_casilla_mundo(nx, ny) == 7) {
                    // Girar 90 grados si topa con agua o borde
                    int tmp = g->dir_x;
                    g->dir_x = -g->dir_y;
                    g->dir_y = tmp;
                } else {
                    g->gx = nx;
                    g->gy = ny;
                }
            }
        } else if (g->tarea == 3) {
            // TAREA 3: INGENIERO DE PUENTES DE MADERA Y CAMINOS
            int nx = g->gx + g->dir_x;
            int ny = g->gy + g->dir_y;
            if (nx > 2 && nx < MAP_WIDTH - 3 && ny > 2 && ny < MAP_HEIGHT - 3) {
                // Construye una calzada/puente de 3 casillas de ancho al avanzar
                for (int lat = -1; lat <= 1; lat++) {
                    int bx = nx + (g->dir_y != 0 ? lat : 0);
                    int by = ny + (g->dir_x != 0 ? lat : 0);
                    int dest = obtener_casilla_mundo(bx, by);
                    if (dest == 2 || dest == 7) {
                        establecer_casilla_mundo(bx, by, 8); // Agua o Lava -> Puente de Madera (8)
                    } else if (lat == 0 && dest != 8 && dest != 5 && dest != 3) {
                        if (dest == 4 || dest == 1 || dest == 16) inventario[dest]++;
                        establecer_casilla_mundo(bx, by, 5); // Tierra -> Camino (5)
                    }
                }
                g->gx = nx;
                g->gy = ny;
            } else {
                g->dir_x = -g->dir_x;
                g->dir_y = -g->dir_y;
            }
        }
    }

    // 2. Actualizar IA de Enemigos Hostiles cercanos al jugador
    for (int e = 0; e < MAX_ENEMIGOS; e++) {
        if (!lista_enemigos[e].activo) continue;
        EntidadEnemigo *en = &lista_enemigos[e];

        int dx = jugador_x - en->gx;
        int dy = jugador_y - en->gy;
        int dist = abs(dx) + abs(dy);

        if (dist > 48) continue; // Enemigos muy lejanos hibernan para ahorrar CPU

        if (dist <= 1) {
            // El enemigo ataca al jugador
            if (!modo_creativo) {
                int dano = (en->tipo == 2) ? 8 : ((en->tipo == 1) ? 5 : 3);
                jugador_hp -= dano;
                reproducir_sonido_motor(SONIDO_ALERTA);
                if (jugador_hp <= 0) {
                    jugador_hp = jugador_max_hp;
                    jugador_x = 512;
                    jugador_y = 512;
                    actualizar_chunks_cercanos();
                    mostrar_notificacion_toast("💀 ¡Fuiste abatido! Has reaparecido en el Crucero Central (512,512).");
                }
            }
        } else if (dist <= 18) {
            // Perseguir al jugador
            int nx = en->gx + (dx > 0 ? 1 : (dx < 0 ? -1 : 0));
            int ny = en->gy + (dy > 0 ? 1 : (dy < 0 ? -1 : 0));
            if (casilla_es_transitable(nx, ny)) {
                en->gx = nx;
                en->gy = ny;
            } else if (casilla_es_transitable(nx, en->gy)) {
                en->gx = nx;
            } else if (casilla_es_transitable(en->gx, ny)) {
                en->gy = ny;
            }
        }
    }
}

static void inicializar_mundo_con_latino(bool conservar_estado)
{
    int prev_x = jugador_x;
    int prev_y = jugador_y;
    int inv_copia[MAX_NODOS];
    memcpy(inv_copia, inventario, sizeof(inventario));

    memset(mundo, 0, sizeof(mundo));
    memset(lista_carteles, 0, sizeof(lista_carteles));
    memset(lista_semillas, 0, sizeof(lista_semillas));
    memset(lista_tnt, 0, sizeof(lista_tnt));
    if (!conservar_estado) {
        memset(lista_golems, 0, sizeof(lista_golems));
        memset(lista_enemigos, 0, sizeof(lista_enemigos));
        jugador_hp = 100;
    }
    memset(tabla_nodos, 0, sizeof(tabla_nodos));
    memset(inventario, 0, sizeof(inventario));
    memset(widgets_ui, 0, sizeof(widgets_ui));
    memset(pestanas_ui, 0, sizeof(pestanas_ui));
    total_widgets_ui = 0;
    memset(separacion_fila_ui, 0, sizeof(separacion_fila_ui));
    UIContResultado inicializado = ui_cont_inicializar(
        &contenedores_ui, miembros_contenedor_ui, MAX_WIDGETS_UI,
        separacion_fila_ui);
    if (inicializado != UI_CONT_OK) {
        fprintf(stderr, "LatinoUI: no se pudo inicializar contenedores\n");
        return;
    }
    total_pestanas_ui = 0;
    pestana_en_construccion = 0;
    total_chunks_generados = 0;
    id_caja_activa = -1;
    id_slider_arrastrando = -1;
    dir_mov_x = 0;
    dir_mov_y = 0;

    tabla_nodos[0] = (DefNodo){ true, "Pasto",  (Color){175, 215, 155, 255}, false, 0 };
    tabla_nodos[1] = (DefNodo){ true, "Piedra", DARKGRAY, true, 0 };

    const char *ruta = obtener_ruta_script();
    if (ruta != NULL) {
        ultima_modificacion_script = GetFileModTime(ruta);
    }

    ejecutar_script_latino(0, 0, 0);
    if (total_widgets_ui >= MAX_WIDGETS_UI) {
        fprintf(stderr, "LatinoUI: limite de %d widgets alcanzado; revise los controles\n", MAX_WIDGETS_UI);
    }

    if (conservar_estado) {
        jugador_x = prev_x;
        jugador_y = prev_y;
        memcpy(inventario, inv_copia, sizeof(inventario));
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
        fwrite(&jugador_hp, sizeof(int), 1, f);
        fwrite(&total_chunks_generados, sizeof(int), 1, f);
        fwrite(inventario, sizeof(inventario), 1, f);
        fwrite(lista_golems, sizeof(lista_golems), 1, f);
        fwrite(mundo, sizeof(mundo), 1, f);
        fwrite(lista_carteles, sizeof(lista_carteles), 1, f);
        fclose(f);
        reproducir_sonido_motor(SONIDO_CRAFT);
        mostrar_notificacion_toast("¡Mundo 1024x1024, Gólems e Inventario guardados!");
    }
}

static void cargar_mundo_disco(void)
{
    FILE *f = fopen(ARCHIVO_GUARDADO, "rb");
    if (f != NULL) {
        if (fread(&jugador_x, sizeof(int), 1, f) == 1 &&
            fread(&jugador_y, sizeof(int), 1, f) == 1 &&
            fread(&jugador_hp, sizeof(int), 1, f) == 1 &&
            fread(&total_chunks_generados, sizeof(int), 1, f) == 1 &&
            fread(inventario, sizeof(inventario), 1, f) == 1 &&
            fread(lista_golems, sizeof(lista_golems), 1, f) == 1 &&
            fread(mundo, sizeof(mundo), 1, f) == 1 &&
            fread(lista_carteles, sizeof(lista_carteles), 1, f) == 1) {
            dir_mov_x = 0;
            dir_mov_y = 0;
            WidgetUI *w_prog = buscar_widget_por_id(400);
            if (w_prog != NULL) w_prog->valor = total_chunks_generados;
            reproducir_sonido_motor(SONIDO_CRAFT);
            mostrar_notificacion_toast("¡Mundo 1024x1024, Gólems e Inventario cargados!");
        }
        fclose(f);
    } else {
        reproducir_sonido_motor(SONIDO_ALERTA);
        mostrar_notificacion_toast("Aún no existe un archivo guardado de v0.8.");
    }
}

static const char *nombre_bloque(int tipo)
{
    if (tipo >= 0 && tipo < MAX_NODOS && tabla_nodos[tipo].registrado) {
        return tabla_nodos[tipo].nombre;
    }
    return "Desconocido";
}

static void aplicar_herramienta_mundo(int centro_x, int centro_y, int modo_accion)
{
    // Si hace Clic Izquierdo sobre un enemigo, lo ataca con la espada
    if (modo_accion == 0) {
        for (int e = 0; e < MAX_ENEMIGOS; e++) {
            if (lista_enemigos[e].activo &&
                abs(lista_enemigos[e].gx - centro_x) <= 1 &&
                abs(lista_enemigos[e].gy - centro_y) <= 1) {
                ejecutar_ataque_espada_jugador();
                return;
            }
        }
    }

    int r_ini = -(tamano_pincel - 1) / 2;
    int r_fin = tamano_pincel / 2;
    static double t_aviso_sin_stock = 0.0;
    static double t_ultimo_sonido = 0.0;
    bool hubo_cambio = false;

    for (int dy = r_ini; dy <= r_fin; dy++) {
        for (int dx = r_ini; dx <= r_fin; dx++) {
            int tx = centro_x + dx;
            int ty = centro_y + dy;
            if (tx <= 0 || tx >= MAP_WIDTH - 1 || ty <= 0 || ty >= MAP_HEIGHT - 1) continue;

            int actual = obtener_casilla_mundo(tx, ty);

            if (modo_accion == 0) {
                if (actual != 0 && actual != 2 && actual != 15 && actual != 18) {
                    int id_recurso = (actual == 11) ? 10 : actual;
                    if (id_recurso > 0 && id_recurso < MAX_NODOS) {
                        inventario[id_recurso]++;
                    }
                    TipoBioma b = obtener_bioma_en(tx, ty);
                    int suelo_base = (b == BIOMA_DESIERTO) ? 6 : ((b == BIOMA_NIEVE) ? 15 : ((b == BIOMA_VOLCAN) ? 18 : 0));
                    establecer_casilla_mundo(tx, ty, suelo_base);
                    hubo_cambio = true;
                } else if ((actual == 2 || actual == 7) && modo_creativo) {
                    establecer_casilla_mundo(tx, ty, 0);
                    hubo_cambio = true;
                }
            } else {
                // Si hace clic derecho sobre Agua (2) o Lava (7) con un bloque sólido, tiende Puente de Madera (8) automáticamente
                int bloque_a_poner = bloque_seleccionado;
                if ((actual == 2 || actual == 7) && tabla_nodos[bloque_seleccionado].es_solido && inventario[8] > 0) {
                    bloque_a_poner = 8;
                }

                if (actual == bloque_a_poner) continue;
                if (tx == jugador_x && ty == jugador_y && tabla_nodos[bloque_a_poner].es_solido) {
                    continue;
                }

                if (!modo_creativo) {
                    if (inventario[bloque_a_poner] <= 0) {
                        if (GetTime() - t_aviso_sin_stock > 1.2) {
                            reproducir_sonido_motor(SONIDO_ALERTA);
                            mostrar_notificacion_toast("¡Sin stock! Recolecta con Clic Izq o ve a 'Artesanía'.");
                            t_aviso_sin_stock = GetTime();
                        }
                        return;
                    }
                    inventario[bloque_a_poner]--;
                }
                establecer_casilla_mundo(tx, ty, bloque_a_poner);
                hubo_cambio = true;

                if (bloque_a_poner == 14) {
                    mostrar_notificacion_toast("💣 ¡TNT activada! Explotará en 3 segundos (o pulsa [E] ya).");
                } else if (bloque_a_poner == 13) {
                    mostrar_notificacion_toast("🌱 Semilla plantada: crecerá en 5 segundos (o pulsa [E]).");
                }
            }
        }
    }

    if (hubo_cambio && (GetTime() - t_ultimo_sonido > 0.08)) {
        reproducir_sonido_motor(modo_accion == 0 ? SONIDO_MINAR : SONIDO_CONSTRUIR);
        t_ultimo_sonido = GetTime();
    }
}

// ============================================================================
// INTERACCIÓN INSTANTÁNEA CON [E] (PUERTAS, TNT, SEMILLAS Y PUENTES EN C)
// ============================================================================

static void interactuar_con_entorno(int raton_tx, int raton_ty)
{
    // 1. Prioridad: Revisar lo que el usuario está apuntando con el ratón
    if (raton_tx > 0 && raton_tx < MAP_WIDTH - 1 &&
        raton_ty > 0 && raton_ty < MAP_HEIGHT - 1) {
        int tr = obtener_casilla_mundo(raton_tx, raton_ty);
        if (tr == 10) {
            establecer_casilla_mundo(raton_tx, raton_ty, 11);
            reproducir_sonido_motor(SONIDO_PUERTA);
            mostrar_notificacion_toast("🚪 ¡Puerta abierta [E]!");
            return;
        } else if (tr == 11 && (raton_tx != jugador_x || raton_ty != jugador_y)) {
            establecer_casilla_mundo(raton_tx, raton_ty, 10);
            reproducir_sonido_motor(SONIDO_PUERTA);
            mostrar_notificacion_toast("🚪 ¡Puerta cerrada [E]!");
            return;
        } else if (tr == 14) {
            detonar_tnt_en(raton_tx, raton_ty);
            return;
        } else if (tr == 13) {
            establecer_casilla_mundo(raton_tx, raton_ty, 4);
            reproducir_sonido_motor(SONIDO_CRAFT);
            mostrar_notificacion_toast("🌳 ¡Semilla fertilizada con [E]! Ha crecido un Árbol.");
            return;
        }
    }

    // 2. Revisar un radio de 2 casillas alrededor del jugador (5x5)
    for (int dy = -2; dy <= 2; dy++) {
        for (int dx = -2; dx <= 2; dx++) {
            int gx = jugador_x + dx;
            int gy = jugador_y + dy;
            int t = obtener_casilla_mundo(gx, gy);

            if (t == 10) {
                establecer_casilla_mundo(gx, gy, 11);
                reproducir_sonido_motor(SONIDO_PUERTA);
                mostrar_notificacion_toast("🚪 ¡Puerta abierta [E]!");
                return;
            } else if (t == 11 && (dx != 0 || dy != 0)) {
                establecer_casilla_mundo(gx, gy, 10);
                reproducir_sonido_motor(SONIDO_PUERTA);
                mostrar_notificacion_toast("🚪 ¡Puerta cerrada [E]!");
                return;
            } else if (t == 14) {
                detonar_tnt_en(gx, gy);
                return;
            } else if (t == 13) {
                establecer_casilla_mundo(gx, gy, 4);
                reproducir_sonido_motor(SONIDO_CRAFT);
                mostrar_notificacion_toast("🌳 ¡Semilla fertilizada con [E]! Ha crecido un Árbol.");
                return;
            }
        }
    }

    // 3. Si está cerca de Agua (2) o Lava (7), tiende un Puente de Madera automáticamente
    if (tender_puente_cercano()) {
        return;
    }

    reproducir_sonido_motor(SONIDO_ALERTA);
    mostrar_notificacion_toast("Pulsa [E] frente a Puertas, TNT, Semillas o Agua/Lava (para tender Puente).");
}

static float calcular_oscuridad_ambiental(float h)
{
    if (h >= 7.0f && h <= 17.5f) return 0.0f;
    if (h > 17.5f && h < 20.5f)  return ((h - 17.5f) / 3.0f) * 0.84f;
    if (h >= 20.5f || h <= 4.0f) return 0.84f;
    return ((7.0f - h) / 3.0f) * 0.84f;
}

static Color aplicar_iluminacion_casilla(Color base, float oscuridad, float luz_local)
{
    if (oscuridad <= 0.01f) return base;

    float sombra = oscuridad * (1.0f - luz_local);
    if (sombra < 0.0f) sombra = 0.0f;
    if (sombra > 0.88f) sombra = 0.88f;

    float factor = 1.0f - sombra;
    int r = (int)((float)base.r * factor + 28.0f * luz_local * oscuridad);
    int g = (int)((float)base.g * factor + 16.0f * luz_local * oscuridad);
    int b = (int)((float)base.b * (factor + 0.06f * oscuridad));

    if (r > 255) r = 255;
    if (g > 255) g = 255;
    if (b > 255) b = 255;

    return (Color){ (unsigned char)r, (unsigned char)g, (unsigned char)b, 255 };
}


/*
 * Calcula la geometría de los widgets de la pestaña activa.
 *
 * Mantiene las alturas del diseño original, el espacio
 * de 10 píxeles y el desplazamiento vertical existente.
 *
 * Las coordenadas resultantes son coordenadas de pantalla.
 */
static void actualizar_geometria_widgets(
    Rectangle panel, Rectangle area_scroll, int font_w
) {
    UIElemento elementos[MAX_WIDGETS_UI];
    UIRect rectangulos[MAX_WIDGETS_UI];
    int indices[MAX_WIDGETS_UI];
    int longitudes[MAX_WIDGETS_UI];
    size_t grupos = 0;
    float altura_total = 0.0f;
    const float separacion_vertical = 10.0f;

    for (int i = 0; i < total_widgets_ui;) {
        WidgetUI *w = &widgets_ui[i];
        if (!w->activo || w->pestana != pestana_actual) { i++; continue; }

        int largo = 1;
        int fila = w->fila_id;
        if (fila > 0) {
            while (i + largo < total_widgets_ui &&
                   widgets_ui[i + largo].activo &&
                   widgets_ui[i + largo].pestana == pestana_actual &&
                   widgets_ui[i + largo].fila_id == fila) largo++;
        }
        float alto_grupo = 0.0f;
        for (int j = 0; j < largo; j++) {
            WidgetUI *h = &widgets_ui[i + j];
            bool foco = h->tipo == WIDGET_CAJA_TEXTO && id_caja_activa == h->id;
            float alto = (float)(font_w + 22);
            if (h->tipo == WIDGET_ETIQUETA) alto = (float)(font_w + 10);
            if (h->tipo == WIDGET_CAJA_TEXTO) alto = (float)(font_w * 2 + 26 + (foco ? 30 : 0));
            if (h->tipo == WIDGET_DESLIZADOR) alto = (float)(font_w * 2 + 24);
            if (h->tipo == WIDGET_PROGRESO) alto = (float)(font_w * 2 + 22);
            if (h->tipo == WIDGET_INVENTARIO) alto = (float)((font_w + 16) * 3 + 12);
            if (alto > alto_grupo) alto_grupo = alto;
        }
        indices[grupos] = i;
        longitudes[grupos] = largo;
        elementos[grupos] = (UIElemento){alto_grupo, 0.0f};
        altura_total += alto_grupo;
        grupos++;
        i += largo;
    }
    if (grupos == 0) { scroll_panel_y = 0.0f; return; }
    altura_total += (float)(grupos - 1) * separacion_vertical;
    float visible = area_scroll.height - 16.0f;
    if (visible < 0) visible = 0;
    float minimo_scroll = visible - altura_total;
    if (minimo_scroll > 0) minimo_scroll = 0;
    if (scroll_panel_y < minimo_scroll) scroll_panel_y = minimo_scroll;
    if (scroll_panel_y > 0) scroll_panel_y = 0;

    UIRect contenedor = {panel.x + 14.0f,
                        area_scroll.y + scroll_panel_y + 8.0f,
                        fmaxf(0.0f, panel.width - 28.0f), altura_total};
    UIResultado estado = ui_distribuir(contenedor, UI_COLUMNA,
                         separacion_vertical, 0, elementos, grupos, rectangulos);
    if (estado != UI_LAYOUT_OK) {
        for (int i = 0; i < total_widgets_ui; ++i)
            widgets_ui[i].rect_calculado = (Rectangle){0};
        return;
    }

    for (size_t g = 0; g < grupos; ++g) {
        int inicio = indices[g];
        int largo = longitudes[g];
        UIRect r = rectangulos[g];
        if (largo == 1) {
            widgets_ui[inicio].rect_calculado =
                (Rectangle){r.x, r.y, r.ancho, r.alto};
            continue;
        }
        UIElemento hijos[MAX_WIDGETS_UI];
        UIRect salida[MAX_WIDGETS_UI];
        int fila = widgets_ui[inicio].fila_id;
        float espacio = 0.0f;

        UIContResultado consulta = ui_cont_obtener_espaciado(
            &contenedores_ui,
            (size_t)fila,
            &espacio
        );

        if (consulta != UI_CONT_OK) {
            for (int j = 0; j < largo; j++) {
                widgets_ui[inicio + j].rect_calculado =
                    (Rectangle){0};
            }
            continue;
        }
        float espacio_util = r.ancho - (float)(largo - 1) * espacio;
        // Reducir el tamaño base si la ventana no admite 80 px por botón.
        // Si no existe espacio positivo, el layout devuelve error y no
        // presentamos rectángulos con dimensiones negativas.
        float base_boton = fminf(80.0f, espacio_util / (float)largo);
        for (int j = 0; j < largo; j++)
            hijos[j] = (UIElemento){fmaxf(0.0f, base_boton), 1.0f};
        estado = ui_distribuir(r, UI_FILA, espacio, 0, hijos,
                               (size_t)largo, salida);
        for (int j = 0; j < largo; j++) {
            widgets_ui[inicio + j].rect_calculado =
                estado == UI_LAYOUT_OK
                ? (Rectangle){salida[j].x, salida[j].y,
                              salida[j].ancho, salida[j].alto}
                : (Rectangle){0};
        }
    }
}


int main(void)
{
    setlocale(LC_ALL, "C.UTF-8");
    setlocale(LC_NUMERIC, "C");

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);

    InitWindow(1560, 960, "LatinoVox 2D & LatinoUI v0.8 - 5 Biomas (1024x1024), Combate y Gólems Ilimitados");
    if (!IsWindowReady()) {
        return 1;
    }

    SetWindowMinSize(1024, 700);
    SetTargetFPS(60);

    inicializar_fuente_utf8();
    inicializar_audio_procedural();

    agregar_log_consola("=== Consola Interactiva Latino (.lat) en Vivo ===");
    agregar_log_consola("Escribe comandos en español y pulsa [ENTER], o usa los botones rápidos.");
    cursor_consola_bytes = (int)strlen(entrada_consola);

    inicializar_mundo_con_latino(false);

    Camera2D camara = { 0 };
    camara.rotation = 0.0f;
    camara.zoom = 1.35f;

    const int orden_hotbar[12] = { 8, 9, 10, 12, 13, 14, 1, 4, 5, 6, 19, 7 };
    const char *etiqueta_tecla_hb[12] = { "8", "9", "0", "T", "Y", "U", "1", "4", "5", "6", "K", "7" };

    while (!WindowShouldClose()) {
        int screenWidth = GetScreenWidth();
        int screenHeight = GetScreenHeight();
        float dt = GetFrameTime();
        TemaUI tema = paleta_temas[tema_actual];

        if (tiempo_toast > 0.0f) {
            tiempo_toast -= dt;
            if (tiempo_toast < 0.0f) tiempo_toast = 0.0f;
        }
        if (tiempo_onda_explosion > 0.0f) {
            tiempo_onda_explosion -= dt;
        }
        if (tiempo_ataque_espada > 0.0f) {
            tiempo_ataque_espada -= dt;
        }

        if (ciclo_luz_auto && !modal_activo && !consola_activa) {
            hora_dia += dt * 0.22f;
            if (hora_dia >= 24.0f) hora_dia -= 24.0f;
            WidgetUI *w_hora = buscar_widget_por_id(306);
            if (w_hora != NULL && id_slider_arrastrando != 306) {
                w_hora->valor = (int)hora_dia;
            }
        }

        // --- TEMPORIZADOR DE SEMILLAS Y MECHA AUTOMÁTICA DE DINAMITA TNT ---
        for (int s = 0; s < MAX_SEMILLAS_ACTIVAS; s++) {
            if (lista_semillas[s].activa) {
                if (obtener_casilla_mundo(lista_semillas[s].gx, lista_semillas[s].gy) != 13) {
                    lista_semillas[s].activa = false;
                    continue;
                }
                lista_semillas[s].tiempo_restante -= dt;
                if (lista_semillas[s].tiempo_restante <= 0.0f) {
                    establecer_casilla_mundo(lista_semillas[s].gx, lista_semillas[s].gy, 4);
                    lista_semillas[s].activa = false;
                    reproducir_sonido_motor(SONIDO_CRAFT);
                }
            }
        }

        for (int t = 0; t < MAX_TNT_ACTIVAS; t++) {
            if (lista_tnt[t].activa) {
                if (obtener_casilla_mundo(lista_tnt[t].gx, lista_tnt[t].gy) != 14) {
                    lista_tnt[t].activa = false;
                    continue;
                }
                lista_tnt[t].tiempo_restante -= dt;
                if (lista_tnt[t].tiempo_restante <= 0.0f) {
                    int bx = lista_tnt[t].gx;
                    int by = lista_tnt[t].gy;
                    lista_tnt[t].activa = false;
                    detonar_tnt_en(bx, by);
                }
            }
        }

        // --- TICK DE IA DE GÓLEMS Y ENEMIGOS (CADA 0.35s EN C) ---
        temporizador_ia_tick += dt;
        if (temporizador_ia_tick >= 0.35f) {
            temporizador_ia_tick = 0.0f;
            if (!modal_activo) {
                actualizar_ia_golems_y_enemigos();
            }
        }

        // Generación periódica de enemigos si está activo
        temporizador_spawn_enemigo += dt;
        if (spawner_enemigos_activo && temporizador_spawn_enemigo >= 9.0f) {
            temporizador_spawn_enemigo = 0.0f;
            if (contar_enemigos_activos() < 24) {
                int ox = ((rand() % 2 == 0) ? 1 : -1) * (10 + (rand() % 6));
                int oy = ((rand() % 2 == 0) ? 1 : -1) * (10 + (rand() % 6));
                int ex = jugador_x + ox;
                int ey = jugador_y + oy;
                if (casilla_es_transitable(ex, ey)) {
                    TipoBioma b = obtener_bioma_en(ex, ey);
                    int t_en = (b == BIOMA_VOLCAN) ? 2 : ((b == BIOMA_DESIERTO || b == BIOMA_NIEVE) ? 1 : 0);
                    crear_enemigo_en(ex, ey, t_en);
                }
            }
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
                    reproducir_sonido_motor(SONIDO_CRAFT);
                    mostrar_notificacion_toast("¡Auto Hot-Reload: arranque.lat actualizado en vivo!");
                }
            }
        }

        float escala_ui = (float)screenWidth / 1440.0f;
        if (escala_ui < 1.0f) escala_ui = 1.0f;
        if (escala_ui > 2.0f) escala_ui = 2.0f;

        int font_hud = (int)(20.0f * escala_ui) + ajuste_fuente;
        if (font_hud < 15) font_hud = 15;

        int sep_linea = font_hud + 8;
        int alto_hud = sep_linea * 3 + 16;

        int ancho_panel_ui = (int)(440.0f * escala_ui);
        if (ancho_panel_ui > screenWidth / 2) ancho_panel_ui = screenWidth / 2;

        Rectangle rect_panel_ui = {
            (float)(screenWidth - ancho_panel_ui - 12),
            (float)(alto_hud + 12),
            (float)ancho_panel_ui,
            (float)(screenHeight - alto_hud - 24)
        };

        int slots_hotbar = 12;
        int lado_slot = (int)(50.0f * escala_ui);
        int ancho_hotbar = slots_hotbar * (lado_slot + 5) + 14;
        int alto_hotbar = lado_slot + 16;
        int area_libre_w = mostrar_panel_ui ? (int)rect_panel_ui.x : screenWidth;
        Rectangle rect_hotbar = {
            (float)((area_libre_w - ancho_hotbar) / 2),
            (float)(screenHeight - alto_hotbar - 12),
            (float)ancho_hotbar,
            (float)alto_hotbar
        };

        int w_con = area_libre_w - 60;
        if (w_con < 480) w_con = 480;
        int h_con = (int)(350.0f * escala_ui);
        Rectangle rect_consola = {
            (float)((area_libre_w - w_con) / 2),
            (float)(screenHeight - alto_hotbar - h_con - 24),
            (float)w_con,
            (float)h_con
        };

        Vector2 pos_raton = GetMousePosition();
        bool raton_en_panel = mostrar_panel_ui && CheckCollisionPointRec(pos_raton, rect_panel_ui);
        bool raton_en_hotbar = CheckCollisionPointRec(pos_raton, rect_hotbar);
        bool raton_en_consola = consola_activa && CheckCollisionPointRec(pos_raton, rect_consola);
        bool raton_sobre_gui = modal_activo || raton_en_panel || raton_en_hotbar ||
                               raton_en_consola || (pos_raton.y <= (float)alto_hud);

        Vector2 pos_mundo_raton = GetScreenToWorld2D(pos_raton, camara);
        int raton_tx = (int)(pos_mundo_raton.x / TILE_SIZE);
        int raton_ty = (int)(pos_mundo_raton.y / TILE_SIZE);

        if (IsKeyPressed(KEY_F3)) {
            consola_activa = !consola_activa;
            cursor_consola_bytes = (int)strlen(entrada_consola);
            id_caja_activa = -1;
            dir_mov_x = 0;
            dir_mov_y = 0;
            reproducir_sonido_motor(SONIDO_CLIC);
        }

        // --- A. GESTIÓN DE VENTANA MODAL EMERGENTE ---
        if (modal_activo) {
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE) ||
                IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                modal_activo = false;
                reproducir_sonido_motor(SONIDO_CLIC);
            }
        }

        // --- B. EDICIÓN UTF-8 CON CURSOR EN CONSOLA [F3] O EN CAJA DE TEXTO ---
        if (!modal_activo && consola_activa) {
            procesar_edicion_utf8(entrada_consola, sizeof(entrada_consola), &cursor_consola_bytes);
            if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
                evaluar_comando_consola_latino(entrada_consola);
            }
        } else if (!modal_activo && id_caja_activa != -1) {
            WidgetUI *w_caja = buscar_widget_por_id(id_caja_activa);
            if (w_caja != NULL && w_caja->tipo == WIDGET_CAJA_TEXTO) {
                procesar_edicion_utf8(w_caja->texto, sizeof(w_caja->texto), &cursor_caja_bytes);
                if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
                    id_caja_activa = -1;
                }
            } else {
                id_caja_activa = -1;
            }
        }

        // --- C. ATAJOS DE SISTEMA, COMBATE [F], GÓLEMS ILIMITADOS [V] Y ACCIÓN [E] ---
        if (!modal_activo && !consola_activa && id_caja_activa == -1) {
            if (IsKeyPressed(KEY_R)) {
                inicializar_mundo_con_latino(false);
                reproducir_sonido_motor(SONIDO_CRAFT);
                mostrar_notificacion_toast("Script recargado manualmente [R]");
            }
            if (IsKeyPressed(KEY_E)) interactuar_con_entorno(raton_tx, raton_ty);
            if (IsKeyPressed(KEY_F)) ejecutar_ataque_espada_jugador();
            if (IsKeyPressed(KEY_V)) {
                crear_nuevo_golem(jugador_x + ultima_dir_x, jugador_y + ultima_dir_y, golem_tarea_global);
                reproducir_sonido_motor(SONIDO_CRAFT);
                mostrar_notificacion_toast(TextFormat("🤖 ¡Nuevo Gólem creado [V]! Total activos: %d", contar_golems_activos()));
            }
            if (IsKeyPressed(KEY_G)) guardar_mundo_disco();
            if (IsKeyPressed(KEY_C)) cargar_mundo_disco();
            if (IsKeyPressed(KEY_B)) mostrar_bordes_chunk = !mostrar_bordes_chunk;
            if (IsKeyPressed(KEY_TAB)) {
                mostrar_panel_ui = !mostrar_panel_ui;
                reproducir_sonido_motor(SONIDO_CLIC);
            }
            if (IsKeyPressed(KEY_F11)) ToggleFullscreen();

            if (IsKeyPressed(KEY_M)) {
                modo_un_toque = !modo_un_toque;
                dir_mov_x = 0;
                dir_mov_y = 0;
            }

            if (IsKeyPressed(KEY_F1) && ajuste_fuente > -5) ajuste_fuente -= 1;
            if (IsKeyPressed(KEY_F2) && ajuste_fuente < 10) ajuste_fuente += 1;

            for (int k = 1; k <= 9; k++) {
                if (IsKeyPressed(KEY_ZERO + k) && tabla_nodos[k].registrado) {
                    bloque_seleccionado = k;
                    reproducir_sonido_motor(SONIDO_CLIC);
                }
            }
            if (IsKeyPressed(KEY_ZERO) && tabla_nodos[10].registrado) {
                bloque_seleccionado = 10;
                reproducir_sonido_motor(SONIDO_CLIC);
            }
            if (IsKeyPressed(KEY_T) && tabla_nodos[12].registrado) {
                bloque_seleccionado = 12;
                reproducir_sonido_motor(SONIDO_CLIC);
            }
            if (IsKeyPressed(KEY_Y) && tabla_nodos[13].registrado) {
                bloque_seleccionado = 13;
                reproducir_sonido_motor(SONIDO_CLIC);
            }
            if (IsKeyPressed(KEY_U) && tabla_nodos[14].registrado) {
                bloque_seleccionado = 14;
                reproducir_sonido_motor(SONIDO_CLIC);
            }
            if (IsKeyPressed(KEY_K) && tabla_nodos[19].registrado) {
                bloque_seleccionado = 19;
                reproducir_sonido_motor(SONIDO_CLIC);
            }
        } else if (IsKeyPressed(KEY_TAB) && !consola_activa) {
            id_caja_activa = -1;
        }

        // --- D. CLIC EN LA HOTBAR INFERIOR ---
        if (!modal_activo && raton_en_hotbar && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            for (int s = 0; s < slots_hotbar; s++) {
                Rectangle r_slot = {
                    rect_hotbar.x + 10.0f + (float)(s * (lado_slot + 5)),
                    rect_hotbar.y + 8.0f,
                    (float)lado_slot,
                    (float)lado_slot
                };
                if (CheckCollisionPointRec(pos_raton, r_slot)) {
                    bloque_seleccionado = orden_hotbar[s];
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
            } else if (!raton_en_consola) {
                camara.zoom += rueda * 0.15f;
            }
        }

        if (!modal_activo && !consola_activa && id_caja_activa == -1) {
            if (IsKeyPressed(KEY_EQUAL) || IsKeyPressed(KEY_KP_ADD)) camara.zoom += 0.2f;
            if (IsKeyPressed(KEY_MINUS) || IsKeyPressed(KEY_KP_SUBTRACT)) camara.zoom -= 0.2f;
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

        const char *acentos_es[10] = { "á", "é", "í", "ó", "ú", "ñ", "Ñ", "¡", "¿", "ü" };


        if (!modal_activo && mostrar_panel_ui) {
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && !raton_en_panel) {
                id_caja_activa = -1;
            }

            if (IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
                id_slider_arrastrando = -1;
            }

            if (total_pestanas_ui > 0 && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                float w_tab = (rect_panel_ui.width - 20.0f) / (float)total_pestanas_ui;
                for (int t = 0; t < total_pestanas_ui; t++) {
                    if (!pestanas_ui[t].activa) continue;
                    Rectangle r_tab = {
                        rect_panel_ui.x + 10.0f + (float)t * w_tab,
                        rect_panel_ui.y + (float)alto_titulo_panel,
                        w_tab - 3.0f,
                        (float)alto_barra_tabs
                    };
                    if (CheckCollisionPointRec(pos_raton, r_tab)) {
                        pestana_actual = t;
                        scroll_panel_y = 0.0f;
                        id_caja_activa = -1;
                        reproducir_sonido_motor(SONIDO_CLIC);
                        break;
                    }
                }
            }

            actualizar_geometria_widgets(rect_panel_ui, area_scroll_ui, font_hud - 2);

            int font_w = font_hud - 2;

            for (int i = 0; i < total_widgets_ui; i++) {
                WidgetUI *w = &widgets_ui[i];
                if (!w->activo || w->pestana != pestana_actual) continue;

                bool tiene_foco_caja = (w->tipo == WIDGET_CAJA_TEXTO && id_caja_activa == w->id);
                Rectangle rect_item = w->rect_calculado;
                if (rect_item.width <= 0.0f || rect_item.height <= 0.0f) continue;

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
                    bool clic_en_chip = false;
                    if (tiene_foco_caja && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                        float w_ch = (rect_item.width - 18.0f) / 10.0f;
                        float y_ch = rect_item.y + (float)(font_w * 2) + 26.0f;
                        for (int c = 0; c < 10; c++) {
                            Rectangle r_ch = { rect_item.x + (float)c * (w_ch + 2.0f), y_ch, w_ch, 24.0f };
                            if (CheckCollisionPointRec(pos_raton, r_ch)) {
                                utf8_insertar_en_cursor(w->texto, sizeof(w->texto), &cursor_caja_bytes, acentos_es[c]);
                                reproducir_sonido_motor(SONIDO_CLIC);
                                clic_en_chip = true;
                                break;
                            }
                        }
                    }

                    if (!clic_en_chip && hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                        if (id_caja_activa != w->id) {
                            id_caja_activa = w->id;
                            cursor_caja_bytes = (int)strlen(w->texto);
                        }
                        consola_activa = false;
                        reproducir_sonido_motor(SONIDO_CLIC);
                    }
                } else if (w->tipo == WIDGET_INTERRUPTOR) {
                    if (hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                        id_caja_activa = -1;
                        w->valor = (w->valor == 0) ? 1 : 0;
                        reproducir_sonido_motor(SONIDO_CLIC);
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
                        if (barra_w <= 0.0f) continue;
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

                
            }
        }

        // --- G. MOVIMIENTO DEL JUGADOR ---
        if (!modal_activo && !consola_activa && id_caja_activa == -1) {
            if (modo_un_toque) {
                if (IsKeyPressed(KEY_W) || IsKeyPressed(KEY_UP)) {
                    dir_mov_x = 0; dir_mov_y = -1; ultima_dir_x = 0; ultima_dir_y = -1; temporizador_paso = 1.0f;
                } else if (IsKeyPressed(KEY_S) || IsKeyPressed(KEY_DOWN)) {
                    dir_mov_x = 0; dir_mov_y = 1;  ultima_dir_x = 0; ultima_dir_y = 1;  temporizador_paso = 1.0f;
                } else if (IsKeyPressed(KEY_A) || IsKeyPressed(KEY_LEFT)) {
                    dir_mov_x = -1; dir_mov_y = 0; ultima_dir_x = -1; ultima_dir_y = 0; temporizador_paso = 1.0f;
                } else if (IsKeyPressed(KEY_D) || IsKeyPressed(KEY_RIGHT)) {
                    dir_mov_x = 1; dir_mov_y = 0;  ultima_dir_x = 1; ultima_dir_y = 0;  temporizador_paso = 1.0f;
                } else if (IsKeyPressed(KEY_SPACE)) {
                    dir_mov_x = 0; dir_mov_y = 0;
                }
            } else {
                dir_mov_x = 0; dir_mov_y = 0;
                if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))         { dir_mov_y = -1; ultima_dir_x = 0; ultima_dir_y = -1; }
                else if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN))  { dir_mov_y = 1;  ultima_dir_x = 0; ultima_dir_y = 1; }
                else if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))  { dir_mov_x = -1; ultima_dir_x = -1; ultima_dir_y = 0; }
                else if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) { dir_mov_x = 1;  ultima_dir_x = 1; ultima_dir_y = 0; }
            }
        } else {
            dir_mov_x = 0;
            dir_mov_y = 0;
        }

        float intervalo_paso = (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) ? 0.055f : 0.105f;

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

        // --- H. RECOLECCIÓN Y CONSTRUCCIÓN CON EL RATÓN ---
        if (!raton_sobre_gui && id_slider_arrastrando == -1 &&
            raton_tx > 0 && raton_tx < MAP_WIDTH - 1 &&
            raton_ty > 0 && raton_ty < MAP_HEIGHT - 1) {
            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                id_caja_activa = -1;
                aplicar_herramienta_mundo(raton_tx, raton_ty, 0);
            } else if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
                id_caja_activa = -1;
                aplicar_herramienta_mundo(raton_tx, raton_ty, 1);
            }
        }

        // --- I. VIEW CULLING E ILUMINACIÓN DINÁMICA 2D ---
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

        float oscuridad = calcular_oscuridad_ambiental(hora_dia);

        FuenteLuz luces[MAX_LUCES_VISIBLES];
        int total_luces = 0;

        if (oscuridad > 0.01f) {
            luces[total_luces++] = (FuenteLuz){ jugador_x, jugador_y, 5 };

            for (int g = 0; g < MAX_GOLEMS && total_luces < MAX_LUCES_VISIBLES; g++) {
                if (lista_golems[g].activo &&
                    lista_golems[g].gx >= min_x - 4 && lista_golems[g].gx <= max_x + 4 &&
                    lista_golems[g].gy >= min_y - 4 && lista_golems[g].gy <= max_y + 4) {
                    luces[total_luces++] = (FuenteLuz){ lista_golems[g].gx, lista_golems[g].gy, 3 };
                }
            }

            int l_min_x = (min_x - 7 < 0) ? 0 : min_x - 7;
            int l_min_y = (min_y - 7 < 0) ? 0 : min_y - 7;
            int l_max_x = (max_x + 7 >= MAP_WIDTH)  ? MAP_WIDTH - 1  : max_x + 7;
            int l_max_y = (max_y + 7 >= MAP_HEIGHT) ? MAP_HEIGHT - 1 : max_y + 7;

            for (int ly = l_min_y; ly <= l_max_y && total_luces < MAX_LUCES_VISIBLES; ly++) {
                for (int lx = l_min_x; lx <= l_max_x && total_luces < MAX_LUCES_VISIBLES; lx++) {
                    int t = obtener_casilla_mundo(lx, ly);
                    if (t >= 0 && t < MAX_NODOS && tabla_nodos[t].radio_luz > 0) {
                        luces[total_luces++] = (FuenteLuz){ lx, ly, tabla_nodos[t].radio_luz };
                    }
                }
            }
        }

        BeginDrawing();
        ClearBackground((Color){ 12, 15, 22, 255 });

        BeginMode2D(camara);

        for (int gy = min_y; gy <= max_y; gy++) {
            for (int gx = min_x; gx <= max_x; gx++) {
                int cx = gx / CHUNK_SIZE;
                int cy = gy / CHUNK_SIZE;
                int px = gx * TILE_SIZE;
                int py = gy * TILE_SIZE;

                if (!mundo[cy][cx].generado) {
                    DrawRectangle(px, py, TILE_SIZE, TILE_SIZE, (Color){ 20, 24, 32, 255 });
                    continue;
                }

                float luz_local = 0.0f;
                if (oscuridad > 0.01f) {
                    for (int l = 0; l < total_luces; l++) {
                        int dx = gx - luces[l].gx;
                        int dy = gy - luces[l].gy;
                        int r_max = luces[l].radio;
                        if (abs(dx) <= r_max && abs(dy) <= r_max) {
                            float dist = sqrtf((float)(dx * dx + dy * dy));
                            if (dist < (float)r_max) {
                                float aporte = 1.0f - (dist / (float)r_max);
                                if (aporte > luz_local) luz_local = aporte;
                            }
                        }
                    }
                }

                int tipo = obtener_casilla_mundo(gx, gy);
                TipoBioma b_tile = obtener_bioma_en(gx, gy);
                int id_suelo = (b_tile == BIOMA_DESIERTO) ? 6 : ((b_tile == BIOMA_NIEVE) ? 15 : ((b_tile == BIOMA_VOLCAN) ? 18 : 0));
                Color color_suelo = aplicar_iluminacion_casilla(tabla_nodos[id_suelo].color, oscuridad, luz_local);
                Color color_nodo = color_suelo;
                if (tipo >= 0 && tipo < MAX_NODOS && tabla_nodos[tipo].registrado) {
                    color_nodo = aplicar_iluminacion_casilla(tabla_nodos[tipo].color, oscuridad, luz_local);
                }

                if (tipo == 3 || tipo == 4) {
                    DrawRectangle(px, py, TILE_SIZE, TILE_SIZE, color_suelo);
                    DrawRectangle(px + 10, py + 10, TILE_SIZE - 20, TILE_SIZE - 20, color_nodo);
                } else if (tipo == 8) {
                    // Puente / Tablones de Madera con barandillas laterales
                    DrawRectangle(px, py, TILE_SIZE, TILE_SIZE, color_nodo);
                    Color c_veta = aplicar_iluminacion_casilla((Color){ 120, 75, 35, 255 }, oscuridad, luz_local);
                    DrawLine(px, py + 16, px + TILE_SIZE, py + 16, c_veta);
                    DrawLine(px, py + 32, px + TILE_SIZE, py + 32, c_veta);
                    DrawRectangleLines(px + 2, py + 2, TILE_SIZE - 4, TILE_SIZE - 4, c_veta);
                } else if (tipo == 9) {
                    DrawRectangle(px, py, TILE_SIZE, TILE_SIZE, color_nodo);
                    Color c_junta = aplicar_iluminacion_casilla((Color){ 210, 195, 185, 255 }, oscuridad, luz_local);
                    DrawLine(px, py + 24, px + TILE_SIZE, py + 24, c_junta);
                    DrawLine(px + 24, py, px + 24, py + 24, c_junta);
                    DrawLine(px + 12, py + 24, px + 12, py + TILE_SIZE, c_junta);
                } else if (tipo == 10) {
                    DrawRectangle(px, py, TILE_SIZE, TILE_SIZE, color_suelo);
                    DrawRectangle(px + 4, py + 14, TILE_SIZE - 8, TILE_SIZE - 28, color_nodo);
                    DrawRectangleLines(px + 4, py + 14, TILE_SIZE - 8, TILE_SIZE - 28, BLACK);
                    DrawCircle(px + TILE_SIZE - 12, py + TILE_SIZE / 2, 3.5f, GOLD);
                } else if (tipo == 11) {
                    DrawRectangle(px, py, TILE_SIZE, TILE_SIZE, color_suelo);
                    DrawRectangle(px + 4, py + 4, 12, TILE_SIZE - 8, color_nodo);
                    DrawRectangleLines(px + 4, py + 4, 12, TILE_SIZE - 8, GOLD);
                } else if (tipo == 12) {
                    DrawRectangle(px, py, TILE_SIZE, TILE_SIZE, color_suelo);
                    DrawRectangle(px + 21, py + 22, 6, 16, BROWN);
                    float pulso = 7.0f + sinf((float)GetTime() * 8.0f + (float)(gx + gy)) * 1.5f;
                    DrawCircle(px + 24, py + 18, pulso, (Color){ 255, 140, 20, 220 });
                    DrawCircle(px + 24, py + 18, pulso * 0.55f, YELLOW);
                } else if (tipo == 13) {
                    DrawRectangle(px, py, TILE_SIZE, TILE_SIZE, color_suelo);
                    DrawRectangle(px + 22, py + 24, 4, 14, BROWN);
                    DrawCircle(px + 20, py + 22, 6.0f, color_nodo);
                    DrawCircle(px + 28, py + 22, 6.0f, LIME);
                } else if (tipo == 14) {
                    // Dinamita TNT con parpadeo y cuenta regresiva
                    DrawRectangle(px, py, TILE_SIZE, TILE_SIZE, color_suelo);
                    bool flash = (((int)(GetTime() * 8.0)) % 2 == 0);
                    DrawRectangle(px + 6, py + 6, TILE_SIZE - 12, TILE_SIZE - 12, flash ? RED : WHITE);
                    DrawRectangle(px + 6, py + 19, TILE_SIZE - 12, 10, BLACK);
                    dibujar_texto_ui("TNT", px + 10, py + 16, 14, YELLOW);
                } else if (tipo == 16) {
                    // Cactus del desierto
                    DrawRectangle(px, py, TILE_SIZE, TILE_SIZE, color_suelo);
                    DrawRectangle(px + 18, py + 8, 12, TILE_SIZE - 14, color_nodo);
                    DrawRectangle(px + 10, py + 16, 8, 6, color_nodo);
                    DrawRectangle(px + 30, py + 20, 8, 6, color_nodo);
                } else if (tipo == 19) {
                    // Cristal Rubí Luminoso
                    DrawRectangle(px, py, TILE_SIZE, TILE_SIZE, color_suelo);
                    DrawCircle(px + 24, py + 24, 14.0f, color_nodo);
                    DrawCircle(px + 24, py + 24, 7.0f, PINK);
                } else {
                    DrawRectangle(px, py, TILE_SIZE, TILE_SIZE, color_nodo);
                }
            }
        }

        // Onda expansiva visual cuando explota una TNT
        if (tiempo_onda_explosion > 0.0f) {
            float radio_px = (0.45f - tiempo_onda_explosion) * 420.0f;
            DrawCircleLines(
                explosion_vis_x * TILE_SIZE + TILE_SIZE / 2,
                explosion_vis_y * TILE_SIZE + TILE_SIZE / 2,
                radio_px, ORANGE
            );
            DrawCircleLines(
                explosion_vis_x * TILE_SIZE + TILE_SIZE / 2,
                explosion_vis_y * TILE_SIZE + TILE_SIZE / 2,
                radio_px * 0.7f, YELLOW
            );
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

        // Dibujar todos los Gólems visibles en pantalla
        for (int i = 0; i < MAX_GOLEMS; i++) {
            if (!lista_golems[i].activo) continue;
            int gx = lista_golems[i].gx;
            int gy = lista_golems[i].gy;
            if (gx < min_x || gx > max_x || gy < min_y || gy > max_y) continue;

            int gpx = gx * TILE_SIZE;
            int gpy = gy * TILE_SIZE;
            Color c_chasis = (lista_golems[i].tarea == 3) ? (Color){ 230, 155, 40, 255 } :
                             ((lista_golems[i].tarea == 2) ? (Color){ 60, 190, 95, 255 } : (Color){ 40, 175, 225, 255 });
            DrawRectangle(gpx + 8, gpy + 10, TILE_SIZE - 16, TILE_SIZE - 14, c_chasis);
            DrawRectangleLines(gpx + 8, gpy + 10, TILE_SIZE - 16, TILE_SIZE - 14, WHITE);
            DrawRectangle(gpx + 13, gpy + 16, TILE_SIZE - 26, 8, YELLOW);
            DrawCircle(gpx + TILE_SIZE / 2, gpy + 4, 3.5f, (lista_golems[i].tarea > 0) ? LIME : RED);
        }

        // Dibujar todos los Enemigos visibles con barra de vida
        for (int e = 0; e < MAX_ENEMIGOS; e++) {
            if (!lista_enemigos[e].activo) continue;
            int ex = lista_enemigos[e].gx;
            int ey = lista_enemigos[e].gy;
            if (ex < min_x || ex > max_x || ey < min_y || ey > max_y) continue;

            int epx = ex * TILE_SIZE;
            int epy = ey * TILE_SIZE;
            Color c_enem = (lista_enemigos[e].tipo == 2) ? (Color){ 235, 65, 30, 255 } :
                           ((lista_enemigos[e].tipo == 1) ? (Color){ 220, 220, 230, 255 } : (Color){ 75, 215, 65, 255 });

            DrawCircle(epx + TILE_SIZE / 2, epy + TILE_SIZE / 2 + 2, TILE_SIZE * 0.36f, c_enem);
            DrawCircleLines(epx + TILE_SIZE / 2, epy + TILE_SIZE / 2 + 2, TILE_SIZE * 0.36f, BLACK);
            // Ojos hostiles
            DrawCircle(epx + 17, epy + 20, 4.0f, RED);
            DrawCircle(epx + 31, epy + 20, 4.0f, RED);

            // Barra de salud sobre el enemigo
            float ratio_hp = (float)lista_enemigos[e].hp / (float)lista_enemigos[e].max_hp;
            DrawRectangle(epx + 6, epy + 2, TILE_SIZE - 12, 6, BLACK);
            DrawRectangle(epx + 6, epy + 2, (int)((TILE_SIZE - 12) * ratio_hp), 6, RED);
        }

        // Dibujar Jugador y Efecto de Ataque de Espada [F]
        int jpx = jugador_x * TILE_SIZE + TILE_SIZE / 2;
        int jpy = jugador_y * TILE_SIZE + TILE_SIZE / 2;

        if (tiempo_ataque_espada > 0.0f) {
            DrawCircleLines(jpx, jpy, TILE_SIZE * 1.65f, SKYBLUE);
            DrawCircleLines(jpx, jpy, TILE_SIZE * 1.40f, GOLD);
            DrawCircle(jpx, jpy, TILE_SIZE * 1.50f, (Color){ 100, 220, 255, 65 });
        }

        DrawCircle(jpx, jpy, TILE_SIZE * 0.36f, RED);
        DrawCircleLines(jpx, jpy, TILE_SIZE * 0.36f, WHITE);

        EndMode2D();

        // --- J. HUD SUPERIOR CON BIOMA ACTUAL, SALUD, GÓLEMS Y ENEMIGOS ---
        int j_cx = jugador_x / CHUNK_SIZE;
        int j_cy = jugador_y / CHUNK_SIZE;
        int hora_ent = (int)hora_dia;
        int min_ent = (int)((hora_dia - (float)hora_ent) * 60.0f);
        TipoBioma bioma_actual = obtener_bioma_en(jugador_x, jugador_y);

        DrawRectangle(0, 0, screenWidth, alto_hud, tema.fondo_panel);
        DrawLine(0, alto_hud, screenWidth, alto_hud, tema.borde_panel);

        Rectangle r_hud1 = { 14.0f, 6.0f, (float)(screenWidth - 28), (float)sep_linea };
        Rectangle r_hud2 = { 14.0f, (float)(6 + sep_linea), (float)(screenWidth - 28), (float)sep_linea };
        Rectangle r_hud3 = { 14.0f, (float)(6 + sep_linea * 2), (float)(screenWidth - 28), (float)sep_linea };

        dibujar_texto_ajustado(
            (id_caja_activa != -1 || consola_activa)
                ? "MODO EDICIÓN UTF-8: Flechas [Izq/Der] mueven cursor | [ENTER] Aceptar | Barra inferior: áéíóúñ"
                : "WASD: Caminar | [F] Espada | [V] +1 Gólem | [E] Puerta/TNT/Puente | Clic Izq: Minar/Atacar | Clic Der: Poner",
            r_hud1, font_hud, false, (id_caja_activa != -1 || consola_activa) ? GOLD : tema.texto_normal
        );

        dibujar_texto_ajustado(
            TextFormat("FPS: %d | Pos: (%d,%d) Chunk:(%d,%d) | Bioma: [%s] | Vida: %d/%d HP | Gólems: %d | Enemigos: %d",
                       GetFPS(), jugador_x, jugador_y, j_cx, j_cy,
                       nombre_bioma(bioma_actual), jugador_hp, jugador_max_hp,
                       contar_golems_activos(), contar_enemigos_activos()),
            r_hud2, font_hud, false, (tema_actual == 1) ? DARKGREEN : GREEN
        );

        dibujar_texto_ajustado(
            TextFormat("Mano: [%s] Stock: %s | Reloj: %02d:%02d | Abatidos: %d | %s",
                       nombre_bloque(bloque_seleccionado),
                       modo_creativo ? "Infinito" : TextFormat("x%d", inventario[bloque_seleccionado]),
                       hora_ent, min_ent, enemigos_derrotados, estado_sistema),
            r_hud3, font_hud, false, tema.texto_etiqueta
        );

        // --- K. MINIMAPA 2D CENTRADO EN EL JUGADOR (RADAR DE BIOMAS, GÓLEMS Y ENEMIGOS) ---
        if (mostrar_minimapa && !consola_activa) {
            int map_px = 180;
            int x_mm = 16;
            int y_mm = screenHeight - map_px - 16;

            DrawRectangle(x_mm - 4, y_mm - 28, map_px + 8, map_px + 32, tema.fondo_panel);
            DrawRectangleLines(x_mm - 4, y_mm - 28, map_px + 8, map_px + 32, tema.borde_panel);
            dibujar_texto_ui(nombre_bioma(bioma_actual), x_mm + 6, y_mm - 24, 16, tema.texto_titulo);

            int radio_radar = 60;
            for (int my = 0; my < 60; my++) {
                for (int mx = 0; mx < 60; mx++) {
                    int gx = jugador_x + (mx - 30) * 2;
                    int gy = jugador_y + (my - 30) * 2;
                    Color cm = (Color){ 25, 30, 40, 255 };
                    if (gx >= 0 && gx < MAP_WIDTH && gy >= 0 && gy < MAP_HEIGHT) {
                        int cx = gx / CHUNK_SIZE;
                        int cy = gy / CHUNK_SIZE;
                        if (mundo[cy][cx].generado) {
                            int t = obtener_casilla_mundo(gx, gy);
                            cm = tabla_nodos[t].color;
                        }
                    }
                    DrawRectangle(x_mm + mx * 3, y_mm + my * 3, 3, 3, cm);
                }
            }

            // Dibujar Gólems (Cian) y Enemigos (Morado/Rojo) en el radar
            for (int g = 0; g < MAX_GOLEMS; g++) {
                if (!lista_golems[g].activo) continue;
                int dx = (lista_golems[g].gx - jugador_x) / 2 + 30;
                int dy = (lista_golems[g].gy - jugador_y) / 2 + 30;
                if (dx >= 0 && dx < radio_radar && dy >= 0 && dy < radio_radar) {
                    DrawRectangle(x_mm + dx * 3, y_mm + dy * 3, 4, 4, SKYBLUE);
                }
            }
            for (int e = 0; e < MAX_ENEMIGOS; e++) {
                if (!lista_enemigos[e].activo) continue;
                int dx = (lista_enemigos[e].gx - jugador_x) / 2 + 30;
                int dy = (lista_enemigos[e].gy - jugador_y) / 2 + 30;
                if (dx >= 0 && dx < radio_radar && dy >= 0 && dy < radio_radar) {
                    DrawRectangle(x_mm + dx * 3, y_mm + dy * 3, 4, 4, MAGENTA);
                }
            }

            DrawCircle(x_mm + 90, y_mm + 90, 4.0f, RED);
            DrawCircleLines(x_mm + 90, y_mm + 90, 5.0f, WHITE);
        }

        // --- L. BARRA RÁPIDA DE HERRAMIENTAS (HOTBAR) ---
        DrawRectangleRec(rect_hotbar, tema.fondo_panel);
        DrawRectangleLinesEx(rect_hotbar, 2.0f, tema.borde_panel);

        for (int s = 0; s < slots_hotbar; s++) {
            int id_b = orden_hotbar[s];
            Rectangle r_slot = {
                rect_hotbar.x + 10.0f + (float)(s * (lado_slot + 5)),
                rect_hotbar.y + 8.0f,
                (float)lado_slot,
                (float)lado_slot
            };
            bool activo = (bloque_seleccionado == id_b);
            bool hover_s = CheckCollisionPointRec(pos_raton, r_slot);

            DrawRectangleRec(r_slot, tabla_nodos[id_b].color);
            DrawRectangleLinesEx(r_slot, activo ? 3.5f : 1.5f,
                                 activo ? GOLD : (hover_s ? WHITE : DARKGRAY));

            dibujar_texto_ui(etiqueta_tecla_hb[s], (int)r_slot.x + 5, (int)r_slot.y + 2, 14, BLACK);
            dibujar_texto_ui(etiqueta_tecla_hb[s], (int)r_slot.x + 4, (int)r_slot.y + 1, 14, WHITE);

            const char *txt_cant = modo_creativo ? "Inf" : TextFormat("x%d", inventario[id_b]);
            Rectangle r_cant = { r_slot.x + 2.0f, r_slot.y + r_slot.height - 18.0f, r_slot.width - 4.0f, 16.0f };
            DrawRectangleRec(r_cant, (Color){ 0, 0, 0, 180 });
            dibujar_texto_ajustado(txt_cant, r_cant, 14, true,
                                   (inventario[id_b] > 0 || modo_creativo) ? YELLOW : RED);
        }

        // --- M. PANEL LATERAL FRONTEND GUI ---
        if (mostrar_panel_ui) {
            DrawRectangleRec(rect_panel_ui, tema.fondo_panel);
            DrawRectangleLinesEx(rect_panel_ui, 2.0f, tema.borde_panel);

            Rectangle r_tit_panel = { rect_panel_ui.x + 10.0f, rect_panel_ui.y + 6.0f, rect_panel_ui.width - 20.0f, (float)alto_titulo_panel };
            dibujar_texto_ajustado(titulo_ui, r_tit_panel, font_hud, false, tema.texto_titulo);

            if (total_pestanas_ui > 0) {
                float w_tab = (rect_panel_ui.width - 20.0f) / (float)total_pestanas_ui;
                int f_tab = font_hud - 4;
                if (f_tab < 12) f_tab = 12;

                for (int t = 0; t < total_pestanas_ui; t++) {
                    if (!pestanas_ui[t].activa) continue;
                    Rectangle r_tab = {
                        rect_panel_ui.x + 10.0f + (float)t * w_tab,
                        rect_panel_ui.y + (float)alto_titulo_panel,
                        w_tab - 3.0f,
                        (float)alto_barra_tabs
                    };
                    bool es_actual = (pestana_actual == t);
                    bool hover_t = CheckCollisionPointRec(pos_raton, r_tab);

                    DrawRectangleRec(r_tab, es_actual ? tema.borde_panel : tema.fondo_widget);
                    DrawRectangleLinesEx(r_tab, 1.5f, (es_actual || hover_t) ? GOLD : tema.borde_panel);
                    dibujar_texto_ajustado(
                        pestanas_ui[t].titulo, r_tab, f_tab, true,
                        es_actual ? BLACK : tema.texto_normal
                    );
                }
            }

            // El evento de una pestaña o caja de texto puede cambiar el layout.
            actualizar_geometria_widgets(rect_panel_ui, area_scroll_ui, font_hud - 2);

            BeginScissorMode(
                (int)area_scroll_ui.x,
                (int)area_scroll_ui.y,
                (int)area_scroll_ui.width,
                (int)area_scroll_ui.height
            );
            int font_w = font_hud - 2;

            for (int i = 0; i < total_widgets_ui; i++) {
                WidgetUI *w = &widgets_ui[i];
                if (!w->activo || w->pestana != pestana_actual) continue;
                bool tiene_foco_caja = (w->tipo == WIDGET_CAJA_TEXTO && id_caja_activa == w->id);
                Rectangle rect_item = w->rect_calculado;
                if (rect_item.width <= 0.0f || rect_item.height <= 0.0f) continue;

                bool visible = (rect_item.y + rect_item.height >= area_scroll_ui.y) &&
                               (rect_item.y <= area_scroll_ui.y + area_scroll_ui.height);

                bool hover = visible && CheckCollisionPointRec(pos_raton, area_scroll_ui) &&
                             CheckCollisionPointRec(pos_raton, rect_item);

                if (w->tipo == WIDGET_ETIQUETA) {
                    dibujar_texto_ajustado(w->etiqueta, rect_item, font_w, false, tema.texto_etiqueta);
                } else if (w->tipo == WIDGET_INVENTARIO) {
                    DrawRectangleRec(rect_item, tema.fondo_widget);
                    DrawRectangleLinesEx(rect_item, 1.5f, tema.borde_panel);

                    const int ids_mostrar[9] = { 4, 1, 6, 8, 9, 10, 12, 14, 19 };
                    int col_w = (int)(rect_item.width - 16.0f) / 3;
                    int fila_h = font_w + 14;

                    for (int m = 0; m < 9; m++) {
                        int id_m = ids_mostrar[m];
                        int col = m % 3;
                        int fila = m / 3;
                        int ix = (int)rect_item.x + 8 + col * col_w;
                        int iy = (int)rect_item.y + 8 + fila * fila_h;

                        DrawRectangle(ix, iy + 2, 16, 16, tabla_nodos[id_m].color);
                        DrawRectangleLines(ix, iy + 2, 16, 16, WHITE);
                        Rectangle r_celda = { (float)(ix + 20), (float)iy, (float)(col_w - 22), (float)fila_h };
                        dibujar_texto_ajustado(
                            TextFormat("%s:%d", tabla_nodos[id_m].nombre, inventario[id_m]),
                            r_celda, font_w - 4, false,
                            (inventario[id_m] > 0) ? tema.texto_normal : GRAY
                        );
                    }
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
                    dibujar_texto_ajustado(w->etiqueta, rect_item, font_w, false, RAYWHITE);
                } else if (w->tipo == WIDGET_CAJA_TEXTO) {
                    Rectangle r_lbl = { rect_item.x, rect_item.y, rect_item.width, (float)font_w + 4.0f };
                    dibujar_texto_ajustado(w->etiqueta, r_lbl, font_w - 2, false, tema.texto_normal);

                    Rectangle rect_caja = {
                        rect_item.x,
                        rect_item.y + (float)font_w + 4.0f,
                        rect_item.width,
                        (float)font_w + 18.0f
                    };
                    DrawRectangleRec(rect_caja, tema.fondo_widget);
                    DrawRectangleLinesEx(rect_caja, tiene_foco_caja ? 2.5f : 1.5f,
                                         tiene_foco_caja ? GOLD : (hover ? tema.borde_panel : GRAY));

                    bool parpadeo = tiene_foco_caja && (((int)(GetTime() * 2.5)) % 2 == 0);
                    char vista_cursor[160];
                    construir_texto_con_cursor(w->texto, cursor_caja_bytes, tiene_foco_caja && parpadeo, vista_cursor, sizeof(vista_cursor));

                    dibujar_texto_ajustado(
                        vista_cursor, rect_caja, font_w - 1, false,
                        tiene_foco_caja ? GOLD : tema.texto_normal
                    );

                    if (tiene_foco_caja) {
                        float w_ch = (rect_item.width - 18.0f) / 10.0f;
                        float y_ch = rect_item.y + (float)(font_w * 2) + 26.0f;
                        for (int c = 0; c < 10; c++) {
                            Rectangle r_ch = { rect_item.x + (float)c * (w_ch + 2.0f), y_ch, w_ch, 24.0f };
                            bool hov_ch = CheckCollisionPointRec(pos_raton, r_ch);
                            DrawRectangleRec(r_ch, hov_ch ? (Color){ 65, 105, 160, 255 } : (Color){ 35, 45, 65, 255 });
                            DrawRectangleLinesEx(r_ch, 1.0f, hov_ch ? GOLD : SKYBLUE);
                            dibujar_texto_ajustado(acentos_es[c], r_ch, font_w - 3, true, WHITE);
                        }
                    }
                } else if (w->tipo == WIDGET_DESLIZADOR) {
                    Rectangle r_lbl = { rect_item.x, rect_item.y, rect_item.width, (float)font_w + 4.0f };
                    dibujar_texto_ajustado(
                        TextFormat("%s: %d", w->etiqueta, w->valor),
                        r_lbl, font_w - 2, false, tema.texto_normal
                    );

                    float barra_x = rect_item.x + 10.0f;
                    float barra_y = rect_item.y + (float)font_w + 13.0f;
                    float barra_w = rect_item.width - 20.0f;
                    float ratio = (float)(w->valor - w->min_val) / (float)(w->max_val - w->min_val);

                    DrawRectangle((int)barra_x, (int)barra_y, (int)barra_w, 10, DARKGRAY);
                    DrawRectangle((int)barra_x, (int)barra_y, (int)(barra_w * ratio), 10, tema.borde_panel);
                    DrawCircle((int)(barra_x + barra_w * ratio), (int)barra_y + 5, 10.0f, hover ? GOLD : RAYWHITE);
                } else if (w->tipo == WIDGET_INTERRUPTOR) {
                    bool encendido = (w->valor != 0);
                    DrawRectangleRec(rect_item, tema.fondo_widget);
                    DrawRectangleLinesEx(rect_item, 1.5f, hover ? GOLD : tema.borde_panel);

                    Rectangle r_lbl = { rect_item.x + 4.0f, rect_item.y, rect_item.width - 74.0f, rect_item.height };
                    dibujar_texto_ajustado(w->etiqueta, r_lbl, font_w - 2, false, tema.texto_normal);

                    Rectangle sw_box = {
                        rect_item.x + rect_item.width - 64.0f,
                        rect_item.y + (rect_item.height - 24.0f) / 2.0f,
                        52.0f,
                        24.0f
                    };
                    DrawRectangleRec(sw_box, encendido ? (Color){ 45, 165, 85, 255 } : (Color){ 90, 95, 105, 255 });
                    DrawCircle(
                        (int)(encendido ? sw_box.x + sw_box.width - 12.0f : sw_box.x + 12.0f),
                        (int)(sw_box.y + 12.0f),
                        9.0f, RAYWHITE
                    );
                } else if (w->tipo == WIDGET_PROGRESO) {
                    int pct = (w->valor * 100) / w->max_val;
                    Rectangle r_lbl = { rect_item.x, rect_item.y, rect_item.width, (float)font_w + 4.0f };
                    dibujar_texto_ajustado(
                        TextFormat("%s: %d / %d (%d%%)", w->etiqueta, w->valor, w->max_val, pct),
                        r_lbl, font_w - 2, false, tema.texto_normal
                    );
                    Rectangle r_bar = {
                        rect_item.x + 4.0f,
                        rect_item.y + (float)font_w + 6.0f,
                        rect_item.width - 8.0f,
                        16.0f
                    };
                    float ratio = (float)w->valor / (float)w->max_val;
                    if (ratio < 0.0f) ratio = 0.0f;
                    if (ratio > 1.0f) ratio = 1.0f;

                    DrawRectangleRec(r_bar, tema.fondo_widget);
                    DrawRectangle((int)r_bar.x, (int)r_bar.y, (int)(r_bar.width * ratio), (int)r_bar.height, (Color){ 50, 185, 95, 255 });
                    DrawRectangleLinesEx(r_bar, 1.5f, tema.borde_panel);
                }
            }

            EndScissorMode();
        }

        // --- N. CONSOLA .LAT INTERACTIVA IN-GAME [F3] ---
        if (consola_activa) {
            DrawRectangleRec(rect_consola, (Color){ 10, 14, 22, 246 });
            DrawRectangleLinesEx(rect_consola, 2.5f, GOLD);

            Rectangle r_tit_con = { rect_consola.x + 12.0f, rect_consola.y + 6.0f, rect_consola.width - 24.0f, (float)font_hud + 8.0f };
            dibujar_texto_ajustado(
                "Consola .lat en Vivo (Flechas [Izq/Der] mueven cursor | [ENTER] Ejecutar | [F3] Cerrar)",
                r_tit_con, font_hud - 2, false, GOLD
            );
            DrawLine((int)rect_consola.x + 12, (int)rect_consola.y + font_hud + 16,
                     (int)(rect_consola.x + rect_consola.width - 12), (int)rect_consola.y + font_hud + 16, SKYBLUE);

            const char *chips_txt[4] = {
                "1: +10 Gólems Ingenieros",
                "2: Oleada 8 Enemigos",
                "3: Tender Puente",
                "4: Ir al Volcán"
            };
            const char *chips_cmd[4] = {
                "i=0 mientras(i<10) motor.golem_crear(motor.jugador_x()+i-4, motor.jugador_y()+1, 3) i=i+1 fin",
                "motor.enemigo_oleada(8)",
                "motor.construir_puente()",
                "motor.colocar_jugador(280, 512)"
            };

            float w_chip = (rect_consola.width - 36.0f) / 4.0f;
            float y_chip = rect_consola.y + (float)font_hud + 22.0f;
            for (int c = 0; c < 4; c++) {
                Rectangle r_chip = { rect_consola.x + 12.0f + (float)c * (w_chip + 4.0f), y_chip, w_chip, (float)font_hud + 10.0f };
                bool hov_c = CheckCollisionPointRec(pos_raton, r_chip);
                DrawRectangleRec(r_chip, hov_c ? (Color){ 65, 95, 145, 255 } : (Color){ 32, 44, 68, 255 });
                DrawRectangleLinesEx(r_chip, 1.0f, hov_c ? GOLD : SKYBLUE);
                dibujar_texto_ajustado(chips_txt[c], r_chip, font_hud - 4, true, RAYWHITE);

                if (hov_c && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    snprintf(entrada_consola, sizeof(entrada_consola), "%s", chips_cmd[c]);
                    cursor_consola_bytes = (int)strlen(entrada_consola);
                    evaluar_comando_consola_latino(entrada_consola);
                }
            }

            float w_ac = (rect_consola.width - 42.0f) / 10.0f;
            float y_ac = rect_consola.y + rect_consola.height - (float)(font_hud * 2) - 42.0f;
            for (int c = 0; c < 10; c++) {
                Rectangle r_ac = { rect_consola.x + 12.0f + (float)c * (w_ac + 2.0f), y_ac, w_ac, 24.0f };
                bool hov_ac = CheckCollisionPointRec(pos_raton, r_ac);
                DrawRectangleRec(r_ac, hov_ac ? (Color){ 65, 105, 160, 255 } : (Color){ 28, 38, 56, 255 });
                DrawRectangleLinesEx(r_ac, 1.0f, hov_ac ? GOLD : SKYBLUE);
                dibujar_texto_ajustado(acentos_es[c], r_ac, font_hud - 3, true, WHITE);
                if (hov_ac && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                    utf8_insertar_en_cursor(entrada_consola, sizeof(entrada_consola), &cursor_consola_bytes, acentos_es[c]);
                    reproducir_sonido_motor(SONIDO_CLIC);
                }
            }

            int y_log = (int)(y_chip + font_hud + 16.0f);
            int max_y_log = (int)(y_ac - 6.0f);
            for (int l = 0; l < total_logs_consola && y_log < max_y_log; l++) {
                Rectangle r_linea = { rect_consola.x + 16.0f, (float)y_log, rect_consola.width - 32.0f, (float)font_hud + 2.0f };
                Color c_log = (strncmp(historial_consola[l], "latino>", 7) == 0) ? SKYBLUE :
                              (strstr(historial_consola[l], "[Error") != NULL ? RED : GREEN);
                dibujar_texto_ajustado(historial_consola[l], r_linea, font_hud - 3, false, c_log);
                y_log += font_hud + 2;
            }

            Rectangle r_input_con = {
                rect_consola.x + 12.0f,
                rect_consola.y + rect_consola.height - (float)font_hud - 24.0f,
                rect_consola.width - 24.0f,
                (float)font_hud + 16.0f
            };
            DrawRectangleRec(r_input_con, (Color){ 22, 28, 38, 255 });
            DrawRectangleLinesEx(r_input_con, 2.0f, GOLD);

            bool blink = (((int)(GetTime() * 2.5)) % 2 == 0);
            char con_cursor[192];
            construir_texto_con_cursor(entrada_consola, cursor_consola_bytes, blink, con_cursor, sizeof(con_cursor));
            dibujar_texto_ajustado(
                TextFormat("latino> %s", con_cursor),
                r_input_con, font_hud - 2, false, YELLOW
            );
        }

        // --- O. NOTIFICACIÓN FLOTANTE (TOAST UX) ---
        if (tiempo_toast > 0.0f && texto_toast[0] != '\0') {
            float alpha = (tiempo_toast < 0.8f) ? (tiempo_toast / 0.8f) : 1.0f;
            unsigned char opacidad = (unsigned char)(240.0f * alpha);
            int max_w_toast = area_libre_w - 40;
            int ancho_t = medir_texto_ui(texto_toast, font_hud) + 40;
            if (ancho_t > max_w_toast) ancho_t = max_w_toast;

            int x_t = (area_libre_w - ancho_t) / 2;
            if (x_t < 16) x_t = 16;
            int y_t = alto_hud + 16;

            Rectangle r_toast = { (float)x_t, (float)y_t, (float)ancho_t, (float)(font_hud + 22) };
            DrawRectangleRec(r_toast, (Color){ 20, 65, 95, opacidad });
            DrawRectangleLinesEx(r_toast, 2.0f, (Color){ 100, 220, 255, opacidad });
            dibujar_texto_ajustado(texto_toast, r_toast, font_hud - 1, true, (Color){ 255, 255, 255, opacidad });
        }

        // --- P. TOOLTIP UX ---
        if (!modal_activo && tooltip_activo != NULL) {
            int f_tip = font_hud - 4;
            if (f_tip < 14) f_tip = 14;
            int w_tip = 310;
            Rectangle area_med = { 0, 0, (float)(w_tip - 24), 200 };
            int alto_texto_tip = procesar_texto_multilinea(tooltip_activo, area_med, f_tip, YELLOW, false);
            int h_tip = alto_texto_tip + 18;

            int x_tip = (int)pos_raton.x - w_tip - 16;
            int y_tip = (int)pos_raton.y + 12;
            if (x_tip < 12) x_tip = 12;
            if (y_tip + h_tip > screenHeight - 12) y_tip = screenHeight - h_tip - 12;

            DrawRectangle(x_tip, y_tip, w_tip, h_tip, (Color){ 10, 12, 18, 248 });
            DrawRectangleLinesEx((Rectangle){ (float)x_tip, (float)y_tip, (float)w_tip, (float)h_tip }, 1.5f, GOLD);
            Rectangle area_dib = { (float)(x_tip + 12), (float)(y_tip + 9), (float)(w_tip - 24), (float)alto_texto_tip };
            procesar_texto_multilinea(tooltip_activo, area_dib, f_tip, YELLOW, true);
        }

        // --- Q. LECTURA DE CARTELES ---
        if (!consola_activa && obtener_casilla_mundo(jugador_x, jugador_y) == 3) {
            const char *msg = buscar_texto_cartel(jugador_x, jugador_y);
            if (msg != NULL) {
                int x_cartel = mostrar_minimapa ? 214 : 24;
                int w_cartel = area_libre_w - x_cartel - 24;
                if (w_cartel < 240) w_cartel = 240;

                Rectangle area_med = { 0, 0, (float)(w_cartel - 32), 200 };
                int h_txt = procesar_texto_multilinea(msg, area_med, font_hud - 2, YELLOW, false);
                int alto_caja = h_txt + 22;
                int y_cartel = (int)rect_hotbar.y - alto_caja - 12;

                DrawRectangle(x_cartel, y_cartel, w_cartel, alto_caja, (Color){ 20, 20, 20, 242 });
                DrawRectangleLinesEx((Rectangle){ (float)x_cartel, (float)y_cartel, (float)w_cartel, (float)alto_caja }, 2.0f, GOLD);

                Rectangle area_dib = { (float)(x_cartel + 16), (float)(y_cartel + 11), (float)(w_cartel - 32), (float)h_txt };
                procesar_texto_multilinea(msg, area_dib, font_hud - 2, YELLOW, true);
            }
        }

        // --- R. VENTANA MODAL EMERGENTE ---
        if (modal_activo) {
            DrawRectangle(0, 0, screenWidth, screenHeight, (Color){ 0, 0, 0, 185 });
            int w_mod = (int)(680.0f * escala_ui);
            if (w_mod > screenWidth - 80) w_mod = screenWidth - 80;

            Rectangle area_med = { 0, 0, (float)(w_mod - 56), 400 };
            int alto_cuerpo = procesar_texto_multilinea(modal_mensaje, area_med, font_hud - 1, tema.texto_normal, false);
            int h_mod = font_hud + 48 + alto_cuerpo + 84;

            int x_mod = (screenWidth - w_mod) / 2;
            int y_mod = (screenHeight - h_mod) / 2;

            DrawRectangle(x_mod, y_mod, w_mod, h_mod, tema.fondo_panel);
            DrawRectangleLinesEx((Rectangle){ (float)x_mod, (float)y_mod, (float)w_mod, (float)h_mod }, 3.0f, GOLD);

            Rectangle r_tit_mod = { (float)(x_mod + 24), (float)(y_mod + 16), (float)(w_mod - 48), (float)(font_hud + 10) };
            dibujar_texto_ajustado(modal_titulo, r_tit_mod, font_hud + 2, false, tema.texto_titulo);
            DrawLine(x_mod + 24, y_mod + font_hud + 34, x_mod + w_mod - 24, y_mod + font_hud + 34, tema.borde_panel);

            Rectangle area_cuerpo = { (float)(x_mod + 28), (float)(y_mod + font_hud + 48), (float)(w_mod - 56), (float)alto_cuerpo };
            procesar_texto_multilinea(modal_mensaje, area_cuerpo, font_hud - 1, tema.texto_normal, true);

            Rectangle btn_cerrar = {
                (float)(x_mod + w_mod / 2 - 140),
                (float)(y_mod + h_mod - 62),
                280.0f,
                44.0f
            };
            DrawRectangleRec(btn_cerrar, (Color){ 45, 135, 85, 255 });
            DrawRectangleLinesEx(btn_cerrar, 2.0f, WHITE);
            dibujar_texto_ajustado("Aceptar [Clic / Enter]", btn_cerrar, font_hud - 2, true, WHITE);
        }

        EndDrawing();
    }

    if (fuente_ttf_cargada) {
        UnloadFont(fuente_ui);
    }
    cerrar_audio_procedural();
    CloseWindow();
    return 0;
}