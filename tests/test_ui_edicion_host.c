#define _POSIX_C_SOURCE 200809L
#include "latino.h"
#include "ui_biblioteca.h"
#include "ui_catalogo.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define CHECK(c)                                                               \
  do {                                                                         \
    if (!(c)) {                                                                \
      fprintf(stderr, "FALLO edicion frame=%d linea=%d: %s\n", frame,          \
              __LINE__, #c);                                                   \
      exit(2);                                                                 \
    }                                                                          \
  } while (0)
static int frame, click_id, estado_io = 1, modo;
static bool abierta;
static lat_mv *vm_observada;
static size_t memoria_caliente, cadenas_calientes;
static UIBiblioteca *crear_prueba(lat_mv *, const ui_catalogo_io_t *);
static lat_mv *crear_vm(void) {
  vm_observada = latC_crear_mv();
  return vm_observada;
}
#define main host_edicion_main
#define latC_crear_mv crear_vm
#define ui_biblioteca_crear crear_prueba
#include "../src/ui/host_ui.c"
#undef main
#undef latC_crear_mv
#undef ui_biblioteca_crear
static int escribir(void *ctx, const char *r, const char *s, size_t n) {
  if (estado_io <= 0)
    return estado_io;
  ui_catalogo_io_t io = ui_catalogo_io_archivo();
  int res = io.escribir(ctx, r, s, n);
  return res == 1 && estado_io == 2 ? 2 : res;
}
static UIBiblioteca *crear_prueba(lat_mv *mv, const ui_catalogo_io_t *io) {
  (void)io;
  ui_catalogo_io_t prueba = ui_catalogo_io_archivo();
  prueba.escribir = escribir;
  return ui_biblioteca_crear(mv, &prueba);
}
void SetConfigFlags(unsigned int f) { (void)f; }
void InitWindow(int w, int h, const char *t) {
  CHECK(w > 0 && h > 0 && !abierta && strstr(t, "Biblioteca13"));
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
  CHECK(abierta && frame < 140);
  click_id = 0;
  if (modo == 2) {
    if (frame == 0)
      CHECK(lui_establecer_texto(&ui, 102, "Borrador Escape"));
    if (frame == 1)
      click_id = 208;
    if (frame == 2)
      return true;
    if (frame == 3)
      click_id = 207;
    return false;
  }
  if (modo == 3) {
    if (frame == 0) {
      estado_io = 2;
      CHECK(lui_establecer_texto(&ui, 102, "Guardado con aviso"));
      click_id = 202;
    }
    if (frame == 1)
      return true;
    if (frame == 2)
      click_id = 209;
    return false;
  }
  if (modo == 1) {
    if (frame == 125)
      return true;
    CHECK(lui_enfocar(&ui, 102));
    UIEvento e;
    (void)lui_tecla(&ui, UI_TECLA_BORRAR, &e);
    CHECK(lui_caracter(&ui, frame % 2 ? 'A' : 'B'));
    if (frame == 124)
      click_id = 202;
    return false;
  }
  switch (frame) {
  case 1:
    CHECK(lui_establecer_texto(&ui, 102, "Titulo editado"));
    click_id = 212;
    break;
  case 2:
    click_id = 211;
    break; /* fondo deshabilitado */
  case 3:
    click_id = 208;
    break;
  case 4:
    click_id = 212;
    break;
  case 5:
    click_id = 207;
    break;
  case 6:
    CHECK(lui_establecer_texto(&ui, 103, "Autora nueva"));
    CHECK(lui_establecer_texto(&ui, 100, "LIB-1"));
    click_id = 210;
    break;
  case 7:
    CHECK(lui_establecer_texto(&ui, 100, "consulta posterior"));
    click_id = 211;
    break;
  case 8:
    click_id = 206;
    break;
  case 9:
    CHECK(lui_establecer_texto(&ui, 104, "0"));
    click_id = 202;
    break;
  case 10:
    CHECK(lui_establecer_texto(&ui, 104, "2001"));
    return true;
  case 11:
    estado_io = 0;
    click_id = 206;
    break;
  case 12:
    click_id = 208;
    break;
  case 13:
    return true;
  case 14:
    estado_io = 2;
    click_id = 206;
    break;
  case 15:
    click_id = 208;
    break;
  case 16:
    click_id = 212;
    break;
  case 17:
    estado_io = 1;
    click_id = 202;
    break;
  case 18:
  case 19:
  case 21:
    click_id = 203;
    break;
  case 20:
    click_id = 204;
    break;
  case 22:
    click_id = 205;
    break;
  case 23:
    CHECK(lui_establecer_texto(&ui, 102, "Borrador al salir"));
    return true;
  case 24:
    click_id = 207;
    break;
  default:
    break;
  }
  return false;
}
bool IsMouseButtonPressed(int b) {
  return b == MOUSE_BUTTON_LEFT && click_id != 0;
}
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
  return p.x >= r.x && p.x <= r.x + r.width && p.y >= r.y &&
         p.y <= r.y + r.height;
}
bool IsKeyPressed(int k) { return modo == 2 && frame == 0 && k == KEY_ESCAPE; }
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
  CHECK(lui_validar(&ui) && ui.arbol.cantidad < LUI_MAX_NODOS);
  if (modo == 2) {
    CHECK(!strcmp(lui_texto(&ui, 102), "Borrador Escape"));
    if (frame == 0)
      CHECK(lui_habilitado(&ui, 208) && !lui_habilitado(&ui, 202));
    ++frame;
    return;
  }
  if (modo == 3) {
    CHECK(!strcmp(lui_texto(&ui, 303), "Sin cambios"));
    CHECK(strstr(lui_texto(&ui, 304), "sincronizacion"));
    if (frame == 1)
      CHECK(lui_habilitado(&ui, 209) && !lui_habilitado(&ui, 202));
    ++frame;
    return;
  }
  if (modo == 1) {
    if (frame == 20) {
      memoria_caliente = vm_observada->memoria_usada;
      cadenas_calientes = vm_observada->global->strt.nuse;
    }
    if (frame == 120) {
      CHECK(memoria_caliente == vm_observada->memoria_usada);
      CHECK(cadenas_calientes == vm_observada->global->strt.nuse);
    }
    ++frame;
    return;
  }
  const char *estado = lui_texto(&ui, 302), *sucio = lui_texto(&ui, 303),
             *sync = lui_texto(&ui, 304);
  if (frame == 1 || frame == 2 || frame == 4 || frame == 6 || frame == 7 ||
      frame == 10 || frame == 11 || frame == 13 || frame == 23) {
    CHECK(!strcmp(sucio, "Cambios pendientes"));
    CHECK(lui_habilitado(&ui, 208) && !lui_habilitado(&ui, 211));
    CHECK(!lui_habilitado(&ui, 101));
  }
  if (frame == 1)
    CHECK(ui.nodos[ui.foco].id_widget == 208);
  if (frame == 3)
    CHECK(!strcmp(lui_texto(&ui, 102), "Titulo editado"));
  if (frame == 5)
    CHECK(!strcmp(lui_texto(&ui, 101), "LIB-2") &&
          !strcmp(sucio, "Sin cambios"));
  if (frame == 8)
    CHECK(!strcmp(lui_texto(&ui, 101), "LIB-1") &&
          !strcmp(sucio, "Sin cambios"));
  if (frame == 9)
    CHECK(ui.nodos[ui.foco].id_widget == 104 && strstr(estado, "anio"));
  if (frame == 11)
    CHECK(strstr(estado, "No se guardo") &&
          !strcmp(lui_texto(&ui, 104), "2001"));
  if (frame == 14)
    CHECK(lui_habilitado(&ui, 209) && !lui_habilitado(&ui, 202) &&
          !strcmp(sucio, "Sin cambios"));
  if (frame == 14 || frame == 15 || frame == 16)
    CHECK(strstr(sync, "sincronizacion"));
  if (frame == 17)
    CHECK(strstr(sync, "sin advertencia"));
  if (frame == 18 || frame == 19 || frame == 21) {
    CHECK(lui_habilitado(&ui, 205) && !lui_habilitado(&ui, 203));
    CHECK(ui.nodos[ui.foco].id_widget == 204);
  }
  if (frame == 20)
    CHECK(strstr(estado, "cancelada"));
  if (frame == 22)
    CHECK(strstr(lui_texto(&ui, 300), "Libros: 1"));
  ++frame;
}
int main(int argc, char **argv) {
  CHECK(argc == 2 && argv[1][0] == '/');
  char anterior[4096], dir[] = "/tmp/latino-edicion13-XXXXXX";
  CHECK(getcwd(anterior, sizeof anterior) && mkdtemp(dir) && chdir(dir) == 0);
  ui_catalogo_t *cat = malloc(sizeof *cat);
  CHECK(cat);
  ui_catalogo_inicializar(cat);
  CHECK(ui_catalogo_cargar_archivo(cat, "catalogo-v1.tsv") == 0);
  CHECK(ui_catalogo_registrar(cat, cat->ruta, "LIB-1", "Rayuela", "Cortázar",
                              1963, NULL) == 0);
  CHECK(ui_catalogo_registrar(cat, cat->ruta, "LIB-2", "Pedro Páramo", "Rulfo",
                              1955, NULL) == 0);
  char *args[] = {"host_ui", argv[1], NULL};
  CHECK(host_edicion_main(2, args) == 0 && !abierta && frame == 24);
  ui_catalogo_inicializar(cat);
  CHECK(ui_catalogo_cargar_archivo(cat, "catalogo-v1.tsv") == 0);
  CHECK(cat->total == 1 && cat->libros[0].id_interno == 1 &&
        cat->libros[0].anio == 2001);
  CHECK(!strcmp(cat->libros[0].titulo,
                "Rayuela")); /* borrador descartado sólo tras decisión */
  modo = 1;
  frame = 0;
  estado_io = 1;
  CHECK(host_edicion_main(2, args) == 0 && frame == 125 && !abierta);
  modo = 2;
  frame = 0;
  CHECK(host_edicion_main(2, args) == 0 && frame == 3 && !abierta);
  modo = 3;
  frame = 0;
  CHECK(host_edicion_main(2, args) == 0 && frame == 2 && !abierta);
  /* Un script con IDs correctos pero tipos equivocados falla antes de abrir
     la ventana; nunca continúa silenciosamente sin protección. */
  FILE *entrada = fopen(argv[1], "r"),
       *salida = fopen("contrato_invalido.lat", "w");
  CHECK(entrada && salida);
  char *linea = NULL;
  size_t capacidad = 0;
  while (getline(&linea, &capacidad, entrada) >= 0) {
    if (strstr(linea, "ui.campo_texto(101,"))
      CHECK(fputs("        ui.boton(101, \"Codigo\")\n", salida) >= 0);
    else
      CHECK(fputs(linea, salida) >= 0);
  }
  CHECK(!ferror(entrada));
  free(linea);
  CHECK(fclose(entrada) == 0 && fclose(salida) == 0);
  char *malos[] = {"host_ui", "contrato_invalido.lat", NULL};
  frame = 0;
  CHECK(host_edicion_main(2, malos) == 1 && !abierta && frame == 0);
  CHECK(unlink("contrato_invalido.lat") == 0);
  CHECK(unlink("catalogo-v1.tsv") == 0 && chdir(anterior) == 0 &&
        rmdir(dir) == 0);
  free(cat);
  puts("OK: host y script13 reales, búsqueda capturada, foco, modales, borrado "
       "separado, cierre retenido, 0/2/1 y 100 frames sin nuevas cadenas");
  return 0;
}
