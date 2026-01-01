# CISV PHP Extension Makefile
PHP_CONFIG ?= php-config
PHPIZE ?= phpize

.PHONY: all build install clean test

all: build

build: configure
	$(MAKE) -f Makefile.ext

configure: config.m4
	$(PHPIZE)
	./configure --enable-cisv

install: build
	$(MAKE) -f Makefile.ext install

clean:
	$(PHPIZE) --clean 2>/dev/null || true
	rm -rf modules autom4te.cache
	rm -f Makefile.ext configure config.h config.h.in config.log config.status
	rm -f acinclude.m4 aclocal.m4 build config.guess config.sub install-sh ltmain.sh missing mkinstalldirs run-tests.php

test: build
	php -d extension=./modules/cisv.so -r 'var_dump(new CisvParser());'
	@echo "Extension loaded successfully"
