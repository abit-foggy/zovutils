"""Parity tests for split including the generated files."""

import os
import subprocess
from pathlib import Path

from support import ParityFailure, Suite


def output_case(
    s: Suite,
    data: bytes,
    options: list[str],
    *,
    input_from_stdin: bool = False,
    suffix: str = "",
) -> None:
    results = []
    files = []
    for label, executable in (("zov", s.zovutils), ("gnu", s.gnu_dir / "split")):
        directory = s.root / f"split-{label}-{s.count}"
        directory.mkdir()
        (directory / "input").write_bytes(data)
        env = s.env.copy()
        if label == "gnu":
            env["PATH"] = str(s.gnu_dir) + os.pathsep + env.get("PATH", "")
        input_name = "-" if input_from_stdin else "input"
        command = [str(executable), "split", *options, input_name, "part"] if label == "zov" else ["split", *options, input_name, "part"]
        try:
            result = subprocess.run(
                command,
                cwd=directory,
                input=data if input_from_stdin else b"",
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                env=env,
                timeout=15,
                check=False,
            )
        except subprocess.TimeoutExpired as exc:
            raise ParityFailure(f"split timed out: {options!r}") from exc
        results.append((result.returncode, result.stdout, result.stderr))
        files.append([(path.name, path.read_bytes()) for path in sorted(directory.glob(f"part*{suffix}"))])
    s.count += 1
    if results[0] != results[1] or files[0] != files[1]:
        raise ParityFailure(
            f"split parity failed: {options!r}\nresults: zov={results[0]!r} gnu={results[1]!r}\n"
            f"files: zov={files[0]!r} gnu={files[1]!r}"
        )


def run(s: Suite) -> None:
    data = b"aa\nbb\nccc\ndddd\ne\n"
    for options in (
        [],
        ["-l2"],
        ["-b4"],
        ["-C5"],
        ["-n3"],
        ["-n2/3"],
        ["-nl/3"],
        ["-nl/2/3"],
        ["-nr/3"],
        ["-nr/2/3"],
        ["-t", "b", "-l1"],
        ["-a1", "-d", "-l1"],
        ["-a1", "-x", "-l1"],
        ["--numeric-suffixes=5", "--additional-suffix=.dat", "-l2"],
        ["--verbose", "-l2"],
        ["--filter=cat > \"$FILE\"", "-l2"],
        ["--verbose", "--filter=cat > \"$FILE\"", "-l2"],
        ["-n3", "-e"],
    ):
        output_case(s, data, options, suffix=".dat" if "--additional-suffix=.dat" in options else "")
    output_case(s, b"a\0b\0c\0", ["-t\\0", "-l1"])
    output_case(s, b"", ["-n3"])
    output_case(s, b"", ["-n3", "-e"])
    output_case(s, data, ["-l2"], input_from_stdin=True)
    s.case("split", ["--help"])
    s.product_version("split")
