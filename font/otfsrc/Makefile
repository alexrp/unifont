
SHELL = /bin/sh

BINDIR = ../../bin

#
# Default values, if not set on the command line when make is invoked:
#
# FONTFILE:  Prefix of the file name for input and output files.
# FONTNAME:  Name of the font inside a TTF file.
# PSNAME:    PostScript name of the font inside a TTF file; can't have spaces.
# COMBINING: Prefix of the file containing a list of combining characters.
#
FONTFILE=unifont
FONTNAME=Unifont
PSNAME=Unifont
COMBINING=combining

#
# Designate language for TrueType text as American English.
#
#
TTF_LANG = 0x409
#
# The PostScript name of a font can't contain spaces--remove them.
# Could also use bash string replacement if you know you're using bash.
#

COPYRIGHT = "Copyright © 1998-2024 Roman Czyborra, Paul Hardy, \
Qianqian Fang, Andrew Miller, Johnnie Weaver, David Corbett, \
Nils Moskopp, Rebecca Bettencourt, Ho-Seok Ee, et al."

VERSION = 15.1.05

all: otf

#
# Set the location of the hex2otf binary executable.
#
HEX2OTF = $(BINDIR)/hex2otf

#
# Create the OpenType font file.  In case $(COMBINING).txt does not
# exist, touch the filename.  Then run hex2otf.
#	$(HEX2OTF) hex=$(FONTFILE).hex pos=otf-$(COMBINING).txt \
#	touch otf-$(COMBINING).txt
#
otf: $(FONTFILE).hex $(HEX2OTF)
	@echo "Creating OpenType font."
	$(HEX2OTF) \
	   hex=$(FONTFILE).hex \
	   pos=$(COMBINING).txt \
	   format=cff,gpos,gsub \
	   0=$(COPYRIGHT) \
	   1="$(FONTNAME)" \
	   2="Regular" \
	   3="$(PSNAME)-$(VERSION).otf" \
	   4="$(FONTNAME)" \
	   5="Version $(VERSION)" 6="$(PSNAME)" \
	   11="https://unifoundry.com/unifont/" \
	   13="Dual license: SIL Open Font License version 1.1, and GNU GPL version 2 or later with the GNU Font Embedding Exception." \
	   14="https://gnu.org/licenses/gpl.html, https://scripts.sil.org/OFL" \
	   out=$(FONTFILE).otf

#
# Delete files copied into this directory to build OpenType fonts.
#
clean:
	rm -f *.hex *.txt

#
# Delete files created within this directory while building OpenType fonts.
#
distclean: clean
	rm -f *.otf

.PHONY: all otf clean distclean
