#!/usr/bin/env bash
# Compila Latino-Core y LatinoVox sin instalar nada en el sistema.
# Requiere Latino-Core constants08 (2722523) o una version compatible.
set -euo pipefail
ui=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
core=${1:-"$HOME/latino-core"}
core=$(cd -- "$core" && pwd)
registro=$(mktemp "$HOME/latino-ui-host09-XXXXXX.txt")
printf 'Registro completo: %s\n' "$registro"

ejecutar() {
    printf '\nComando:' >> "$registro"
    printf ' %q' "$@" >> "$registro"
    printf '\n' >> "$registro"
    if "$@" >> "$registro" 2>&1; then
        printf 'Codigo de salida: 0\n' >> "$registro"
    else
        codigo=$?
        printf '\nCodigo de salida: %s\n' "$codigo" >> "$registro"
        tail -n 45 "$registro"
        printf '\nRegistro del fallo: %s\n' "$registro"
        exit "$codigo"
    fi
}

for herramienta in cmake ctest ldd; do
    ejecutar command -v "$herramienta"
done
ejecutar test -f "$core/include/latino.h"
ejecutar test -f "$core/tests/constants_regression.c"
ejecutar test -f "$ui/tests/test_ui_host.c"

export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
printf 'ASAN_OPTIONS=%s\nUBSAN_OPTIONS=%s\n' \
    "$ASAN_OPTIONS" "$UBSAN_OPTIONS" >> "$registro"
flags='-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined'
build_core="$core/build-asan-host09"
build_ui="$ui/build-asan-host09"

ejecutar cmake -S "$core" -B "$build_core" \
    -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
    "-DCMAKE_C_FLAGS=$flags" \
    -DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined \
    -DCMAKE_SHARED_LINKER_FLAGS=-fsanitize=address,undefined
ejecutar cmake --build "$build_core" --parallel 2
ejecutar cmake -S "$ui" -B "$build_ui" \
    -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON \
    "-DLATINO_INCLUDE_DIR=$core/include" \
    "-DLATINO_LIBRARY=$build_core/liblatino.so" \
    "-DCMAKE_BUILD_RPATH=$build_core" \
    "-DCMAKE_C_FLAGS=$flags" \
    -DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined \
    -DCMAKE_SHARED_LINKER_FLAGS=-fsanitize=address,undefined
ejecutar cmake --build "$build_ui" --parallel 2
ejecutar test -x "$build_ui/test_ui_host"
# Priorizar la biblioteca recien compilada incluso si habia otra en el entorno.
export LD_LIBRARY_PATH="$build_core${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
ejecutar ldd "$build_ui/host_ui"
ejecutar ldd "$build_ui/test_ui_host"
ejecutar ctest --test-dir "$build_ui" --output-on-failure -V
printf '\nPruebas automatizadas terminadas sin errores. Registro: %s\n' "$registro"
tail -n 30 "$registro"
