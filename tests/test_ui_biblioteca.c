#define _POSIX_C_SOURCE 200809L
#include "latino.h"
#include "ui_catalogo.h"
#include "ui_biblioteca.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        if (!(c)) {                                                                                \
            fprintf(stderr, "FALLO biblioteca frame=%d %s:%d: %s\n", frame, __FILE__, __LINE__,    \
                    #c);                                                                           \
            exit(2);                                                                               \
        }                                                                                          \
    } while (0)
static int frame, modo, click_id, max_frames;
static bool abierta;
static lat_mv *vm_observada;
static size_t saldo_caliente, cadenas_calientes;
static int estado_io_prueba;
static UIBiblioteca *crear_biblioteca_observada(lat_mv *, const ui_catalogo_io_t *);
static lat_mv *crear_observada(void) {
    vm_observada = latC_crear_mv();
    return vm_observada;
}
/* Host de produccion incluido sin cambiar su logica. Solo Raylib es simulado. */
#define main host_biblioteca_main
#define latC_crear_mv crear_observada
#define ui_biblioteca_crear crear_biblioteca_observada
#include "../src/ui/host_ui.c"
#undef latC_crear_mv
#undef ui_biblioteca_crear
#undef main

static int escribir_probado(void *ctx, const char *r, const char *s, size_t n) {
    if (estado_io_prueba == 0 || estado_io_prueba == -1)
        return estado_io_prueba;
    ui_catalogo_io_t io = ui_catalogo_io_archivo();
    int estado = io.escribir(ctx, r, s, n);
    return estado == 1 && estado_io_prueba == 2 ? 2 : estado;
}
static UIBiblioteca *crear_biblioteca_observada(lat_mv *mv, const ui_catalogo_io_t *io) {
    if (modo != 3)
        return ui_biblioteca_crear(mv, io);
    ui_catalogo_io_t prueba = ui_catalogo_io_archivo();
    prueba.escribir = escribir_probado;
    return ui_biblioteca_crear(mv, &prueba);
}

static void ficha(const char *c, const char *t, const char *a, const char *y) {
    CHECK(lui_establecer_texto(&ui, 101, c));
    CHECK(lui_establecer_texto(&ui, 102, t));
    CHECK(lui_establecer_texto(&ui, 103, a));
    CHECK(lui_establecer_texto(&ui, 104, y));
}
void SetConfigFlags(unsigned int f) { (void)f; }
void InitWindow(int w, int h, const char *t) {
    CHECK(w > 0 && h > 0 && !abierta && strstr(t, "Biblioteca12"));
    abierta = true;
}
bool IsWindowReady(void) { return abierta; }
void CloseWindow(void) {
    CHECK(abierta);
    abierta = false;
}
void SetWindowMinSize(int w, int h) {
    (void)w;
    (void)h;
}
void SetTargetFPS(int n) { CHECK(n == 60); }
void SetExitKey(int k) { CHECK(k == KEY_NULL); }
int GetScreenWidth(void) { return 900; }
int GetScreenHeight(void) { return 600; }
bool WindowShouldClose(void) {
    CHECK(abierta && frame <= max_frames);
    if (frame == max_frames)
        return true;
    click_id = 0;
    if (modo == 3) {
        if (frame == 0 || frame == 2 || frame == 4) {
            estado_io_prueba = frame == 0 ? 0 : frame == 2 ? -1 : 2;
            ficha("LIB-1", frame == 4 ? "Advertencia" : "Rechazado", "Autor", "2000");
            click_id = 202;
        } else
            click_id = 204;
        return false;
    }
    if (modo == 2) {
        if (frame == 1) {
            ficha("X", "Intento", "Autor", "2000");
            click_id = 201;
        }
        return false;
    }
    if (modo == 1)
        return false;
    switch (frame) {
    case 1:
        ficha("LIB-1", "Cien años de soledad", "García Márquez", "1967");
        click_id = 201;
        break;
    case 2:
        click_id = 201;
        break;
    case 3:
        click_id = 213;
        break;
    case 4:
        ficha("LIB-2", "Pedro Páramo", "Juan Rulfo", "1955");
        click_id = 201;
        break;
    case 5:
        CHECK(lui_establecer_texto(&ui, 102, "El llano en llamas"));
        click_id = 202;
        break;
    case 6:
        click_id = 211;
        break;
    case 7:
        CHECK(lui_establecer_texto(&ui, 100, "rulfo"));
        click_id = 210;
        break;
    case 8:
    case 10:
    case 11:
    case 114:
    case 116:
        click_id = 203;
        break;
    case 9:
    case 117:
        click_id = 204;
        break;
    case 12:
        click_id = 213;
        break;
    case 13:
        ficha("LIB-3", "Ficciones", "Borges", "1944");
        click_id = 201;
        break;
    case 115:
        click_id = 211;
        break;
    default:
        if (frame >= 14 && frame < 114) {
            CHECK(lui_establecer_texto(&ui, 102, frame % 2 ? "Cambio B" : "Cambio A"));
            click_id = 202;
        }
        break;
    }
    return false;
}
bool IsMouseButtonPressed(int b) { return b == MOUSE_BUTTON_LEFT && click_id != 0; }
Vector2 GetMousePosition(void) {
    for (size_t i = 0; i < ui.arbol.cantidad; ++i)
        if (ui.nodos[i].id_widget == click_id) {
            UIRect r = ui.rectangulos[i];
            CHECK(r.ancho > 0 && r.alto > 0);
            return (Vector2){r.x + r.ancho / 2, r.y + r.alto / 2};
        }
    CHECK(false);
    return (Vector2){0, 0};
}
bool CheckCollisionPointRec(Vector2 p, Rectangle r) {
    return p.x >= r.x && p.x <= r.x + r.width && p.y >= r.y && p.y <= r.y + r.height;
}
bool IsKeyPressed(int k) {
    (void)k;
    return false;
}
bool IsKeyDown(int k) {
    (void)k;
    return false;
}
int GetCharPressed(void) { return 0; }
void BeginDrawing(void) {}
void ClearBackground(Color c) { (void)c; }
void DrawText(const char *t, int x, int y, int s, Color c) {
    (void)t;
    (void)x;
    (void)y;
    (void)s;
    (void)c;
}
void DrawRectangleRec(Rectangle r, Color c) {
    (void)r;
    (void)c;
}
void DrawRectangleLinesEx(Rectangle r, float t, Color c) {
    (void)r;
    (void)t;
    (void)c;
}
void BeginScissorMode(int x, int y, int w, int h) {
    (void)x;
    (void)y;
    (void)w;
    (void)h;
}
void EndScissorMode(void) {}
const char *TextFormat(const char *fmt, ...) {
    static char s[256];
    va_list a;
    va_start(a, fmt);
    vsnprintf(s, sizeof s, fmt, a);
    va_end(a);
    return s;
}
void EndDrawing(void) {
    CHECK(vm_observada->ptrctx == 0 && vm_observada->ptrpila == 0);
    CHECK(lui_validar(&ui));
    CHECK(ui.arbol.cantidad < 128);
    const char *estado = lui_texto(&ui, 302);
    if (modo == 0) {
        if (frame == 1 || frame == 4 || frame == 5 || frame == 11 || frame == 13 ||
            (frame >= 14 && frame < 114))
            CHECK(!strcmp(estado, "Operacion confirmada."));
        if (frame == 2)
            CHECK(strstr(estado, "codigo ya"));
        if (frame == 3 || frame == 12)
            CHECK(!*lui_texto(&ui, 101));
        if (frame == 6 || frame == 11 || frame == 115)
            CHECK(!strcmp(lui_texto(&ui, 101), "LIB-1"));
        if (frame == 7 || frame == 9)
            CHECK(!strcmp(lui_texto(&ui, 101), "LIB-2"));
        if (frame == 8 || frame == 10 || frame == 114 || frame == 116)
            CHECK(strstr(estado, "otra vez"));
        if (frame == 9 || frame == 117)
            CHECK(strstr(estado, "canceladas"));
        if (frame == 30) {
            saldo_caliente = vm_observada->memoria_usada;
            cadenas_calientes = vm_observada->global->strt.nuse;
        }
        if (frame == 112) {
            CHECK(vm_observada->memoria_usada == saldo_caliente);
            CHECK(vm_observada->global->strt.nuse == cadenas_calientes);
        }
    } else if (modo == 3) {
        if (frame == 0)
            CHECK(strstr(estado, "No se guardo"));
        if (frame == 1 || frame == 3)
            CHECK(!strcmp(lui_texto(&ui, 102), "Cien años de soledad"));
        if (frame == 2)
            CHECK(strstr(estado, "no disponible"));
        if (frame == 4)
            CHECK(strstr(estado, "sincronizacion final"));
        if (frame == 5)
            CHECK(!strcmp(lui_texto(&ui, 102), "Advertencia"));
    } else if (modo == 1)
        CHECK(!strcmp(lui_texto(&ui, 101), "LIB-1"));
    else if (frame == 1)
        CHECK(strstr(estado, "Abre un catalogo valido"));
    ++frame;
}
static void ejecutar(const char *ruta, int caso, int frames) {
    frame = 0;
    modo = caso;
    max_frames = frames;
    char *args[] = {"host_ui", (char *)ruta, NULL};
    CHECK(host_biblioteca_main(2, args) == 0);
    CHECK(!abierta);
    vm_observada = NULL;
}
int main(int argc, char **argv) {
    CHECK(argc == 2 && argv[1][0] == '/');
    char anterior[4096], dir[] = "/tmp/latino-biblioteca12-XXXXXX";
    CHECK(getcwd(anterior, sizeof anterior));
    CHECK(mkdtemp(dir));
    CHECK(chdir(dir) == 0);
    ejecutar(argv[1], 0, 118);
    ejecutar(argv[1], 1, 1);
    ejecutar(argv[1], 3, 6);
    ui_catalogo_t *cat = malloc(sizeof *cat);
    CHECK(cat);
    ui_catalogo_inicializar(cat);
    CHECK(ui_catalogo_cargar_archivo(cat, "catalogo-v1.tsv") == UI_CAT_OK);
    CHECK(cat->total == 2 && cat->siguiente_id == 4);
    CHECK(ui_catalogo_buscar_por_id(cat, 2) == NULL);
    CHECK(!strcmp(ui_catalogo_buscar_por_id(cat, 1)->titulo, "Advertencia"));
    CHECK(!strcmp(ui_catalogo_buscar_por_id(cat, 3)->titulo, "Cambio B"));
    free(cat);
    const char bytes[] = "LATINOUI-CATALOGO-V1\t1\t0\n\0OCULTO\n";
    FILE *f = fopen("catalogo-v1.tsv", "wb");
    CHECK(f);
    CHECK(fwrite(bytes, 1, sizeof bytes - 1, f) == sizeof bytes - 1);
    CHECK(fclose(f) == 0);
    ejecutar(argv[1], 2, 2);
    char leido[sizeof bytes];
    f = fopen("catalogo-v1.tsv", "rb");
    CHECK(f);
    CHECK(fread(leido, 1, sizeof leido, f) == sizeof bytes - 1);
    CHECK(!memcmp(leido, bytes, sizeof bytes - 1));
    CHECK(fclose(f) == 0);
    CHECK(unlink("catalogo-v1.tsv") == 0);
    CHECK(unlink("catalogo-v1.tsv.lock") == 0);
    CHECK(chdir(anterior) == 0);
    CHECK(rmdir(dir) == 0);
    puts("OK: host y script reales, CRUD, busqueda, doble confirmacion, Cancelar, 100 ediciones "
         "sin crecimiento tras calentamiento, estados -1/0/1/2, reapertura y archivo corrupto "
         "protegido");
    return 0;
}
