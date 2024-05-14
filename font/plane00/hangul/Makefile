CC=gcc

CFLAGS = -g -O2

VERSION = 15.1.05

BINDIR=../../../bin

all: hangul-base.html hangul-syllables.hex hangul-syllables2.hex check man

hangul-base.hex: hangul-base.txt
	$(BINDIR)/hexdraw hangul-base.txt > hangul-base.hex
	for i in 0 1 2 3 4 ; do \
	   $(BINDIR)/unihex2bmp -p$$i < hangul-base.hex > hangul-0$$i.bmp ; \
	done

hangul-base.html: hangul-base.hex
	$(BINDIR)/unijohab2html < hangul-base.hex > hangul-base.html

hangul-syllables.hex: hangul-base.hex
	$(BINDIR)/unigen-hangul -i hangul-base.hex -o hangul-syllables.hex

hangul-syllables2.hex: hangul-base.hex
	$(BINDIR)/johab2syllables -i hangul-base.hex -o hangul-syllables2.hex

reference: hangul-syllables.hex hangul-base.html
	if [ ! -d reference ] ; then mkdir reference ; fi
	tar cf - hangul-syllables.hex hangul-base.html | \
	   (cd reference ; tar xpf -)
	touch reference

check: hangul-base.html hangul-syllables.hex check-overlaps

check-diffs: hangul-base.html hangul-syllables.hex hangul-syllables2.hex
	@echo ""
	@echo "Checking against reference files..."
	@echo ""
	- diff reference/hangul-base.html hangul-base.html
	- diff reference/hangul-syllables.hex hangul-syllables.hex

#
# Check for overlaps between syllable components.
# Print how many were found.  The key at the top
# of the HTML file contains 4 overlapping color
# codes, so subtract those 4 from the total.
#
check-overlaps:
	@$(eval OVERLAPS = `egrep "CCCC|CC00CC" hangul-base.html | wc -l`)
	@echo ""
	@echo "Checking for overlaps..."
	@if [ $(OVERLAPS) -ne 4 ] ; then \
	   echo "Cells with Overlaps Found:" ; \
	   expr $(OVERLAPS) - 4 || true ; \
	else \
	   echo "No Overlaps Found" ; \
	fi

clean:
	rm -f hangul-syllables.hex hangul-syllables2.hex

distclean: clean

.PHONY: all man check check-diffs check-overlaps clean dist distclean
