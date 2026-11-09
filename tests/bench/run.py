#!/usr/bin/env python3
"""Automated throughput and latency benchmark suite for zovutils vs GNU Coreutils."""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import tempfile
import time
from pathlib import Path


def default_gnu_dir() -> Path:
    env_dir = os.environ.get("GNU_COREUTILS_DIR")
    if env_dir and Path(env_dir).is_dir():
        return Path(env_dir)
    for candidate in ("/tmp/zovutils-coreutils-9.11/bin", "/usr/bin"):
        p = Path(candidate)
        if (p / "cat").is_file():
            return p
    return Path("/usr/bin")


def generate_fixtures(tmpdir: Path, size_mb: int) -> dict[str, Path]:
    text_path = tmpdir / f"text_{size_mb}m.txt"
    csv_path = tmpdir / f"csv_{size_mb}m.csv"
    num_path = tmpdir / "numbers.txt"

    print(f"Generating synthetic benchmark fixtures ({size_mb}MB) in {tmpdir}...")

    # Text fixture: repetitive English lines
    line = "The quick brown fox jumps over the lazy dog. 0123456789 ABCDEF\n" * 16  # ~1KB
    with open(text_path, "w") as f:
        for _ in range(size_mb * 1024):
            f.write(line)

    # CSV fixture
    csv_line = "alpha,12345,beta,67890,gamma,some text description,delta\n" * 16  # ~1KB
    with open(csv_path, "w") as f:
        for _ in range(max(10, size_mb // 2) * 1024):
            f.write(csv_line)

    # Number fixture for sorting (100,000 randomized integers)
    import random
    rng = random.Random(42)
    with open(num_path, "w") as f:
        for _ in range(100_000):
            f.write(f"{rng.randint(-1_000_000, 1_000_000)}\n")

    return {
        "text": text_path,
        "csv": csv_path,
        "numbers": num_path,
    }


def measure(cmd: list[str], stdin_file: Path | None = None, shell_cmd: str | None = None) -> float:
    # 1 warmup run
    if shell_cmd:
        subprocess.run(shell_cmd, shell=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=True)
    elif stdin_file:
        with open(stdin_file, "rb") as f_in:
            subprocess.run(cmd, stdin=f_in, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=True)
    else:
        subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=True)

    # 3 timed trials, return minimum time
    best = float("inf")
    for _ in range(3):
        t0 = time.perf_counter()
        if shell_cmd:
            subprocess.run(shell_cmd, shell=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=True)
        elif stdin_file:
            with open(stdin_file, "rb") as f_in:
                subprocess.run(cmd, stdin=f_in, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=True)
        else:
            subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=True)
        elapsed = time.perf_counter() - t0
        if elapsed < best:
            best = elapsed
    return best


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--zovutils", type=Path, default=Path("target/zovutils"))
    parser.add_argument("--gnu-dir", type=Path, default=default_gnu_dir())
    parser.add_argument("--quick", action="store_true", help="Run with reduced data sizes (20MB)")
    parser.add_argument("--full", action="store_true", help="Run with full data sizes (500MB)")
    parser.add_argument("--json", type=Path, default=None, help="Save metrics to JSON file")
    args = parser.parse_args()

    zovutils = args.zovutils.resolve()
    gnu_dir = args.gnu_dir.resolve()

    if not zovutils.is_file() or not os.access(zovutils, os.X_OK):
        print(f"Error: zovutils binary not found at {zovutils}. Run 'make build' first.", file=sys.stderr)
        return 1

    size_mb = 100
    if args.quick:
        size_mb = 20
    elif args.full:
        size_mb = 500

    with tempfile.TemporaryDirectory(prefix="zovutils-bench-") as tmpdir_str:
        tmpdir = Path(tmpdir_str)
        fixtures = generate_fixtures(tmpdir, size_mb)
        text_f = fixtures["text"]
        csv_f = fixtures["csv"]
        num_f = fixtures["numbers"]

        # Benchmarks: (Name, Zov command, GNU command, stdin file or None, shell string or None, data size in MB)
        benchmarks = [
            ("cat (stream to /dev/null)",
             [str(zovutils), "cat", str(text_f)],
             [str(gnu_dir / "cat"), str(text_f)],
             None, None, size_mb),
            ("wc -l (line count)",
             [str(zovutils), "wc", "-l", str(text_f)],
             [str(gnu_dir / "wc"), "-l", str(text_f)],
             None, None, size_mb),
            ("wc -w (word count)",
             [str(zovutils), "wc", "-w", str(text_f)],
             [str(gnu_dir / "wc"), "-w", str(text_f)],
             None, None, size_mb),
            ("head -n 100000 (top lines)",
             [str(zovutils), "head", "-n", "100000", str(text_f)],
             [str(gnu_dir / "head"), "-n", "100000", str(text_f)],
             None, None, size_mb),
            ("tail -n 100000 (bottom lines)",
             [str(zovutils), "tail", "-n", "100000", str(text_f)],
             [str(gnu_dir / "tail"), "-n", "100000", str(text_f)],
             None, None, size_mb),
            ("tail -n 10 (low count latency)",
             [str(zovutils), "tail", "-n", "10", str(text_f)],
             [str(gnu_dir / "tail"), "-n", "10", str(text_f)],
             None, None, size_mb),
            ("tr 'a-z' 'A-Z' (translation)",
             [str(zovutils), "tr", "a-z", "A-Z"],
             [str(gnu_dir / "tr"), "a-z", "A-Z"],
             text_f, None, size_mb),
            ("cut -d, -f2 (CSV column)",
             [str(zovutils), "cut", "-d,", "-f2", str(csv_f)],
             [str(gnu_dir / "cut"), "-d,", "-f2", str(csv_f)],
             None, None, max(10, size_mb // 2)),
            ("sort -n (100k numbers)",
             [str(zovutils), "sort", "-n", str(num_f)],
             [str(gnu_dir / "sort"), "-n", str(num_f)],
             None, None, None),
            ("base64 (encode)",
             [str(zovutils), "base64", str(text_f)],
             [str(gnu_dir / "base64"), str(text_f)],
             None, None, size_mb),
            ("md5sum (hashing)",
             [str(zovutils), "md5sum", str(text_f)],
             [str(gnu_dir / "md5sum"), str(text_f)],
             None, None, size_mb),
            ("sha256sum (hashing)",
             [str(zovutils), "sha256sum", str(text_f)],
             [str(gnu_dir / "sha256sum"), str(text_f)],
             None, None, size_mb),
            ("yes | head -n 2000000",
             None, None, None,
             f"{zovutils} yes | {zovutils} head -n 2000000",
             None),
        ]

        results = []
        print(f"\n{'Benchmark':<35} {'GNU 9.11':<12} {'Zovutils':<12} {'Ratio (Zov/GNU)':<15} {'Throughput (Zov)':<18}")
        print("-" * 95)

        for name, zov_cmd, gnu_cmd, stdin_f, shell_tmpl, mb_size in benchmarks:
            if shell_tmpl:
                zov_shell = shell_tmpl
                gnu_shell = f"{gnu_dir}/yes | {gnu_dir}/head -n 2000000"
                gnu_time = measure([], shell_cmd=gnu_shell)
                zov_time = measure([], shell_cmd=zov_shell)
            else:
                gnu_time = measure(gnu_cmd, stdin_file=stdin_f)
                zov_time = measure(zov_cmd, stdin_file=stdin_f)

            ratio = zov_time / gnu_time if gnu_time > 0 else 1.0
            ratio_str = f"{ratio:.2f}x"
            if ratio <= 1.05:
                ratio_str += " (parity)"

            throughput_str = "-"
            if mb_size and zov_time > 0:
                mb_per_sec = mb_size / zov_time
                if mb_per_sec >= 1000:
                    throughput_str = f"{mb_per_sec / 1000.0:.2f} GB/s"
                else:
                    throughput_str = f"{mb_per_sec:.1f} MB/s"

            print(f"{name:<35} {gnu_time*1000:>8.1f} ms {zov_time*1000:>8.1f} ms {ratio_str:>15} {throughput_str:>18}")

            results.append({
                "name": name,
                "gnu_ms": round(gnu_time * 1000, 2),
                "zov_ms": round(zov_time * 1000, 2),
                "ratio": round(ratio, 2),
                "throughput": throughput_str,
            })

    print("-" * 95)
    print("Benchmark complete.")

    if args.json:
        args.json.write_text(json.dumps(results, indent=2))
        print(f"Results written to {args.json}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
