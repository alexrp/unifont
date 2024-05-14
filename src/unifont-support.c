/**
   @file: unifont-support.c

   @brief: Support functions for Unifont .hex files.

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
#include <string.h>


/**
   @brief Decode a Unifont .hex file into Uniocde code point and glyph.

   This function takes one line from a Unifont .hex file and decodes
   it into a code point followed by a 16-row glyph array.  The glyph
   array can be one byte (8 columns) or two bytes (16 columns).

   @param[in] hexstring The Unicode .hex string for one code point.
   @param[out] width The number of columns in a glyph with 16 rows.
   @param[out] codept The code point, contained in the first .hex file field.
   @param[out] glyph The Unifont glyph, as 16 rows by 1 or 2 bytes wide.
*/
void
parse_hex (char *hexstring,
           int *width,
           unsigned *codept,
           unsigned char glyph[16][2]) {

   int i;
   int row;
   int length;

   sscanf (hexstring, "%X", codept);
   length = strlen (hexstring);
   for (i = length - 1; i > 0 && hexstring[i] != '\n'; i--);
   hexstring[i] = '\0';
   for (i = 0; i < 9 && hexstring[i] != ':'; i++);
   i++;  /* Skip over ':' */
   *width = (length - i) * 4 / 16;  /* 16 rows per glyphbits */

   for (row = 0; row < 16; row++) {
      sscanf (&hexstring[i], "%2hhX", &glyph [row][0]);
      i += 2;
      if (*width > 8) {
        sscanf (&hexstring[i], "%2hhX", &glyph [row][1]);
        i += 2;
      }
      else {
         glyph [row][1] = 0x00;
      }
   }


   return;
}


/**
   @brief Convert a Unifont binary glyph into a binary glyph array of bits.

   This function takes a Unifont 16-row by 1- or 2-byte wide binary glyph
   and returns an array of 16 rows by 16 columns.  For each output array
   element, a 1 indicates the corresponding bit was set in the binary
   glyph, and a 0 indicates the corresponding bit was not set.

   @param[in] width The number of columns in the glyph.
   @param[in] glyph The binary glyph, as a 16-row by 2-byte array.
   @param[out] glyphbits The converted glyph, as a 16-row, 16-column array.
*/
void
glyph2bits (int width, 
            unsigned char glyph[16][2],
            unsigned char glyphbits [16][16]) {

   unsigned char tmp_byte;
   unsigned char mask;
   int row, column;

   for (row = 0; row < 16; row++) {
      tmp_byte = glyph [row][0];
      mask = 0x80;
      for (column = 0; column < 8; column++) {
         glyphbits [row][column] = tmp_byte & mask ? 1 : 0;
         mask >>= 1;
      }

      if (width > 8)
         tmp_byte = glyph [row][1];
      else
         tmp_byte = 0x00;

      mask = 0x80;
      for (column = 8; column < 16; column++) {
         glyphbits [row][column] = tmp_byte & mask ? 1 : 0;
         mask >>= 1;
      }
   }


   return;
}


/**
   @brief Transpose a Unifont .hex format glyph into 2 column-major sub-arrays.

   This function takes a 16-by-16 cell bit array made from a Unifont
   glyph (as created by the glyph2bits function) and outputs a transposed
   array of 2 sets of 8 or 16 columns, depending on the glyph width.
   This format simplifies outputting these bit patterns on a graphics
   display with a controller chip designed to output a column of 8 pixels
   at a time.

   For a line of text with Unifont output, first all glyphs can have
   their first 8 rows of pixels displayed on a line.  Then the second
   8 rows of all glyphs on the line can be displayed.  This simplifies
   code for such controller chips that are designed to automatically
   increment input bytes of column data by one column at a time for
   each successive byte.

   The glyphbits array contains a '1' in each cell where the corresponding
   non-transposed glyph has a pixel set, and 0 in each cell where a pixel
   is not set.

   @param[in] width The number of columns in the glyph.
   @param[in] glyphbits The 16-by-16 pixel glyph bits.
   @param[out] transpose The array of 2 sets of 8 ot 16 columns of 8 pixels.
*/
void
hexpose (int width,
         unsigned char glyphbits [16][16],
         unsigned char transpose [2][16]) {

   int column;


   for (column = 0; column < 8; column++) {
      transpose [0][column] =
               (glyphbits [ 0][column] << 7) |
               (glyphbits [ 1][column] << 6) |
               (glyphbits [ 2][column] << 5) |
               (glyphbits [ 3][column] << 4) |
               (glyphbits [ 4][column] << 3) |
               (glyphbits [ 5][column] << 2) |
               (glyphbits [ 6][column] << 1) |
               (glyphbits [ 7][column]     );
      transpose [1][column] =
               (glyphbits [ 8][column] << 7) |
               (glyphbits [ 9][column] << 6) |
               (glyphbits [10][column] << 5) |
               (glyphbits [11][column] << 4) |
               (glyphbits [12][column] << 3) |
               (glyphbits [13][column] << 2) |
               (glyphbits [14][column] << 1) |
               (glyphbits [15][column]     );
   }
   if (width > 8) {
      for (column = 8; column < width; column++) {
         transpose [0][column] =
                  (glyphbits [0][column] << 7) |
                  (glyphbits [1][column] << 6) |
                  (glyphbits [2][column] << 5) |
                  (glyphbits [3][column] << 4) |
                  (glyphbits [4][column] << 3) |
                  (glyphbits [5][column] << 2) |
                  (glyphbits [6][column] << 1) |
                  (glyphbits [7][column]     );
         transpose [1][column] =
                  (glyphbits [ 8][column] << 7) |
                  (glyphbits [ 9][column] << 6) |
                  (glyphbits [10][column] << 5) |
                  (glyphbits [11][column] << 4) |
                  (glyphbits [12][column] << 3) |
                  (glyphbits [13][column] << 2) |
                  (glyphbits [14][column] << 1) |
                  (glyphbits [15][column]     );
      }
   }
   else {
      for (column = 8; column < width; column++)
         transpose [0][column] = transpose [1][column] = 0x00;
   }


   return;
}


/**
   @brief Convert a glyph code point and byte array into a Unifont .hex string.

   This function takes a code point and a 16-row by 1- or 2-byte binary
   glyph, and converts it into a Unifont .hex format character array.

   @param[in] width The number of columns in the glyph.
   @param[in] codept The code point to appear in the output .hex string.
   @param[in] glyph The glyph, with each of 16 rows 1 or 2 bytes wide.
   @param[out] outstring The output string, in Unifont .hex format.
*/
void
glyph2string (int width, unsigned codept,
              unsigned char glyph [16][2],
              char *outstring) {

   int i;            /* index into outstring array */
   int row;

   if (codept <= 0xFFFF) {
      sprintf (outstring, "%04X:", codept);
      i = 5;
   }
   else {
      sprintf (outstring, "%06X:", codept);
      i = 7;
   }

   for (row = 0; row < 16; row++) {
      sprintf (&outstring[i], "%02X", glyph [row][0]);
      i += 2;

      if (width > 8) {
         sprintf (&outstring[i], "%02X", glyph [row][1]);
         i += 2;
      }
   }

   outstring[i] = '\0';  /* terminate output string */
  

   return;
}


/**
   @brief Convert a code point and transposed glyph into a Unifont .hex string.

   This function takes a code point and a transposed Unifont glyph
   of 2 rows of 8 pixels in a column, and converts it into a Unifont
   .hex format character array.

   @param[in] width The number of columns in the glyph.
   @param[in] codept The code point to appear in the output .hex string.
   @param[in] transpose The transposed glyph, with 2 sets of 8-row data.
   @param[out] outstring The output string, in Unifont .hex format.
*/
void
xglyph2string (int width, unsigned codept,
               unsigned char transpose [2][16],
               char *outstring) {

   int i;            /* index into outstring array */
   int  column;

   if (codept <= 0xFFFF) {
      sprintf (outstring, "%04X:", codept);
      i = 5;
   }
   else {
      sprintf (outstring, "%06X:", codept);
      i = 7;
   }

   for (column = 0; column < 8; column++) {
      sprintf (&outstring[i], "%02X", transpose [0][column]);
      i += 2;
   }
   if (width > 8) {
      for (column = 8; column < 16; column++) {
         sprintf (&outstring[i], "%02X", transpose [0][column]);
         i += 2;
      }
   }
   for (column = 0; column < 8; column++) {
      sprintf (&outstring[i], "%02X", transpose [1][column]);
      i += 2;
   }
   if (width > 8) {
      for (column = 8; column < 16; column++) {
         sprintf (&outstring[i], "%02X", transpose [1][column]);
         i += 2;
      }
   }

   outstring[i] = '\0';  /* terminate output string */
  

   return;
}

