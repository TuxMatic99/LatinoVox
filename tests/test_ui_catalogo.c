#define _POSIX_C_SOURCE 200809L
#include "ui_catalogo.h"
#include "latino.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define CHECK(c)                                                                                   \
    do {                                                                                           \
        if (!(c)) {                                                                                \
            fprintf(stderr, "FALLO %s:%d: %s\n", __FILE__, __LINE__, #c);                          \
            exit(2);                                                                               \
        }                                                                                          \
    } while (0)

typedef struct {
    char *bytes;
    size_t n;
    int lectura, escritura;
    unsigned writes, pendientes;
} disco;
static int leer(void *p, const char *ruta, char **out, size_t *n) {
    disco *d = p;
    (void)ruta;
    *out = NULL;
    *n = 0;
    if (d->lectura != 1)
        return d->lectura;
    *out = malloc(d->n ? d->n : 1);
    CHECK(*out);
    if (d->n)
        memcpy(*out, d->bytes, d->n);
    *n = d->n;
    d->pendientes++;
    return 1;
}
static void liberar(void *p, char *s) {
    disco *d = p;
    CHECK(d->pendientes > 0);
    d->pendientes--;
    free(s);
}
static void poner(disco *d, const void *s, size_t n) {
    char *c = malloc(n ? n : 1);
    CHECK(c);
    if (n)
        memcpy(c, s, n);
    free(d->bytes);
    d->bytes = c;
    d->n = n;
    d->lectura = 1;
}
static int escribir(void *p, const char *r, const char *s, size_t n) {
    disco *d = p;
    (void)r;
    d->writes++;
    if (d->escritura == 1 || d->escritura == 2)
        poner(d, s, n);
    return d->escritura;
}
static void iniciar(ui_catalogo_t *c, disco *d) {
    *d = (disco){.escritura = 1};
    CHECK(ui_catalogo_inicializar_con_io(c, (ui_catalogo_io_t){d, leer, escribir, liberar}));
    CHECK(ui_catalogo_cargar_archivo(c, "catalogo.tsv") == UI_CAT_OK);
}
static void terminar(disco *d) {
    CHECK(d->pendientes == 0);
    free(d->bytes);
}
static ui_catalogo_t *reservar(void) {
    ui_catalogo_t *c = malloc(sizeof *c);
    CHECK(c);
    return c;
}

static void estados(void) {
    ui_catalogo_t *c = reservar(), *antes = reservar(), *rec = reservar();
    disco d;
    iniciar(c, &d);
    int id = -99;
    CHECK(ui_catalogo_registrar(c, "catalogo.tsv", "LIB-01", "Cien años de soledad",
                                "García Márquez", 1967, &id) == UI_CAT_OK &&
          id == 1);
    CHECK(ui_catalogo_registrar(c, "catalogo.tsv", "LIB-02", "Pedro Páramo", "Juan Rulfo", 1955,
                                &id) == UI_CAT_OK &&
          id == 2);
    CHECK(ui_catalogo_actualizar(c, "catalogo.tsv", 2, "LIB-02B", "El llano en llamas",
                                 "Juan Rulfo", 1953) == UI_CAT_OK);
    CHECK(c->libros[1].id_interno == 2);
    CHECK(ui_catalogo_eliminar(c, "catalogo.tsv", 1) == UI_CAT_OK);
    CHECK(ui_catalogo_registrar(c, "catalogo.tsv", "LIB-01", "Ficciones", "Borges", 1944, &id) ==
              UI_CAT_OK &&
          id == 3);
    for (int estado = -1; estado <= 2; ++estado) {
        *antes = *c;
        id = -99;
        d.escritura = estado;
        size_t n = d.n;
        char *viejo = malloc(n);
        CHECK(viejo);
        memcpy(viejo, d.bytes, n);
        ui_cat_resultado_t r = ui_catalogo_actualizar(
            c, "catalogo.tsv", 3, "LIB-01", estado == 2 ? "Aleph" : "Ficciones", "Borges", 1944);
        if (estado <= 0) {
            CHECK(r == (estado == 0 ? UI_CAT_ERR_ESCRITURA_ESTADO_0 : UI_CAT_ERR_NO_SOPORTADO));
            CHECK(!memcmp(c, antes, sizeof *c));
            CHECK(n == d.n && !memcmp(viejo, d.bytes, n));
        } else {
            CHECK(r == (estado == 2 ? UI_CAT_ADVERTENCIA_SYNC : UI_CAT_OK));
            CHECK(ui_catalogo_inicializar_con_io(rec, c->io));
            CHECK(ui_catalogo_cargar_archivo(rec, "catalogo.tsv") == UI_CAT_OK);
            CHECK(rec->total == c->total && rec->siguiente_id == c->siguiente_id);
            CHECK(!strcmp(rec->libros[1].titulo, c->libros[1].titulo));
        }
        free(viejo);
    }
    d.escritura = 0;
    *antes = *c;
    CHECK(ui_catalogo_registrar(c, "catalogo.tsv", "X", "Titulo", "Autor", 2000, &id) ==
              UI_CAT_ERR_ESCRITURA_ESTADO_0 &&
          id == -99);
    CHECK(!memcmp(c, antes, sizeof *c));
    CHECK(ui_catalogo_eliminar(c, "catalogo.tsv", 2) == UI_CAT_ERR_ESCRITURA_ESTADO_0);
    CHECK(!memcmp(c, antes, sizeof *c));
    d.escritura = 2;
    CHECK(ui_catalogo_eliminar(c, "catalogo.tsv", 2) == UI_CAT_ADVERTENCIA_SYNC);
    CHECK(ui_catalogo_eliminar(c, "catalogo.tsv", 3) == UI_CAT_ADVERTENCIA_SYNC);
    CHECK(c->total == 0 && c->siguiente_id == 4);
    CHECK(ui_catalogo_cargar_archivo(c, "catalogo.tsv") == UI_CAT_OK);
    CHECK(ui_catalogo_registrar(c, "catalogo.tsv", "N", "Nuevo", "Autor", 1, &id) ==
              UI_CAT_ADVERTENCIA_SYNC &&
          id == 4);
    *antes = *c;
    d.escritura = 99;
    CHECK(ui_catalogo_eliminar(c, "catalogo.tsv", 4) == UI_CAT_ERR_ESTADO_IO &&
          c->bloqueado_por_corrupcion);
    CHECK(c->total == antes->total && c->siguiente_id == antes->siguiente_id);
    terminar(&d);
    free(rec);
    free(antes);
    free(c);
    puts("OK: catalogo estados -1/0/1/2, disco y memoria coherentes, IDs y protocolo desconocido");
}

static void rechazar(ui_catalogo_t *c, disco *d, const void *s, size_t n) {
    ui_catalogo_t *antes = reservar();
    *antes = *c;
    poner(d, s, n);
    CHECK(ui_catalogo_cargar_archivo(c, "catalogo.tsv") != UI_CAT_OK);
    CHECK(c->bloqueado_por_corrupcion);
    CHECK(c->total == antes->total && c->siguiente_id == antes->siguiente_id);
    CHECK(!memcmp(c->libros, antes->libros, sizeof c->libros));
    unsigned writes = d->writes;
    CHECK(ui_catalogo_eliminar(c, "catalogo.tsv", 1) != UI_CAT_OK && d->writes == writes);
    CHECK(d->pendientes == 0);
    *c = *antes;
    free(antes);
}
static void lector(void) {
    ui_catalogo_t *c = reservar();
    disco d;
    iniciar(c, &d);
    CHECK(ui_catalogo_registrar(c, "catalogo.tsv", "A", "Titulo", "Autor", 2000, NULL) ==
          UI_CAT_OK);
    const char *malos[] = {"",
                           "LATINOUI-CATALOGO-V1\t2\t1\n",
                           "LATINOUI-CATALOGO-V1\t2\t1\n1\tA\tTitulo\tAutor\t2000", // sin LF
                           "LATINOUI-CATALOGO-V1\t2\t1\n1\t\tTitulo\tAutor\t2000\n",
                           "LATINOUI-CATALOGO-V1\t2\t1\n1\tA\tTitulo\tAutor\t2000\tX\n",
                           "LATINOUI-CATALOGO-V1\t2\t1\n1\tA\tTitulo\tAutor\t2e3\n",
                           "LATINOUI-CATALOGO-V1\t2\t1\n1\tA\tTitulo\tAutor\t2000.0\n",
                           "LATINOUI-CATALOGO-V1\t2\t1\n1\tA\tTitulo\tAutor\tNaN\n",
                           "LATINOUI-CATALOGO-V1\t2\t1\n1\tA\tTitulo\tAutor\t0\n",
                           "LATINOUI-CATALOGO-V1\t1\t1\n1\tA\tTitulo\tAutor\t2000\n",
                           "LATINOUI-CATALOGO-V1\t3\t2\n1\tA\tT\tA\t2000\n1\tB\tT\tA\t2000\n",
                           "LATINOUI-CATALOGO-V1\t3\t2\n1\tA\tT\tA\t2000\n2\tA\tT\tA\t2000\n",
                           "LATINOUI-CATALOGO-V1\t999999999999999999999\t0\n",
                           "LATINOUI-CATALOGO-V1\t01\t0\n",
                           "LATINOUI-CATALOGO-V1\t1\t00\n",
                           "LATINOUI-CATALOGO-V1\t1\t257\n",
                           "LATINOUI-CATALOGO-V2\t1\t0\n",
                           "LATINOUI-CATALOGO-V1\t2\t1\n1\tA\t \tAutor\t2000\n",
                           "LATINOUI-CATALOGO-V1\t1\t0\n\n"};
    for (size_t i = 0; i < sizeof malos / sizeof malos[0]; ++i)
        rechazar(c, &d, malos[i], strlen(malos[i]));
    const char oculto[] = "LATINOUI-CATALOGO-V1\t1\t0\n\0OCULTO\n";
    rechazar(c, &d, oculto, sizeof oculto - 1);
    const char *utf8[] = {"\xc0\xaf", "\xed\xa0\x80", "\xf4\x90\x80\x80", "\xf0\x80\x80\x80",
                          "\xe2\x82", "\x80",         "\xc2\x85"};
    char b[256];
    for (size_t i = 0; i < sizeof utf8 / sizeof utf8[0]; ++i) {
        int n =
            snprintf(b, sizeof b, "LATINOUI-CATALOGO-V1\t2\t1\n1\tA\t%s\tAutor\t2000\n", utf8[i]);
        CHECK(n > 0);
        rechazar(c, &d, b, (size_t)n);
    }
    char *grande = malloc(UI_CAT_ARCHIVO_MAX + 1);
    CHECK(grande);
    memset(grande, 'x', UI_CAT_ARCHIVO_MAX + 1);
    rechazar(c, &d, grande, UI_CAT_ARCHIVO_MAX + 1);
    free(grande);
    // Ausencia posterior no reinicia los IDs ni borra la memoria activa.
    d.lectura = 0;
    CHECK(ui_catalogo_cargar_archivo(c, "catalogo.tsv") == UI_CAT_ERR_LECTURA);
    CHECK(c->total == 1 && c->siguiente_id == 2 && c->bloqueado_por_corrupcion);
    terminar(&d);
    free(c);
    puts("OK: lector completo, NUL, UTF-8 estricto, campos, duplicados, limites y desaparicion");
}

static void limites(void) {
    ui_catalogo_t *c = reservar();
    disco d;
    iniciar(c, &d);
    int v = 22;
    CHECK(!ui_catalogo_entero("99999999999999999999", INT_MAX, &v) && v == 22);
    CHECK(!ui_catalogo_entero("+1", INT_MAX, &v));
    CHECK(!ui_catalogo_entero("1.0", INT_MAX, &v));
    CHECK(!ui_catalogo_entero("01", INT_MAX, &v));
    CHECK(ui_catalogo_entero("9999", 9999, &v) && v == 9999);
    char titulo[UI_CAT_TITULO_CAP], autor[UI_CAT_AUTOR_CAP], codigo[UI_CAT_CODIGO_CAP];
    memset(titulo, 't', sizeof titulo - 1);
    titulo[sizeof titulo - 1] = 0;
    memset(autor, 'a', sizeof autor - 1);
    autor[sizeof autor - 1] = 0;
    for (int i = 0; i < UI_CAT_MAX_LIBROS; ++i) {
        snprintf(codigo, sizeof codigo, "%035d", i);
        CHECK(ui_catalogo_registrar(c, "catalogo.tsv", codigo, titulo, autor, 9999, NULL) ==
              UI_CAT_OK);
    }
    CHECK(c->total == UI_CAT_MAX_LIBROS);
    CHECK(d.n < UI_CAT_ARCHIVO_MAX);
    CHECK(ui_catalogo_cargar_archivo(c, "catalogo.tsv") == UI_CAT_OK);
    CHECK(ui_catalogo_registrar(c, "catalogo.tsv", "extra", "T", "A", 1, NULL) ==
          UI_CAT_ERR_CAPACIDAD);
    c->total = UI_CAT_MAX_LIBROS + 1;
    CHECK(ui_catalogo_eliminar(c, "catalogo.tsv", 1) == UI_CAT_ERR_ARCHIVO_CORRUPTO);
    CHECK(ui_catalogo_buscar_por_id(c, 1) == NULL);
    CHECK(ui_catalogo_cargar_archivo(c, "catalogo.tsv") == UI_CAT_OK);
    memset(c->libros[0].codigo, 'x', sizeof c->libros[0].codigo);
    CHECK(ui_catalogo_buscar_por_id(c, 1) == NULL);
    CHECK(ui_catalogo_cargar_archivo(c, "catalogo.tsv") == UI_CAT_OK);
    char ultimo[120];
    int n = snprintf(ultimo, sizeof ultimo, "LATINOUI-CATALOGO-V1\t%d\t0\n", INT_MAX - 1);
    poner(&d, ultimo, (size_t)n);
    CHECK(ui_catalogo_cargar_archivo(c, "catalogo.tsv") == UI_CAT_OK);
    int id;
    CHECK(ui_catalogo_registrar(c, "catalogo.tsv", "Z", "T", "A", 1, &id) == UI_CAT_OK &&
          id == INT_MAX - 1);
    CHECK(c->siguiente_id == INT_MAX);
    CHECK(ui_catalogo_cargar_archivo(c, "catalogo.tsv") == UI_CAT_OK);
    CHECK(ui_catalogo_registrar(c, "catalogo.tsv", "Y", "T", "A", 1, NULL) ==
          UI_CAT_ERR_ID_AGOTADO);
    CHECK(ui_catalogo_eliminar(c, "catalogo.tsv", id) == UI_CAT_OK);
    CHECK(ui_catalogo_cargar_archivo(c, "catalogo.tsv") == UI_CAT_OK && c->siguiente_id == INT_MAX);
    CHECK(ui_catalogo_registrar(c, "catalogo.tsv", "Y", "T", "A", 1, NULL) ==
          UI_CAT_ERR_ID_AGOTADO);
    terminar(&d);
    free(c);
    puts("OK: 256 registros maximos, terminadores acotados y agotamiento de IDs sin overflow");
}

static void archivo_real(void) {
    char dir[] = "/tmp/latinoui-catalogo12-XXXXXX";
    CHECK(mkdtemp(dir));
    char ruta[256], enlace[256];
    snprintf(ruta, sizeof ruta, "%s/catalogo.tsv", dir);
    snprintf(enlace, sizeof enlace, "%s/enlace.tsv", dir);
    ui_catalogo_t *c = reservar();
    ui_catalogo_inicializar(c);
    CHECK(ui_catalogo_cargar_archivo(c, ruta) == UI_CAT_OK);
    CHECK(ui_catalogo_registrar(c, ruta, "LIB-1", "María Ñáñez | \\", "Autor", 2000, NULL) ==
          UI_CAT_OK);
    for (int i = 0; i < 100; ++i) {
        CHECK(ui_catalogo_actualizar(c, ruta, 1, "LIB-1", i % 2 ? "Título A" : "Título B", "Autor",
                                     2000) == UI_CAT_OK);
        CHECK(ui_catalogo_cargar_archivo(c, ruta) == UI_CAT_OK);
    }
    CHECK(c->total == 1 && c->siguiente_id == 2);
    CHECK(symlink(ruta, enlace) == 0);
    ui_catalogo_t *otro = reservar();
    ui_catalogo_inicializar(otro);
    CHECK(ui_catalogo_cargar_archivo(otro, enlace) == UI_CAT_ERR_LECTURA);
    FILE *fp = fopen(ruta, "wb");
    CHECK(fp);
    const char nulo[] = "LATINOUI-CATALOGO-V1\t1\t0\n\0OCULTO\n";
    CHECK(fwrite(nulo, 1, sizeof nulo - 1, fp) == sizeof nulo - 1);
    CHECK(fclose(fp) == 0);
    CHECK(ui_catalogo_cargar_archivo(c, ruta) == UI_CAT_ERR_LECTURA && c->total == 1);
    CHECK(ui_catalogo_eliminar(c, ruta, 1) != UI_CAT_OK);
    CHECK(unlink(enlace) == 0 && unlink(ruta) == 0 && rmdir(dir) == 0);
    free(otro);
    free(c);
    puts("OK: liblatino.so real, 100 ediciones/reaperturas, NUL y enlaces sin sobrescritura");
}
int main(int argc, char **argv) {
    CHECK(argc == 2);
    if (!strcmp(argv[1], "estados"))
        estados();
    else if (!strcmp(argv[1], "lector"))
        lector();
    else if (!strcmp(argv[1], "limites"))
        limites();
    else if (!strcmp(argv[1], "archivo"))
        archivo_real();
    else
        return 2;
    return 0;
}
