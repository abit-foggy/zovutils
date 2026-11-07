"""GNU parity cases for chroot."""
from support import Suite

def run(s: Suite) -> None:
    root=s.path("chroot-root")
    import os
    os.mkdir(root)
    for args in ((root,), (s.path("missing-root"),), ("--bad-option",)):
        s.case("chroot", list(args))
    s.product_version("chroot")
    s.symlink_dispatch("chroot", [s.path("missing-root")])
