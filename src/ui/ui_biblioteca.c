#include "ui_biblioteca.h"
#include "ui_catalogo_lock.h"
#include "ui_edicion.h"
#include "ui_utf8.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

_Static_assert(UI_ED_TEXTO_CAP == LUI_TEXTO_MAX,
               "El borrador debe conservar todo el texto del control");

struct UIBiblioteca {
  lat_mv *mv;
  ui_catalogo_t catalogo;
  int seleccion;
  UIRuntime *ui;
  UIEdicion edicion;
  UICatalogoLock lock;
  bool io_nativa, gestionada, cerrar;
  int foco_previo;
  ui_cat_resultado_t resultado;
  struct UIBiblioteca *siguiente;
};
static UIBiblioteca *registros;
static UIBiblioteca *obtener(lat_mv *mv) {
  for (UIBiblioteca *b = registros; b; b = b->siguiente)
    if (b->mv == mv)
      return b;
  return NULL;
}
static const char *cadena(lat_objeto *o) {
  return o && o->tipo == T_STR ? getstr(getCadena(o)) : NULL;
}
static int numero(lat_objeto *o, int maximo) {
  if (!o || o->tipo != T_NUMERIC)
    return -1;
  double n = getNumerico(o);
  return isfinite(n) && n >= 0 && n <= maximo && floor(n) == n ? (int)n : -1;
}
static void devolver(lat_mv *mv, UIBiblioteca *b, ui_cat_resultado_t r) {
  if (b)
    b->resultado = r;
  latC_apilar_double(mv, r);
}
static void n_abrir(lat_mv *mv) {
  const char *r = cadena(latC_desapilar(mv));
  UIBiblioteca *b = obtener(mv);
  ui_cat_resultado_t e = UI_CAT_ERR_ARGUMENTO;
  if (b && r && !b->gestionada) {
    e = b->io_nativa ? ui_catalogo_lock_adquirir(&b->lock, r) : UI_CAT_OK;
    if (e == UI_CAT_OK)
      e = ui_catalogo_cargar_archivo(&b->catalogo, r);
  }
  if (b && e == UI_CAT_OK)
    b->seleccion = b->catalogo.total ? b->catalogo.libros[0].id_interno : 0;
  devolver(mv, b, e);
}
static void n_total(lat_mv *mv) {
  UIBiblioteca *b = obtener(mv);
  latC_apilar_double(mv, b ? (double)b->catalogo.total : 0);
}
static void n_seleccion(lat_mv *mv) {
  UIBiblioteca *b = obtener(mv);
  latC_apilar_double(mv, b ? b->seleccion : 0);
}
static void n_seleccionar(lat_mv *mv) {
  int id = numero(latC_desapilar(mv), UI_CAT_ID_MAX);
  UIBiblioteca *b = obtener(mv);
  bool ok = b && !b->gestionada && id >= 0 &&
            (id == 0 || ui_catalogo_buscar_por_id(&b->catalogo, id));
  if (ok)
    b->seleccion = id;
  latC_apilar(mv, ok ? latO_verdadero : latO_falso);
}
static void n_mover(lat_mv *mv) {
  lat_objeto *o = latC_desapilar(mv);
  UIBiblioteca *b = obtener(mv);
  int paso =
      o && o->tipo == T_NUMERIC && getNumerico(o) == -1 ? -1 : numero(o, 1);
  if (b && !b->gestionada && b->catalogo.total && (paso == 1 || paso == -1)) {
    int idx = -1;
    for (size_t i = 0; i < b->catalogo.total; ++i)
      if (b->catalogo.libros[i].id_interno == b->seleccion)
        idx = (int)i;
    int total = (int)b->catalogo.total;
    idx = idx < 0 ? (paso == 1 ? 0 : total - 1) : (idx + paso + total) % total;
    b->seleccion = b->catalogo.libros[idx].id_interno;
  }
  latC_apilar_double(mv, b ? b->seleccion : 0);
}
static void n_campo(lat_mv *mv) {
  int col = numero(latC_desapilar(mv), 3);
  UIBiblioteca *b = obtener(mv);
  const ui_libro_t *l =
      b ? ui_catalogo_buscar_por_id(&b->catalogo, b->seleccion) : NULL;
  char anio[16];
  const char *s = "";
  if (l) {
    if (col == 0)
      s = l->codigo;
    else if (col == 1)
      s = l->titulo;
    else if (col == 2)
      s = l->autor;
    else if (col == 3) {
      snprintf(anio, sizeof anio, "%d", l->anio);
      s = anio;
    }
  }
  latC_apilar_string(mv, s);
}
static unsigned char ascii_minuscula(unsigned char c) {
  return c >= 'A' && c <= 'Z' ? (unsigned char)(c + ('a' - 'A')) : c;
}
static bool contiene(const char *s, const char *q) {
  size_t n = strlen(q), m = strlen(s);
  if (n > m)
    return false;
  for (size_t i = 0; i <= m - n; ++i) {
    size_t j = 0;
    while (j < n && ascii_minuscula((unsigned char)s[i + j]) ==
                        ascii_minuscula((unsigned char)q[j]))
      ++j;
    if (j == n)
      return true;
  }
  return false;
}
static void n_buscar(lat_mv *mv) {
  const char *q = cadena(latC_desapilar(mv));
  UIBiblioteca *b = obtener(mv);
  int encontrado = 0;
  if (b && !b->gestionada && q && *q && strlen(q) < 256 &&
      ui_utf8_validar(q, NULL)) {
    for (size_t i = 0; i < b->catalogo.total; ++i) {
      const ui_libro_t *l = &b->catalogo.libros[i];
      if (contiene(l->codigo, q) || contiene(l->titulo, q) ||
          contiene(l->autor, q)) {
        encontrado = l->id_interno;
        b->seleccion = encontrado;
        break;
      }
    }
  }
  if (b)
    b->resultado = encontrado ? UI_CAT_OK : UI_CAT_ERR_NO_ENCONTRADO;
  latC_apilar_double(mv, encontrado);
}
static void guardar(lat_mv *mv, bool actualizar) {
  const char *anio = cadena(latC_desapilar(mv));
  const char *autor = cadena(latC_desapilar(mv));
  const char *titulo = cadena(latC_desapilar(mv));
  const char *codigo = cadena(latC_desapilar(mv));
  UIBiblioteca *b = obtener(mv);
  int valor = 0, id = 0;
  ui_cat_resultado_t e = UI_CAT_ERR_ARGUMENTO;
  if (b && !b->gestionada && codigo && titulo && autor && anio) {
    if (!ui_catalogo_entero(anio, UI_CAT_ANIO_MAX, &valor))
      e = UI_CAT_ERR_ANIO_INVALIDO;
    else if (actualizar)
      e = ui_catalogo_actualizar(&b->catalogo, b->catalogo.ruta, b->seleccion,
                                 codigo, titulo, autor, valor);
    else
      e = ui_catalogo_registrar(&b->catalogo, b->catalogo.ruta, codigo, titulo,
                                autor, valor, &id);
    if (!actualizar && (e == UI_CAT_OK || e == UI_CAT_ADVERTENCIA_SYNC))
      b->seleccion = id;
  }
  devolver(mv, b, e);
}
static void n_registrar(lat_mv *mv) { guardar(mv, false); }
static void n_actualizar(lat_mv *mv) { guardar(mv, true); }
static void n_eliminar(lat_mv *mv) {
  UIBiblioteca *b = obtener(mv);
  ui_cat_resultado_t r = UI_CAT_ERR_ARGUMENTO;
  if (b && !b->gestionada) {
    r = ui_catalogo_eliminar(&b->catalogo, b->catalogo.ruta, b->seleccion);
    if (r == UI_CAT_OK || r == UI_CAT_ADVERTENCIA_SYNC)
      b->seleccion = b->catalogo.total ? b->catalogo.libros[0].id_interno : 0;
  }
  devolver(mv, b, r);
}
static void n_mensaje(lat_mv *mv) {
  UIBiblioteca *b = obtener(mv);
  latC_apilar_string(
      mv, ui_catalogo_mensaje(b ? b->resultado : UI_CAT_ERR_ARGUMENTO));
}
/* Controlador de Biblioteca13: los IDs pertenecen a la aplicación, no al
   catálogo ni al runtime genérico. Un único borrador y una única decisión. */
static const int campos_ui[] = {101, 102, 103, 104};
static void poner(UIBiblioteca *b, int id, const char *s) {
  const char *actual = lui_texto(b->ui, id);
  if (actual && strcmp(actual, s))
    (void)lui_establecer_texto(b->ui, id, s);
}
static void notificar(UIBiblioteca *b, ui_cat_resultado_t r) {
  b->resultado = r;
  poner(b, 302, ui_catalogo_mensaje(r));
}
static void reflejar(UIBiblioteca *b) {
  for (int i = 0; i < UI_ED_CAMPOS; ++i)
    poner(b, campos_ui[i], b->edicion.texto[i]);
  char resumen[128];
  (void)snprintf(resumen, sizeof resumen, "Libros: %zu | ID seleccionado: %d",
                 b->catalogo.total, b->edicion.id);
  poner(b, 300, resumen);
  b->seleccion = b->edicion.id;
}
static void habilitar_controles(UIBiblioteca *b) {
  UIEdModo modo = b->edicion.modo;
  for (size_t i = 0; i < b->ui->arbol.cantidad; ++i) {
    if (b->ui->nodos[i].tipo != UI_NODO_WIDGET ||
        b->ui->tipos[i] == UI_WIDGET_ETIQUETA)
      continue;
    int id = b->ui->nodos[i].id_widget;
    bool habilitado = false;
    if (modo == UI_ED_NORMAL) {
      habilitado = id < 205 || id > 209;
      if (id == 201)
        habilitado = b->edicion.id == 0;
      if (id == 202 || id == 203)
        habilitado = b->edicion.id > 0;
    } else if (modo == UI_ED_CAMBIOS)
      habilitado = id >= 206 && id <= 208;
    else if (modo == UI_ED_BORRADO)
      habilitado = id == 204 || id == 205;
    else if (modo == UI_ED_AVISO_CIERRE)
      habilitado = id == 208 || id == 209;
    (void)lui_habilitar(b->ui, id, habilitado);
  }
  poner(b, 303,
        ui_edicion_sucia(&b->edicion) ? "Cambios pendientes" : "Sin cambios");
  poner(b, 304,
        b->edicion.aviso_sync ? "Aviso: sincronizacion en disco sin confirmar."
                              : "Disco: sin advertencia pendiente.");
}
static void modal(UIBiblioteca *b, UIEdModo modo) {
  if (b->edicion.modo == UI_ED_NORMAL && b->ui->foco < b->ui->arbol.cantidad)
    b->foco_previo = b->ui->nodos[b->ui->foco].id_widget;
  b->edicion.modo = modo;
  habilitar_controles(b);
  (void)lui_enfocar(b->ui, modo == UI_ED_BORRADO ? 204 : 208);
}
static void volver(UIBiblioteca *b) {
  habilitar_controles(b);
  int id = b->edicion.campo_invalido >= 0 ? campos_ui[b->edicion.campo_invalido]
                                          : b->foco_previo;
  if (!lui_enfocar(b->ui, id))
    (void)lui_enfocar(b->ui, 101);
}
void ui_biblioteca_conectar_ui(UIBiblioteca *b, UIRuntime *ui) {
  if (b)
    b->ui = ui;
}
bool ui_biblioteca_protege_cierre(const UIBiblioteca *b) {
  return b && b->gestionada;
}
void ui_biblioteca_sincronizar_ui(UIBiblioteca *b) {
  if (!b || !b->gestionada)
    return;
  if (b->edicion.modo == UI_ED_NORMAL)
    for (int i = 0; i < UI_ED_CAMPOS; ++i)
      (void)ui_edicion_editar(&b->edicion, (UIEdCampo)i,
                              lui_texto(b->ui, campos_ui[i]));
  habilitar_controles(b);
}
static void ejecutar_accion(UIBiblioteca *b, UIEdSolicitud p) {
  const ui_libro_t *l = NULL;
  switch (p.accion) {
  case UI_ED_ANTERIOR:
  case UI_ED_SIGUIENTE: {
    size_t n = b->catalogo.total, idx = 0;
    if (!n)
      break;
    bool encontrado = false;
    for (size_t i = 0; i < n; ++i)
      if (b->catalogo.libros[i].id_interno == b->edicion.id) {
        idx = i;
        encontrado = true;
        break;
      }
    if (p.accion == UI_ED_SIGUIENTE)
      idx = encontrado ? (idx + 1) % n : 0;
    else
      idx = encontrado ? (idx + n - 1) % n : n - 1;
    l = &b->catalogo.libros[idx];
    break;
  }
  case UI_ED_BUSCAR:
    if (*p.consulta)
      for (size_t i = 0; i < b->catalogo.total; ++i) {
        const ui_libro_t *c = &b->catalogo.libros[i];
        if (contiene(c->codigo, p.consulta) ||
            contiene(c->titulo, p.consulta) || contiene(c->autor, p.consulta)) {
          l = c;
          break;
        }
      }
    if (!l) {
      notificar(b, UI_CAT_ERR_NO_ENCONTRADO);
      return;
    }
    break;
  case UI_ED_CERRAR:
    if (b->edicion.aviso_sync) {
      modal(b, UI_ED_AVISO_CIERRE);
      poner(b, 302,
            "El archivo fue reemplazado. Cierra con aviso o vuelve a editar.");
    } else
      b->cerrar = true;
    return;
  case UI_ED_ELIMINAR:
    if (!b->edicion.id) {
      notificar(b, UI_CAT_ERR_NO_ENCONTRADO);
      return;
    }
    b->edicion.id_borrado = b->edicion.id;
    modal(b, UI_ED_BORRADO);
    poner(b, 302, "Confirma la eliminacion con el boton separado, o Cancelar.");
    return;
  case UI_ED_RESTAURAR:
    l = ui_catalogo_buscar_por_id(&b->catalogo, b->edicion.id);
    break;
  case UI_ED_NUEVO:
    break;
  default:
    return;
  }
  ui_edicion_cargar(&b->edicion, l);
  reflejar(b);
  volver(b);
}
static void solicitar(UIBiblioteca *b, UIEdAccion accion) {
  ui_biblioteca_sincronizar_ui(b);
  UIEdSolicitud p = {0};
  p.accion = accion;
  if (accion == UI_ED_BUSCAR)
    (void)ui_utf8_copiar_seguro(p.consulta, sizeof p.consulta,
                                lui_texto(b->ui, 100));
  if (b->edicion.modo == UI_ED_NORMAL && b->ui->foco < b->ui->arbol.cantidad)
    b->foco_previo = b->ui->nodos[b->ui->foco].id_widget;
  int r = ui_edicion_solicitar(&b->edicion, p);
  if (r == 0) {
    modal(b, UI_ED_CAMBIOS);
    poner(b, 302, "Hay cambios. Guarda, descarta o continua editando.");
  } else if (r == 1)
    ejecutar_accion(b, p);
  /* Una petición posterior no reemplaza el destino que espera decisión. */
}
void ui_biblioteca_pedir_cierre(UIBiblioteca *b) {
  if (ui_biblioteca_protege_cierre(b))
    solicitar(b, UI_ED_CERRAR);
}
bool ui_biblioteca_consumir_cierre(UIBiblioteca *b) {
  if (!b || !b->cerrar)
    return false;
  b->cerrar = false;
  return true;
}
static void n_activar_edicion(lat_mv *mv) {
  UIBiblioteca *b = obtener(mv);
  bool ok = b && b->ui && !b->gestionada;
  const int requeridos[] = {100, 101, 102, 103, 104, 201, 202, 203,
                            204, 205, 206, 207, 208, 209, 210, 211,
                            212, 213, 300, 302, 303, 304};
  if (ok)
    for (size_t i = 0; i < sizeof requeridos / sizeof *requeridos; ++i)
      if (!lui_texto(b->ui, requeridos[i]))
        ok = false;
  if (b && b->ui)
    for (size_t i = 0; i < b->ui->arbol.cantidad; ++i) {
      if (b->ui->nodos[i].tipo != UI_NODO_WIDGET)
        continue;
      int id = b->ui->nodos[i].id_widget;
      if (id >= 100 && id <= 104 && b->ui->tipos[i] != UI_WIDGET_CAMPO_TEXTO)
        ok = false;
      if (id >= 201 && id <= 213 && b->ui->tipos[i] != UI_WIDGET_BOTON)
        ok = false;
      if ((id == 300 || (id >= 302 && id <= 304)) &&
          b->ui->tipos[i] != UI_WIDGET_ETIQUETA)
        ok = false;
    }
  if (ok) {
    b->gestionada = true;
    ui_edicion_cargar(&b->edicion,
                      ui_catalogo_buscar_por_id(&b->catalogo, b->seleccion));
    reflejar(b);
    habilitar_controles(b);
    poner(b, 302, ui_catalogo_mensaje(b->resultado));
  }
  if (!ok && b && b->ui && !b->gestionada)
    (void)snprintf(b->ui->error, sizeof b->ui->error,
                   "Biblioteca13: faltan controles para activar la proteccion");
  latC_apilar(mv, ok ? latO_verdadero : latO_falso);
}
static void n_accion(lat_mv *mv) {
  int tipo = numero(latC_desapilar(mv), UI_EVENTO_CANCELAR);
  int id = numero(latC_desapilar(mv), 9999);
  UIBiblioteca *b = obtener(mv);
  if (!b || !b->gestionada || tipo < UI_EVENTO_CLIC || id < 0) {
    latC_apilar(mv, latO_falso);
    return;
  }
  ui_biblioteca_sincronizar_ui(b);
  UIEdicion *s = &b->edicion;
  if (tipo == UI_EVENTO_CANCELAR)
    id = s->modo == UI_ED_BORRADO ? 204 : s->modo != UI_ED_NORMAL ? 208 : 204;
  else if (tipo == UI_EVENTO_SUBMIT && id == 100)
    id = 210;
  else if (tipo == UI_EVENTO_SUBMIT && id >= 101 && id <= 104)
    id = s->id ? 202 : 201;
  /* También rechaza eventos de fondo ya encolados antes de abrir el diálogo. */
  if (!lui_habilitado(b->ui, id)) {
    latC_apilar(mv, latO_falso);
    return;
  }
  if (s->modo == UI_ED_CAMBIOS && id >= 206 && id <= 208) {
    UIEdSolicitud p;
    ui_cat_resultado_t r =
        ui_edicion_resolver(s, &b->catalogo, (UIEdDecision)(id - 206), &p);
    notificar(b, r);
    if (r >= 0) {
      reflejar(b);
      volver(b);
      ejecutar_accion(b, p);
    }
    /* Conservar modal y destino cuando falla guardar. Continuar permite
       corregir el campo inválido y le devuelve el foco. */
  } else if (s->modo == UI_ED_BORRADO) {
    if (id == 204) {
      s->modo = UI_ED_NORMAL;
      s->id_borrado = 0;
      volver(b);
      poner(b, 302, "Eliminacion cancelada.");
    } else if (id == 205) {
      ui_cat_resultado_t r =
          ui_catalogo_eliminar(&b->catalogo, b->catalogo.ruta, s->id_borrado);
      notificar(b, r);
      if (r >= 0) {
        s->aviso_sync = r == UI_CAT_ADVERTENCIA_SYNC;
        ui_edicion_cargar(s, b->catalogo.total ? &b->catalogo.libros[0] : NULL);
        reflejar(b);
        volver(b);
      }
    }
  } else if (s->modo == UI_ED_AVISO_CIERRE) {
    if (id == 209)
      b->cerrar = true;
    else if (id == 208) {
      s->modo = UI_ED_NORMAL;
      volver(b);
    }
  } else if (s->modo == UI_ED_NORMAL) {
    if (id == 201 || id == 202) {
      ui_cat_resultado_t r = ui_edicion_guardar(s, &b->catalogo);
      notificar(b, r);
      if (r >= 0)
        reflejar(b);
      volver(b);
    } else {
      UIEdAccion a = id == 211   ? UI_ED_ANTERIOR
                     : id == 212 ? UI_ED_SIGUIENTE
                     : id == 210 ? UI_ED_BUSCAR
                     : id == 213 ? UI_ED_NUEVO
                     : id == 203 ? UI_ED_ELIMINAR
                     : id == 204 ? UI_ED_RESTAURAR
                                 : UI_ED_NINGUNA;
      if (a != UI_ED_NINGUNA)
        solicitar(b, a);
    }
  }
  habilitar_controles(b);
  latC_apilar(mv, latO_verdadero);
}

static const lat_CReg funciones[] = {{"activar_edicion", n_activar_edicion, 0},
                                     {"accion", n_accion, 2},
                                     {"abrir", n_abrir, 1},
                                     {"total", n_total, 0},
                                     {"seleccion", n_seleccion, 0},
                                     {"seleccionar", n_seleccionar, 1},
                                     {"mover", n_mover, 1},
                                     {"campo", n_campo, 1},
                                     {"buscar", n_buscar, 1},
                                     {"registrar", n_registrar, 4},
                                     {"actualizar", n_actualizar, 4},
                                     {"eliminar", n_eliminar, 0},
                                     {"mensaje", n_mensaje, 0},
                                     {NULL, NULL, 0}};
UIBiblioteca *ui_biblioteca_crear(lat_mv *mv, const ui_catalogo_io_t *io) {
  if (!mv || obtener(mv))
    return NULL;
  UIBiblioteca *b = calloc(1, sizeof *b);
  if (!b)
    return NULL;
  if (!ui_catalogo_inicializar_con_io(&b->catalogo,
                                      io ? *io : ui_catalogo_io_archivo())) {
    free(b);
    return NULL;
  }
  b->mv = mv;
  b->io_nativa = io == NULL;
  ui_catalogo_lock_inicializar(&b->lock);
  ui_edicion_inicializar(&b->edicion);
  b->siguiente = registros;
  registros = b;
  latC_abrir_liblatino(mv, "biblioteca", funciones);
  return b;
}
void ui_biblioteca_destruir(UIBiblioteca *b) {
  if (!b)
    return;
  for (UIBiblioteca **p = &registros; *p; p = &(*p)->siguiente)
    if (*p == b) {
      *p = b->siguiente;
      ui_catalogo_lock_liberar(&b->lock);
      free(b);
      return;
    }
}
