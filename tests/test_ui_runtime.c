#include "ui_runtime.h"
#include <stdio.h>
#define V(x) do { if (!(x)) { fprintf(stderr, "FALLO en linea %d: %s\n", __LINE__, #x); return 1; } } while (0)
int main(void) {
    UIRuntime ui;
    V(lui_inicializar(&ui));
    V(lui_titulo(&ui, "Prueba"));
    V(lui_iniciar_contenedor(&ui, UI_NODO_COLUMNA, 12));
    V(lui_agregar_widget(&ui, 10000, "Titulo"));
    V(lui_iniciar_contenedor(&ui, UI_NODO_FILA, 10));
    V(lui_agregar_widget(&ui, 1, "Guardar"));
    V(lui_agregar_widget(&ui, 2, "Cancelar"));
    V(lui_fin_contenedor(&ui));
    V(lui_fin_contenedor(&ui));
    V(lui_validar(&ui));
    V(ui.arbol.cantidad == 5);
    V(lui_calcular(&ui, (UIRect){0, 0, 900, 600}));
    V(ui.rectangulos[3].ancho > 0 && ui.rectangulos[4].ancho > 0);
    V(ui.rectangulos[3].x < ui.rectangulos[4].x);
    V(!lui_agregar_widget(&ui, 3, "Fuera"));
    puts("OK: runtime jerarquico");
    return 0;
}
