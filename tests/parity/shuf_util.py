"""GNU parity and permutation checks for shuf."""
import os
from support import Suite

def run(s: Suite) -> None:
    s.case("shuf", ["-n", "0"], stdin=b"alpha\nbeta\n")
    s.case("shuf", ["-e", "-n", "1", "single"])
    s.case("shuf", ["-i", "5-5"])
    s.case("shuf", ["-r", "-n", "4", "-i", "7-7"])
    s.case("shuf", ["--bad-option"])
    path=s.fixture("shuf-input",b"a\nb\nc\n")
    env={**s.env,"PATH":str(s.gnu_dir)+os.pathsep+s.env.get("PATH","")}
    z=s._run([str(s.zovutils),"shuf",path],b"")
    g=s._run(["shuf",path],b"",env=env)
    assert z[0]==g[0]==0 and sorted(z[1].splitlines())==sorted(g[1].splitlines())==[b"a",b"b",b"c"]
    s.product_version("shuf")
    s.symlink_dispatch("shuf", ["-n", "0"])
