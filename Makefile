VHBA_VERSION = $(shell date +%Y%m%d)
PACKAGE = vhba-module-$(VHBA_VERSION)

EXTRA_CFLAGS += -DVHBA_VERSION=\"$(VHBA_VERSION)\" -I$(PWD)

obj-m += vhba.o

PWD	?= `pwd`
KERNELRELEASE ?= `uname -r`
KDIR ?= /lib/modules/$(KERNELRELEASE)/build
KMAKE := $(MAKE) -C $(KDIR) M=$(PWD)

DOCS = AUTHORS ChangeLog COPYING INSTALL NEWS README

KAT_TESTS = \
	kat/have_scsi_macros.c \
	kat/scatterlist_has_page_link.c

all: modules

kernel.api.h: ${KAT_TESTS}
	kat/kat ${KDIR} $@ $^

modules: kernel.api.h
	$(KMAKE) modules

module_install:
	$(KMAKE) modules_install

install: module_install

clean:
	$(KMAKE) clean
	rm -fr $(PACKAGE) kernel.api.h
	make -C kat clean > /dev/null

dist-dirs = kat debian debian/patches
dist-files = vhba.c Makefile $(wildcard kat/*)

dist: dist-gzip

dist-dir:
	rm -fr $(PACKAGE)
	mkdir $(PACKAGE)
	cp vhba.c Makefile $(DOCS) $(PACKAGE)
	mkdir $(PACKAGE)/kat
	cp -f $(wildcard kat/*) $(PACKAGE)/kat
	mkdir $(PACKAGE)/debian
	cp -f $(subst debian/patches,,$(wildcard debian/*)) $(PACKAGE)/debian
	mkdir $(PACKAGE)/debian/patches
	cp -f $(wildcard debian/patches/*) $(PACKAGE)/debian/patches

dist-gzip: dist-dir
	tar -czf $(PACKAGE).tar.gz $(PACKAGE)
	rm -rf $(PACKAGE)

dist-bzip2: dist-dir
	tar -cjf $(PACKAGE).tar.bz2 $(PACKAGE)
	rm -rf $(PACKAGE)

.PHONY: clean kernel.api.h
