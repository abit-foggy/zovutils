#!/usr/bin/env python3
"""Run command-specific GNU Coreutils 9.11 byte-parity suites."""

from __future__ import annotations

import argparse
import os
import sys
from pathlib import Path

import basename
import cat
import comm_util
import cp_util
import mv_util
import rm_util
import chmod_util
import csplit_util
import split_util
import tac_util
import od_util
import mkdir_util
import touch_util
import ln_util
import rmdir_util
import cut_util
import dirname
import echo
import expand_util
import env
import false
import fold_util
import fmt_util
import fuzz
import head
import join_util
import nl_util
import printenv
import paste_util
import printf
import pwd_util
import seq_util
import sleep_util
import sort_util
import test_util
import tr_util
import unexpand_util
import yes_util
import tail
import true
import uniq_util
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
        for module in (basename, cat, chmod_util, comm_util, cp_util, csplit_util, cut_util, dirname, echo, expand_util, env, false, fold_util, fmt_util, head, join_util, ln_util, mkdir_util, mv_util, nl_util, paste_util, printenv, printf, pwd_util, rm_util, rmdir_util, seq_util, sleep_util, sort_util, split_util, tac_util, od_util, test_util, tail, touch_util, tr_util, true, uniq_util, unexpand_util, wc, yes_util, fuzz):
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
