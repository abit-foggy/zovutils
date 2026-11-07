"""GNU parity cases for ls, dir and vdir."""
import os
from support import Suite

def run(s: Suite) -> None:
    root=s.root/"listing"
    root.mkdir()
    (root/"alpha").write_text("one\n")
    (root/"beta").write_bytes(b"two")
    (root/".hidden").write_text("hidden")
    (root/"dir").mkdir()
    os.symlink("alpha",root/"link")
    path=str(root)
    for utility,cases in (("ls", ([], ["-a"], ["-A"], ["-1"], ["-C"], ["-l"], ["-ld"], ["-F"])),
                          ("dir", ([], ["-1"], ["-a"])),
                          ("vdir", ([], ["-a"]))):
        for args in cases:
            s.case(utility,[*args,path])
        s.product_version(utility)
        s.symlink_dispatch(utility,["-1",path])
