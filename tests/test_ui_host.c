/* Ejecuta el main real del host con un backend grafico determinista.
   Latino-Core, el runtime, la FIFO y los callbacks NO se simulan. */
#ifdef __linux__
#define _POSIX_C_SOURCE 200809L
#include <unistd.h>
#include <sys/stat.h>
#include <limits.h>
#endif
#include "raylib.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EXIGIR(c) do { if (!(c)) { \
    fprintf(stderr, "FALLO: %s en %s:%d\n", #c, __FILE__, __LINE__); \
    exit(EXIT_FAILURE); \
} } while (0)

int latino_ui_host_main(int argc, char **argv);

static struct {
    bool abierta;
    bool formulario;
    int persistencia;
    int caracter_persistencia;
    const char *campo_esperado;
    const char *mensaje_esperado;
    unsigned int vista_persistencia;
    int ultimo_caracter;
    unsigned int comprobaciones;
    bool fallar_apertura;
    int aperturas;
    int cierres;
    int frames;
    int max_frames;
    int titulos_correctos;
    int clics_correctos;
    int consultas_cierre;
    char titulo[256];
} pantalla;

static void reiniciar(int frames, bool fallar) {
    EXIGIR(!pantalla.abierta);
    memset(&pantalla, 0, sizeof(pantalla));
    pantalla.max_frames = frames;
    pantalla.fallar_apertura = fallar;
}

void SetExitKey(int key) { EXIGIR(key==KEY_NULL && pantalla.abierta); }
bool IsKeyPressed(int key) {
    if (pantalla.persistencia) {
        int f=pantalla.frames, caso=pantalla.persistencia;
        return (key==KEY_ENTER && ((caso==2 || caso==4) ? f==1 : caso==3 && f==3)) ||
            (key==KEY_TAB && caso==3 && (f==1 || f==2)) ||
            (key==KEY_BACKSPACE && caso==4 && f==0);
    }
    if (!pantalla.formulario) return false;
    int f=pantalla.frames;
    return (key==KEY_BACKSPACE && f==1) || (key==KEY_ESCAPE && f==3) ||
        (key==KEY_TAB && (f==4 || f==6 || f==8 || f==9)) ||
        (key==KEY_ENTER && (f==5 || f==7 || f==11));
}
bool IsKeyDown(int key) {
    return pantalla.formulario && key==KEY_LEFT_SHIFT && (pantalla.frames==8 || pantalla.frames==9);
}
int GetCharPressed(void) {
    if (pantalla.persistencia) {
        static const int nombre[]={'M','a','r',0xed,'a',' ',0xd1,0xe1,0xf1,'e','z',0};
        if (pantalla.frames!=0) return 0;
        if (pantalla.persistencia==2)
            return nombre[pantalla.caracter_persistencia<11 ? pantalla.caracter_persistencia++ : 11];
        if (pantalla.persistencia==3 && !pantalla.caracter_persistencia++) return 'X';
        return 0;
    }
    if (!pantalla.formulario || pantalla.ultimo_caracter==pantalla.frames+1) return 0;
    pantalla.ultimo_caracter=pantalla.frames+1;
    return pantalla.frames==0 ? '!' : pantalla.frames==2 ? 'X' : pantalla.frames==10 ? 'A' : 0;
}
void SetConfigFlags(unsigned int flags) { (void)flags; }
void InitWindow(int width, int height, const char *title) {
    EXIGIR(!pantalla.abierta && width > 0 && height > 0);
    pantalla.aperturas++;
    pantalla.abierta = !pantalla.fallar_apertura;
    snprintf(pantalla.titulo, sizeof(pantalla.titulo), "%s", title);
}
bool IsWindowReady(void) { return pantalla.abierta; }
void CloseWindow(void) {
    EXIGIR(pantalla.abierta);
    pantalla.cierres++;
    pantalla.abierta = false;
}
void SetWindowMinSize(int w, int h) { (void)w; (void)h; EXIGIR(pantalla.abierta); }
void SetTargetFPS(int fps) { EXIGIR(fps > 0 && pantalla.abierta); }
bool WindowShouldClose(void) {
    EXIGIR(pantalla.abierta);
    EXIGIR(++pantalla.consultas_cierre < 1000);
    return pantalla.frames >= pantalla.max_frames;
}
int GetScreenWidth(void) { return 900; }
int GetScreenHeight(void) { return 600; }
bool IsMouseButtonPressed(int button) { return !pantalla.formulario && !pantalla.persistencia && button == MOUSE_BUTTON_LEFT; }
Vector2 GetMousePosition(void) { return (Vector2){35.0f, 81.0f}; }
bool CheckCollisionPointRec(Vector2 p, Rectangle r) {
    return p.x >= r.x && p.x <= r.x + r.width &&
           p.y >= r.y && p.y <= r.y + r.height;
}
void BeginDrawing(void) { EXIGIR(pantalla.abierta); }
void EndDrawing(void) { EXIGIR(pantalla.abierta); pantalla.frames++; }
void ClearBackground(Color c) { (void)c; }
void DrawText(const char *t, int x, int y, int tam, Color c) {
    (void)x; (void)y; (void)tam; (void)c;
    if (pantalla.persistencia && pantalla.frames==pantalla.max_frames-1) {
        if (!strcmp(t,pantalla.campo_esperado)) pantalla.vista_persistencia|=1;
        if (!strcmp(t,pantalla.mensaje_esperado)) pantalla.vista_persistencia|=2;
    }
    if (pantalla.formulario) {
        int f=pantalla.frames;
        if ((f==0 && !strcmp(t,"María Ñáñez!")) ||
            (f==1 && !strcmp(t,"María Ñáñez")) ||
            (f==2 && !strcmp(t,"María ÑáñezX")) ||
            (f==3 && !strcmp(t,"Edición cancelada.")) ||
            (f==5 && !strcmp(t,"María Ñáñez") && y>400) ||
            (f==7 && !strcmp(t,"Formulario cancelado.")) ||
            (f==11 && !strcmp(t,"Escribe un nombre de al menos dos caracteres.")))
            pantalla.comprobaciones |= 1u << f;
    }
    if (!strcmp(t, "Callback correcto")) pantalla.titulos_correctos++;
    if (!strcmp(t, "Ultimo clic: 7")) pantalla.clics_correctos++;
}
void DrawRectangleRec(Rectangle r, Color c) { (void)r; (void)c; }
void DrawRectangleLinesEx(Rectangle r, float t, Color c) { (void)r; (void)t; (void)c; }
void BeginScissorMode(int x, int y, int w, int h) { (void)x; (void)y; (void)w; (void)h; }
void EndScissorMode(void) {}
const char *TextFormat(const char *fmt, ...) {
    static char texto[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(texto, sizeof(texto), fmt, args);
    va_end(args);
    return texto;
}

static void abrir(const char *archivo, int esperado, int frames, bool fallar) {
    char *args[] = {"host_ui", (char *)archivo, NULL};
    reiniciar(frames, fallar);
    EXIGIR(latino_ui_host_main(2, args) == esperado);
    EXIGIR(!pantalla.abierta);
    if (esperado == EXIT_SUCCESS) {
        EXIGIR(pantalla.aperturas == 1 && pantalla.cierres == 1);
        EXIGIR(!strcmp(pantalla.titulo, "Prueba de host09"));
        EXIGIR(pantalla.frames == frames);
        EXIGIR(pantalla.titulos_correctos == frames);
        EXIGIR(pantalla.clics_correctos == frames);
    } else {
        EXIGIR(pantalla.aperturas == (fallar ? 1 : 0));
        EXIGIR(pantalla.cierres == 0 && pantalla.frames == 0);
    }
}


#ifdef __linux__
/* Disco real y VM real; solo el teclado y la ventana son simulados. */
static void escribir_datos(const void *datos,size_t n) {
    FILE *f=fopen("nombre-v1.txt","wb"); EXIGIR(f);
    EXIGIR(fwrite(datos,1,n,f)==n); EXIGIR(fclose(f)==0);
}
static void verificar_datos(const void *datos,size_t n) {
    char buf[512]; EXIGIR(n<sizeof buf);
    FILE *f=fopen("nombre-v1.txt","rb"); EXIGIR(f);
    EXIGIR(fread(buf,1,sizeof buf,f)==n && !memcmp(buf,datos,n));
    EXIGIR(!ferror(f) && fclose(f)==0);
}
static void abrir_persistencia(const char *script,int accion,const char *campo,const char *mensaje) {
    reiniciar(accion==1 ? 1 : accion==3 ? 4 : 2,false);
    pantalla.persistencia=accion;
    pantalla.campo_esperado=campo; pantalla.mensaje_esperado=mensaje;
    char *args[]={"host_ui",(char *)script,NULL};
    EXIGIR(latino_ui_host_main(2,args)==EXIT_SUCCESS);
    EXIGIR(!pantalla.abierta && pantalla.aperturas==1 && pantalla.cierres==1);
    EXIGIR(pantalla.vista_persistencia==3);
}
static void probar_persistencia(const char *ruta) {
    /* CTest pasa una ruta absoluta porque cada caso cambia el directorio. */
    EXIGIR(ruta[0]=='/');
    char anterior[PATH_MAX],dir[]="/tmp/latinoui-persistencia11-XXXXXX";
    EXIGIR(getcwd(anterior,sizeof anterior)); EXIGIR(mkdtemp(dir)); EXIGIR(chdir(dir)==0);
    abrir_persistencia(ruta,1,"","Primer uso: escribe tu nombre y pulsa Guardar.");
    const char *nombre="María Ñáñez";
    for(int i=0;i<5;++i) {
        abrir_persistencia(ruta,2,nombre,"Nombre guardado. Puedes cerrar y volver a abrir.");
        verificar_datos(nombre,strlen(nombre));
        abrir_persistencia(ruta,1,nombre,"Nombre recuperado del archivo.");
        abrir_persistencia(ruta,3,nombre,"Cambios descartados. Ultimo nombre guardado recuperado.");
        verificar_datos(nombre,strlen(nombre));
        escribir_datos("AB",2);
        abrir_persistencia(ruta,4,"A","Escribe un nombre de al menos dos caracteres.");
        verificar_datos("AB",2); EXIGIR(unlink("nombre-v1.txt")==0);
    }
    const unsigned char invalido[]={0xc0,0x80};
    escribir_datos(invalido,sizeof invalido);
    abrir_persistencia(ruta,1,"","Archivo invalido. No se sobrescribira en esta sesion.");
    abrir_persistencia(ruta,2,nombre,"Guardado bloqueado. Revisa el archivo antes de reabrir.");
    verificar_datos(invalido,sizeof invalido);
    char largo[256]; memset(largo,'x',sizeof largo);
    escribir_datos(largo,sizeof largo);
    abrir_persistencia(ruta,2,nombre,"Guardado bloqueado. Revisa el archivo antes de reabrir.");
    verificar_datos(largo,sizeof largo);
    const char con_nul[]={'A','\0','B'};
    escribir_datos(con_nul,sizeof con_nul);
    abrir_persistencia(ruta,1,"","No se pudo leer el archivo. Guardado bloqueado.");
    abrir_persistencia(ruta,2,nombre,"Guardado bloqueado. Revisa el archivo antes de reabrir.");
    verificar_datos(con_nul,sizeof con_nul);
    escribir_datos("A\nB",3);
    abrir_persistencia(ruta,2,nombre,"Guardado bloqueado. Revisa el archivo antes de reabrir.");
    verificar_datos("A\nB",3); EXIGIR(unlink("nombre-v1.txt")==0);
    EXIGIR(mkdir("nombre-v1.txt",0700)==0);
    abrir_persistencia(ruta,2,nombre,"Guardado bloqueado. Revisa el archivo antes de reabrir.");
    EXIGIR(rmdir("nombre-v1.txt")==0);
    EXIGIR(chdir(anterior)==0 && rmdir(dir)==0);
    puts("OK: guardar/reabrir, Cancelar, validacion, UTF-8/longitud/NUL invalidos y errores de lectura sin sobrescritura");
}
#endif

int main(int argc, char **argv) {
    EXIGIR(argc == 3);
#ifdef __linux__
    if (!strcmp(argv[1],"--persistencia")) { probar_persistencia(argv[2]); return 0; }
#endif
    if (!strcmp(argv[1],"--formulario")) {
        reiniciar(12,false); pantalla.formulario=true;
        char *args[]={"host_ui",argv[2],NULL};
        EXIGIR(latino_ui_host_main(2,args)==EXIT_SUCCESS);
        EXIGIR(pantalla.aperturas==1 && pantalla.cierres==1 && pantalla.frames==12);
        EXIGIR(pantalla.comprobaciones==((1u<<0)|(1u<<1)|(1u<<2)|(1u<<3)|(1u<<5)|(1u<<7)|(1u<<11)));
        puts("OK: formulario Latino real, UTF-8, Tab/Shift+Tab, Enter, Escape, validacion y Cancelar");
        return 0;
    }
    const char *bueno = argv[1];
    const char *malo = argv[2];
    char *ayuda[] = {"host_ui", "--ayuda", NULL};
    char *opcion[] = {"host_ui", "--opcion-inexistente", NULL};
    char *sobrantes[] = {"host_ui", (char *)bueno, "sobrante", NULL};
    reiniciar(0, false);
    EXIGIR(latino_ui_host_main(2, ayuda) == EXIT_SUCCESS);
    EXIGIR(latino_ui_host_main(2, opcion) == EXIT_FAILURE);
    EXIGIR(latino_ui_host_main(3, sobrantes) == EXIT_FAILURE);
    EXIGIR(pantalla.aperturas == 0);

    char ausente[4096];
    int n = snprintf(ausente, sizeof(ausente), "%s.inexistente.lat", bueno);
    EXIGIR(n > 0 && (size_t)n < sizeof(ausente));
    abrir(ausente, EXIT_FAILURE, 0, false);
    abrir(bueno, EXIT_FAILURE, 0, true);
    /* Cada fallo debe retirar el registro FFI para poder abrir otra VM.
       Veinte sesiones distintas, con 25 callbacks reales en cada sesion. */
    for (int ciclo = 0; ciclo < 20; ciclo++) {
        abrir(malo, EXIT_FAILURE, 0, false);
        abrir(bueno, EXIT_SUCCESS, 25, false);
    }
    char *separador[] = {"host_ui", "--", (char *)bueno, NULL};
    reiniciar(0, false);
    EXIGIR(latino_ui_host_main(3, separador) == EXIT_SUCCESS);
    EXIGIR(pantalla.aperturas == 1 && pantalla.cierres == 1);
    puts("OK: host real, ruta elegida, 20 sesiones, 500 callbacks y limpieza tras fallos controlados");
    return EXIT_SUCCESS;
}
