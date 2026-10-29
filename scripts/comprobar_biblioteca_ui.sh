#!/usr/bin/env bash
set -euo pipefail
ui=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
core=$(cd -- "${1:-$HOME/latino-core}" && pwd)
registro=$(mktemp "$HOME/latino-biblioteca12-XXXXXX.txt")
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
        tail -n 50 "$registro"
        printf '\nRegistro del fallo: %s\n' "$registro"
        exit "$codigo"
    fi
}
[ "$(uname -s)" = Linux ] || { printf 'Esta entrega se valida en Linux.\n'; exit 1; }
ejecutar test -f "$core/tests/object_text_regression.c"
ejecutar grep -q latF_escribir_atomico "$core/include/latino.h"
export ASAN_OPTIONS=detect_leaks=1:halt_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
printf 'ASAN_OPTIONS=%s\nUBSAN_OPTIONS=%s\n' "$ASAN_OPTIONS" "$UBSAN_OPTIONS" >> "$registro"
for suite in hash context ffi bytecode label modules constants strings_io atomic_io object_text; do
    ejecutar bash "$core/tests/run_${suite}_regression.sh"
done
ld_anterior=${LD_LIBRARY_PATH:-}
for modo in asan release; do
    if [ "$modo" = asan ]; then
        tipo=Debug
        flags='-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined'
        enlace='-fsanitize=address,undefined'
    else
        tipo=Release
        flags=''
        enlace=''
    fi
    bc="$core/build-$modo-biblioteca12"
    bu="$ui/build-$modo-biblioteca12"
    ejecutar cmake -S "$core" -B "$bc" "-DCMAKE_BUILD_TYPE=$tipo" "-DCMAKE_C_FLAGS=$flags" "-DCMAKE_EXE_LINKER_FLAGS=$enlace" "-DCMAKE_SHARED_LINKER_FLAGS=$enlace"
    ejecutar cmake --build "$bc" --parallel 2
    ejecutar cmake -S "$ui" -B "$bu" "-DCMAKE_BUILD_TYPE=$tipo" -DBUILD_TESTING=ON "-DLATINO_INCLUDE_DIR=$core/include" "-DLATINO_LIBRARY=$bc/liblatino.so" "-DCMAKE_BUILD_RPATH=$bc" "-DCMAKE_C_FLAGS=$flags" "-DCMAKE_EXE_LINKER_FLAGS=$enlace"
    ejecutar cmake --build "$bu" --parallel 2
    export LD_LIBRARY_PATH="$bc${ld_anterior:+:$ld_anterior}"
    ejecutar ldd "$bu/host_ui"
    ejecutar ldd "$bu/test_ui_biblioteca"
    ejecutar ctest --test-dir "$bu" --output-on-failure -V
done
printf '\nPruebas terminadas sin errores. Registro: %s\n' "$registro"
tail -n 35 "$registro"
