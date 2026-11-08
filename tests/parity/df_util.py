"""GNU parity cases for df."""
import os

from support import ParityFailure
from support import Suite


def _case_with_live_counts(s: Suite, args: list[str]) -> None:
    """Compare df's stable columns while ignoring live filesystem counters."""
    zenv = s.env.copy()
    genv = s.env.copy()
    genv["PATH"] = str(s.gnu_dir) + os.pathsep + genv.get("PATH", "")
    zov = s._run([str(s.zovutils), "df", *args], b"", env=zenv)
    gnu = s._run(["df", *args], b"", env=genv)
    s.count += 1
    if zov[0] != gnu[0] or zov[2] != gnu[2]:
        raise ParityFailure(f"df status or stderr differs for {args!r}: zov={zov!r}, GNU={gnu!r}")
    zlines = zov[1].decode("utf-8", "replace").splitlines()
    glines = gnu[1].decode("utf-8", "replace").splitlines()
    if len(zlines) != len(glines) or not zlines or zlines[0] != glines[0]:
        raise ParityFailure(f"df header or row count differs for {args!r}: zov={zlines!r}, GNU={glines!r}")
    typed = "-T" in args or "--print-type" in args
    for zr, gr in zip(zlines[1:], glines[1:]):
        zcols, gcols = zr.split(), gr.split()
        if len(zcols) != len(gcols) or len(zcols) < (3 if typed else 2):
            raise ParityFailure(f"df column count differs for {args!r}: zov={zr!r}, GNU={gr!r}")
        zstable = (zcols[0], zcols[1], zcols[-1]) if typed else (zcols[0], zcols[-1])
        gstable = (gcols[0], gcols[1], gcols[-1]) if typed else (gcols[0], gcols[-1])
        if zstable != gstable:
            raise ParityFailure(f"df filesystem/type/mount differs for {args!r}: zov={zr!r}, GNU={gr!r}")


def run(s: Suite) -> None:
    path=s.fixture("df-file",b"filesystem")
    _case_with_live_counts(s,["-P",path])
    _case_with_live_counts(s,["-h","/"])
    _case_with_live_counts(s,["-H","/"])
    _case_with_live_counts(s,["--human-readable","/"])
    _case_with_live_counts(s,["--si","/"])
    _case_with_live_counts(s,["-i",path])
    _case_with_live_counts(s,["-i","/"])
    _case_with_live_counts(s,["-T",path])
    _case_with_live_counts(s,["-T","/"])
    s.case("df",["--bad-option"])
    s.product_version("df")
    s.symlink_dispatch("df",["--bad-option"])
