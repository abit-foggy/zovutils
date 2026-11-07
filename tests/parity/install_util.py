"""GNU parity cases for install."""
import os
import stat
from support import Suite

def run(s: Suite) -> None:
    src=s.fixture("install-source", b"installed bytes\0")
    zd=s.root/"z-installed"
    gd=s.root/"g-installed"
    env={**s.env,"PATH":str(s.gnu_dir)+os.pathsep+s.env.get("PATH","")}
    z=s._run([str(s.zovutils),"install","-m","0640",src,str(zd)],b"")
    g=s._run(["install","-m","0640",src,str(gd)],b"",env=env)
    assert z==g,(z,g)
    assert zd.read_bytes()==gd.read_bytes()
    assert stat.S_IMODE(zd.stat().st_mode)==stat.S_IMODE(gd.stat().st_mode)
    zd2=s.root/"z-parent"/"nested"/"out"
    gd2=s.root/"g-parent"/"nested"/"out"
    z=s._run([str(s.zovutils),"install","-D",src,str(zd2)],b"")
    g=s._run(["install","-D",src,str(gd2)],b"",env=env)
    assert z==g,(z,g)
    assert zd2.read_bytes()==gd2.read_bytes()
    s.case("install",["--bad-option"])
    s.product_version("install")
    s.symlink_dispatch("install",["-d",str(s.root/"dispatch-dir")])
