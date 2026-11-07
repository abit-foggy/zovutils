"""GNU parity cases for expr."""
from support import Suite

def run(s: Suite) -> None:
    cases = (
        ["1", "+", "2", "*", "3"],
        ["(", "1", "+", "2", ")", "*", "3"],
        ["11", ">", "3"], ["abc", "=", "abc"],
        ["0", "|", "fallback"], ["a", "&", "b"],
        ["length", "abc"], ["substr", "abcdef", "2", "3"],
        ["index", "abcdef", "xzde"], ["match", "abc123", "[a-z]*"],
        ["abc123", ":", "[a-z]*"],
        ["", "|", "fallback"], ["-4", "%", "3"],
    )
    for args in cases:
        s.case("expr", args)
    s.case("expr", [])
    s.product_version("expr")
    s.symlink_dispatch("expr", ["1", "+", "1"])
