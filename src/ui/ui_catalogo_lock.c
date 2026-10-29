#define _GNU_SOURCE
#include "ui_catalogo_lock.h"
#include <string.h>
#ifdef __linux__
#include <errno.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
void ui_catalogo_lock_inicializar(UICatalogoLock *l) {
  if (l) {
    memset(l, 0, sizeof *l);
    l->fd = -1;
  }
}
ui_cat_resultado_t ui_catalogo_lock_adquirir(UICatalogoLock *l,
                                             const char *ruta) {
  if (!l || !ruta || !*ruta || strlen(ruta) >= UI_CAT_RUTA_CAP - 5)
    return UI_CAT_ERR_ARGUMENTO;
  if (l->fd >= 0)
    return strcmp(l->ruta, ruta) ? UI_CAT_ERR_ARGUMENTO : UI_CAT_OK;
#ifdef __linux__
  char nombre[UI_CAT_RUTA_CAP];
  size_t n = strlen(ruta);
  memcpy(nombre, ruta, n);
  memcpy(nombre + n, ".lock", 6);
  int fd;
  do {
    fd = open(nombre, O_CREAT | O_RDWR | O_CLOEXEC | O_NOFOLLOW, 0600);
  } while (fd < 0 && errno == EINTR);
  if (fd < 0)
    return UI_CAT_ERR_LECTURA;
  struct stat st;
  if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_uid != geteuid() ||
      st.st_nlink != 1) {
    close(fd);
    return UI_CAT_ERR_LECTURA;
  }
  int r;
  do {
    r = flock(fd, LOCK_EX | LOCK_NB);
  } while (r < 0 && errno == EINTR);
  if (r < 0) {
    int e = errno;
    close(fd);
    return (e == EWOULDBLOCK || e == EAGAIN) ? UI_CAT_ERR_BLOQUEO_CONCURRENTE
                                             : UI_CAT_ERR_LECTURA;
  }
  l->fd = fd;
  memcpy(l->ruta, ruta, n + 1);
  return UI_CAT_OK;
#else
  return UI_CAT_ERR_NO_SOPORTADO;
#endif
}
void ui_catalogo_lock_liberar(UICatalogoLock *l) {
  if (!l)
    return;
#ifdef __linux__
  /* close libera el lock; no reintentar close(EINTR) en Linux. */
  if (l->fd >= 0)
    (void)close(l->fd);
#endif
  ui_catalogo_lock_inicializar(l);
}
