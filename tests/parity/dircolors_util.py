"""GNU parity cases for dircolors parsing, terminal selection, and output modes."""
from support import Suite


def run(s: Suite) -> None:
    config = s.fixture(
        "colors.conf",
        b"# sample\nTERM xterm*\nDIR 01;34\n*.foo 31\nLINK 01;36\nEXEC 01;32\nDIR 32\n",
    )
    s.case("dircolors", ["-b", config], extra_env={"TERM": "xterm-256color"})
    s.case("dircolors", ["--bourne-shell", config], extra_env={"TERM": "dumb"})
    s.case("dircolors", ["-c", config], extra_env={"TERM": "xterm"})
    s.case("dircolors", ["-b", "-c", config], extra_env={"TERM": "xterm"})
    s.case("dircolors", ["--c-shell", config], extra_env={"TERM": "xterm"})
    s.case("dircolors", ["--print-ls-colors", config], extra_env={"TERM": "xterm"})
    s.case("dircolors", ["--print-ls-colors", "-b", config], extra_env={"TERM": "xterm"})
    malformed = s.fixture("malformed-colors.conf", b"DIR\n")
    s.case("dircolors", [malformed])
    s.case("dircolors", ["--bad-option"])
    s.case("dircolors", [config, config])
    s.case("dircolors", [s.path("missing-colors.conf")])
    s.case("dircolors", [], extra_env={"TERM": "dumb"})
    s.case("dircolors", ["--print-database"])
    s.case("dircolors", ["-p", config])
    feature_config = s.fixture(
        "colors-features.conf",
        b"# ignored comment\nCOLOR tty\nOPTIONS -F\nEIGHTBIT 1\n"
        b"TERM vt100\nTERM xterm*\nDIR 34\n*.archive 01;31\n*.plain 0\n",
    )
    s.case("dircolors", [feature_config], extra_env={"TERM": "xterm-256color"})
    s.case("dircolors", [feature_config], extra_env={"TERM": "screen"})
    s.case("dircolors", ["--print-ls-colors", feature_config], extra_env={"TERM": "vt100"})
    s.product_version("dircolors")
    s.symlink_dispatch("dircolors", ["-b", config])
