# Makefile for wipe by Berke Durak
# 
# echo berke1lambda-diode2com|tr 12 @.
#
# define HAVE_OSYNC if an O_SYNC bit can be specified for the open () call
# (see open(2))
#
# define HAVE_DEV_URANDOM if /dev/urandom is available; will use a simple
# getpid() ^ clock() like scheme to seed the pseudorandom generator
# otherwise.
#
# define HAVE_RANDOM if the random () library call is available on your
# system.
#
# define HAVE_STRCASECMP if the strcasecmp () library call is available
# on your system.
#
# the REMOVE_ON_INT option has been removed since version 0.17,
# as it is difficult to maintain and has no significant purpose IMHO.
#
# define WEAK_RC6 if you wish to use four-round RC6 instead of the
# standard 20 rounds
#
# define RC6_ENABLED ONLY IF RC6 HAS BEEN ACCEPTED AS THE ADVANCED ENCRYPTION
# STANDARD and if you wish it to be available as a PRNG. WARNING. The RC6
# algorythm is patented by RSADSI; unless it is selected as the AES,
# it requires licensing from RSADSI, and you might expose yourself to legal
# problems if you use it without checking with them. I take no responsibility
# on this. I just implemented it since its specifications were freely available
# and it was easy to implement. by default this is disabled.
#
# define SYNC_WAITS_FOR_SYNC if the sync () system call waits for physical
# i/o to be completed before returning, as under Linux.
#
# define FIND_DEVICE_SIZE_BY_BLKGETSIZE if ioctl BLKGETSIZE is available
# for determinating the block size of a device, as under Linux.
# 
# define SIXTYFOUR,__USE_LARGEFILE and __USE_FILE_OFFSET64 to be able to
# wipe devices or files greater than 4Gb (works under Linux)
# --------------------------------------------------------------------------
# Linux 2.0.x
#

CC_LINUX=gcc
CCO_LINUX=-Wall -DHAVE_DEV_URANDOM -DHAVE_OSYNC -DHAVE_STRCASECMP -DHAVE_RANDOM -DWEAK_RC6 -DSYNC_WAITS_FOR_SYNC -DFIND_DEVICE_SIZE_BY_BLKGETSIZE -DSIXTYFOUR -D__USE_LARGEFILE -D_FILE_OFFSET_BITS=64
# default should be to turn off debugging and to turn on optimization.
#CCO_LINUX+=-O9 -pipe -fomit-frame-pointer -finline-functions -funroll-loops -fstrength-reduce
CCO_LINUX+=$(CFLAGS) $(LDFLAGS) $(CPPFLAGS)
#CCO_LINUX+=-DDEBUG -g
CCOC_LINUX=-c

# --------------------------------------------------------------------------
# SunOS 5.5.1
#

CC_SUNOS=gcc
CCO_SUNOS=-Wall -O6 -pipe -fomit-frame-pointer
CCOC_SUNOS=-c


# --------------------------------------------------------------------------
# AIX 4.1
#

CC_AIX=gcc
CCO_AIX=$(CFLAGS)
CCOC_AIX=-c

# --------------------------------------------------------------------------
# Generic UNIX (gcc)
#

CC_GENERIC=gcc
CCO_GENERIC=-Wall -O6 -pipe -fomit-frame-pointer -D_LARGEFILE_SOURCE -D_FILE_OFFSET_BITS=64 $(CFLAGS) $(LDFLAGS) $(CPPFLAGS)
CCOC_GENERIC=-c

# Thanks to Chris L. Mason <cmason@unixzone.com> for these:
#

# --------------------------------------------------------------------------
# Solaris 2.6 sparc
#

CC_SOLARISSP=gcc
CCO_SOLARISSP=-Wall -O6 -pipe -fomit-frame-pointer
CCOC_SOLARISSP=-c

# --------------------------------------------------------------------------
# Solaris 2.6 x86
#

CC_SOLARISX86=gcc
CCO_SOLARISX86=-Wall -O6 -fomit-frame-pointer
CCOC_SOLARISX86=-c

# --------------------------------------------------------------------------
# FreeBSD 2.2.6-STABLE
#

CC_FREEBSD=gcc
CCO_FREEBSD=-Wall -O6 -fomit-frame-pointer
CCOC_FREEBSD=-c

# --------------------------------------------------------------------------
# Digital/Compaq UNIX Alpha
#
# Thanks to Cyrus Durgin for this entry.

CC_DIGITALALPHA=gcc
CCO_DIGITALALPHA=-Wall -O6 -fomit-frame-pointer -DHAVE_OSYNC -DHAVE_STRCASECMP
CCOC_DIGITALALPHA=-c

#

OBJECTS=wipe.o arcfour.o md5.o misc.o random.o
TARGETS=wipe wipe.tr-asc.1

all	:	
		@echo "Please type $(MAKE) <system> where <system> can be one of:"; \
		echo "  linux        -- for Linux (kernel 2.0.x or higher)"; \
		echo "  sunos        -- for SunOS (tested on 5.5.1)"; \
		echo "  aix          -- for AIX (tested on 4.2)"; \
		echo "  solarissp    -- for Solaris SPARC (tested on 2.6)"; \
		echo "  solarisx86   -- for Solaris x86 (tested on 2.6)"; \
		echo "  freebsd      -- for FreeBSD (tested on 2.2.6-STABLE)"; \
		echo "  digitalalpha -- for Digital/Compaq UNIX Alpha"; \
		echo "  generic      -- for generic unix"

linux	:	
		$(MAKE) $(TARGETS) "CC=$(CC_LINUX)" "CCO=$(CCO_LINUX)" "CCOC=$(CCOC_LINUX)"

sunos	:	
		$(MAKE) $(TARGETS) "CC=$(CC_SUNOS)" "CCO=$(CCO_SUNOS)" "CCOC=$(CCOC_SUNOS)"

aix	:	
		$(MAKE) $(TARGETS) "CC=$(CC_AIX)" "CCO=$(CCO_AIX)" "CCOC=$(CCOC_AIX)"

freebsd	:	
		$(MAKE) $(TARGETS) "CC=$(CC_FREEBSD)" "CCO=$(CCO_FREEBSD)" "CCOC=$(CCOC_FREEBSD)"

solarissp	:	
		$(MAKE) $(TARGETS) "CC=$(CC_SOLARISSP)" "CCO=$(CCO_SOLARISSP)" "CCOC=$(CCOC_SOLARISSP)"

solarisx86	:	
		$(MAKE) $(TARGETS) "CC=$(CC_SOLARISX86)" "CCO=$(CCO_SOLARISX86)" "CCOC=$(CCOC_SOLARISX86)"

digitalalpha	:	
		$(MAKE) $(TARGETS) "CC=$(CC_DIGITALALPHA)" "CCO=$(CCO_DIGITALALPHA)" "CCOC=$(CCOC_DIGITALALPHA)"

generic	:	
		$(MAKE) $(TARGETS) "CC=$(CC_GENERIC)" "CCO=$(CCO_GENERIC)" "CCOC=$(CCOC_GENERIC)"

wipe	:	$(OBJECTS)
		$(CC) $(CCO) $(OBJECTS) -o wipe

wipe.o	:	wipe.c random.h misc.h version.h
		$(CC) $(CCO) $(CCOC) wipe.c -o wipe.o

version.h: always
		if which git >/dev/null 2>&1 ; then \
			git rev-list --max-count=1 HEAD | sed -e 's/^/#define WIPE_GIT "/' -e 's/$$/"/' >version.h ; \
	  else \
			echo '#define WIPE_GIT "(unknown, compiled without git)"' >version.h ; \
	  fi

random.o	:	random.c misc.h md5.h
		$(CC) $(CCO) $(CCOC) random.c -o random.o

rc6.o	:	rc6.c rc6.h
		$(CC) $(CCO) $(CCOC) rc6.c -o rc6.o

arcfour.o	:	arcfour.c arcfour.h
		$(CC) $(CCO) $(CCOC) arcfour.c -o arcfour.o

md5.o	:	md5.c md5.h
		$(CC) $(CCO) $(CCOC) md5.c -o md5.o

misc.o	:	misc.c misc.h
		$(CC) $(CCO) $(CCOC) misc.c -o misc.o

wipe.tr-asc.1	:	wipe.tr.1
			./trtur <wipe.tr.1 >wipe.tr-asc.1

clean	:	
		rm -f wipe $(OBJECTS) wipe.tr-asc.1 version.h

install:
	install -m755 -o root -g root wipe $(DESTDIR)/usr/bin

.PHONY: always clean install
