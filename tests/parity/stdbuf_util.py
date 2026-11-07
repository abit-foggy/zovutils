"""GNU parity cases for stdbuf options and actual stream buffering."""
from __future__ import annotations

import os
import select
import shlex
import subprocess
from pathlib import Path

from support import Suite


def _early_output(s: Suite, utility: str, helper: Path) -> tuple[int, bytes, bytes, bool]:
    env = s.env.copy()
    command = [str(s.zovutils), utility, "-o0", str(helper)]
    if utility == "gnu-stdbuf":
        env["PATH"] = str(s.gnu_dir) + os.pathsep + env.get("PATH", "")
        command = ["stdbuf", "-o0", str(helper)]
    proc = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, env=env)
    assert proc.stdout is not None and proc.stderr is not None
    readable, _, _ = select.select([proc.stdout], [], [], 0.2)
    emitted_early = bool(readable)
    prefix = proc.stdout.read(5) if emitted_early else b""
    try:
        rest, stderr = proc.communicate(timeout=3)
    except subprocess.TimeoutExpired:
        proc.kill()
        proc.communicate()
        raise AssertionError(f"{utility} buffered child did not exit")
    status = proc.returncode
    output = prefix + rest
    return status, output, stderr, emitted_early


def run(s: Suite) -> None:
    for args in (
        ["--bad-option"],
        [],
        ["-o"],
        ["-iL", "true"],
        ["-o", "bad", "true"],
        ["-o-1", "true"],
        ["-o1KB", "true"],
        ["-o1KiB", "true"],
        ["--output=1024", "true"],
        ["-o0", "/zovutils/no-such-command"],
    ):
        s.case("stdbuf", args)
    s.product_version("stdbuf")

    source = s.root / "stdbuf-probe.c"
    helper = s.root / "stdbuf-probe"
    source.write_text(
        '#include <stdio.h>\n#include <time.h>\n'
        'int main(void) { struct timespec t = {0, 600000000}; '
        'fputs("ready", stdout); nanosleep(&t, 0); '
        'fputs(" done\\n", stdout); return 0; }\n'
    )
    compiler = shlex.split(os.environ.get("CC", "cc"))
    built = subprocess.run(compiler + [str(source), "-o", str(helper)],
                           stdout=subprocess.PIPE, stderr=subprocess.PIPE, timeout=20)
    if built.returncode:
        raise AssertionError(f"could not compile stdbuf probe: {built.stderr!r}")
    zov = _early_output(s, "stdbuf", helper)
    gnu = _early_output(s, "gnu-stdbuf", helper)
    s.count += 1
    if zov != gnu or zov[0] != 0 or zov[1] != b"ready done\n" or not zov[3]:
        raise AssertionError(f"stdbuf output buffering differs: zov={zov!r}, GNU={gnu!r}")
