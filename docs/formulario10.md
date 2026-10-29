# Formulario 10: texto, foco y eventos sobre Host09

Base: LatinoVox `773d788` con Host09 aplicado. Dependencia: Latino-Core
`2722523` con el parche `cadenas-io10`. Recompilar ambos proyectos y todos sus
consumidores: las estructuras públicas de LatinoUI han crecido. El parche no
cambia la estructura `lat_mv` de Core08.

## API disponible

| Función Latino | Resultado y contrato |
|---|---|
| `ui.campo_texto(id, texto)` | Declara un campo de una línea; permite texto vacío. |
| `ui.etiqueta_id(id, texto)` | Declara una etiqueta que se puede actualizar por ID. |
| `ui.texto(id)` | Cadena actual, o falso para un ID inexistente. |
| `ui.poner_texto(id, texto)` | Verdadero si actualizó; falso si el ID, UTF-8 o longitud son inválidos. |
| `ui.longitud_texto(id)` | Puntos de código, o falso si no existe el ID. No cuenta grafemas. |
| `ui_cb.tipo_actual()` | 1: clic, 2: Enter en campo, 3: Escape en campo. Solo dentro del callback. |

Se conservan las funciones anteriores y `ui_cb.id_actual()`. Los IDs públicos
son enteros entre 0 y 9999. Las etiquetas anónimas siguen usando IDs internos.
El renderizado y el foco usan ahora tipos explícitos, no rangos de IDs.

Capturar los valores de retorno: `actualizado = ui.poner_texto(3, "María")`.
En esta versión de Latino, ignorar el retorno de una llamada puede dejar un
valor en la pila del callback. El trampolín exige pila vacía y conserva su
cortacircuitos; este parche no modifica el compilador para corregir ese problema
general. `tipo` ya designa una función global; usar `ev_tipo` como variable.

## Interacción

- Tab y Shift+Tab recorren campos y botones circularmente, saltando etiquetas.
- El clic da foco. En botones también encola un clic; en campos inicia edición.
- La escritura se añade al final del campo. Retroceso elimina el último punto
  de código completo. No hay cursor intermedio, selección, portapapeles ni IME.
- Enter en un campo emite SUBMIT; en un botón emite CLIC.
- Escape en un campo restaura el texto que tenía al recibir foco (o el último
  texto establecido desde Latino) y emite CANCELAR. No cierra la ventana.
- El botón Cancelar del ejemplo vacía el formulario. Guardar recorta y valida
  el nombre y lo muestra en la etiqueta de estado; no guarda en disco.

La captura de cada tecla modifica exclusivamente el buffer C del campo. Solo
`ui.texto()` crea una cadena en la VM. Esto evita internar cada edición, pero
no garantiza memoria acotada si el programa consulta constantemente textos
nuevos: Core mantiene sus cadenas internadas hasta cerrar la VM.

Los eventos transportan tipo e ID, no una copia histórica del texto. Un
callback consulta el estado actual; no usarlo como registro de cada pulsación.
La cola conserva su límite de 64 eventos y rechaza los excedentes. Su
presupuesto es cooperativo: `max_callback_ms` mide duración observada, no puede
interrumpir un callback largo.

## Límites de texto y renderizado

Cada texto admite como máximo 255 bytes más el terminador. El runtime rechaza
textos completos demasiado largos o UTF-8 inválido, sin cortar un carácter.
El helper `ui_utf8_copiar_seguro()` puede copiar un prefijo completo para otros
consumidores y avisa con falso si no cupo todo. Rechaza secuencias truncadas,
codificaciones excesivamente largas, sustitutos UTF-16 y valores mayores que
U+10FFFF. El campo rechaza controles ASCII y saltos de línea.

UTF-8 válido no implica disponibilidad de glifos. Este parche conserva la
fuente predeterminada de Raylib; no añade una fuente con cobertura universal.
Acentos y ñ deben comprobarse visualmente en el equipo del usuario. Grafemas,
normalización, selección e IME quedan pendientes.

## Validación

`bash scripts/comprobar_formulario_ui.sh /ruta/latino-core` ejecuta ocho suites
de Core, recompila ambos proyectos y ejecuta 14 pruebas LatinoUI. Activa
ASan/UBSan y detección de fugas. No instala, confirma commits ni publica ramas.

La prueba nueva del formulario ejecuta el host y el intérprete reales con una
ventana simulada: escritura con acentos, retroceso, Escape, Tab, Shift+Tab,
Guardar, Cancelar y validación del nombre corto. La simulación no acredita
OpenGL, fuentes ni interacción física. Después abrir:

```bash
./build-asan-formulario10/host_ui scripts/formulario_nombre.lat
```

No se ha implementado CRUD, lectura CSV, guardado atómico, recarga de programas,
un editor visual o instalación pública. No se debe etiquetar esta entrega
como Alfa-1 terminada. La recuperación de una VM comprometida por un error
real sigue pendiente; el host conserva la política de Host09.
