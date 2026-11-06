#!/usr/bin/env python3
"""Run command-specific GNU Coreutils 9.11 byte-parity suites."""

from __future__ import annotations

import argparse
import os
import sys
from pathlib import Path

import basename
import cat
import cut_util
import dirname
import echo
import env
import false
import fuzz
import head
import printenv
import paste_util
import printf
import pwd_util
import seq_util
import sleep_util
import sort_util
import test_util
import yes_util
import tail
import true
import wc
from support import ParityFailure, Suite, require_executables


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--zovutils", type=Path, default=Path("target/zovutils"))
    parser.add_argument(
        "--gnu-dir",
        type=Path,
        default=Path(os.environ.get("GNU_COREUTILS_DIR", "/usr/bin")),
    )
    args = parser.parse_args()
    suite = Suite(args.zovutils.resolve(), args.gnu_dir.resolve())
    try:
        require_executables(suite)
        for module in (basename, cat, cut_util, dirname, echo, env, false, head, paste_util, printenv, printf, pwd_util, seq_util, sleep_util, sort_util, test_util, tail, true, wc, yes_util, fuzz):
            start = suite.count
            module.run(suite)
            print(f"[PASS] {module.__name__}: {suite.count - start} parity cases")
        print(f"GNU Coreutils 9.11 parity passed: {suite.count} cases")
        return 0
    except (ParityFailure, AssertionError, OSError) as exc:
        print(str(exc), file=sys.stderr)
        return 1
    finally:
        suite.close()


if __name__ == "__main__":
    raise SystemExit(main())
