"""GNU parity cases for mknod."""
import os
import stat
from support import Suite

def run(s: Suite) -> None:
    for args in (("--bad-option",), ("name", "p", "extra"),):
        s.case("mknod", list(args))
    zp=s.root/"z-fifo"
    gp=s.root/"g-fifo"
    z=s._run([str(s.zovutils),"mknod","-m","0640",str(zp),"p"],b"")
    g=s._run(["mknod","-m","0640",str(gp),"p"],b"",env={**s.env,"PATH":str(s.gnu_dir)+":"+s.env.get("PATH","")})
    assert z==g,(z,g)
    assert stat.S_ISFIFO(zp.stat().st_mode) and stat.S_ISFIFO(gp.stat().st_mode)
    assert stat.S_IMODE(zp.stat().st_mode)==stat.S_IMODE(gp.stat().st_mode)
    s.product_version("mknod")
