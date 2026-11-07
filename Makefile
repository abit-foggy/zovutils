ZOVC ?= zovc
PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
UTILS = basename cat comm csplit cut dirname echo env expand false fold fmt head join ln mkdir nl od paste printenv printf pwd rmdir seq sleep sort split tac test tail touch tr true uniq unexpand wc yes

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
