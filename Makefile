ZOVC ?= zovc
PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
ifeq ($(shell uname -s),Darwin)
STDBUF_LIB_FLAGS = -dynamiclib
else
STDBUF_LIB_FLAGS = -shared
endif
UTILS = b2sum base32 base64 basename basenc cat chcon chgrp chmod chown chroot cksum comm cp csplit cut date dd df dir dircolors dirname du echo env expand expr factor false fmt fold groups head hostid id install join link ln logname ls md5sum mkdir mkfifo mknod mktemp mv nice nl nohup nproc numfmt od paste pathchk pinky pr printenv printf ptx pwd readlink realpath rm rmdir runcon seq sha1sum sha224sum sha256sum sha384sum sha512sum shred shuf sleep sort split stat stdbuf stty sum sync tac tail tee test timeout touch tr true truncate tsort tty uname unexpand uniq unlink users vdir wc who whoami yes

.PHONY: all check build unit parity asan-parity bench benchmark test install uninstall clean

all: build

check:
	$(ZOVC) check

build:
	$(ZOVC) build
	mkdir -p target
	$(CC) -std=c99 -fPIC $(STDBUF_LIB_FLAGS) -o target/libzovstdbuf.dylib src/shared/libstdbuf.c

unit:
	ZOVC="$(ZOVC)" sh tests/unit/run.sh

parity: build
	python3 tests/parity/run.py

asan-parity:
	ZOVC="$(ZOVC)" sh tests/asan/run.sh

test: unit parity

bench: benchmark

benchmark: build
	python3 tests/bench/run.py

install: build
	install -d "$(BINDIR)"
	install -m 755 target/zovutils "$(BINDIR)/zovutils"
	install -m 755 target/libzovstdbuf.dylib "$(BINDIR)/libzovstdbuf.dylib"
	for utility in $(UTILS); do ln -sf zovutils "$(BINDIR)/$$utility"; done
	ln -sf zovutils "$(BINDIR)/["

uninstall:
	rm -f "$(BINDIR)/zovutils" "$(BINDIR)/libzovstdbuf.dylib" "$(BINDIR)/[" $(UTILS:%="$(BINDIR)/%")

clean:
	rm -rf target
