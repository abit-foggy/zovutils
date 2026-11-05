#!/bin/sh
set -eu

ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/../.." && pwd)
ZOVUTILS_BIN=${ZOVUTILS_BIN:-$ROOT/target/zovutils}
GNU_COREUTILS_DIR=${GNU_COREUTILS_DIR:-/usr/bin}
TMPDIR_BASE=${TMPDIR:-/tmp}
WORK=$(mktemp -d "$TMPDIR_BASE/zovutils-parity.XXXXXX")
trap 'rm -rf "$WORK"' EXIT HUP INT TERM

if [ ! -x "$ZOVUTILS_BIN" ]; then
    echo "parity: missing zovutils binary: $ZOVUTILS_BIN" >&2
    exit 2
fi
for utility in cat head tail wc; do
    if [ ! -x "$GNU_COREUTILS_DIR/$utility" ]; then
        echo "parity: missing GNU Coreutils 9.11 utility: $GNU_COREUTILS_DIR/$utility" >&2
        exit 2
    fi
done

oracle_version=$($GNU_COREUTILS_DIR/cat --version | sed -n '1p')
case "$oracle_version" in
    *"GNU coreutils) 9.11"*) ;;
    *) echo "parity: expected GNU Coreutils 9.11, found: $oracle_version" >&2; exit 2 ;;
esac

export LC_ALL=C
export TZ=UTC0
printf 'alpha\nbeta\n\ngamma\n' > "$WORK/text"
printf 'one\ntwo\nthree\nfour\n' > "$WORK/lines"
printf 'no-final-newline' > "$WORK/no-final-newline"
printf 'a\000b\n\377z' > "$WORK/binary"
printf 'a\tb\n' > "$WORK/tabbed"
printf '%s\000%s\000' "$WORK/text" "$WORK/lines" > "$WORK/files0"
: > "$WORK/empty-files0"
printf '\000' > "$WORK/zero-name-files0"
: > "$WORK/empty"
awk 'BEGIN { for (i = 0; i < 25000; i++) print "streaming-row" }' > "$WORK/large"

run_case() {
    label=$1
    utility=$2
    input=$3
    shift 3
    if "$ZOVUTILS_BIN" "$utility" "$@" < "$input" > "$WORK/zov.out" 2> "$WORK/zov.err"; then
        zov_status=0
    else
        zov_status=$?
    fi
    if "$GNU_COREUTILS_DIR/$utility" "$@" < "$input" > "$WORK/gnu.out" 2> "$WORK/gnu.err"; then
        gnu_status=0
    else
        gnu_status=$?
    fi
    if [ "$zov_status" -ne "$gnu_status" ] || ! cmp -s "$WORK/zov.out" "$WORK/gnu.out" || ! cmp -s "$WORK/zov.err" "$WORK/gnu.err"; then
        echo "parity failed: $label (statuses zov=$zov_status gnu=$gnu_status)" >&2
        diff -u "$WORK/gnu.err" "$WORK/zov.err" >&2 || true
        diff -u "$WORK/gnu.out" "$WORK/zov.out" >&2 || true
        return 1
    fi
}

run_case cat-stdin cat "$WORK/text"
run_case cat-number-squeeze cat "$WORK/empty" -n -s "$WORK/text"
run_case cat-visible-bytes cat "$WORK/empty" -A "$WORK/binary"
run_case cat-number-nonblank cat "$WORK/empty" -n -b "$WORK/text"
run_case cat-options-after-file cat "$WORK/empty" "$WORK/text" -n
run_case cat-multiple-files cat "$WORK/empty" "$WORK/text" "$WORK/lines"
run_case cat-ends cat "$WORK/empty" -E "$WORK/no-final-newline"
run_case cat-missing-file cat "$WORK/empty" "$WORK/missing"

run_case head-default head "$WORK/lines"
run_case head-lines head "$WORK/empty" -n 2 "$WORK/lines"
run_case head-old-number head "$WORK/empty" -2 "$WORK/lines"
run_case head-exclude-last-line head "$WORK/empty" -n -1 "$WORK/lines"
run_case head-exclude-last-bytes head "$WORK/empty" -c -2 "$WORK/no-final-newline"
run_case head-long-equals head "$WORK/empty" --bytes=5 "$WORK/binary"
run_case head-exclude-zero head "$WORK/empty" -n -0 "$WORK/lines"
run_case head-bytes head "$WORK/empty" -c 5 "$WORK/binary"
run_case head-zero-delimited head "$WORK/empty" -z -n 1 "$WORK/binary"
run_case head-large head "$WORK/empty" -n 1000 "$WORK/large"
run_case head-multiple-files head "$WORK/empty" -n 1 "$WORK/text" "$WORK/lines"
run_case head-quiet head "$WORK/empty" -q -n 1 "$WORK/text" "$WORK/lines"
run_case head-verbose head "$WORK/empty" -v -n 1 "$WORK/text" "$WORK/lines"

run_case tail-default tail "$WORK/lines"
run_case tail-lines tail "$WORK/empty" -n 2 "$WORK/lines"
run_case tail-negative-lines tail "$WORK/empty" -n -2 "$WORK/lines"
run_case tail-zero-lines tail "$WORK/empty" -n 0 "$WORK/lines"
run_case tail-bytes tail "$WORK/empty" -c 4 "$WORK/binary"
run_case tail-positive-bytes tail "$WORK/empty" -c +3 "$WORK/binary"
run_case tail-zero-bytes tail "$WORK/empty" -c 0 "$WORK/binary"
run_case tail-from-line tail "$WORK/empty" -n +2 "$WORK/lines"
run_case tail-from-zero tail "$WORK/empty" -n +0 "$WORK/lines"
run_case tail-long-equals tail "$WORK/empty" --lines=2 "$WORK/lines"
run_case tail-zero-delimited tail "$WORK/empty" -z -n 2 "$WORK/binary"
run_case tail-large tail "$WORK/empty" -n 1000 "$WORK/large"
run_case tail-multiple-files tail "$WORK/empty" -n 1 "$WORK/text" "$WORK/lines"
run_case tail-quiet tail "$WORK/empty" -q -n 1 "$WORK/text" "$WORK/lines"
run_case tail-verbose tail "$WORK/empty" -v -n 1 "$WORK/text" "$WORK/lines"

printf 'initial\n' > "$WORK/follow"
"$ZOVUTILS_BIN" tail -f "$WORK/follow" > "$WORK/follow-zov.out" 2> "$WORK/follow-zov.err" &
follow_zov_pid=$!
"$GNU_COREUTILS_DIR/tail" -f "$WORK/follow" > "$WORK/follow-gnu.out" 2> "$WORK/follow-gnu.err" &
follow_gnu_pid=$!
sleep 1
printf 'appended\n' >> "$WORK/follow"
sleep 2
kill "$follow_zov_pid" "$follow_gnu_pid" 2>/dev/null || true
wait "$follow_zov_pid" 2>/dev/null || true
wait "$follow_gnu_pid" 2>/dev/null || true
if ! cmp -s "$WORK/follow-zov.out" "$WORK/follow-gnu.out"; then
    echo "parity failed: tail -f append output" >&2
    diff -u "$WORK/follow-gnu.out" "$WORK/follow-zov.out" >&2 || true
    exit 1
fi

printf 'pid-bound\n' > "$WORK/follow-pid"
sleep 1 &
follow_pid=$!
"$ZOVUTILS_BIN" tail -f -s 0.1 "--pid=$follow_pid" "$WORK/follow-pid" > "$WORK/pid-zov.out" 2> "$WORK/pid-zov.err" &
follow_zov_pid=$!
"$GNU_COREUTILS_DIR/tail" -f -s 0.1 "--pid=$follow_pid" "$WORK/follow-pid" > "$WORK/pid-gnu.out" 2> "$WORK/pid-gnu.err" &
follow_gnu_pid=$!
wait "$follow_pid" 2>/dev/null || true
wait "$follow_zov_pid" 2>/dev/null || true
wait "$follow_gnu_pid" 2>/dev/null || true
if [ ! -s "$WORK/pid-zov.out" ] || ! cmp -s "$WORK/pid-zov.out" "$WORK/pid-gnu.out"; then
    echo "parity failed: tail --pid bounded follow" >&2
    diff -u "$WORK/pid-gnu.out" "$WORK/pid-zov.out" >&2 || true
    exit 1
fi

printf 'old-entry\n' > "$WORK/follow-name"
"$ZOVUTILS_BIN" tail --follow=name --sleep-interval=0.1 "$WORK/follow-name" > "$WORK/name-zov.out" 2> "$WORK/name-zov.err" &
follow_zov_pid=$!
"$GNU_COREUTILS_DIR/tail" --follow=name --sleep-interval=0.1 "$WORK/follow-name" > "$WORK/name-gnu.out" 2> "$WORK/name-gnu.err" &
follow_gnu_pid=$!
sleep 1
printf 'new-append\n' >> "$WORK/follow-name"
sleep 1
kill "$follow_zov_pid" "$follow_gnu_pid" 2>/dev/null || true
wait "$follow_zov_pid" 2>/dev/null || true
wait "$follow_gnu_pid" 2>/dev/null || true
if ! cmp -s "$WORK/name-zov.out" "$WORK/name-gnu.out"; then
    echo "parity failed: tail --follow=name append" >&2
    diff -u "$WORK/name-gnu.out" "$WORK/name-zov.out" >&2 || true
    exit 1
fi

run_case wc-default wc "$WORK/empty" "$WORK/text"
run_case wc-stdin wc "$WORK/text"
run_case wc-select wc "$WORK/empty" -lcw "$WORK/binary"
run_case wc-max-line wc "$WORK/empty" -L "$WORK/no-final-newline"
run_case wc-max-display wc "$WORK/empty" -L "$WORK/tabbed"
run_case wc-multiple-files wc "$WORK/empty" "$WORK/text" "$WORK/lines"
run_case wc-chars wc "$WORK/empty" -m "$WORK/binary"
run_case wc-large wc "$WORK/empty" -lcw "$WORK/large"
run_case wc-total-always wc "$WORK/empty" --total=always "$WORK/text"
run_case wc-total-only wc "$WORK/empty" --total=only "$WORK/text"
run_case wc-total-never wc "$WORK/empty" --total=never "$WORK/text"
run_case wc-files0 wc "$WORK/empty" "--files0-from=$WORK/files0"
run_case wc-files0-stdin wc "$WORK/files0" --files0-from=-
run_case wc-files0-empty-name wc "$WORK/empty" --files0-from="$WORK/zero-name-files0"
run_case wc-missing-file wc "$WORK/empty" "$WORK/missing"

mkdir "$WORK/bin"
ln -s "$ZOVUTILS_BIN" "$WORK/bin/cat"
if "$WORK/bin/cat" -n < "$WORK/text" > "$WORK/alias.out" 2> "$WORK/alias.err"; then
    alias_status=0
else
    alias_status=$?
fi
if "$GNU_COREUTILS_DIR/cat" -n < "$WORK/text" > "$WORK/gnu.out" 2> "$WORK/gnu.err"; then
    gnu_status=0
else
    gnu_status=$?
fi
if [ "$alias_status" -ne "$gnu_status" ] || ! cmp -s "$WORK/alias.out" "$WORK/gnu.out" || ! cmp -s "$WORK/alias.err" "$WORK/gnu.err"; then
    echo "parity failed: cat argv[0] symlink dispatch" >&2
    exit 1
fi

echo "GNU Coreutils 9.11 parity cases passed"
