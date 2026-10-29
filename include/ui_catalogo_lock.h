#ifndef UI_CATALOGO_LOCK_H
#define UI_CATALOGO_LOCK_H
#include "ui_catalogo.h"
/* Linux, directorio local privado y estable; escritores cooperantes.
   Adquirir ANTES de cargar, conservar durante toda la sesión. El archivo
   sidecar nunca se elimina al liberar: su inode es el punto de encuentro
   permanente. */
typedef struct {
  int fd;
  char ruta[UI_CAT_RUTA_CAP];
} UICatalogoLock;
void ui_catalogo_lock_inicializar(UICatalogoLock *lock);
ui_cat_resultado_t ui_catalogo_lock_adquirir(UICatalogoLock *lock,
                                             const char *ruta);
void ui_catalogo_lock_liberar(UICatalogoLock *lock);
#endif
