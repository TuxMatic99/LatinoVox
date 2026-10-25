#include "ui_contenedores.h"
#include <math.h>
#include <string.h>

UIContResultado ui_cont_inicializar(UIContContexto *ctx,
                                    UIContElemento *elementos,
                                    size_t capacidad, float *espaciados)
{
    if (!ctx || !elementos || !espaciados || !capacidad)
        return UI_CONT_ARGUMENTO_INVALIDO;
    memset(ctx, 0, sizeof(*ctx));
    ctx->elementos = elementos;
    ctx->espaciados = espaciados;
    ctx->capacidad = capacidad;
    memset(espaciados, 0, (capacidad + 1) * sizeof(*espaciados));
    return UI_CONT_OK;
}

UIContResultado ui_cont_iniciar_fila(UIContContexto *ctx, float espacio)
{
    if (!ctx || !ctx->elementos || !ctx->espaciados ||
        !isfinite(espacio) || espacio < 0.0f || espacio > 100.0f)
        return UI_CONT_ARGUMENTO_INVALIDO;
    if (ctx->grupo_actual) return UI_CONT_YA_ABIERTO;
    if (ctx->siguiente_grupo >= ctx->capacidad)
        return UI_CONT_CAPACIDAD_AGOTADA;
    ctx->grupo_actual = ++ctx->siguiente_grupo;
    ctx->espaciados[ctx->grupo_actual] = espacio;
    return UI_CONT_OK;
}

UIContResultado ui_cont_agregar(UIContContexto *ctx, int id_widget)
{
    if (!ctx || !ctx->elementos) return UI_CONT_ARGUMENTO_INVALIDO;
    if (ctx->cantidad >= ctx->capacidad) return UI_CONT_CAPACIDAD_AGOTADA;
    ctx->elementos[ctx->cantidad++] =
        (UIContElemento){ctx->grupo_actual, id_widget};
    return UI_CONT_OK;
}

UIContResultado ui_cont_fin(UIContContexto *ctx)
{
    if (!ctx) return UI_CONT_ARGUMENTO_INVALIDO;
    if (!ctx->grupo_actual) return UI_CONT_NO_ABIERTO;
    ctx->grupo_actual = 0;
    return UI_CONT_OK;
}

size_t ui_cont_grupo_de(const UIContContexto *ctx, size_t indice)
{
    if (!ctx || !ctx->elementos || indice >= ctx->cantidad) return 0;
    return ctx->elementos[indice].id_grupo;
}
