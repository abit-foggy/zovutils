ZOVC ?= zovc
PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
UTILS = b2sum base32 base64 basenc basename cat chgrp cksum chmod chown comm cp csplit cut dirname echo env expand expr false factor fold fmt groups head join link ln logname md5sum mkdir mkfifo mktemp mv nice nl nproc numfmt od paste pathchk printenv printf pwd readlink realpath rm rmdir sha1sum sha224sum sha256sum seq sleep sort split sum sync tac tee test tsort tail touch tr truncate true tty uniq unlink unexpand uname wc whoami yes

.PHONY: all check build unit parity asan-parity test install uninstall clean

all: build

check:
	$(ZOVC) check

build:
	$(ZOVC) build

unit:
	ZOVC="$(ZOVC)" sh tests/unit/run.sh

parity: build
	python3 tests/parity/run.py

asan-parity:
	ZOVC="$(ZOVC)" sh tests/asan/run.sh

test: unit parity

install: build
	install -d "$(BINDIR)"
	install -m 755 target/zovutils "$(BINDIR)/zovutils"
	for utility in $(UTILS); do ln -sf zovutils "$(BINDIR)/$$utility"; done
	ln -sf zovutils "$(BINDIR)/["

uninstall:
	rm -f "$(BINDIR)/zovutils" "$(BINDIR)/[" $(UTILS:%="$(BINDIR)/%")

clean:
	rm -rf target
