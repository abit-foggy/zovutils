"""GNU parity cases for truncate."""
from support import Suite
from pathlib import Path

def run(s: Suite) -> None:
    s.case("truncate", ["--bad-option"])
    s.case("truncate", ["-s", "bad", s.path("bad-size")])
    def setup(root: Path) -> None:
        (root / "payload").write_bytes(b"abcdefgh")
    s.case_filesystem("truncate", ["--size=3", "payload"], setup=setup)
    s.case_filesystem("truncate", ["--size=+2", "payload"], setup=setup)
    s.product_version("truncate")
    initial=b"abcdefgh"
    for args in (("-s", "3"), ("-s", "+2"), ("-s", "-1"), ("-s", "2K")):
        zp=s.root/"z-truncate"
        gp=s.root/"g-truncate"
        zp.write_bytes(initial);gp.write_bytes(initial)
        z=s._run([str(s.zovutils),"truncate",*args,str(zp)],b"")
        g=s._run(["truncate",*args,str(gp)],b"",env={**s.env,"PATH":str(s.gnu_dir)+":"+s.env.get("PATH","")})
        assert z==g,(args,z,g)
        assert zp.read_bytes()==gp.read_bytes(),args
    s.symlink_dispatch("truncate", ["--no-create", "--size=0", "missing-truncate"])
