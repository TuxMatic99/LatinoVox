#include "ui_catalogo.h"
#include "ui_utf8.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t longitud_acotada(const char *s, size_t cap) {
    size_t n = 0;
    if (!s)
        return cap;
    while (n < cap && s[n])
        ++n;
    return n;
}

static ui_cat_resultado_t campo(const char *s, size_t cap) {
    if (!s)
        return UI_CAT_ERR_ARGUMENTO;
    size_t n = longitud_acotada(s, cap);
    if (!n)
        return UI_CAT_ERR_CAMPO_VACIO;
    if (n == cap)
        return UI_CAT_ERR_LONGITUD;
    if (!ui_utf8_validar(s, NULL))
        return UI_CAT_ERR_CARACTER_ESPECIAL;
    bool visible = false;
    for (size_t i = 0; i < n; ++i) {
        unsigned char c = (unsigned char)s[i];
        if (c < 0x20 || c == 0x7f ||
            (c == 0xc2 && i + 1 < n && (unsigned char)s[i + 1] >= 0x80 &&
             (unsigned char)s[i + 1] <= 0x9f))
            return UI_CAT_ERR_CARACTER_ESPECIAL;
        if (c != ' ')
            visible = true;
    }
    return visible ? UI_CAT_OK : UI_CAT_ERR_CAMPO_VACIO;
}

static bool entero_span(const char *s, size_t n, int maximo, bool cero, int *out) {
    if (!s || !n || !out || maximo < 0 || (n > 1 && s[0] == '0'))
        return false;
    int v = 0;
    for (size_t i = 0; i < n; ++i) {
        if (s[i] < '0' || s[i] > '9')
            return false;
        int d = s[i] - '0';
        if (v > maximo / 10 || (v == maximo / 10 && d > maximo % 10))
            return false;
        v = v * 10 + d;
    }
    if (!cero && v == 0)
        return false;
    *out = v;
    return true;
}

bool ui_catalogo_entero(const char *s, int maximo, int *out) {
    size_t n = longitud_acotada(s, 32);
    return n < 32 && entero_span(s, n, maximo, false, out);
}

bool ui_catalogo_inicializar_con_io(ui_catalogo_t *cat, ui_catalogo_io_t io) {
    if (!cat)
        return false;
    memset(cat, 0, sizeof *cat);
    cat->siguiente_id = 1;
    cat->bloqueado_por_corrupcion = true; /* hasta abrir el archivo */
    if (!io.leer || !io.escribir || !io.liberar)
        return false;
    cat->io = io;
    return true;
}

void ui_catalogo_inicializar(ui_catalogo_t *cat) {
    (void)ui_catalogo_inicializar_con_io(cat, ui_catalogo_io_archivo());
}

ui_cat_resultado_t ui_catalogo_validar(const ui_catalogo_t *cat) {
    if (!cat || cat->total > UI_CAT_MAX_LIBROS || cat->siguiente_id < 1)
        return UI_CAT_ERR_ARCHIVO_CORRUPTO;
    for (size_t i = 0; i < cat->total; ++i) {
        const ui_libro_t *b = &cat->libros[i];
        if (b->id_interno < 1 || b->id_interno >= cat->siguiente_id)
            return UI_CAT_ERR_ARCHIVO_CORRUPTO;
        ui_cat_resultado_t r = campo(b->codigo, sizeof b->codigo);
        if (r != UI_CAT_OK)
            return r;
        r = campo(b->titulo, sizeof b->titulo);
        if (r != UI_CAT_OK)
            return r;
        r = campo(b->autor, sizeof b->autor);
        if (r != UI_CAT_OK)
            return r;
        if (b->anio < UI_CAT_ANIO_MIN || b->anio > UI_CAT_ANIO_MAX)
            return UI_CAT_ERR_ANIO_INVALIDO;
        for (size_t j = 0; j < i; ++j) {
            if (cat->libros[j].id_interno == b->id_interno)
                return UI_CAT_ERR_ARCHIVO_CORRUPTO;
            if (!strcmp(cat->libros[j].codigo, b->codigo))
                return UI_CAT_ERR_CODIGO_DUPLICADO;
        }
    }
    return UI_CAT_OK;
}

ui_cat_resultado_t ui_catalogo_serializar(const ui_catalogo_t *cat, char **out, size_t *len) {
    if (out)
        *out = NULL;
    if (len)
        *len = 0;
    if (!out || !len)
        return UI_CAT_ERR_ARGUMENTO;
    ui_cat_resultado_t r = ui_catalogo_validar(cat);
    if (r != UI_CAT_OK)
        return r;
    /* Los limites del esquema caben en 64 KiB incluso con IDs de 10 digitos.
       Una sola reserva en heap; ninguna cadena truncada llega al escritor. */
    char *s = malloc(UI_CAT_ARCHIVO_MAX + 1u);
    if (!s)
        return UI_CAT_ERR_MEMORIA;
    int n = snprintf(s, UI_CAT_ARCHIVO_MAX + 1u, "%s\t%d\t%zu\n", UI_CAT_CABECERA_V1,
                     cat->siguiente_id, cat->total);
    if (n < 0 || (size_t)n > UI_CAT_ARCHIVO_MAX) {
        free(s);
        return UI_CAT_ERR_CAPACIDAD;
    }
    size_t usados = (size_t)n;
    for (size_t i = 0; i < cat->total; ++i) {
        const ui_libro_t *b = &cat->libros[i];
        size_t disponible = UI_CAT_ARCHIVO_MAX + 1u - usados;
        n = snprintf(s + usados, disponible, "%d\t%s\t%s\t%s\t%d\n", b->id_interno, b->codigo,
                     b->titulo, b->autor, b->anio);
        if (n < 0 || (size_t)n >= disponible) {
            free(s);
            return UI_CAT_ERR_CAPACIDAD;
        }
        usados += (size_t)n;
    }
    *out = s;
    *len = usados;
    return UI_CAT_OK;
}

typedef struct {
    const char *p;
    size_t n;
} tramo;
static bool columnas(tramo linea, tramo *out, size_t cantidad) {
    for (size_t i = 0; i < cantidad; ++i) {
        const char *tab = memchr(linea.p, '\t', linea.n);
        if ((i + 1 < cantidad) != (tab != NULL))
            return false;
        size_t n = tab ? (size_t)(tab - linea.p) : linea.n;
        out[i] = (tramo){linea.p, n};
        if (!n)
            return false;
        if (tab) {
            linea.p = tab + 1;
            linea.n -= n + 1;
        }
    }
    return true;
}

static bool copiar_campo(char *out, size_t cap, tramo s) {
    if (!s.n || s.n >= cap)
        return false;
    memcpy(out, s.p, s.n);
    out[s.n] = '\0';
    return campo(out, cap) == UI_CAT_OK;
}

static ui_cat_resultado_t decodificar(ui_catalogo_t *cat, const char *s, size_t n) {
    if (!s || !n || n > UI_CAT_ARCHIVO_MAX || memchr(s, '\0', n) || s[n - 1] != '\n')
        return UI_CAT_ERR_ARCHIVO_CORRUPTO;
    const char *fin = memchr(s, '\n', n);
    tramo h[3];
    int total;
    if (!fin || !columnas((tramo){s, (size_t)(fin - s)}, h, 3) ||
        h[0].n != sizeof(UI_CAT_CABECERA_V1) - 1 || memcmp(h[0].p, UI_CAT_CABECERA_V1, h[0].n) ||
        !entero_span(h[1].p, h[1].n, INT_MAX, false, &cat->siguiente_id) ||
        !entero_span(h[2].p, h[2].n, UI_CAT_MAX_LIBROS, true, &total))
        return UI_CAT_ERR_ARCHIVO_CORRUPTO;
    size_t offset = (size_t)(fin - s) + 1;
    cat->total = 0;
    while (offset < n) {
        if (cat->total >= (size_t)total)
            return UI_CAT_ERR_ARCHIVO_CORRUPTO;
        fin = memchr(s + offset, '\n', n - offset);
        tramo c[5];
        if (!fin || !columnas((tramo){s + offset, (size_t)(fin - s - offset)}, c, 5))
            return UI_CAT_ERR_ARCHIVO_CORRUPTO;
        ui_libro_t *b = &cat->libros[cat->total];
        if (!entero_span(c[0].p, c[0].n, UI_CAT_ID_MAX, false, &b->id_interno) ||
            !entero_span(c[4].p, c[4].n, UI_CAT_ANIO_MAX, false, &b->anio) ||
            !copiar_campo(b->codigo, sizeof b->codigo, c[1]) ||
            !copiar_campo(b->titulo, sizeof b->titulo, c[2]) ||
            !copiar_campo(b->autor, sizeof b->autor, c[3]))
            return UI_CAT_ERR_ARCHIVO_CORRUPTO;
        ++cat->total;
        offset = (size_t)(fin - s) + 1;
    }
    return cat->total == (size_t)total && ui_catalogo_validar(cat) == UI_CAT_OK
               ? UI_CAT_OK
               : UI_CAT_ERR_ARCHIVO_CORRUPTO;
}

ui_cat_resultado_t ui_catalogo_cargar_archivo(ui_catalogo_t *cat, const char *ruta) {
    size_t nr = longitud_acotada(ruta, UI_CAT_RUTA_CAP);
    if (!cat || !nr || nr == UI_CAT_RUTA_CAP || !cat->io.leer || !cat->io.liberar ||
        (cat->abierto && (longitud_acotada(cat->ruta, sizeof cat->ruta) == sizeof cat->ruta ||
                          strcmp(cat->ruta, ruta))))
        return UI_CAT_ERR_ARGUMENTO;
    char *s = NULL;
    size_t n = 0;
    int estado = cat->io.leer(cat->io.contexto, ruta, &s, &n);
    ui_cat_resultado_t r = UI_CAT_ERR_LECTURA;
    ui_catalogo_t *cand = NULL;
    if ((estado == 0 && !cat->abierto && !s && !n) || estado == 1) {
        cand = calloc(1, sizeof *cand);
        if (!cand)
            r = UI_CAT_ERR_MEMORIA;
        else {
            cand->siguiente_id = 1;
            r = estado == 0 ? UI_CAT_OK : decodificar(cand, s, n);
        }
    }
    if (s)
        cat->io.liberar(cat->io.contexto, s);
    if (r == UI_CAT_OK) {
        cand->io = cat->io;
        memcpy(cand->ruta, ruta, nr + 1);
        cand->abierto = true;
        *cat = *cand;
    } else
        cat->bloqueado_por_corrupcion = true;
    free(cand);
    return r;
}

static ui_cat_resultado_t precondicion(const ui_catalogo_t *cat, const char *ruta) {
    if (!cat || !ruta)
        return UI_CAT_ERR_ARGUMENTO;
    if (!cat->abierto)
        return cat->bloqueado_por_corrupcion ? UI_CAT_ERR_NO_ABIERTO : UI_CAT_ERR_ARGUMENTO;
    if (cat->bloqueado_por_corrupcion)
        return UI_CAT_ERR_ARCHIVO_CORRUPTO;
    if (!*ruta || longitud_acotada(ruta, UI_CAT_RUTA_CAP) == UI_CAT_RUTA_CAP ||
        longitud_acotada(cat->ruta, sizeof cat->ruta) == sizeof cat->ruta ||
        strcmp(cat->ruta, ruta) || !cat->io.escribir)
        return UI_CAT_ERR_ARGUMENTO;
    return ui_catalogo_validar(cat);
}

static ui_cat_resultado_t entrada(const char *codigo, const char *titulo, const char *autor,
                                  int anio) {
    ui_cat_resultado_t r = campo(codigo, UI_CAT_CODIGO_CAP);
    if (r)
        return r;
    r = campo(titulo, UI_CAT_TITULO_CAP);
    if (r)
        return r;
    r = campo(autor, UI_CAT_AUTOR_CAP);
    if (r)
        return r;
    return anio >= UI_CAT_ANIO_MIN && anio <= UI_CAT_ANIO_MAX ? UI_CAT_OK
                                                              : UI_CAT_ERR_ANIO_INVALIDO;
}

static ui_cat_resultado_t confirmar(ui_catalogo_t *cat, ui_catalogo_t *cand) {
    char *s = NULL;
    size_t n = 0;
    ui_cat_resultado_t r = ui_catalogo_serializar(cand, &s, &n);
    if (r)
        return r;
    int estado = cat->io.escribir(cat->io.contexto, cat->ruta, s, n);
    free(s);
    if (estado == UI_ATOMICO_CONFIRMADO || estado == UI_ATOMICO_SIN_SYNC_FINAL) {
        *cat = *cand;
        return estado == UI_ATOMICO_CONFIRMADO ? UI_CAT_OK : UI_CAT_ADVERTENCIA_SYNC;
    }
    if (estado == UI_ATOMICO_NO_REEMPLAZADO)
        return UI_CAT_ERR_ESCRITURA_ESTADO_0;
    if (estado == UI_ATOMICO_NO_SOPORTADO)
        return UI_CAT_ERR_NO_SOPORTADO;
    cat->bloqueado_por_corrupcion = true; /* resultado ajeno al contrato: recargar */
    return UI_CAT_ERR_ESTADO_IO;
}

ui_cat_resultado_t ui_catalogo_registrar(ui_catalogo_t *cat, const char *ruta, const char *codigo,
                                         const char *titulo, const char *autor, int anio,
                                         int *id_out) {
    ui_cat_resultado_t r = precondicion(cat, ruta);
    if (r)
        return r;
    r = entrada(codigo, titulo, autor, anio);
    if (r)
        return r;
    if (cat->total == UI_CAT_MAX_LIBROS)
        return UI_CAT_ERR_CAPACIDAD;
    if (cat->siguiente_id > UI_CAT_ID_MAX)
        return UI_CAT_ERR_ID_AGOTADO;
    for (size_t i = 0; i < cat->total; ++i)
        if (!strcmp(cat->libros[i].codigo, codigo))
            return UI_CAT_ERR_CODIGO_DUPLICADO;
    ui_catalogo_t *cand = malloc(sizeof *cand);
    if (!cand)
        return UI_CAT_ERR_MEMORIA;
    *cand = *cat;
    ui_libro_t *b = &cand->libros[cand->total++];
    memset(b, 0, sizeof *b);
    int id = cand->siguiente_id++;
    b->id_interno = id;
    b->anio = anio;
    strcpy(b->codigo, codigo);
    strcpy(b->titulo, titulo);
    strcpy(b->autor, autor);
    r = confirmar(cat, cand);
    free(cand);
    if ((r == UI_CAT_OK || r == UI_CAT_ADVERTENCIA_SYNC) && id_out)
        *id_out = id;
    return r;
}

ui_cat_resultado_t ui_catalogo_actualizar(ui_catalogo_t *cat, const char *ruta, int id,
                                          const char *codigo, const char *titulo, const char *autor,
                                          int anio) {
    ui_cat_resultado_t r = precondicion(cat, ruta);
    if (r)
        return r;
    r = entrada(codigo, titulo, autor, anio);
    if (r)
        return r;
    size_t idx = cat->total;
    for (size_t i = 0; i < cat->total; ++i) {
        if (cat->libros[i].id_interno == id)
            idx = i;
        else if (!strcmp(cat->libros[i].codigo, codigo))
            return UI_CAT_ERR_CODIGO_DUPLICADO;
    }
    if (idx == cat->total)
        return UI_CAT_ERR_NO_ENCONTRADO;
    ui_catalogo_t *cand = malloc(sizeof *cand);
    if (!cand)
        return UI_CAT_ERR_MEMORIA;
    *cand = *cat;
    ui_libro_t *b = &cand->libros[idx];
    /* Entradas pueden ser referencias prestadas del catalogo activo. */
    strcpy(b->codigo, codigo);
    strcpy(b->titulo, titulo);
    strcpy(b->autor, autor);
    b->anio = anio;
    r = confirmar(cat, cand);
    free(cand);
    return r;
}

ui_cat_resultado_t ui_catalogo_eliminar(ui_catalogo_t *cat, const char *ruta, int id) {
    ui_cat_resultado_t r = precondicion(cat, ruta);
    if (r)
        return r;
    size_t idx = cat->total;
    for (size_t i = 0; i < cat->total; ++i)
        if (cat->libros[i].id_interno == id) {
            idx = i;
            break;
        }
    if (idx == cat->total)
        return UI_CAT_ERR_NO_ENCONTRADO;
    ui_catalogo_t *cand = malloc(sizeof *cand);
    if (!cand)
        return UI_CAT_ERR_MEMORIA;
    *cand = *cat;
    memmove(&cand->libros[idx], &cand->libros[idx + 1],
            (cand->total - idx - 1) * sizeof cand->libros[0]);
    --cand->total;
    memset(&cand->libros[cand->total], 0, sizeof cand->libros[0]);
    r = confirmar(cat, cand);
    free(cand);
    return r;
}

const ui_libro_t *ui_catalogo_buscar_por_id(const ui_catalogo_t *cat, int id) {
    if (!cat || id < 1 || ui_catalogo_validar(cat) != UI_CAT_OK)
        return NULL;
    for (size_t i = 0; i < cat->total; ++i)
        if (cat->libros[i].id_interno == id)
            return &cat->libros[i];
    return NULL;
}
const ui_libro_t *ui_catalogo_buscar_por_codigo(const ui_catalogo_t *cat, const char *s) {
    if (campo(s, UI_CAT_CODIGO_CAP) != UI_CAT_OK || ui_catalogo_validar(cat) != UI_CAT_OK)
        return NULL;
    for (size_t i = 0; i < cat->total; ++i)
        if (!strcmp(cat->libros[i].codigo, s))
            return &cat->libros[i];
    return NULL;
}

const char *ui_catalogo_mensaje(ui_cat_resultado_t r) {
    switch (r) {
    case UI_CAT_OK:
        return "Operacion confirmada.";
    case UI_CAT_ADVERTENCIA_SYNC:
        return "Archivo reemplazado; sincronizacion final sin confirmar.";
    case UI_CAT_ERR_CAMPO_VACIO:
        return "Completa codigo, titulo y autor.";
    case UI_CAT_ERR_CARACTER_ESPECIAL:
        return "Texto UTF-8 invalido o con caracteres de control.";
    case UI_CAT_ERR_ANIO_INVALIDO:
        return "El anio debe ser un entero entre 1 y 9999.";
    case UI_CAT_ERR_CODIGO_DUPLICADO:
        return "El codigo ya pertenece a otro libro.";
    case UI_CAT_ERR_NO_ENCONTRADO:
        return "Libro no encontrado.";
    case UI_CAT_ERR_ARCHIVO_CORRUPTO:
        return "Catalogo invalido. Guardado bloqueado.";
    case UI_CAT_ERR_ESCRITURA_ESTADO_0:
        return "No se guardo; se conserva el catalogo anterior.";
    case UI_CAT_ERR_CAPACIDAD:
        return "Se alcanzo la capacidad del catalogo.";
    case UI_CAT_ERR_LONGITUD:
        return "Campo demasiado largo (limites en bytes UTF-8).";
    case UI_CAT_ERR_ID_AGOTADO:
        return "Se agotaron los identificadores; no se reutilizaran.";
    case UI_CAT_ERR_MEMORIA:
        return "No hay memoria suficiente para la operacion.";
    case UI_CAT_ERR_LECTURA:
        return "Error de lectura o archivo desaparecido. Guardado bloqueado.";
    case UI_CAT_ERR_NO_SOPORTADO:
        return "Guardado atomico no disponible en esta plataforma.";
    case UI_CAT_ERR_ESTADO_IO:
        return "Resultado de E/S desconocido. Recarga antes de guardar.";
    case UI_CAT_ERR_BLOQUEO_CONCURRENTE:
        return "Catalogo ocupado por otra instancia. Cierra y vuelve a abrir.";
    case UI_CAT_ERR_NO_ABIERTO:
        return "Abre un catalogo valido antes de guardar.";
    default:
        return "Argumentos invalidos.";
    }
}
