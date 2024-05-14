/**
   @file: unihetranspose.c

   @brief: Transpose Unifont glyph bitmaps.

   This program takes Unifont .hex format glyphs and converts those
   glyphs so that each byte (two hexadecimal digits in the .hex file)
   represents a column of 8 rows.  This simplifies use with graphics
   display controllers that write lines consisting of 8 rows at a time
   to a display.

   The bytes are ordered as first all the columns for the glyph in
   the first 8 rows, then all the columns in the next 8 rows, with
   columns ordered from left to right.

   This file must be linked with functions in unifont-support.c.

   @author Paul Hardy

   @copyright Copyright © 2023 Paul Hardy
*/
/*
   LICENSE:

      This program is free software: you can redistribute it and/or modify
      it under the terms of the GNU General Public License as published by
      the Free Software Foundation, either version 2 of the License, or
      (at your option) any later version.

      This program is distributed in the hope that it will be useful,
      but WITHOUT ANY WARRANTY; without even the implied warranty of
      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
      GNU General Public License for more details.

      You should have received a copy of the GNU General Public License
      along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include <stdio.h>
#include <stdlib.h>

#define MAXWIDTH 128

int
main (int argc, char *argv[]) {
   unsigned codept;     /* Unicode code point for glyph */
   char instring [MAXWIDTH];   /* input Unifont hex string */
   char outstring [MAXWIDTH];  /* output Unfont hex string */
   int  width;          /* width of current glyph */
   unsigned char glyph     [16][2];
   unsigned char glyphbits [16][16];  /* One glyphbits row, for transposing */
   unsigned char transpose [2][16];  /* Transponsed glyphbits bitmap   */

   void print_syntax ();

   void parse_hex (char *hexstring,
                   int *width,
                   unsigned *codept,
                   unsigned char glyph[16][2]);

   void glyph2bits (int width, 
                    unsigned char glyph[16][2],
                    unsigned char glyphbits [16][16]);

   void hexpose (int  width,
                 unsigned char glyphbits [16][16],
                 unsigned char transpose [2][16]);

   void xglyph2string (int width, unsigned codept,
                       unsigned char transpose [2][16],
                       char *outstring);

   if (argc > 1) {
      print_syntax ();
      exit (EXIT_FAILURE);
   }

   while (fgets (instring, MAXWIDTH, stdin) != NULL) {
      parse_hex (instring, &width, &codept, glyph);

      glyph2bits (width, glyph, glyphbits);

      hexpose (width, glyphbits, transpose);

      xglyph2string (width, codept, transpose, outstring);

      fprintf (stdout, "%s\n", outstring);
   }

   exit (EXIT_SUCCESS);
}


void
print_syntax () {

   fprintf (stderr, "\nSyntax: unihexpose < input.hex > output.hex\n\n");

   return;
}

