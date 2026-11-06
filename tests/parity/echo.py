"""Parity cases for echo."""

from support import Suite


def run(s: Suite) -> None:
    for args in (
        [],
        [""],
        ["one", "two", "three"],
        ["-n", "no newline"],
        ["-nnn", "no newline"],
        ["-e", r"one\ntwo"],
        ["-E", r"one\ntwo"],
        ["-en", r"one\ntwo"],
        ["-neE", r"one\ntwo"],
        ["-E", "-e", r"one\ntwo"],
        ["-nfoo"],
        ["--", "literal"],
        ["--help", "literal"],
        ["--version", "literal"],
        ["-e", r"\\ \a \b \e \f \n \r \t \v"],
        ["-e", r"\0 \07 \0123X \777"],
        ["-e", r"\x \xG1 \x4 \x414"],
        ["-e", r"unknown: \q trailing-slash: " + "\\"],
        ["-e", "before\\cafter", "ignored"],
        ["-e", r"\c"],
        ["café", "猫"],
    ):
        s.case("echo", args)

    # POSIXLY_CORRECT suppresses GNU's -e/-E options; a leading -n remains
    # recognized by GNU echo and permits following echo options.
    s.case("echo", ["-e", r"one\ntwo"], extra_env={"POSIXLY_CORRECT": "1"})
    s.case("echo", [r"one\ntwo"], extra_env={"POSIXLY_CORRECT": "1"})
    s.case("echo", ["-n", "-e", r"one\ntwo"], extra_env={"POSIXLY_CORRECT": "1"})
    s.case("echo", ["-n", "-E", r"one\ntwo"], extra_env={"POSIXLY_CORRECT": "1"})
    s.case("echo", ["-nfoo"], extra_env={"POSIXLY_CORRECT": "1"})

    s.case("echo", ["--help"])
    s.product_version("echo")
    s.write_failure("echo", ["--help"])
