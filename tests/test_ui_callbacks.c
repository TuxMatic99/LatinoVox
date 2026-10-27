#include <stdio.h>
#include <stdlib.h>
#include "latino.h"
#include "latast.h"
#include "latmv.h"
#include "latobj.h"
#include "ui_callbacks.h"

#define EXIGIR(c) do { if (!(c)) { \
    fprintf(stderr, "FALLO: %s en %s:%d\n", #c, __FILE__, __LINE__); \
    return EXIT_FAILURE; \
} } while (0)

static int ultimo_id = -999;
static int ultima_rama = -999;
static int efectos = 0;

static void registrar_resultado(lat_mv *mv)
{
    lat_objeto *rama = latC_desapilar(mv);
    lat_objeto *id = latC_desapilar(mv);
    ultimo_id = (int)latC_checar_numerico(mv, id);
    ultima_rama = (int)latC_checar_numerico(mv, rama);
    efectos++;
}

static const lat_CReg lib_prueba[] = {
    {"registrar", registrar_resultado, 2},
    {NULL, NULL, 0}
};

int main(int argc, char **argv)
{
    EXIGIR(argc == 2);
    const char *ruta = argv[1];
    lat_mv *mv = latC_crear_mv();
    EXIGIR(mv != NULL);
    UICBContexto cb;
    EXIGIR(ui_cb_inicializar(&cb, mv) == UI_CB_OK);
    latC_abrir_liblatino(mv, "prueba", lib_prueba);
    EXIGIR(ui_cb_registrar(&cb) == UI_CB_OK);

    mv->nombre_archivo = (char *)ruta;
    mv->global->REPL = false;
    int estado = 0;
    ast *arbol = latA_analizar_arch((char *)ruta, &estado);
    EXIGIR(estado == 0 && arbol != NULL);
    lat_objeto *programa = latC_analizar(mv, arbol);
    EXIGIR(programa != NULL);
    EXIGIR(latC_llamar_funcion(mv, programa) == 0);
    /* Se mantiene el AST y el programa hasta terminar las invocaciones. */

    EXIGIR(ui_cb_despachar(&cb, "funcion_inexistente", 1) ==
           UI_CB_FUNCION_INEXISTENTE);
    for (int i = 0; i < 500; ++i) {
        int id = i % 3 == 0 ? 1 : (i % 3 == 1 ? 2 : 99);
        int rama = id == 1 ? 10 : (id == 2 ? 20 : 90);
        int anteriores = efectos;
        UICBResultado resultado = ui_cb_despachar(&cb, "__ui_despachar", id);
        if (resultado != UI_CB_OK) {
            fprintf(stderr, "Despacho %d: codigo %d, pila=%d, ctx=%d\n",
                    i, (int)resultado, mv->ptrpila, mv->ptrctx);
            return EXIT_FAILURE;
        }
        EXIGIR(ultimo_id == id);
        EXIGIR(ultima_rama == rama);
        EXIGIR(efectos == anteriores + 1);
    }
    EXIGIR(cb.llamadas == 500);
    EXIGIR(cb.exitos == 500);
    EXIGIR(cb.errores == 0);
    EXIGIR(!cb.inutilizada);

    ui_cb_desregistrar(&cb);
    latO_destruir(mv, programa);
    latA_destruir(arbol);
    latC_destruir_mv(mv);
    puts("OK: 500 callbacks correctos y 500 efectos unicos");
    return EXIT_SUCCESS;
}
