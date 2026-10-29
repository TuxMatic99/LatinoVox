#include "ui_catalogo.h"
#include "latino.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#ifdef __linux__
#include <sys/stat.h>
#endif

static int leer(void *ctx, const char *ruta, char **salida, size_t *n) {
    (void)ctx;
    *salida = NULL;
    *n = 0;
#ifdef __linux__
    /* Directorio privado estable: no seguir un enlace, dispositivo ni FIFO.
       No constituye proteccion frente a un escritor concurrente malicioso. */
    struct stat st;
    if (lstat(ruta, &st) == 0) {
        if (!S_ISREG(st.st_mode) || st.st_size < 0 ||
            (unsigned long long)st.st_size > UI_CAT_ARCHIVO_MAX)
            return -1;
    } else if (errno != ENOENT)
        return -1;
#endif
    int estado = -1;
    char *s = latF_leer_texto_seguro(ruta, &estado);
    if (s) {
        size_t len = strlen(s);
        if (len > UI_CAT_ARCHIVO_MAX) {
            free(s);
            return -1;
        }
        *salida = s;
        *n = len;
    }
    return estado;
}
static int escribir(void *ctx, const char *ruta, const char *s, size_t n) {
    (void)ctx;
    if (!s || n > UI_CAT_ARCHIVO_MAX || strlen(s) != n)
        return UI_ATOMICO_NO_REEMPLAZADO;
    return latF_escribir_atomico(ruta, s);
}
static void liberar(void *ctx, char *s) {
    (void)ctx;
    free(s);
}
ui_catalogo_io_t ui_catalogo_io_archivo(void) {
    return (ui_catalogo_io_t){NULL, leer, escribir, liberar};
}
