# Formulario persistente — entrega 11

La entrega 10 conserva su ejemplo y sus pruebas. La entrega 11 añade
`scripts/formulario_nombre_persistente.lat`, que usa el mismo host, runtime y
callbacks. La E/S permanece en el módulo `archivo` de Latino-Core.

## Comportamiento

- Primer uso: campo vacío y mensaje para introducir un nombre.
- Guardar o Enter: recorta espacios, valida al menos dos puntos de código y
  guarda el nombre. El campo admite como máximo 255 bytes UTF-8.
- Reabrir: recupera el último nombre guardado.
- Cancelar: relee el archivo y descarta la edición. Sin archivo, limpia el campo.
- Escape: restaura la edición actual según el contrato del campo de entrega 10;
  no borra el archivo ni cierra la ventana.
- Archivo inválido, demasiado largo o ilegible al iniciar: se muestra el problema
  y se bloquea Guardar durante esa sesión, para conservar el contenido original.
- Si el reemplazo se realizó pero no se confirmó su sincronización final, se
  muestra una advertencia. No se interpreta como archivo anterior conservado.

## Datos y ejecución

Primero ejecutar:

```bash
bash scripts/comprobar_persistencia_ui.sh /home/tuxmatic/latino-core
```

Ejecuta nueve suites Core, recompila ambos proyectos en directorios
`build-asan-persistencia11` y ejecuta quince pruebas UI con ASan/UBSan y detección
de fugas activada. No instala nada en el sistema. Requiere Linux, compilador C,
CMake, Raylib y Readline de desarrollo ya usados para entrega 10.

Después, desde cualquier directorio:

```bash
bash /home/tuxmatic/Proyectos/LatinoVox/scripts/probar_persistencia_ui.sh
```

El lanzador usa `$HOME/LatinoUI/datos-formulario11`, perteneciente al usuario y
con permisos 0700. Allí guarda `nombre-v1.txt`: una única cadena UTF-8 sin salto
final ni NUL. El nombre del archivo identifica esta versión del formato. El
lanzador imprime la ruta de datos y el registro. No sobreescribe datos de entrega
10, cuyo ejemplo solo mantenía el nombre en memoria. No abrir dos instancias
simultáneas sobre ese archivo: esta entrega no implementa bloqueo de escritores.

Para usar una ruta de Core diferente, pasarla como primer argumento al lanzador.
Ejecutar el script .lat directamente utiliza el directorio actual del proceso;
se recomienda el lanzador para mantener los datos fuera del repositorio.

## Aceptación manual en Linux

1. Escribir «María Ñáñez», Guardar, cerrar con la X.
2. Ejecutar de nuevo el mismo lanzador: comprobar que el nombre reaparece.
3. Modificar el nombre y pulsar Cancelar: debe volver el valor guardado.
4. Dejar un solo carácter e intentar guardar: debe mostrar la validación.
5. Cerrar y reabrir: el dato válido anterior debe seguir presente.
6. Conservar los registros y comprobar código 0 sin informes de los sanitizadores.

La prueba 15 realiza esos recorridos con VM, script, disco y host reales; simula
solo la ventana y el teclado. También intenta guardar tras encontrar UTF-8
inválido, NUL, exceso de longitud, salto de línea y un directorio en lugar del
archivo. Los casos no modifican los datos originales. Usan un directorio temporal
propio y lo retiran al terminar correctamente.

## Lo que aún falta para el producto

Esta es una prueba vertical de persistencia de un campo, no el CRUD de libros.
El siguiente formato necesitará versión, IDs estables, campos vacíos, escapes,
validación de registros y tratamiento de datos dañados. No usar cadena.separar
como lector CSV ni usar listas/diccionarios extensivamente sin probar su propiedad.

Después: altas/búsquedas/edición/bajas desde Latino; CI con revisiones fijas y
pruebas Debug/Release; selección, cursor, portapapeles y fuentes explícitas;
instalación y ejemplo externo reproducibles. El presupuesto temporal de callbacks
es cooperativo; el guardado actual es síncrono y no garantiza latencia máxima.
La recarga repetida de programas y los errores fatales de VM siguen fuera de la
aceptación de este formulario. No se declara completada una Alfa-1 distribuible.
