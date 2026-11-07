#!/bin/sh
set -eu

cc_path=$(command -v "${CC:-clang}")
wrapper_dir=$(mktemp -d "${TMPDIR:-/tmp}/zovutils-asan-cc.XXXXXX")
trap 'rm -rf "$wrapper_dir"' EXIT HUP INT TERM

cat > "$wrapper_dir/cc" <<'WRAPPER'
#!/bin/sh
exec "$ZOVUTILS_REAL_CC" -Wno-parentheses-equality -Wno-pointer-sign \
    -Wno-incompatible-pointer-types \
    -fsanitize=address,undefined \
    -fno-omit-frame-pointer "$@"
WRAPPER
chmod +x "$wrapper_dir/cc"

PATH="$wrapper_dir:$PATH" ZOVUTILS_REAL_CC="$cc_path" \
    "${ZOVC:-zovc}" build --release
ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=1:abort_on_error=1}" \
    python3 tests/parity/run.py
