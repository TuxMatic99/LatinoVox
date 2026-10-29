#ifndef UI_EDICION_H
#define UI_EDICION_H
#include "ui_catalogo.h"
#include <stdint.h>

/* Borradores del mismo tamaño que los controles. Los límites del catálogo se
   verifican al guardar: nunca se trunca silenciosamente lo que escribió
   alguien. */
#define UI_ED_CAMPOS 4
#define UI_ED_TEXTO_CAP 256
typedef enum { UI_ED_CODIGO, UI_ED_TITULO, UI_ED_AUTOR, UI_ED_ANIO } UIEdCampo;
typedef enum {
  UI_ED_NINGUNA,
  UI_ED_ANTERIOR,
  UI_ED_SIGUIENTE,
  UI_ED_BUSCAR,
  UI_ED_NUEVO,
  UI_ED_CERRAR,
  UI_ED_ELIMINAR,
  UI_ED_RESTAURAR
} UIEdAccion;
typedef enum {
  UI_ED_NORMAL,
  UI_ED_CAMBIOS,
  UI_ED_BORRADO,
  UI_ED_AVISO_CIERRE
} UIEdModo;
typedef enum { UI_ED_GUARDAR, UI_ED_DESCARTAR, UI_ED_CONTINUAR } UIEdDecision;
typedef struct {
  UIEdAccion accion;
  char consulta[UI_ED_TEXTO_CAP];
} UIEdSolicitud;
typedef struct {
  int id;
  char base[UI_ED_CAMPOS][UI_ED_TEXTO_CAP];
  char texto[UI_ED_CAMPOS][UI_ED_TEXTO_CAP];
  uint64_t revision[UI_ED_CAMPOS]; /* Diagnóstico saturado; no decide el estado
                                      sucio. */
  unsigned cambios;
  UIEdModo modo;
  UIEdSolicitud pendiente;
  int id_borrado;
  bool aviso_sync;
  int campo_invalido; /* -1 o índice 0..3, para devolver el foco. */
} UIEdicion;

void ui_edicion_inicializar(UIEdicion *s);
/* Llamar al completar una navegación autorizada; conserva el aviso de disco. */
void ui_edicion_cargar(UIEdicion *s, const ui_libro_t *libro);
/* -1 entrada inválida/modal activo; 0 mismo texto; 1 cambio aceptado. */
int ui_edicion_editar(UIEdicion *s, UIEdCampo campo, const char *texto);
bool ui_edicion_sucia(
    const UIEdicion *s); /* O(1); la comparación ocurre al editar. */
/* -1 inválida/ocupada; 0 retenida; 1 autorizada inmediatamente. */
int ui_edicion_solicitar(UIEdicion *s, UIEdSolicitud solicitud);
ui_cat_resultado_t ui_edicion_guardar(UIEdicion *s, ui_catalogo_t *cat);
/* Sólo para UI_ED_CAMBIOS. En error conserva pendiente y borrador.
   salida se vacía siempre; una acción se entrega una única vez. */
ui_cat_resultado_t ui_edicion_resolver(UIEdicion *s, ui_catalogo_t *cat,
                                       UIEdDecision decision,
                                       UIEdSolicitud *salida);
void ui_edicion_descartar(UIEdicion *s);
#endif
