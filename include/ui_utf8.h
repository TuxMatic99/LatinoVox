#ifndef UI_UTF8_H
#define UI_UTF8_H
#include <stdbool.h>
#include <stddef.h>
/* Cadenas UTF-8 terminadas en NUL; cuenta puntos de codigo, no grafemas. */
bool ui_utf8_validar(const char *s, size_t *cantidad);
/* Conserva un prefijo completo si falta capacidad; false tambien para UTF-8
   invalido (en ese caso vacia dst). Admite solapamiento con memmove. */
bool ui_utf8_copiar_seguro(char *dst, size_t capacidad, const char *src);
bool ui_utf8_agregar(char *dst, size_t capacidad, unsigned int punto);
bool ui_utf8_borrar_ultimo(char *s);
#endif
