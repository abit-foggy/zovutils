"""Deterministic randomized byte and option parity coverage."""

import random

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

    suffix_fixture = fixtures[0]
    for suffix in [*(chr(codepoint) for codepoint in range(1, 128)), "é"]:
        value = "1" + suffix
        for utility in ("head", "tail"):
            for flag in ("-n", "-c"):
                s.case(utility, [flag, value, suffix_fixture])

    for utility, prefix in (("head", "-1"), ("tail", "+1"), ("tail", "-1")):
        for codepoint in range(1, 128):
            suffix = chr(codepoint)
            if utility == "tail" and suffix == "f":
                continue
            s.case(utility, [prefix + suffix, suffix_fixture])

    for codepoint in (*range(1, 128), ord("é")):
        value = "x" + chr(codepoint)
        s.case("tail", ["--pid=" + value])
        s.case("tail", ["--sleep-interval=" + value])
        s.case("tail", ["--max-unchanged-stats=" + value])
        s.case("tail", ["--follow=" + value])
        s.case("wc", ["--total=" + value])
