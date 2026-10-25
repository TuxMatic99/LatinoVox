#ifndef UI_CONTENEDORES_H
#define UI_CONTENEDORES_H

#include <stddef.h>

/* Primera version: filas planas, sin anidamiento.
 * Los identificadores de widget/evento son independientes del grupo. */
typedef enum {
    UI_CONT_OK = 0,
    UI_CONT_ARGUMENTO_INVALIDO,
    UI_CONT_YA_ABIERTO,
    UI_CONT_NO_ABIERTO,
    UI_CONT_CAPACIDAD_AGOTADA
} UIContResultado;

typedef struct {
    size_t id_grupo; /* 0 significa fuera de fila */
    int id_widget;   /* ID de evento, se conserva intacto */
} UIContElemento;

typedef struct {
    UIContElemento *elementos;
    size_t capacidad;
    size_t cantidad;
    size_t grupo_actual;
    size_t siguiente_grupo;
    float *espaciados; /* indices 1..capacidad, arreglo de capacidad+1 */
} UIContContexto;

UIContResultado ui_cont_inicializar(UIContContexto *ctx,
                                    UIContElemento *elementos,
                                    size_t capacidad, float *espaciados);
UIContResultado ui_cont_iniciar_fila(UIContContexto *ctx, float espacio);
UIContResultado ui_cont_agregar(UIContContexto *ctx, int id_widget);
UIContResultado ui_cont_fin(UIContContexto *ctx);
size_t ui_cont_grupo_de(const UIContContexto *ctx, size_t indice);
UIContResultado ui_cont_obtener_espaciado(
    const UIContContexto *ctx,
    size_t id_grupo,
    float *espacio
);
#endif
