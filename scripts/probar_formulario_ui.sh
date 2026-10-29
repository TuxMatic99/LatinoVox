#!/usr/bin/env bash
set -uo pipefail
ui=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd) || exit 1
registro=$(mktemp "$HOME/latino-ui-formulario10-visual-XXXXXX.txt") || exit 1
printf 'Registro visual: %s\n' "$registro"
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
"$ui/build-asan-formulario10/host_ui" "$ui/scripts/formulario_nombre.lat" \
    > "$registro" 2>&1
codigo=$?
printf '\nCodigo de salida: %s\n' "$codigo" >> "$registro"
tail -n 35 "$registro"
printf '\nRegistro completo: %s\n' "$registro"
exit "$codigo"
