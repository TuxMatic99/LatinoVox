#define _POSIX_C_SOURCE 200809L
#include "ui_catalogo_lock.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#define CHECK(c)                                                               \
  do {                                                                         \
    if (!(c)) {                                                                \
      fprintf(stderr, "FALLO lock %d: %s\n", __LINE__, #c);                    \
      exit(2);                                                                 \
    }                                                                          \
  } while (0)
static void hijo(UICatalogoLock *heredado, const char *ruta, int esperado) {
  pid_t pid = fork();
  CHECK(pid >= 0);
  if (pid == 0) {
    /* Cerrar el descriptor heredado sin LOCK_UN: el padre sigue siendo dueño.
     */
    if (heredado->fd >= 0)
      close(heredado->fd);
    UICatalogoLock segundo;
    ui_catalogo_lock_inicializar(&segundo);
    int r = ui_catalogo_lock_adquirir(&segundo, ruta);
    ui_catalogo_lock_liberar(&segundo);
    _exit(r == esperado ? 0 : 3);
  }
  int estado = 0;
  CHECK(waitpid(pid, &estado, 0) == pid && WIFEXITED(estado) &&
        WEXITSTATUS(estado) == 0);
}
int main(void) {
  char d[] = "/tmp/latino-lock13-XXXXXX", r[512], l[520], otra[520];
  CHECK(mkdtemp(d));
  snprintf(r, sizeof r, "%s/catalogo.tsv", d);
  snprintf(l, sizeof l, "%s.lock", r);
  snprintf(otra, sizeof otra, "%s/simbolico", d);
  UICatalogoLock a;
  ui_catalogo_lock_inicializar(&a);
  CHECK(ui_catalogo_lock_adquirir(&a, r) ==
        UI_CAT_OK); /* Aún no hay catálogo: ya impide lectura obsoleta. */
  CHECK(access(r, F_OK) != 0 && access(l, F_OK) == 0);
  struct stat antes, despues;
  CHECK(stat(l, &antes) == 0);
  CHECK((fcntl(a.fd, F_GETFD) & FD_CLOEXEC) != 0);
  hijo(&a, r, UI_CAT_ERR_BLOQUEO_CONCURRENTE);
  ui_catalogo_lock_liberar(&a);
  CHECK(stat(l, &despues) == 0 && antes.st_ino == despues.st_ino);
  hijo(&a, r, UI_CAT_OK);
  CHECK(stat(l, &despues) == 0 && antes.st_ino == despues.st_ino);
  CHECK(symlink(l, otra) == 0);
  char base[520];
  snprintf(base, sizeof base, "%s/simbolico.lock", d);
  CHECK(rename(otra, base) == 0);
  CHECK(ui_catalogo_lock_adquirir(&a, otra) == UI_CAT_ERR_LECTURA);
  CHECK(unlink(base) == 0);
  /* Un proceso que termina sin liberar explícitamente deja disponible el lock.
   */
  pid_t p = fork();
  CHECK(p >= 0);
  if (!p) {
    UICatalogoLock b;
    ui_catalogo_lock_inicializar(&b);
    _exit(ui_catalogo_lock_adquirir(&b, r) == 0 ? 0 : 4);
  }
  int estado;
  CHECK(waitpid(p, &estado, 0) == p && WIFEXITED(estado) &&
        WEXITSTATUS(estado) == 0);
  CHECK(ui_catalogo_lock_adquirir(&a, r) == 0);
  ui_catalogo_lock_liberar(&a);
  CHECK(unlink(l) == 0 &&
        rmdir(d) ==
            0); /* Sólo limpieza del directorio privado de esta prueba. */
  puts("OK: dos procesos, bloqueo antes de cargar, inode estable, CLOEXEC, "
       "symlink y salida del dueño");
  return 0;
}
