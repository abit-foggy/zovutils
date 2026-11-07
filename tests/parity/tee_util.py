"""GNU parity cases for tee."""
import os
import subprocess
from pathlib import Path
from support import Suite

def run(s: Suite) -> None:
    for data in (b"", b"hello\n", bytes(range(256)), b"no final newline"):
        s.case("tee", [], stdin=data)
    s.case("tee", ["-a", s.path("tee-output")], stdin=b"append\n")
    s.case("tee", ["--", s.path("--tee-literal")], stdin=b"literal\n")
    for mode in ("warn", "warn-nopipe", "exit", "exit-nopipe"):
        s.case("tee", [f"--output-error={mode}", s.path(f"tee-{mode}")], stdin=b"mode selection\n")
    s.case("tee", ["-p"], stdin=b"pipe mode\n")
    s.case("tee", ["--output-error=invalid"])
    full = Path("/dev/full")
    if full.exists():
        for mode in ("warn", "warn-nopipe", "exit", "exit-nopipe"):
            results = []
            for command, env in (
                ([str(s.zovutils), "tee", f"--output-error={mode}"], s.env),
                ([str(s.gnu_dir / "tee"), f"--output-error={mode}"], s.env),
            ):
                with full.open("wb") as out:
                    proc = subprocess.run(command, input=b"write failure\n", stdout=out,
                                          stderr=subprocess.PIPE, env=env, timeout=5, check=False)
                results.append((proc.returncode, proc.stderr))
            s.count += 1
            if results[0] != results[1]:
                raise AssertionError(f"tee output-error={mode} differs on /dev/full: {results!r}")
    s.case("tee", ["--bad-option"])
    s.product_version("tee")
    s.symlink_dispatch("tee", [])
