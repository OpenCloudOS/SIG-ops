XFSPROGS_VERSION = 5.9.0
XFSPROGS_DIR = xfsprogs-$(XFSPROGS_VERSION)

default: $(XFSPROGS_DIR)
	cp recover/* $(XFSPROGS_DIR)/db
	make -C $(XFSPROGS_DIR)
	cp $(XFSPROGS_DIR)/db/xfsrecover recover

$(XFSPROGS_DIR):
	curl -fsSL https://kernel.org/pub/linux/utils/fs/xfs/xfsprogs/xfsprogs-$(XFSPROGS_VERSION).tar.xz | tar -xJf -
	cd ${XFSPROGS_DIR} && ./configure 

