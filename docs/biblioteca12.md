# Biblioteca12: modelo único, aplicación Latino y almacenamiento

Entrega experimental incremental sobre Persistencia11 y Objetos11B. La pantalla
y sus acciones se escriben en `scripts/app_biblioteca.lat`. `biblioteca.*` ofrece
un modelo nativo por VM, independiente de Raylib, y utiliza el motor de archivos
de Latino-Core. No se modifica C para cada pantalla.

## Archivos

| Archivo | Función |
| --- | --- |
| `include/ui_catalogo.h`, `src/ui/ui_catalogo.c` | Validación, formato, IDs y transacciones del modelo |
| `src/ui/ui_catalogo_archivo.c` | Adaptador a las dos funciones `latF_*` de Core |
| `include/ui_biblioteca.h`, `src/ui/ui_biblioteca.c` | Registro por VM y API Latino |
| `scripts/app_biblioteca.lat` | Formulario, búsqueda, navegación y confirmación de borrado |
| `tests/test_ui_catalogo.c` | Estados de E/S, parser, límites y archivos reales |
| `tests/test_ui_biblioteca.c` | Host y programa Latino reales con entradas gráficas simuladas |
| `cmake/LatinoUICatalogo.cmake` | Bibliotecas y cinco casos CTest nuevos |

## Contrato del modelo C

Inicializar siempre con `ui_catalogo_inicializar()` antes de usar la estructura.
Debe abrirse un catálogo válido o una ruta ausente antes de guardar. Una instancia
queda asociada a una ruta; para cambiarla, reinicializar. No modificar miembros
manualmente ni compartir una instancia entre hilos.

La carga se valida completa antes de sustituir el modelo. Un fallo conserva los
datos activos y bloquea posteriores escrituras. Una ruta ausente en la primera
apertura empieza vacía; si desaparece después de abrirla, la recarga falla y
conserva los IDs. Abrir otra vez un archivo corregido permite recuperar la
instancia. La aplicación inicial no incluye un editor de recuperación.

Registrar, actualizar y eliminar construyen una copia candidata en heap. Solo
los estados atómicos 1 y 2 publican esa copia. Los errores dejan intactos el
catálogo anterior y el parámetro `id_out`. Los resultados del catálogo son
`UI_CAT_OK = 0`, `UI_CAT_ADVERTENCIA_SYNC = 1` y errores negativos; no confundirlos
con los estados del escritor atómico, cuyos valores son -1, 0, 1 y 2.

Los punteros devueltos por las búsquedas son prestados: no liberarlos ni
conservarlos después de una mutación, carga o reinicialización. El texto de
`ui_catalogo_serializar()` se libera con `free()`.

`ui_catalogo_inicializar_con_io()` permite un backend por instancia. El contexto
se presta y debe seguir vivo. Los callbacks no retienen el texto escrito, y el
modelo entrega cada buffer leído a `liberar`, incluso si la lectura falló.
Esto permite inyectar errores sin una variable global en producción.

## Formato y límites

```text
LATINOUI-CATALOGO-V1\t<siguiente_id>\t<total>\n
<id>\t<codigo>\t<titulo>\t<autor>\t<anio>\n
```

Los símbolos `\t` y `\n` representan bytes de tabulador y LF, no los dos
caracteres barra y letra. Se exige LF final, tres columnas de cabecera y cinco
por registro, sin filas ni columnas adicionales. El parser rechaza NUL interno,
UTF-8 inválido, truncamiento, recuentos incorrectos, IDs o códigos duplicados.

- Máximo: 256 libros y 64 KiB por archivo.
- Código: 35 bytes; título: 95; autor: 79. Las capacidades del encabezado
  incluyen además un byte NUL. Una letra acentuada puede ocupar varios bytes.
- Años: enteros de 1 a 9999. Es una política explícita de esta aplicación,
  ampliada respecto a 1450–2100 para admitir obras antiguas; puede ajustarse en
  las constantes y sus pruebas antes de fijar la primera versión pública.
- IDs: de 1 a `INT_MAX - 1`; `INT_MAX` se reserva como contador agotado. No se
  reutilizan IDs al eliminar, ni cuando se elimina el último libro. Restaurar
  una copia antigua del archivo puede retroceder su historial; no es un servicio
  de IDs globales entre copias independientes.
- Los enteros son decimales ASCII canónicos: sin signo, espacios, fracción,
  exponente ni ceros iniciales; el total admite exactamente `0`.
- No se admiten controles ASCII/C1, tabuladores, CR ni LF en los campos. Se
  rechazan cadenas vacías o compuestas solo por espacios ASCII. No se realiza
  normalización Unicode ni clasificación completa de espacios Unicode.
- Comillas, barra vertical y barra inversa se conservan. V1 no incorpora un
  mecanismo de escape para tabuladores o saltos de línea.

Con 256 entradas se conserva un vector sencillo: búsquedas lineales y validación
de unicidad O(n²), como máximo 32 640 pares por validación. Se evita añadir índices
o cachés que pudieran desincronizarse. El guardado hace fsync síncrono: no es una
garantía de latencia por fotograma.

## API para Latino

| Llamada | Resultado |
| --- | --- |
| `biblioteca.abrir(ruta)` | Resultado del catálogo; selecciona el primer libro si existe |
| `biblioteca.total()` | Cantidad de libros |
| `biblioteca.seleccion()` | ID seleccionado, o 0 |
| `biblioteca.seleccionar(id)` | Booleano; 0 prepara una ficha nueva |
| `biblioteca.mover(-1)` / `mover(1)` | ID seleccionado; navegación circular |
| `biblioteca.campo(columna)` | Texto: 0 código, 1 título, 2 autor, 3 año |
| `biblioteca.buscar(texto)` | Primer ID coincidente, o 0; busca código, título y autor |
| `biblioteca.registrar(codigo, titulo, autor, anio)` | Resultado del catálogo; año como cadena decimal |
| `biblioteca.actualizar(codigo, titulo, autor, anio)` | Actualiza el libro seleccionado |
| `biblioteca.eliminar()` | Elimina el seleccionado |
| `biblioteca.mensaje()` | Mensaje del último resultado de apertura, búsqueda o mutación |

La búsqueda ignora mayúsculas ASCII; las letras no ASCII se comparan por bytes.
No equivale a búsqueda Unicode sin acentos ni mayúsculas. El registro de puentes
por VM es monohilo; su destrucción se realiza antes de destruir la VM.

## Uso y prueba visual

```bash
bash scripts/comprobar_biblioteca_ui.sh /home/tuxmatic/latino-core
bash scripts/probar_biblioteca_ui.sh /home/tuxmatic/latino-core
```

El primer script ejecuta diez suites Core, compila ambos proyectos y ejecuta
CTest completo en Debug con ASan/UBSan/LSan y en Release. Mantiene las aserciones
de los tests anteriores activas en Release. El segundo usa un directorio privado
separado: `$HOME/LatinoUI/datos-biblioteca12/catalogo-v1.tsv`. No sustituye el
archivo `nombre-v1.txt` del formulario anterior.

1. Registrar dos libros con acentos y códigos distintos.
2. Comprobar que un código repetido y un año fraccionario se rechazan.
3. Editar un título y comprobar que el ID no cambia.
4. Buscar por título o autor y navegar con Anterior/Siguiente.
5. Pulsar Eliminar y después Cancelar: el libro debe seguir presente.
6. Pulsar Eliminar dos veces sobre el mismo seleccionado: debe eliminarse.
7. Cerrar, abrir de nuevo y verificar el catálogo y los IDs.

La confirmación es de dos acciones consecutivas sobre el ID seleccionado, no un
diálogo modal. Navegar o cancelar desarma la confirmación. La pantalla usa 26
nodos fijos y no agrega controles por libro. No abrir dos instancias escribiendo
el mismo archivo: la escritura atómica no evita actualizaciones perdidas.

## Evidencia y pendientes

En preparación: diez suites Core; 20/20 CTest Debug con ASan/UBSan y 20/20 Release.
Se verifican 100 ediciones en una sesión, pila/contextos equilibrados y contadores
estables tras calentamiento para las mismas cadenas reutilizadas. También se
comprueban reapertura, archivo corrupto protegido y estados -1/0/1/2 desde la UI.

Raylib se simula únicamente en los tests del host. Aquí no se construyó ni probó
el ejecutable gráfico real con Raylib: los scripts de destino sí lo compilan y
permiten probarlo. LeakSanitizer se detuvo por ptrace; no se certifica ausencia
de fugas con esta ejecución. Los registros previos del usuario con LSan cubren
11/11B, no certifican automáticamente las novedades de Biblioteca12.

La fuente sigue siendo la integrada del host. Quedan TTF/licencia, cobertura de
glifos, búsqueda Unicode, DPI, recorte visual, manejo de edición pendiente al
navegar, instalación externa y CI sobre revisiones publicadas. El CRUD no cierra
por sí solo la Alfa. Las copias de colecciones y el alcance de variables de Core
requieren un trabajo separado con pruebas de alias y semántica del lenguaje.
