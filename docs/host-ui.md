# Host autonomo de LatinoUI: ciclo de vida y ejecucion

Requiere Latino-Core con constants08 (commit `2722523`) o una version
compatible. Las cabeceras y `liblatino.so` deben corresponder a la misma
version: los cambios internos de `lat_mv` requieren recompilar consumidores.

## Ejecutar una aplicacion

Desde cualquier directorio, indicar rutas absolutas:

```bash
/ruta/LatinoVox/build-asan-host09/host_ui /ruta/mi_aplicacion.lat
```

`host_ui --ayuda` muestra el uso. Sin argumentos se conserva el comportamiento
anterior: cargar `scripts/app_ui.lat` desde el directorio de trabajo. `--`
permite indicar un archivo cuyo nombre empieza por guion.

La API de este paso sigue ofreciendo `ui.titulo`, `ui.etiqueta`,
`ui.boton(id, texto)`, `ui.iniciar_fila`, `ui.iniciar_columna` y
`ui.fin_contenedor`. El callback de clic es `__ui_despachar()`; obtiene el
identificador con `ui_cb.id_actual()`. Un ejemplo completo esta en
`scripts/app_ui.lat`. La entrada de texto y el foco son el siguiente hito.

## Propiedad y cierre

El host conserva el programa compilado, el AST y la ruta durante la sesion.
Al finalizar cierra la ventana, retira el registro de callbacks, destruye el
envoltorio del programa y la VM, y finalmente libera el AST. Asi las
referencias prestadas por el AST siguen vigentes mientras se usa la VM.
Los fallos de carga y de estructura de interfaz tambien retiran el registro
FFI, de modo que una sesion posterior no encuentre un registro colgante.

Una VM interrumpida por un error de ejecucion (`longjmp`) no se considera
reutilizable. Se mantiene la politica conservadora de salida con codigo 1 y
omision del destructor de esa VM; el sistema operativo recupera sus recursos
al terminar el proceso. Ahora esa politica tambien se aplica si falla la
ejecucion inicial. Esta entrega no acredita ausencia de fugas en esa ruta,
recuperacion tras errores del compilador, recarga en una misma VM ni sesiones
con crecimiento de memoria acotado. La API actual admite un registro FFI
activo por proceso y no ejecucion concurrente de hosts.

## Comprobacion automatizada

```bash
bash scripts/comprobar_host_ui.sh /ruta/latino-core
```

El script reconstruye Core y LatinoVox en `build-asan-host09`, activa
ASan/UBSan y deteccion de fugas y ejecuta CTest. La suite tiene ahora doce
casos. El nuevo `latino_ui_host_ciclo_vida` compila **el mismo** `host_ui.c`
con un backend de ventana simulado: Core, layout, eventos y callbacks son
reales. Comprueba la ruta elegida, ayuda y argumentos, veinte sesiones con
quinientos callbacks, fallo de apertura, archivo ausente, estructura de UI
invalida y apertura de una nueva sesion despues de esos fallos.

La simulacion no prueba OpenGL, fuentes ni interaccion visual. El ejecutable
`host_ui` de produccion sigue enlazado con Raylib real; los sustitutos graficos
se incluyen solo en el destino de pruebas.

## Comprobacion visual

Abrir `scripts/app_ui.lat`, pulsar Guardar y Cancelar, redimensionar y cerrar.
Cada clic debe producir un mensaje del callback y actualizar el ultimo ID
mostrado. El cierre normal debe devolver 0 y el registro no debe contener
errores de ASan/UBSan ni fugas. Conservar cualquier diagnostico completo.
