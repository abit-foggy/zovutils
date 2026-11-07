"""GNU parity cases for who."""
from support import Suite

def run(s: Suite) -> None:
    for args in (
        [], ["-H"], ["--heading"], ["-q"], ["--count"],
        ["-a"], ["--all"], ["-b"], ["--boot"], ["-d"], ["--dead"],
        ["-l"], ["--login"], ["-p"], ["--process"], ["-r"], ["--runlevel"],
        ["-s"], ["--short"], ["-t"], ["--time"], ["-T"], ["-w"],
        ["--mesg"], ["--message"], ["--writable"], ["-u"], ["--users"],
        ["--lookup"], ["-Hq"], ["--bad-option"],
    ):
        s.case("who", args)
    s.product_version("who")
    s.symlink_dispatch("who", [])
