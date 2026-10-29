#ifndef UI_BIBLIOTECA_H
#define UI_BIBLIOTECA_H
#include "ui_catalogo.h"
#include "latino.h"
#include "ui_runtime.h"
typedef struct UIBiblioteca UIBiblioteca;
/* Registro por VM, uso monohilo. Destruir antes de latC_destruir_mv.
   io=NULL utiliza la E/S compartida de Latino-Core; otro backend es optativo. */
UIBiblioteca *ui_biblioteca_crear(lat_mv *mv, const ui_catalogo_io_t *io);
/* Adaptador de la aplicación Biblioteca13. Otras aplicaciones no activan la
   protección y conservan su comportamiento. No crea cadenas en la VM al teclear. */
void ui_biblioteca_conectar_ui(UIBiblioteca *biblioteca, UIRuntime *ui);
void ui_biblioteca_sincronizar_ui(UIBiblioteca *biblioteca);
bool ui_biblioteca_protege_cierre(const UIBiblioteca *biblioteca);
void ui_biblioteca_pedir_cierre(UIBiblioteca *biblioteca);
bool ui_biblioteca_consumir_cierre(UIBiblioteca *biblioteca);
void ui_biblioteca_destruir(UIBiblioteca *biblioteca);
#endif
