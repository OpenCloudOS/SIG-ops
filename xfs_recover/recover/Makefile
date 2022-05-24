#

TOPDIR = ..
include $(TOPDIR)/include/builddefs

LTCOMMAND = xfsrecover

HFILES = addr.h agf.h agfl.h agi.h attr.h attrshort.h bit.h block.h bmap.h \
	btblock.h bmroot.h check.h command.h crc.h debug.h \
	dir2.h dir2sf.h dquot.h echo.h faddr.h field.h \
	flist.h fprint.h frag.h freesp.h hash.h help.h init.h inode.h input.h \
	io.h logformat.h malloc.h metadump.h output.h print.h quit.h sb.h \
	sig.h strvec.h text.h type.h write.h attrset.h symlink.h fsmap.h \
	fuzz.h
CFILES = $(HFILES:.h=.c) btdump.c btheight.c convert.c info.c

LLDLIBS	= $(LIBXFS) $(LIBXLOG) $(LIBFROG) $(LIBUUID) $(LIBRT) $(LIBPTHREAD)
LTDEPENDENCIES = $(LIBXFS) $(LIBXLOG) $(LIBFROG)
LLDFLAGS += -static-libtool-libs

default: depend $(LTCOMMAND)

include $(BUILDRULES)

install-dev:

-include .dep
