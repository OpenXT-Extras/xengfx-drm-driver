KDIR ?= /lib/modules/`uname -r`/build

modules modules_install clean help:
	$(MAKE) -C $(KDIR) M=$$PWD $@
