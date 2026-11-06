"""Parity cases for csplit, including generated output file contents."""

from __future__ import annotations

import os
import subprocess
from pathlib import Path

from support import ParityFailure, Suite


def output_case(s: Suite, data: bytes, options: list[str], patterns: list[str]) -> None:
    results = []
    outputs = []
    for label, executable in (("zov", s.zovutils), ("gnu", s.gnu_dir / "csplit")):
        directory = s.root / f"csplit-{label}-{s.count}"
        directory.mkdir()
        (directory / "input").write_bytes(data)
        env = s.env.copy()
        if label == "gnu":
            env["PATH"] = str(s.gnu_dir) + os.pathsep + env.get("PATH", "")
        command = [str(executable), "csplit", *options, "input", *patterns] if label == "zov" else ["csplit", *options, "input", *patterns]
        try:
            result = subprocess.run(
                command,
                cwd=directory,
                input=b"",
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                env=env,
                timeout=15,
                check=False,
            )
        except subprocess.TimeoutExpired as exc:
            raise ParityFailure(f"csplit timed out: {options!r} {patterns!r}") from exc
        results.append((result.returncode, result.stdout, result.stderr))
        prefix = "part" if "-f" in options else "xx"
        outputs.append([(path.name, path.read_bytes()) for path in sorted(directory.glob(prefix + "*"))])
    s.count += 1
    if results[0] != results[1] or outputs[0] != outputs[1]:
        raise ParityFailure(
            f"csplit parity failed: {options!r} {patterns!r}\n"
            f"results: zov={results[0]!r} gnu={results[1]!r}\n"
            f"files: zov={outputs[0]!r} gnu={outputs[1]!r}"
        )


def run(s: Suite) -> None:
    data = b"alpha\nMARK one\nbeta\nMARK two\ngamma\n"
    for options, patterns in (
        ([], ["2"]),
        (["-s"], ["/MARK/"]),
        (["-s"], ["%MARK%"]),
        (["-s"], ["/MARK/", "{1}"]),
        (["-s"], ["/MARK/", "{*}"]),
        (["-s"], ["/MARK/+1"]),
        (["-s"], ["/MARK/-1"]),
        (["-s", "--suppress-matched"], ["/MARK/", "{*}"]),
        (["-s", "-z"], ["/MARK/"]),
        (["-s", "-n", "3"], ["/MARK/", "{*}"]),
        (["-s", "-b", "%03d.part"], ["/MARK/", "{*}"]),
        (["-s"], ["%MARK%", "/MARK/", "{*}"]),
        (["-s", "-f", "part"], ["/MARK/", "{*}"]),
    ):
        output_case(s, data, options, patterns)
    output_case(s, b"one\ntwo", ["-s"], ["2"])
    output_case(s, b"one\ntwo\n", ["-s", "--elide-empty-files"], ["1"])
    s.case("csplit", ["--help"])
    s.product_version("csplit")
