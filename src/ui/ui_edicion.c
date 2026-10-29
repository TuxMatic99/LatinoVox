#include "ui_edicion.h"
#include "ui_utf8.h"
#include <stdio.h>
#include <string.h>

void ui_edicion_inicializar(UIEdicion *s) {
  if (!s)
    return;
  memset(s, 0, sizeof *s);
  s->campo_invalido = -1;
}
void ui_edicion_cargar(UIEdicion *s, const ui_libro_t *l) {
  if (!s)
    return;
  bool aviso = s->aviso_sync;
  uint64_t revisiones[UI_ED_CAMPOS];
  memcpy(revisiones, s->revision, sizeof revisiones);
  ui_edicion_inicializar(s);
  s->aviso_sync = aviso;
  memcpy(s->revision, revisiones, sizeof revisiones);
  if (l) {
    s->id = l->id_interno;
    (void)ui_utf8_copiar_seguro(s->texto[0], UI_ED_TEXTO_CAP, l->codigo);
    (void)ui_utf8_copiar_seguro(s->texto[1], UI_ED_TEXTO_CAP, l->titulo);
    (void)ui_utf8_copiar_seguro(s->texto[2], UI_ED_TEXTO_CAP, l->autor);
    (void)snprintf(s->texto[3], UI_ED_TEXTO_CAP, "%d", l->anio);
  }
  memcpy(s->base, s->texto, sizeof s->base);
}
int ui_edicion_editar(UIEdicion *s, UIEdCampo campo, const char *texto) {
  if (!s || s->modo != UI_ED_NORMAL || campo < 0 || campo >= UI_ED_CAMPOS ||
      !texto || strlen(texto) >= UI_ED_TEXTO_CAP ||
      !ui_utf8_validar(texto, NULL))
    return -1;
  if (!strcmp(s->texto[campo], texto))
    return 0;
  memmove(s->texto[campo], texto, strlen(texto) + 1);
  if (s->revision[campo] != UINT64_MAX)
    ++s->revision[campo];
  unsigned bit = 1u << campo;
  if (strcmp(s->texto[campo], s->base[campo]))
    s->cambios |= bit;
  else
    s->cambios &= ~bit;
  return 1;
}
bool ui_edicion_sucia(const UIEdicion *s) { return s && s->cambios != 0; }
int ui_edicion_solicitar(UIEdicion *s, UIEdSolicitud peticion) {
  if (!s || s->modo != UI_ED_NORMAL || peticion.accion <= UI_ED_NINGUNA ||
      peticion.accion > UI_ED_RESTAURAR ||
      !memchr(peticion.consulta, 0, sizeof peticion.consulta) ||
      !ui_utf8_validar(peticion.consulta, NULL))
    return -1;
  if (!ui_edicion_sucia(s))
    return 1;
  s->pendiente =
      peticion; /* Captura la búsqueda, no un puntero al campo mutable. */
  s->modo = UI_ED_CAMBIOS;
  return 0;
}
void ui_edicion_descartar(UIEdicion *s) {
  if (!s)
    return;
  memcpy(s->texto, s->base, sizeof s->texto);
  s->cambios = 0;
  s->campo_invalido = -1;
}
ui_cat_resultado_t ui_edicion_guardar(UIEdicion *s, ui_catalogo_t *cat) {
  if (!s || !cat || (s->modo != UI_ED_NORMAL && s->modo != UI_ED_CAMBIOS))
    return UI_CAT_ERR_ARGUMENTO;
  const size_t caps[] = {UI_CAT_CODIGO_CAP, UI_CAT_TITULO_CAP,
                         UI_CAT_AUTOR_CAP};
  s->campo_invalido = -1;
  for (int i = 0; i < 3; ++i) {
    s->campo_invalido = i;
    const char *t = s->texto[i];
    bool contenido = false;
    if (strlen(t) >= caps[i])
      return UI_CAT_ERR_LONGITUD;
    for (const unsigned char *p = (const unsigned char *)t; *p; ++p) {
      if (*p < 32 || *p == 127 || (*p == 0xc2 && p[1] >= 0x80 && p[1] <= 0x9f))
        return UI_CAT_ERR_CARACTER_ESPECIAL;
      if (*p != ' ')
        contenido = true;
    }
    if (!contenido)
      return UI_CAT_ERR_CAMPO_VACIO;
  }
  int anio = 0, id = 0;
  s->campo_invalido = UI_ED_ANIO;
  if (!ui_catalogo_entero(s->texto[3], UI_CAT_ANIO_MAX, &anio) ||
      anio < UI_CAT_ANIO_MIN)
    return UI_CAT_ERR_ANIO_INVALIDO;
  s->campo_invalido = -1;
  ui_cat_resultado_t r =
      s->id ? ui_catalogo_actualizar(cat, cat->ruta, s->id, s->texto[0],
                                     s->texto[1], s->texto[2], anio)
            : ui_catalogo_registrar(cat, cat->ruta, s->texto[0], s->texto[1],
                                    s->texto[2], anio, &id);
  if (r == UI_CAT_ERR_CODIGO_DUPLICADO)
    s->campo_invalido = UI_ED_CODIGO;
  if (r == UI_CAT_OK || r == UI_CAT_ADVERTENCIA_SYNC) {
    if (!s->id)
      s->id = id;
    memcpy(s->base, s->texto, sizeof s->base);
    s->cambios = 0;
    s->aviso_sync = r == UI_CAT_ADVERTENCIA_SYNC;
  }
  return r;
}
ui_cat_resultado_t ui_edicion_resolver(UIEdicion *s, ui_catalogo_t *cat,
                                       UIEdDecision decision,
                                       UIEdSolicitud *salida) {
  if (salida)
    memset(salida, 0, sizeof *salida);
  if (!s || !salida || s->modo != UI_ED_CAMBIOS || decision < UI_ED_GUARDAR ||
      decision > UI_ED_CONTINUAR)
    return UI_CAT_ERR_ARGUMENTO;
  ui_cat_resultado_t r = UI_CAT_OK;
  if (decision == UI_ED_GUARDAR) {
    r = ui_edicion_guardar(s, cat);
    if (r != UI_CAT_OK && r != UI_CAT_ADVERTENCIA_SYNC)
      return r;
  } else if (decision == UI_ED_DESCARTAR)
    ui_edicion_descartar(s);
  if (decision != UI_ED_CONTINUAR)
    *salida = s->pendiente;
  memset(&s->pendiente, 0, sizeof s->pendiente);
  s->modo = UI_ED_NORMAL;
  return r;
}
