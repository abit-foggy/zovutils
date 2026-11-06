"""Parity cases for printf."""

from support import Suite


def run(s: Suite) -> None:
    for args in (
        [],
        ["hello"],
        [r"line one\nline two"],
        [r"quote: \" slash: \\"],
        [r"octal: \1234 hex: \x41 unicode: \u732b"],
        ["%s:%d:%#x", "word", "42", "42"],
        ["%s%s", "one", "two", "three"],
        ["%s", "one", "two", "three"],
        ["%d:%s", "not-a-number", "after"],
        ["%8.2f", "1.25"],
        ["%e %g", "1.25", "12.5"],
        ["%05d", "12"],
        ["%#o %u %X", "8", "-1", "31"],
        ["%c", "猫"],
        ["%*.*f", "8", "2", "1.2"],
        ["%*s", "-5", "word"],
        ["%*s", "bad", "word"],
        ["%*s", "4x", "word"],
        ["%.*s", "-1", "word"],
        ["%.*s", "bad", "word"],
        ["%.*s", "4x", "word"],
        ["%b", r"one\ntwo\cignored"],
        ["%b", r"\\ \a \b \e \f \n \r \t \v \077"],
        ["%q", "plain"],
        ["%q", "a b"],
        ["%q", "it's quoted"],
        ["%q", 'a"b'],
        ["%q", 'a\'b"c'],
        ["%q", "a$b"],
        ["%q", "a\tb"],
        ["%q", "line\nbreak"],
        ["%q", "é 猫"],
        ["%d", "0x10"],
        ["%d", "12trailing"],
        ["%d", "not-a-number"],
        ["%d"],
        ["%n"],
        ["%"],
        [r"\x"],
        [r"\u12"],
        ["%s", "--help"],
        ["--help", "extra"],
    ):
        s.case("printf", args)

    s.case("printf", ["--help"])
    s.product_version("printf")
    s.symlink_dispatch("printf", ["%s", "symlink"])
