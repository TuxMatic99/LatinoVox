#include "ui_contenedores.h"
#include <math.h>
#include <stdio.h>

static int fallos = 0;
#define COMPROBAR(cond, mensaje) do { \
    if (!(cond)) { \
        fprintf(stderr, "FALLO linea %d: %s\n", __LINE__, (mensaje)); \
        ++fallos; \
    } \
} while (0)

static void probar_ciclo_de_vida(void)
{
    UIContContexto ctx = {0};
    UIContElemento elementos[4] = {0};
    float espacios[5] = {0};

    COMPROBAR(ui_cont_inicializar(&ctx, elementos, 4, espacios) == UI_CONT_OK,
              "Inicializacion");
    COMPROBAR(ui_cont_fin(&ctx) == UI_CONT_NO_ABIERTO,
              "Cerrar sin abrir devuelve error");
    COMPROBAR(ui_cont_iniciar_fila(&ctx, 8.0f) == UI_CONT_OK,
              "Abrir fila");
    COMPROBAR(ui_cont_iniciar_fila(&ctx, 4.0f) == UI_CONT_YA_ABIERTO,
              "No se admite anidamiento");
    COMPROBAR(ctx.grupo_actual == 1 && espacios[1] == 8.0f,
              "Un intento fallido no altera la fila actual");
    COMPROBAR(ui_cont_agregar(&ctx, 50) == UI_CONT_OK,
              "Registrar primer boton");
    COMPROBAR(ui_cont_agregar(&ctx, 51) == UI_CONT_OK,
              "Registrar segundo boton");
    COMPROBAR(ui_cont_fin(&ctx) == UI_CONT_OK,
              "Cerrar fila");
    COMPROBAR(ui_cont_agregar(&ctx, 52) == UI_CONT_OK,
              "Registrar boton fuera de fila");
    COMPROBAR(ui_cont_grupo_de(&ctx, 0) == 1 &&
              ui_cont_grupo_de(&ctx, 1) == 1 &&
              ui_cont_grupo_de(&ctx, 2) == 0,
              "Pertenencia correcta de botones");
    COMPROBAR(elementos[0].id_widget == 50 &&
              elementos[1].id_widget == 51 &&
              elementos[2].id_widget == 52,
              "Se conservan los IDs de eventos");

    /* Simula reconstruir la UI tras recargar el script. */
    COMPROBAR(ui_cont_inicializar(&ctx, elementos, 4, espacios) == UI_CONT_OK,
              "Reinicializacion");
    COMPROBAR(ctx.cantidad == 0 && ctx.grupo_actual == 0 &&
              ctx.siguiente_grupo == 0 && espacios[1] == 0.0f,
              "Se reinician contador, fila y espaciados");
    COMPROBAR(ui_cont_iniciar_fila(&ctx, 12.0f) == UI_CONT_OK &&
              ctx.grupo_actual == 1,
              "El primer grupo vuelve a tener ID 1");
    COMPROBAR(ui_cont_agregar(&ctx, 70) == UI_CONT_OK &&
              ui_cont_grupo_de(&ctx, 0) == 1,
              "Registro posterior a reinicializacion");
}

static void probar_capacidad(void)
{
    UIContContexto ctx = {0};
    UIContElemento elementos[2] = {0};
    float espacios[3] = {0};
    COMPROBAR(ui_cont_inicializar(&ctx, elementos, 2, espacios) == UI_CONT_OK,
              "Inicializacion con capacidad 2");
    COMPROBAR(ui_cont_agregar(&ctx, 101) == UI_CONT_OK,
              "Primer elemento");
    COMPROBAR(ui_cont_agregar(&ctx, 102) == UI_CONT_OK,
              "Segundo elemento");
    COMPROBAR(ui_cont_agregar(&ctx, 103) == UI_CONT_CAPACIDAD_AGOTADA,
              "Capacidad agotada detectada");
    COMPROBAR(ctx.cantidad == 2 && elementos[0].id_widget == 101 &&
              elementos[1].id_widget == 102,
              "El error conserva datos registrados");
    COMPROBAR(ui_cont_grupo_de(&ctx, 2) == 0,
              "Indice fuera de rango seguro");
}

static void probar_argumentos(void)
{
    UIContContexto ctx = {0};
    UIContElemento elementos[2] = {0};
    float espacios[3] = {0};
    COMPROBAR(ui_cont_inicializar(NULL, elementos, 2, espacios) == UI_CONT_ARGUMENTO_INVALIDO,
              "Contexto nulo");
    COMPROBAR(ui_cont_inicializar(&ctx, NULL, 2, espacios) == UI_CONT_ARGUMENTO_INVALIDO,
              "Elementos nulos");
    COMPROBAR(ui_cont_inicializar(&ctx, elementos, 0, espacios) == UI_CONT_ARGUMENTO_INVALIDO,
              "Capacidad cero");
    COMPROBAR(ui_cont_inicializar(&ctx, elementos, 2, espacios) == UI_CONT_OK,
              "Contexto valido");
    COMPROBAR(ui_cont_iniciar_fila(&ctx, NAN) == UI_CONT_ARGUMENTO_INVALIDO,
              "Rechazar NaN");
    COMPROBAR(ui_cont_iniciar_fila(&ctx, INFINITY) == UI_CONT_ARGUMENTO_INVALIDO,
              "Rechazar infinito");
    COMPROBAR(ui_cont_iniciar_fila(&ctx, 101.0f) == UI_CONT_ARGUMENTO_INVALIDO,
              "Rechazar espacio mayor al limite");
    COMPROBAR(ctx.grupo_actual == 0 && ctx.siguiente_grupo == 0,
              "Entradas invalidas no modifican los grupos");
}



static int fallos_adicionales = 0;

static void comprobar(int condicion, const char *descripcion)
{
    if (condicion) {
        printf("[OK] %s\n", descripcion);
    } else {
        fprintf(stderr, "[ERROR] %s\n", descripcion);
        fallos_adicionales++;
    }
}


int main(void)
{



    probar_ciclo_de_vida();
    probar_capacidad();
    probar_argumentos();
    if (fallos) {
        fprintf(stderr, "ERROR: %d comprobaciones fallaron\n", fallos);
        return 1;
    }
    puts("OK: estabilidad del modulo de contenedores");

{
    UIContContexto contexto = {0};
    UIContElemento elementos[4] = {0};
    float espaciados[5] = {0};
    float espacio = -1.0f;

    comprobar(
        ui_cont_inicializar(
            &contexto, elementos, 4, espaciados
        ) == UI_CONT_OK,
        "Inicializar contexto para consulta"
    );

    comprobar(
        ui_cont_iniciar_fila(&contexto, 12.0f)
            == UI_CONT_OK,
        "Abrir fila con separacion de 12"
    );

    comprobar(
        ui_cont_obtener_espaciado(
            &contexto, 1, &espacio
        ) == UI_CONT_OK &&
        espacio == 12.0f,
        "Consultar separacion de la fila"
    );

    comprobar(
        ui_cont_obtener_espaciado(
            &contexto, 99, &espacio
        ) == UI_CONT_ARGUMENTO_INVALIDO,
        "Rechazar identificador inexistente"
    );
}


    return fallos_adicionales == 0 ? 0 : 1;


}
