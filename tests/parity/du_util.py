"""GNU parity cases for du."""
from support import Suite

def run(s: Suite) -> None:
    root=s.root/"du-tree"
    (root/"nested").mkdir(parents=True)
    (root/"a").write_bytes(b"a"*800)
    (root/"nested"/"b").write_bytes(b"b"*1900)
    for args in (("-s",str(root)),("-a",str(root)),("-b", "-s",str(root)),("-h", "-s",str(root)),("--max-depth=1",str(root)),("--bad-option",)):
        s.case("du",list(args))
    s.product_version("du")
    s.symlink_dispatch("du",["-s",str(root)])
