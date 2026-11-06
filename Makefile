ZOVC ?= zovc
PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
UTILS = basename cat dirname echo env false head tail true wc

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

uninstall:
	rm -f "$(BINDIR)/zovutils" $(UTILS:%="$(BINDIR)/%")

clean:
	rm -rf target
