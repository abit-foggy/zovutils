"""GNU parity cases for shred on temporary files."""
from support import Suite

def run(s: Suite) -> None:
    for i,args in enumerate((("-n","1","-u"),("-n","0","-z","-u"),("--size=0","-u"))):
        zp=s.root/f"z-shred-{i}";gp=s.root/f"g-shred-{i}"
        zp.write_bytes(b"temporary contents\0");gp.write_bytes(b"temporary contents\0")
        z=s._run([str(s.zovutils),"shred",*args,str(zp)],b"")
        g=s._run(["shred",*args,str(gp)],b"",env={**s.env,"PATH":str(s.gnu_dir)+":"+s.env.get("PATH","")})
        assert z==g,(args,z,g)
        assert not zp.exists() and not gp.exists()
    s.case("shred",["--bad-option"])
    s.product_version("shred")
