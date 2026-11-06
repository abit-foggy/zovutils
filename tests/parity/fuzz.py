"""Deterministic randomized byte and option parity coverage."""

import itertools
import random
import sys

from support import Suite


def run(s: Suite) -> None:
    rng = random.Random(911)
    fixtures = [
        s.fixture(
            f"fuzz-{index}",
            bytes(rng.randrange(256) for _ in range(rng.randrange(300))),
        )
        for index in range(60)
    ]
    count_values = (
        "0", "1", "2", "3", "-0", "-1", "-2", "+0", "+1", "+2",
        "1k", "1m", "1b", "1K", "1KiB", "bad",
    )
    simple_flags = {
        "cat": ("-n", "-b", "-s", "-E", "-T", "-v", "-A", "-e", "-t", "-u"),
        "wc": ("-l", "-w", "-m", "-c", "-L"),
    }

    for fixture in fixtures:
        for utility in ("cat", "head", "tail", "wc"):
            for _ in range(12):
                if utility in simple_flags:
                    flags = simple_flags[utility]
                    options = rng.sample(flags, rng.randrange(len(flags) // 2 + 1))
                    args = [*options, fixture]
                else:
                    options: list[str] = []
                    for _ in range(rng.randrange(4)):
                        flag = rng.choice(("-n", "-c", "-z", "-q", "-v"))
                        if flag in ("-n", "-c"):
                            options.extend((flag, rng.choice(count_values)))
                        else:
                            options.append(flag)
                    args = [*options, fixture]
                s.case(utility, args)

    # Randomize cat's file boundaries as well as its input bytes. This checks
    # that line numbering and blank squeezing retain state across files and
    # stdin, even when getopt-style options follow operands.
    cat_flags = simple_flags["cat"]
    for _ in range(300):
        options = rng.sample(cat_flags, rng.randrange(len(cat_flags) // 2 + 1))
        operands = rng.choices(fixtures, k=rng.randrange(2, 6))
        if rng.randrange(3) == 0:
            operands.insert(rng.randrange(len(operands) + 1), "-")
        args = list(operands)
        for option in options:
            args.insert(rng.randrange(len(args) + 1), option)
        stdin = bytes(rng.randrange(256) for _ in range(rng.randrange(100)))
        posix_env = rng.choice((None, {"POSIXLY_CORRECT": ""}, {"POSIXLY_CORRECT": "1"}))
        s.case("cat", args, stdin=stdin, extra_env=posix_env)

    suffix_fixture = fixtures[0]
    for suffix in [*(chr(codepoint) for codepoint in range(1, 128)), "é"]:
        value = "1" + suffix
        for utility in ("head", "tail"):
            for flag in ("-n", "-c"):
                s.case(utility, [flag, value, suffix_fixture])

    for utility, prefix in (("head", "-1"), ("tail", "+1"), ("tail", "-1")):
        if sys.platform == "darwin" and prefix == "+1":
            continue
        for codepoint in range(1, 128):
            suffix = chr(codepoint)
            if utility == "tail" and suffix == "f":
                continue
            s.case(utility, [prefix + suffix, suffix_fixture])

    # Cover the multi-character historical tail suffix parser, including
    # suffix orders that must remain invalid rather than being cluster-parsed.
    for width in (1, 2, 3):
        for suffix_tuple in itertools.product("bcl", repeat=width):
            suffix = "".join(suffix_tuple)
            for option in ("-" + suffix, "-2" + suffix):
                s.case("tail", [option, suffix_fixture])

    for codepoint in (*range(1, 128), ord("é")):
        value = "x" + chr(codepoint)
        s.case("tail", ["--pid=" + value])
        s.case("tail", ["--sleep-interval=" + value])
        s.case("tail", ["--max-unchanged-stats=" + value])
        s.case("tail", ["--follow=" + value])
        s.case("wc", ["--total=" + value])

    # Exercise raw non-UTF-8 argv bytes as well as valid Unicode text. POSIX
    # Python's surrogateescape round-trips each byte through execve unchanged.
    for byte in range(128, 256):
        raw = bytes((byte,)).decode("utf-8", "surrogateescape")
        value = "1" + raw
        for utility in ("head", "tail"):
            for flag in ("-n", "-c"):
                s.case(utility, [flag, value, suffix_fixture])
        option = "--bad-" + raw
        for utility in ("cat", "head", "tail", "wc"):
            s.case(utility, [option])

    # Multi-file head/tail parsing permutes options around operands and
    # carries delimiters and header state between independently generated
    # binary files.
    ht_count_values = ("0", "1", "2", "-0", "-1", "-2", "+0", "+1", "+2", "1k", "bad")
    for utility in ("head", "tail"):
        for _ in range(300):
            operands = rng.choices(fixtures, k=rng.randrange(2, 5))
            if rng.randrange(3) == 0:
                operands.insert(rng.randrange(len(operands) + 1), "-")
            options: list[str] = []
            for _ in range(rng.randrange(1, 4)):
                flag = rng.choice(("-n", "-c", "-q", "-v", "-z"))
                if flag in ("-n", "-c"):
                    options.extend((flag, rng.choice(ht_count_values)))
                else:
                    options.append(flag)
            args = list(operands)
            for option in options:
                args.insert(rng.randrange(len(args) + 1), option)
            stdin = bytes(rng.randrange(256) for _ in range(rng.randrange(100)))
            posix_env = rng.choice((None, {"POSIXLY_CORRECT": ""}, {"POSIXLY_CORRECT": "1"}))
            s.case(utility, args, stdin=stdin, extra_env=posix_env)

    wc_flags = (
        "-l", "-w", "-m", "-c", "-L", "--total=auto", "--total=always", "--total=never"
    )
    for _ in range(300):
        operands = rng.choices(fixtures, k=rng.randrange(1, 6))
        options = rng.sample(wc_flags, rng.randrange(1, 5))
        args = list(operands)
        for option in options:
            args.insert(rng.randrange(len(args) + 1), option)
        posix_env = rng.choice((None, {"POSIXLY_CORRECT": ""}, {"POSIXLY_CORRECT": "1"}))
        s.case("wc", args, extra_env=posix_env)
