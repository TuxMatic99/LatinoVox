#!/usr/bin/env bash
# Formulario persistente: usa un directorio de datos propio, nunca el repositorio.
set -uo pipefail
ui=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd) || exit 1
core=$(cd -- "${1:-$HOME/latino-core}" && pwd) || exit 1
if [ "$(uname -s)" != Linux ]; then
    printf 'Esta entrega de persistencia requiere Linux.\n' >&2
    exit 1
fi
if [ ! -x "$ui/build-asan-persistencia11/host_ui" ] ||
   [ ! -f "$core/build-asan-persistencia11/liblatino.so" ]; then
    printf 'Ejecuta primero scripts/comprobar_persistencia_ui.sh con la ruta de Latino-Core.\n' >&2
    exit 1
fi
umask 077
datos="$HOME/LatinoUI/datos-formulario11"
if [ -L "$datos" ]; then
    printf 'El directorio de datos no puede ser un enlace: %s\n' "$datos" >&2
    exit 1
fi
mkdir -p -- "$datos" || exit 1
if [ ! -O "$datos" ]; then
    printf 'El directorio de datos debe pertenecer a tu usuario: %s\n' "$datos" >&2
    exit 1
fi
chmod 700 -- "$datos" || exit 1
registro=$(mktemp "$HOME/latino-ui-persistencia11-visual-XXXXXX.txt") || exit 1
printf 'Archivo de datos: %s/nombre-v1.txt\nRegistro visual: %s\n' "$datos" "$registro"
cd -- "$datos" || exit 1
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
LD_LIBRARY_PATH="$core/build-asan-persistencia11${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
"$ui/build-asan-persistencia11/host_ui" "$ui/scripts/formulario_nombre_persistente.lat" \
    > "$registro" 2>&1
codigo=$?
printf '\nCodigo de salida: %s\n' "$codigo" >> "$registro"
tail -n 35 "$registro"
printf '\nRegistro completo: %s\n' "$registro"
exit "$codigo"
