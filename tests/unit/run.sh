#!/bin/sh
set -eu

ZOVC=${ZOVC:-zovc}
"$ZOVC" test src/shared/numbers.zov
"$ZOVC" test
