#include "ui_contenedores.h"
#include <math.h>
#include <stdio.h>

static int fallos = 0;
#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr,"FALLO linea %d: %s\n", __LINE__, #expr); fallos++; \
} } while (0)

int main(void)
{
    UIContContexto c;
    UIContElemento widgets[6];
    float espacios[7];
    CHECK(ui_cont_inicializar(&c, widgets, 6, espacios) == UI_CONT_OK);
    CHECK(ui_cont_fin(&c) == UI_CONT_NO_ABIERTO);
    CHECK(ui_cont_iniciar_fila(&c, NAN) == UI_CONT_ARGUMENTO_INVALIDO);
    CHECK(ui_cont_iniciar_fila(&c, -1) == UI_CONT_ARGUMENTO_INVALIDO);
    CHECK(ui_cont_iniciar_fila(&c, 8) == UI_CONT_OK);
    CHECK(ui_cont_iniciar_fila(&c, 8) == UI_CONT_YA_ABIERTO);
    CHECK(ui_cont_agregar(&c, 50) == UI_CONT_OK);
    CHECK(ui_cont_agregar(&c, 51) == UI_CONT_OK);
    CHECK(ui_cont_grupo_de(&c, 0) == 1 && ui_cont_grupo_de(&c, 1) == 1);
    CHECK(c.elementos[0].id_widget == 50 && c.elementos[1].id_widget == 51);
    CHECK(c.espaciados[1] == 8);
    CHECK(ui_cont_fin(&c) == UI_CONT_OK);
    CHECK(ui_cont_agregar(&c, 52) == UI_CONT_OK);
    CHECK(ui_cont_grupo_de(&c, 2) == 0);
    CHECK(ui_cont_iniciar_fila(&c, 5) == UI_CONT_OK);
    CHECK(ui_cont_fin(&c) == UI_CONT_OK); /* fila vacia */
    CHECK(ui_cont_iniciar_fila(&c, 2) == UI_CONT_OK);
    CHECK(ui_cont_agregar(&c, 60) == UI_CONT_OK); /* un solo boton */
    CHECK(ui_cont_fin(&c) == UI_CONT_OK);
    CHECK(ui_cont_grupo_de(&c, 3) == 3);
    CHECK(ui_cont_agregar(&c, 61) == UI_CONT_OK);
    CHECK(ui_cont_agregar(&c, 62) == UI_CONT_OK);
    CHECK(ui_cont_agregar(&c, 63) == UI_CONT_CAPACIDAD_AGOTADA);
    puts(fallos ? "Pruebas fallidas" : "OK: contenedores planos");
    return fallos ? 1 : 0;
}
