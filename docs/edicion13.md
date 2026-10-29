# Entrega13 revisada: edición protegida sobre Biblioteca12

Esta entrega transforma la propuesta en un cambio incremental sobre los archivos
validados de Biblioteca12. La aplicación mantiene un catálogo único en C y la
pantalla declarada en Latino; el controlador de las decisiones vive en el puente
`ui_biblioteca`. No es todavía un diálogo genérico para cualquier aplicación.

## Diagnóstico de la propuesta

| Hallazgo | Consecuencia | Decisión aplicada |
|---|---|---|
| Sustituir `ui_catalogo.h` elimina ruta, backend de E/S, estado de apertura y resultados ya existentes. El -10 propuesto para bloqueo ya significa error de memoria. | Ruptura de consumidores y pérdida de garantías de Biblioteca12. | Se conserva la API y el formato V1; se añade bloqueo como -17 y la sesión en `ui_edicion.h/.c`. |
| Un `flock` tomado sólo al persistir no valida la copia leída antes. | A y B pueden cargar el mismo catálogo; B sobrescribe después lo guardado por A, aunque sus escrituras nunca se solapen. | En el backend nativo de `biblioteca.*`, adquirir antes de cargar y conservar hasta destruir la sesión. |
| Eliminar el `.lock` al liberar permite que procesos con descriptores abiertos y otros nuevos usen inodes distintos. | Dos escritores pueden creer que poseen exclusividad. | Sidecar permanente, cierre del descriptor al terminar; nunca borrar para desbloquear. |
| `texto_actual[4][96]` y copia truncada. | El título puede recortarse antes de validar; los límites de código, autor y año se confunden con el de título. | Borradores de 256 bytes, iguales al runtime; al guardar se validan los límites del catálogo. Un error conserva íntegro el borrador. |
| Comparar sólo revisiones no detecta que alguien volvió al texto original. | “Cambios pendientes” incorrecto. | Comparar contenido al mutar y mantener una máscara; consultar la máscara es O(1). Revisiones saturadas, sólo diagnósticas. |
| Acción pendiente reemplazable y autorización de cierre persistente. | La decisión puede aplicarse a otro destino o reutilizarse después. | Una petición capturada por diálogo; también se captura la búsqueda. El cierre se consume una vez. |
| Estado 2 seguido de cierre inmediato. | La advertencia desaparece antes de que pueda leerse. | Datos activados y formulario limpio, aviso independiente. Antes de cerrar: “Cerrar con aviso” o “Continuar editando”. Un guardado posterior confirmado retira el aviso. |
| Confirmación sólo en el script, con un supuesto evento 9. | El `while (!WindowShouldClose())` puede salir sin consultar la decisión. | El host retiene la petición nativamente y sigue renderizando. Se conservan los eventos existentes 1/2/3; no se introduce un número ficticio de cierre. |
| Prueba de UTF-8 que sólo mira el último byte. | `F0 9F` la supera a pesar de ser una secuencia incompleta. | Reutilizar `ui_utf8_copiar_seguro`; pruebas de validez completa con fronteras de 1–4 bytes y entradas malformadas. |
| `assert` con operaciones dentro y rutas `/tmp` fijas. | Release puede omitir la prueba; ejecuciones paralelas interfieren. | Pruebas nuevas con CHECK/EXIGIR siempre activos y directorios `mkdtemp`. |
| Copia de respaldo mediante `fopen(...,"wb")`. | Sobrescribe un respaldo previo y puede dañar el original si coinciden las rutas. | Esa función propuesta no se incorpora. La recuperación tendrá una entrega independiente con creación exclusiva, verificación y pruebas de errores. |

El script propuesto también usaba nombres de UI y eventos que no corresponden a
la implementación actual y reintroducía catálogos paralelos en listas Latino.
Se integra la pantalla con `biblioteca.activar_edicion()` y `biblioteca.accion()`;
los métodos antiguos permanecen disponibles para aplicaciones anteriores, pero
no permiten saltarse la protección cuando la sesión gestionada está activada.

No se cambia el rango de años V1 (1–9999) ni el manejo existente de IDs agotados.
La búsqueda conserva su contrato anterior: comparación sin distinción de
mayúsculas ASCII; no promete normalización Unicode ni equivalencia entre `Á` y
`á`. Esto requiere una decisión adicional de producto y pruebas específicas.

## Comportamiento de la edición

- Anterior, Siguiente, Buscar, Nuevo, Cancelar la ficha, Eliminar y cerrar consultan
  los cambios pendientes. Guardar, Descartar y Continuar son decisiones separadas.
- Campo con el mismo texto: sin nueva revisión. Volver al original: se limpia el
  bit correspondiente. Editar se compara en O(longitud); sólo consultar si está
  sucio es O(1). Se sincronizan cuatro buffers acotados por fotograma en C.
- Estado -1 o 0 / validación fallida: conserva texto y acción pendiente. Continuar
  vuelve al campo inválido cuando se conoce, para corregirlo sin perder el destino
  inadvertidamente; luego se solicita de nuevo la navegación deseada.
- Estado 1: activa catálogo y base. Estado 2: también activa ambos, pero conserva
  un aviso separado. No se intenta revertir un archivo ya reemplazado.
- El borrado requiere el botón 205, distinto de Eliminar 203. El foco inicial está
  en Cancelar 204. La confirmación guarda el ID concreto, no un índice mutable.
- Los controles del fondo quedan deshabilitados y fuera de Tab. También se
  rechazan sus eventos al despacharlos, aunque estuvieran encolados previamente.
- Escape en Biblioteca13 solicita cancelación/restauración sin descartar primero
  el buffer del campo. En un diálogo vuelve a editar o cancela el borrado.
- El cierre es una petición del host, no una salida inmediata ni una llamada a
  `exit()`. No se garantiza preservar borradores ante un proceso terminado por
  fuerza, un fallo de VM o una caída del sistema.

## Archivos y compatibilidad

Core cambia únicamente `src/latsyslib.c` y añade la suite `system`:
comprobar el retorno de `getcwd`, devolver nulo para selectores de versión no
válidos y apilar resultados sin dejar envoltorios temporales. Se mantiene el
nombre público existente `latOS_veriones` para no romper consumidores.

UI añade `ui_edicion` y `ui_catalogo_lock`, adapta `ui_biblioteca`, runtime y host,
actualiza `app_biblioteca.lat` y añade tres casos CTest. La pantalla anterior se
conserva como fixture `tests/datos/biblioteca12_compat.lat`: sus pruebas no se
reemplazan por otras más débiles. El toast de `src/main.c` reutiliza el copiador
UTF-8 existente y conserva el tiempo y contenido originales.

`UIRuntime` incorpora habilitación de controles: recompilar sus consumidores.
No hay migración de `catalogo-v1.tsv`. No se instala ninguna biblioteca del
sistema ni se modifica el esquema de propiedad de colecciones de Latino-Core.

## Exclusión entre escritores: alcance exacto

La integración nativa de `biblioteca.*` abre `<ruta>.lock` con CLOEXEC y NOFOLLOW,
comprueba que sea un archivo regular del usuario y toma el bloqueo no bloqueante
antes de cargar. Sólo EWOULDBLOCK/EAGAIN se informa como ocupado; los demás fallos
son errores de E/S. El descriptor se cierra al destruir el puente, incluso si la
carga del catálogo falló. La reserva se conserva durante toda la sesión.

Esto es una política de un único editor para Linux, almacenamiento local y un
directorio privado estable. Todos los escritores deben respetarla. No protege
contra ediciones manuales, binarios antiguos, directorios sustituidos o garantías
no verificadas de NFS/SMB. El modelo de catálogo de bajo nivel sigue declarando
uso de un solo escritor; quien lo use directamente o inyecte otro backend debe
proporcionar su propia exclusión. Los backends de prueba no usan locks físicos.

**Cierra las instancias de Biblioteca12 antes de probar Biblioteca13.** El archivo
`.lock` vacío que queda en disco no es un bloqueo huérfano: la posesión está en el
descriptor del proceso. No lo borres mientras existan sesiones.

## Validación realizada

| Comprobación local | Resultado y límite |
|---|---|
| Core: hash, context, ffi, bytecode, label, modules, constants, strings_io, atomic_io, object_text, system | 11 suites aprobadas con ASan/UBSan; fugas desactivadas en este entorno por la limitación siguiente. |
| CTest instrumentado | 23/23 aprobadas, 1,09 s en esta ejecución; host y script reales con Raylib simulado. |
| CTest Release | 23/23 aprobadas, 0,33 s; comprobaciones de prueba activas. |
| C nuevo de edición, lock, puente y runtime | Compilación de comprobación C11, O2, Wall/Wextra/Wpedantic/Werror sin diagnósticos. |
| Sistema | `getcwd` satisfactorio y fallido sin inicializar el buffer, selectores válidos/inválidos y 200 ciclos sin aumento del saldo de VM. |
| Edición | Borrador largo sin truncar, revisión saturada, reversión, validación, petición congelada, consumo único y estados -1/0/1/2. |
| Host13 | Navegación, búsqueda capturada, foco, borrado separado, Escape, cierre retenido, descarte explícito y aviso de sincronización antes de cerrar. |
| VM persistente | 100 fotogramas de edición posteriores al calentamiento sin aumento de cadenas internadas ni saldo de VM en el escenario. No sustituye un detector de fugas. |
| Lock | Dos procesos reales; bloqueo antes de existir el catálogo; inode persistente; CLOEXEC; rechazo de enlace; liberación al terminar el dueño. |
| LeakSanitizer | Intento registrado: no puede funcionar bajo ptrace aquí. No se acredita ausencia de fugas de Entrega13. El validador para tu equipo fuerza `detect_leaks=1`. |
| Raylib real y `motor` gráfico | No disponibles en este entorno. El cambio de toast se revisó y su helper se probó; queda compilar todos los destinos y probar la ventana en tu equipo. |

Los tiempos son observaciones, no presupuestos de latencia ni benchmarks. La
compilación completa de Core todavía muestra advertencias históricas fuera de
este cambio. No se afirma que todo el proyecto compile sin advertencias.

Los 20/20 de Biblioteca12 y la ventana con salida 0 que compartiste son evidencia
de la base; no prueban por sí mismos esta nueva entrega.

## Ruta de avance tras aceptar esta entrega

1. Validar localmente 11 suites y 23 CTest con detección de fugas, después el flujo
   visual descrito en README. Registrar las revisiones emparejadas de ambos repos.
2. Recuperación explícita: respaldo creado con exclusividad y sin sobrescribir;
   comprobar copia completa, errores de lectura/cierre/sincronización y que el
   original permanezca intacto. Reinicializar sólo por una decisión independiente.
3. Editor: cursor, selección, portapapeles, borrado adelante y recorte/scroll. Definir
   si se opera por punto de código o grafema; UTF-8 válido no equivale a texto
   correctamente segmentado para edición. Probar también caracteres combinantes.
4. Tipografía/escala: fuente redistribuible con licencia incluida, medición y dibujo
   coherentes y pruebas a varias escalas. Una resolución 3072×1728 no demuestra DPI
   correcto; 224 glifos tampoco acreditan cobertura Unicode completa.
5. Extraer el controlador de Biblioteca del host y ofrecer extensiones de aplicación;
   API pública, instalación CMake relocalizable y prueba de consumidor externo.
6. CI con dependencias/revisiones verificadas y fijadas, Debug instrumentado y Release,
   instalación desde árbol limpio y documentación de límites antes de etiquetar alfa.

## Referencias primarias

- Linux `flock(2)`: locks consultivos, descripciones de archivo abiertas y liberación
  al cerrar los descriptores: https://man7.org/linux/man-pages/man2/flock.2.html
- Linux `getcwd(3)`: retorno NULL en fallo y contenido del buffer indefinido:
  https://man7.org/linux/man-pages/man3/getcwd.3.html
- Unicode UAX #29, límites de texto y grafemas:
  https://www.unicode.org/reports/tr29/
- CMake, paquetes para consumidores:
  https://cmake.org/cmake/help/latest/guide/importing-exporting/index.html

La conclusión es una entrega verificable de protección de edición. Las etapas
restantes tienen criterios de aceptación, sin presentar características propuestas
como si ya hubieran sido integradas y probadas.
