
SHELL = /bin/sh

CC = gcc
C99 = $(CC) -std=c99

CFLAGS = -g -O2 -Wall

LOCALBINDIR = ../bin

#
# INSTALL can be defined on the command line to override
# the next definition.  This is only applied to C programs
# to allow INSTALL="install -s" to be specified.  Perl
# programs just use "install", not "$(INSTALL)".
#
INSTALL=install
#
# $(PREFIX) is defined in top-level directory, so don't do a
# 'make install' within this directory -- do it from above.
#
INSTALLDIR = $(PREFIX)/bin

#
# Programs written in C.
#
CPROGS = hex2otf johab2syllables unibmp2hex unicoverage unidup \
	 unibdf2hex unibmpbump unifont1per unifontpic unigen-hangul \
	 unigencircles unigenwidth unihex2bmp unihexgen unihexpose \
	 unijohab2html unipagecount

#
# Programs written in Perl.
#
# The programs johab2ucs2, unifontchojung, and unifontksx are
# only used for generation of Hangul syllables.
#
PPROGS = bdfimplode hex2bdf hex2sfd hexbraille hexdraw hexkinya hexmerge \
	 johab2ucs2 unifontchojung unifontksx unihex2png unihexfill \
	 unihexrotate unipng2hex unifont-viewer

PROGS = $(CPROGS) $(PPROGS)


all: $(CPROGS) bin

#
# Earlier Unifont C programs written before hex2otf follow C89 syntax.
# hex2otf follows C99 syntax, so compile with a C99 flag for older gcc
# versions.   Notably, several platforms have remained with gcc 4.2.1
# because it was the last version of gcc licensed under GPLv2.  In
# the future, the "-std=c99" flag could be added to the global CFLAGS
# declaration at the beginning of this file.
#
hex2otf: hex2otf.c hex2otf.h
	$(C99) $(CFLAGS) hex2otf.c -o hex2otf

unihexpose: unihexpose.o unifont-support.o
	$(CC) $(CFLAGS) unihexpose.o unifont-support.o -o unihexpose

unijohab2html: hangul.h unihangul-support.o unijohab2html.o
	$(CC) $(CFLAGS) unijohab2html.o unihangul-support.o -o unijohab2html

johab2syllables: hangul.h unihangul-support.o johab2syllables.o
	$(CC) $(CFLAGS) johab2syllables.o unihangul-support.o -o johab2syllables

unigen-hangul: hangul.h unihangul-support.o unigen-hangul.o
	$(CC) $(CFLAGS) unigen-hangul.o unihangul-support.o -o unigen-hangul

bin: bin-stamp

bin-stamp: $(CPROGS) $(PPROGS)
	install -m0755 -d $(LOCALBINDIR)
	$(INSTALL) -m0755 $(CPROGS) $(LOCALBINDIR)
	$(INSTALL) -m0755 $(PPROGS) $(LOCALBINDIR)
	# \rm -f $(CPROGS)
	touch $@

# install: $(CPROGS)

install: bin
	if [ x$(PREFIX) = x ] ; \
	then \
	   @echo "Fatal error: PREFIX directory is not defined." && \
	   @echo "Run 'make install' from top-level directory" && \
	   exit 1 ; \
	fi
	install -m0755 -d $(INSTALLDIR)
	set -e && \
	   cd $(LOCALBINDIR) && \
	   $(INSTALL) -m0755 $(CPROGS) $(INSTALLDIR)
	set -e && \
	   cd $(LOCALBINDIR) && \
	   $(INSTALL) -m0755 $(PPROGS) $(INSTALLDIR)

#
# The *.exe is for cygwin; it does nothing elsewhere.
# The *.dSYM is for Mac OS X.
#
clean:
	\rm -f $(CPROGS) *.o
	\rm -f *~
	\rm -f *.exe
	\rm -rf *.dSYM

distclean: clean
	\rm -f bin-stamp *~

.PHONY: all bin install clean distclean
