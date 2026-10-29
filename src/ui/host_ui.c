#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "raylib.h"
#include "latino.h"
#include "latast.h"
#include "latmv.h"
#include "latobj.h"
#include "ui_runtime.h"
#include "ui_utf8.h"
#include "ui_eventos.h"
#include "ui_callbacks.h"
#include "ui_despachador.h"
#include "ui_biblioteca.h"

static UIRuntime ui;
static int siguiente_etiqueta = 10000;

static int *ultimo_clic_ptr = NULL;

/* El despachador es independiente de Latino; este adaptador traduce
   los estados del puente FFI al contrato del despachador. */
static UIDespachoCallbackResultado host_despachar_evento(void *usuario, UIEvento evento)
{
    UICBContexto *cb = (UICBContexto *)usuario;
    UICBResultado r = ui_cb_despachar_evento(cb, "__ui_despachar", evento);
    if (r == UI_CB_OK) {
        if (ultimo_clic_ptr && evento.tipo == UI_EVENTO_CLIC) *ultimo_clic_ptr = evento.id_widget;
        return UI_CB_DESP_OK;
    }
    if (r == UI_CB_FUNCION_INEXISTENTE || r == UI_CB_FIRMA_INVALIDA ||
        r == UI_CB_ARGUMENTO_INVALIDO) {
        fprintf(stderr, "LatinoUI: callback rechazado (codigo %d)\n", (int)r);
        return UI_CB_DESP_CONTRATO;
    }
    fprintf(stderr, "LatinoUI: VM no reutilizable (codigo %d)\n", (int)r);
    return UI_CB_DESP_VM_ERROR;
}


static void n_titulo(lat_mv *mv) {
    const char *texto = latC_checar_cadena(mv, latC_desapilar(mv));
    (void)lui_titulo(&ui, texto);
}
static void n_etiqueta(lat_mv *mv) {
    const char *texto = latC_checar_cadena(mv, latC_desapilar(mv));
    (void)lui_agregar_control(&ui, siguiente_etiqueta++, UI_WIDGET_ETIQUETA, texto);
}
static int leer_id(lat_mv *mv, lat_objeto *o) {
    if (o->tipo != T_NUMERIC) return -1;
    double n = latC_checar_numerico(mv, o);
    return isfinite(n) && n >= 0 && n < 10000 && floor(n) == n ? (int)n : -1;
}
static void crear_control(lat_mv *mv, UITipoWidget tipo) {
    lat_objeto *o_texto=latC_desapilar(mv);
    lat_objeto *o_id=latC_desapilar(mv);
    const char *texto=latC_checar_cadena(mv,o_texto);
    (void)lui_agregar_control(&ui,leer_id(mv,o_id),tipo,texto);
}
static void n_boton(lat_mv *mv) { crear_control(mv,UI_WIDGET_BOTON); }
static void n_campo(lat_mv *mv) { crear_control(mv,UI_WIDGET_CAMPO_TEXTO); }
static void n_etiqueta_id(lat_mv *mv) { crear_control(mv,UI_WIDGET_ETIQUETA); }
static void n_texto(lat_mv *mv) {
    const char *texto=lui_texto(&ui,leer_id(mv,latC_desapilar(mv)));
    if (texto) latC_apilar_string(mv,texto);
    else latC_apilar(mv,latO_falso);
}
static void n_poner_texto(lat_mv *mv) {
    lat_objeto *valor=latC_desapilar(mv);
    int id=leer_id(mv,latC_desapilar(mv));
    bool ok=valor->tipo==T_STR && lui_establecer_texto(&ui,id,latC_checar_cadena(mv,valor));
    latC_apilar(mv,ok ? latO_verdadero : latO_falso);
}
static void n_longitud_texto(lat_mv *mv) {
    const char *texto=lui_texto(&ui,leer_id(mv,latC_desapilar(mv)));
    size_t n=0;
    if (!ui_utf8_validar(texto,&n)) { latC_apilar(mv,latO_falso); return; }
    latC_apilar_double(mv,(double)n);
}
static void n_fila(lat_mv *mv) {
    double espacio = latC_checar_numerico(mv, latC_desapilar(mv));
    (void)lui_iniciar_contenedor(&ui, UI_NODO_FILA, (float)espacio);
}
static void n_columna(lat_mv *mv) {
    double espacio = latC_checar_numerico(mv, latC_desapilar(mv));
    (void)lui_iniciar_contenedor(&ui, UI_NODO_COLUMNA, (float)espacio);
}
static void n_fin(lat_mv *mv) { (void)mv; (void)lui_fin_contenedor(&ui); }

static const lat_CReg libui[] = {
    {"titulo", n_titulo, 1},
    {"etiqueta", n_etiqueta, 1},
    {"boton", n_boton, 2},
    {"campo_texto", n_campo, 2},
    {"etiqueta_id", n_etiqueta_id, 2},
    {"texto", n_texto, 1},
    {"poner_texto", n_poner_texto, 2},
    {"longitud_texto", n_longitud_texto, 1},
    {"iniciar_fila", n_fila, 1},
    {"iniciar_columna", n_columna, 1},
    {"fin_contenedor", n_fin, 0},
    {NULL, NULL, 0}
};

static void mostrar_uso(FILE *salida, const char *programa) {
    fprintf(salida, "Uso: %s [archivo.lat]\n"
                    "     %s --ayuda\n"
                    "Sin archivo se carga scripts/app_ui.lat desde el directorio actual.\n",
            programa, programa);
}

int main(int argc, char **argv) {
    const char *ruta = "scripts/app_ui.lat";
    if (argc == 2 && (!strcmp(argv[1], "--ayuda") || !strcmp(argv[1], "--help"))) {
        mostrar_uso(stdout, argv[0]);
        return EXIT_SUCCESS;
    }
    if (argc == 3 && !strcmp(argv[1], "--") && argv[2][0]) {
        ruta = argv[2];
    } else if (argc == 2 && argv[1][0] && argv[1][0] != '-') {
        ruta = argv[1];
    } else if (argc != 1) {
        mostrar_uso(stderr, argv[0]);
        return EXIT_FAILURE;
    }

    lat_mv *mv = NULL;
    ast *nodo = NULL;
    lat_objeto *funcion = NULL;
    UIBiblioteca *biblioteca = NULL;
    UICBContexto callbacks = {0};
    UIDespachador despachador = {0};
    bool ventana_abierta = false;
    bool vm_comprometida = false;
    int salida = EXIT_FAILURE;
    int ultimo_clic = -1;
    siguiente_etiqueta = 10000;
    ultimo_clic_ptr = NULL;
    if (!lui_inicializar(&ui)) goto cerrar;

    UIEvento memoria_eventos[64] = {0};
    UIColaEventos cola_eventos = {0};

    if (ui_eventos_inicializar(
            &cola_eventos, memoria_eventos, 64)
            != UI_EVENTOS_OK) {
        fprintf(stderr, "Error al iniciar la cola\n");
        goto cerrar;
    }
    mv = latC_crear_mv();
    if (!mv) { fprintf(stderr, "No se pudo crear VM\n"); goto cerrar; }
    latC_abrir_liblatino(mv, "ui", libui);
    biblioteca = ui_biblioteca_crear(mv, NULL);
    if (!biblioteca) { fprintf(stderr, "LatinoUI: no se pudo crear biblioteca\n"); goto cerrar; }
    ui_biblioteca_conectar_ui(biblioteca, &ui);
    ultimo_clic_ptr = &ultimo_clic;
    if (ui_cb_inicializar(&callbacks, mv) != UI_CB_OK ||
        ui_cb_registrar(&callbacks) != UI_CB_OK) {
        fprintf(stderr, "LatinoUI: no se pudo registrar ui_cb\n");
        goto cerrar;
    }
    if (ui_despachador_inicializar_eventos(&despachador, &cola_eventos,
                                  host_despachar_evento, &callbacks) != UI_DESP_OK) {
        fprintf(stderr, "LatinoUI: no se pudo iniciar despachador\n");
        goto cerrar;
    }

    mv->nombre_archivo = (char *)ruta;
    mv->global->REPL = false;
    int estado = 0;
    nodo = latA_analizar_arch((char *)ruta, &estado);
    if (estado || !nodo) {
        fprintf(stderr, "No se pudo analizar %s\n", ruta);
        goto cerrar;
    }
    funcion = latC_analizar(mv, nodo);
    if (!funcion) {
        fprintf(stderr, "No se pudo preparar script\n");
        goto cerrar;
    }
    int resultado = latC_llamar_funcion(mv, funcion);
    if (resultado || mv->ptrpila != 0 || mv->ptrctx != 0 ||
        mv->tope != mv->pila || mv->contexto_actual != mv->contexto[0]) {
        vm_comprometida = true;
        fprintf(stderr, "LatinoUI: fallo al ejecutar el programa inicial\n");
        goto cerrar;
    }
    if (!lui_validar(&ui)) {
        fprintf(stderr, "LatinoUI: estructura invalida: %s\n", ui.error);
        goto cerrar;
    }

    /* El AST y el programa permanecen vivos durante todos los callbacks.
       Algunas referencias del bytecode son prestadas por el AST. */

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(900, 600, ui.titulo);
    ventana_abierta = IsWindowReady();
    if (!ventana_abierta) {
        fprintf(stderr, "LatinoUI: no se pudo abrir la ventana\n");
        goto cerrar;
    }
    SetExitKey(KEY_NULL);
    SetWindowMinSize(600, 400);
    UIEvento inicial;
    (void)lui_tecla(&ui,UI_TECLA_TAB,&inicial);
    SetTargetFPS(60);
    bool aviso_vm_comprometida = false;
   for (;;) {
        if (WindowShouldClose()) {
            if (!ui_biblioteca_protege_cierre(biblioteca) || despachador.estado_vm != UI_VM_SANA) break;
            ui_biblioteca_pedir_cierre(biblioteca);
        }
        if (ui_biblioteca_consumir_cierre(biblioteca)) break;
        ui_biblioteca_sincronizar_ui(biblioteca);
        UIRect area = {24.0f, 70.0f, (float)GetScreenWidth() - 48.0f,
                       (float)GetScreenHeight() - 110.0f};
        bool valido = lui_calcular(&ui, area);
        if (valido && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            Vector2 p = GetMousePosition();
            for (size_t i = 0; i < ui.arbol.cantidad; i++) {
                if (ui.nodos[i].tipo != UI_NODO_WIDGET || ui.tipos[i] == UI_WIDGET_ETIQUETA || !ui.habilitados[i]) continue;
                UIRect b = ui.rectangulos[i];
                Rectangle r = {b.x, b.y, b.ancho, b.alto};
                if (CheckCollisionPointRec(p, r)) {
                   (void)lui_enfocar(&ui,ui.nodos[i].id_widget);
                   if (ui.tipos[i] == UI_WIDGET_CAMPO_TEXTO) break;
                   UIEvento nuevo = {
                       UI_EVENTO_CLIC,
                       ui.nodos[i].id_widget
                   };

                   UIDespachoResultado estado_cola =
                       ui_despachador_encolar(&despachador, nuevo);

                   if (estado_cola != UI_DESP_OK) {
                       fprintf(stderr,
                           "No se pudo encolar evento %d (codigo %d)\n",
                           nuevo.id_widget, (int)estado_cola);
                   }
                    break;
                }
            }
        }

       if (valido && despachador.estado_vm == UI_VM_SANA) {
           /* Un solo gesto de control por frame. La escritura queda en C;
              consultar ui.texto() es lo que crea una cadena en la VM. */
           UIEvento evento;
           bool emitir=false;
           if (IsKeyPressed(KEY_TAB))
               emitir=lui_tecla(&ui,(IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT))
                   ? UI_TECLA_RETROTAB : UI_TECLA_TAB,&evento);
           else if (IsKeyPressed(KEY_ESCAPE)) {
               if (ui_biblioteca_protege_cierre(biblioteca)) {
                   evento=(UIEvento){UI_EVENTO_CANCELAR,204}; emitir=true;
               } else emitir=lui_tecla(&ui,UI_TECLA_ESCAPE,&evento);
           }
           else {
               for (int punto=GetCharPressed();punto>0;punto=GetCharPressed())
                   (void)lui_caracter(&ui,(unsigned int)punto);
               if (IsKeyPressed(KEY_BACKSPACE)) (void)lui_tecla(&ui,UI_TECLA_BORRAR,&evento);
               if (IsKeyPressed(KEY_ENTER)) emitir=lui_tecla(&ui,UI_TECLA_ENTER,&evento);
           }
           if (emitir && ui_despachador_encolar(&despachador,evento)!=UI_DESP_OK)
               fprintf(stderr,"LatinoUI: cola llena; evento de teclado rechazado\n");
       }

       ui_biblioteca_sincronizar_ui(biblioteca);
       UIDespachoResultado estado_frame =
           ui_despachador_procesar_frame(&despachador);
       if (estado_frame == UI_DESP_ERR_VM_COMPROMETIDA) {
           if (!aviso_vm_comprometida) {
               fprintf(stderr, "LatinoUI: despachador bloqueado por fallo de VM\n");
               aviso_vm_comprometida = true;
           }
       } else if (estado_frame == UI_DESP_ERR_CONTRATO) {
           fprintf(stderr, "LatinoUI: evento rechazado por contrato\n");
       } else if (estado_frame == UI_DESP_ERR_RELOJ) {
           fprintf(stderr, "LatinoUI: reloj del despachador no disponible\n");
       }


       if (ui_biblioteca_consumir_cierre(biblioteca)) break;
       BeginDrawing();
        ClearBackground(RAYWHITE);
        DrawText(ui.titulo, 24, 19, 28, DARKBLUE);
        if (!valido) {
            DrawText("Espacio insuficiente para layout", 24, 85, 24, MAROON);
        } else {
            for (size_t i = 0; i < ui.arbol.cantidad; i++) {
                UIRect b = ui.rectangulos[i];
                Rectangle r = {b.x, b.y, b.ancho, b.alto};
                if (ui.nodos[i].tipo == UI_NODO_WIDGET) {
                    bool boton = ui.tipos[i] == UI_WIDGET_BOTON;
                    DrawRectangleRec(r, boton && ui.habilitados[i] ? SKYBLUE : (Color){231, 239, 247, 255});
                    DrawRectangleLinesEx(r, ui.foco==i ? 3.0f : 1.0f,
                                         ui.foco==i ? DARKBLUE : GRAY);
                    BeginScissorMode((int)r.x, (int)r.y, (int)r.width, (int)r.height);
                    DrawText(ui.textos[i], (int)b.x + 9, (int)b.y + 8, 24, ui.habilitados[i] ? DARKBLUE : GRAY);
                    EndScissorMode();
                } else {
                    DrawRectangleLinesEx(r, 1.0f, LIGHTGRAY);
                }
            }
        }
        DrawText(TextFormat("Ultimo clic: %d", ultimo_clic), 24, GetScreenHeight() - 31, 20, DARKGRAY);
        EndDrawing();
    }
    salida = EXIT_SUCCESS;

cerrar:
    if (ventana_abierta) CloseWindow();
    ui_cb_desregistrar(&callbacks);
    ultimo_clic_ptr = NULL;
    ui_biblioteca_destruir(biblioteca);
    if (vm_comprometida || despachador.estado_vm == UI_VM_COMPROMETIDA ||
        callbacks.inutilizada) {
        /* Tras longjmp, Latino no garantiza liberar con seguridad esta VM.
           El SO recuperara los recursos al terminar el proceso. */
        fprintf(stderr, "LatinoUI: cierre con VM comprometida; se omite destructor\n");
        return EXIT_FAILURE;
    }
    if (mv) {
        if (funcion) latO_destruir(mv, funcion);
        latC_destruir_mv(mv);
    }
    if (nodo) latA_destruir(nodo);
    return salida;
}
