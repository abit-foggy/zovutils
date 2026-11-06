"""Parity cases for pwd."""

from __future__ import annotations

import os
import subprocess
import tempfile
from pathlib import Path

from support import ParityFailure, Suite


def run(s: Suite) -> None:
    for args in (
        [],
        ["-L"],
        ["-P"],
        ["--logical"],
        ["--physical"],
        ["-LP"],
        ["-PL"],
        ["--log"],
        ["--phys"],
        ["--bad-option"],
        ["--logical=value"],
        ["--help=value"],
        ["--version=value"],
        ["extra"],
    ):
        s.case("pwd", args)

    s.case("pwd", ["--help"])
    s.product_version("pwd")
    s.symlink_dispatch("pwd", ["-P"])

    # A valid logical PWD retains the symlink path while -P resolves it.
    with tempfile.TemporaryDirectory(prefix="zovutils-pwd-") as temp:
        physical = Path(temp) / "physical"
        logical = Path(temp) / "logical"
        physical.mkdir()
        logical.symlink_to(physical, target_is_directory=True)
        env = s.env.copy()
        env["PWD"] = str(logical)
        for option in ("-L", "-P"):
            zov = subprocess.run(
                [str(s.zovutils), "pwd", option], cwd=logical, env=env,
                stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False,
            )
            gnu_env = env.copy()
            gnu_env["PATH"] = str(s.gnu_dir) + os.pathsep + gnu_env.get("PATH", "")
            gnu = subprocess.run(
                ["pwd", option], cwd=logical, env=gnu_env,
                stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False,
            )
            zov_result = (zov.returncode, zov.stdout, zov.stderr)
            gnu_result = (gnu.returncode, gnu.stdout, gnu.stderr)
            if zov_result != gnu_result or zov.returncode != 0:
                raise ParityFailure(
                    f"pwd {option} in a symlink directory differs: "
                    f"zov={zov_result!r}, GNU={gnu_result!r}"
                )
            s.count += 1
