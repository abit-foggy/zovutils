ZOVC ?= zovc
PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
UTILS = cat head tail wc

.PHONY: all check build unit parity test install uninstall clean

all: build

check:
	$(ZOVC) check

build:
	$(ZOVC) build --release

unit:
	ZOVC="$(ZOVC)" sh tests/unit/run.sh

parity: build
	sh tests/parity/run.sh

test: unit parity

install: build
	install -d "$(BINDIR)"
	install -m 755 target/zovutils "$(BINDIR)/zovutils"
	for utility in $(UTILS); do ln -sf zovutils "$(BINDIR)/$$utility"; done

uninstall:
	rm -f "$(BINDIR)/zovutils" $(UTILS:%="$(BINDIR)/%")

clean:
	rm -rf target
