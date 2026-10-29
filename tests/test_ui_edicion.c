#include "ui_edicion.h"
#include "ui_utf8.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(c)                                                               \
  do {                                                                         \
    if (!(c)) {                                                                \
      fprintf(stderr, "FALLO %d: %s\n", __LINE__, #c);                         \
      exit(2);                                                                 \
    }                                                                          \
  } while (0)
/* Backend propio de la prueba, inyectado por instancia, sin ganchos globales.
 */
typedef struct {
  int estado;
  char *bytes;
  size_t n;
} Disco;
static int leer(void *p, const char *r, char **s, size_t *n) {
  (void)r;
  Disco *d = p;
  *s = NULL;
  *n = 0;
  if (!d->bytes)
    return 0;
  *s = malloc(d->n);
  CHECK(*s);
  memcpy(*s, d->bytes, d->n);
  *n = d->n;
  return 1;
}
static void liberar(void *p, char *s) {
  (void)p;
  free(s);
}
static int escribir(void *p, const char *r, const char *s, size_t n) {
  (void)r;
  Disco *d = p;
  if (d->estado == 1 || d->estado == 2) {
    char *c = malloc(n);
    CHECK(c);
    memcpy(c, s, n);
    free(d->bytes);
    d->bytes = c;
    d->n = n;
  }
  return d->estado;
}
int main(void) {
  ui_catalogo_t *cat = malloc(sizeof *cat);
  CHECK(cat);
  Disco d = {.estado = 1};
  CHECK(ui_catalogo_inicializar_con_io(
      cat, (ui_catalogo_io_t){&d, leer, escribir, liberar}));
  CHECK(ui_catalogo_cargar_archivo(cat, "cat.tsv") == UI_CAT_OK);
  int id = 0;
  CHECK(ui_catalogo_registrar(cat, "cat.tsv", "LIB-1", "Rayuela",
                              "Julio Cortázar", 1963, &id) == 0);
  UIEdicion s;
  ui_edicion_inicializar(&s);
  ui_edicion_cargar(&s, &cat->libros[0]);
  CHECK(!ui_edicion_sucia(&s));
  CHECK(ui_edicion_editar(&s, UI_ED_TITULO, "Rayuela") == 0 &&
        s.revision[1] == 0);
  CHECK(ui_edicion_editar(&s, UI_ED_TITULO, "Otro") == 1 && s.revision[1] == 1);
  CHECK(ui_edicion_sucia(&s));
  CHECK(ui_edicion_editar(&s, UI_ED_TITULO, "Rayuela") == 1 &&
        !ui_edicion_sucia(&s));
  char largo[256];
  memset(largo, 'x', 255);
  largo[255] = 0;
  CHECK(ui_edicion_editar(&s, UI_ED_TITULO, largo) == 1 &&
        strlen(s.texto[1]) == 255);
  CHECK(ui_edicion_guardar(&s, cat) == UI_CAT_ERR_LONGITUD &&
        s.campo_invalido == 1);
  CHECK(strlen(s.texto[1]) == 255 && !strcmp(cat->libros[0].titulo, "Rayuela"));
  CHECK(ui_edicion_editar(&s, UI_ED_TITULO, "\xf0\x9f") == -1 &&
        strlen(s.texto[1]) == 255);
  CHECK(ui_edicion_editar(&s, (UIEdCampo)4, "x") == -1);
  s.revision[1] = UINT64_MAX;
  CHECK(ui_edicion_editar(&s, UI_ED_TITULO, "Edición Ñ") == 1 &&
        s.revision[1] == UINT64_MAX);
  UIEdSolicitud p = {.accion = UI_ED_BUSCAR, .consulta = "Cortázar"},
                salida = {0};
  CHECK(ui_edicion_solicitar(&s, p) == 0 && s.modo == UI_ED_CAMBIOS);
  CHECK(ui_edicion_solicitar(&s, (UIEdSolicitud){.accion = UI_ED_CERRAR}) ==
        -1);
  CHECK(s.pendiente.accion == UI_ED_BUSCAR &&
        !strcmp(s.pendiente.consulta, "Cortázar"));
  CHECK(ui_edicion_editar(&s, UI_ED_TITULO, "Intruso") == -1);
  for (int estado = -1; estado <= 0; ++estado) {
    d.estado = estado;
    size_t n = d.n;
    char *antes = malloc(n);
    CHECK(antes);
    memcpy(antes, d.bytes, n);
    CHECK(ui_edicion_resolver(&s, cat, UI_ED_GUARDAR, &salida) ==
          (estado == 0 ? UI_CAT_ERR_ESCRITURA_ESTADO_0
                       : UI_CAT_ERR_NO_SOPORTADO));
    CHECK(salida.accion == UI_ED_NINGUNA && s.modo == UI_ED_CAMBIOS &&
          ui_edicion_sucia(&s));
    CHECK(n == d.n && !memcmp(antes, d.bytes, n));
    free(antes);
  }
  d.estado = 2;
  CHECK(ui_edicion_resolver(&s, cat, UI_ED_GUARDAR, &salida) ==
        UI_CAT_ADVERTENCIA_SYNC);
  CHECK(salida.accion == UI_ED_BUSCAR && !strcmp(salida.consulta, "Cortázar"));
  CHECK(!ui_edicion_sucia(&s) && s.aviso_sync && s.modo == UI_ED_NORMAL);
  CHECK(ui_edicion_resolver(&s, cat, UI_ED_GUARDAR, &salida) ==
            UI_CAT_ERR_ARGUMENTO &&
        salida.accion == UI_ED_NINGUNA);
  ui_edicion_cargar(&s, &cat->libros[0]);
  CHECK(s.aviso_sync);
  CHECK(ui_edicion_editar(&s, UI_ED_AUTOR, "Temporal") == 1);
  CHECK(ui_edicion_solicitar(&s, (UIEdSolicitud){.accion = UI_ED_CERRAR}) == 0);
  CHECK(ui_edicion_resolver(&s, cat, UI_ED_CONTINUAR, &salida) == 0 &&
        salida.accion == 0 && ui_edicion_sucia(&s));
  CHECK(ui_edicion_solicitar(&s, (UIEdSolicitud){.accion = UI_ED_NUEVO}) == 0);
  CHECK(ui_edicion_resolver(&s, cat, UI_ED_DESCARTAR, &salida) == 0 &&
        salida.accion == UI_ED_NUEVO && !ui_edicion_sucia(&s));
  CHECK(!strcmp(s.texto[2], "Julio Cortázar"));
  d.estado = 1;
  CHECK(ui_edicion_guardar(&s, cat) == 0 && !s.aviso_sync);
  CHECK(ui_edicion_editar(&s, UI_ED_ANIO, "-1") == 1);
  CHECK(ui_edicion_guardar(&s, cat) == UI_CAT_ERR_ANIO_INVALIDO &&
        s.campo_invalido == 3);
  CHECK(ui_edicion_editar(&s, UI_ED_ANIO, "1963") == 1);
  CHECK(ui_edicion_editar(&s, UI_ED_CODIGO, "   ") == 1);
  CHECK(ui_edicion_guardar(&s, cat) == UI_CAT_ERR_CAMPO_VACIO &&
        s.campo_invalido == 0);
  free(cat);
  free(d.bytes);
  puts("OK: borradores completos, revision saturada, reversión, destino "
       "capturado y estados -1/0/1/2");
  return 0;
}
