#ifndef UI_CATALOGO_H
#define UI_CATALOGO_H
#include <stdbool.h>
#include <stddef.h>
#include <limits.h>

#define UI_CAT_CABECERA_V1 "LATINOUI-CATALOGO-V1"
#define UI_CAT_MAX_LIBROS 256
#define UI_CAT_CODIGO_CAP 36
#define UI_CAT_TITULO_CAP 96
#define UI_CAT_AUTOR_CAP 80
#define UI_CAT_RUTA_CAP 4096
#define UI_CAT_ARCHIVO_MAX (64u * 1024u)
/* Politica de esta aplicacion, no restriccion del formato UTF-8. */
#define UI_CAT_ANIO_MIN 1
#define UI_CAT_ANIO_MAX 9999
/* INT_MAX se reserva como siguiente_id agotado: nunca se incrementa. */
#define UI_CAT_ID_MAX (INT_MAX - 1)

typedef enum {
    UI_ATOMICO_NO_SOPORTADO = -1,
    UI_ATOMICO_NO_REEMPLAZADO = 0,
    UI_ATOMICO_CONFIRMADO = 1,
    UI_ATOMICO_SIN_SYNC_FINAL = 2
} ui_estado_atomico_t;

typedef enum {
    UI_CAT_OK = 0,
    UI_CAT_ADVERTENCIA_SYNC = 1,
    UI_CAT_ERR_ARGUMENTO = -1,
    UI_CAT_ERR_CAMPO_VACIO = -2,
    UI_CAT_ERR_CARACTER_ESPECIAL = -3,
    UI_CAT_ERR_ANIO_INVALIDO = -4,
    UI_CAT_ERR_CODIGO_DUPLICADO = -5,
    UI_CAT_ERR_NO_ENCONTRADO = -6,
    UI_CAT_ERR_ARCHIVO_CORRUPTO = -7,
    UI_CAT_ERR_ESCRITURA_ESTADO_0 = -8,
    UI_CAT_ERR_CAPACIDAD = -9,
    UI_CAT_ERR_MEMORIA = -10,
    UI_CAT_ERR_LECTURA = -11,
    UI_CAT_ERR_LONGITUD = -12,
    UI_CAT_ERR_ID_AGOTADO = -13,
    UI_CAT_ERR_NO_SOPORTADO = -14,
    UI_CAT_ERR_ESTADO_IO = -15,
    UI_CAT_ERR_NO_ABIERTO = -16,
    UI_CAT_ERR_BLOQUEO_CONCURRENTE = -17
} ui_cat_resultado_t;

typedef struct {
    int id_interno;
    char codigo[UI_CAT_CODIGO_CAP];
    char titulo[UI_CAT_TITULO_CAP];
    char autor[UI_CAT_AUTOR_CAP];
    int anio;
} ui_libro_t;

/* leer: -1 error, 0 ausente, 1 completo. Los bytes no necesitan terminador.
   Todo buffer devuelto (incluso en error) se entrega a liberar exactamente una
   vez. escribir debe cumplir los estados -1/0/1/2 y consumir sin retener datos.
   Contexto prestado por el llamador, vigente durante la vida del catalogo.
   Permite pruebas por instancia sin variables globales de simulacion. */
typedef struct {
    void *contexto;
    int (*leer)(void *, const char *, char **, size_t *);
    int (*escribir)(void *, const char *, const char *, size_t);
    void (*liberar)(void *, char *);
} ui_catalogo_io_t;

typedef struct {
    int siguiente_id;
    size_t total;
    bool bloqueado_por_corrupcion; /* Tambien bloquea fallos de lectura/E/S incierta. */
    bool abierto;
    char ruta[UI_CAT_RUTA_CAP];
    ui_catalogo_io_t io;
    ui_libro_t libros[UI_CAT_MAX_LIBROS];
} ui_catalogo_t;

/* Inicializar siempre antes del primer uso. No editar miembros manualmente.
   Abrir un unico archivo por instancia; para cambiarlo, reinicializar.
   Uso monohilo y un solo escritor sobre directorio privado estable. */
void ui_catalogo_inicializar(ui_catalogo_t *cat);
bool ui_catalogo_inicializar_con_io(ui_catalogo_t *cat, ui_catalogo_io_t io);
ui_catalogo_io_t ui_catalogo_io_archivo(void);
ui_cat_resultado_t ui_catalogo_validar(const ui_catalogo_t *cat);
/* Serializacion propia (free), sin modificar cat; salida/longitud en 0 al fallar. */
ui_cat_resultado_t ui_catalogo_serializar(const ui_catalogo_t *cat, char **salida,
                                          size_t *longitud);
ui_cat_resultado_t ui_catalogo_cargar_archivo(ui_catalogo_t *cat, const char *ruta);
ui_cat_resultado_t ui_catalogo_registrar(ui_catalogo_t *cat, const char *ruta, const char *codigo,
                                         const char *titulo, const char *autor, int anio,
                                         int *id_out);
ui_cat_resultado_t ui_catalogo_actualizar(ui_catalogo_t *cat, const char *ruta, int id,
                                          const char *codigo, const char *titulo, const char *autor,
                                          int anio);
ui_cat_resultado_t ui_catalogo_eliminar(ui_catalogo_t *cat, const char *ruta, int id);
/* Referencias prestadas: no conservar tras mutacion/carga/reinicializacion. */
const ui_libro_t *ui_catalogo_buscar_por_id(const ui_catalogo_t *cat, int id);
const ui_libro_t *ui_catalogo_buscar_por_codigo(const ui_catalogo_t *cat, const char *codigo);
/* Entero decimal ASCII positivo, sin signo, fraccion, exponente ni ceros iniciales. */
bool ui_catalogo_entero(const char *texto, int maximo, int *salida);
const char *ui_catalogo_mensaje(ui_cat_resultado_t resultado);
#endif
