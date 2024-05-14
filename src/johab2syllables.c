/**
   @file johab2syllables.c

   @brief Create the Unicode Hangul Syllables block from component letters.

   This program reads in a "hangul-base.hex" file containing Hangul
   letters in Johab 6/3/1 format and outputs a Unifont .hex format
   file covering the Unicode Hangul Syllables range of U+AC00..U+D7A3.

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

#include "hangul.h"


/**
   @brief The main function.
*/
int
main (int argc, char *argv[]) {
   int      i;          /* Loop variables     */
   int      arg_count;  /* index into *argv[] */
   unsigned codept;
   unsigned max_codept;
   unsigned char  hangul_base[MAX_GLYPHS][32];
   int      initial, medial, final;         /* Base glyphs for a syllable. */
   unsigned char  syllable[32];        /* Syllable glyph built for output. */

   FILE *infp  = stdin;   /* Input Hangul Johab 6/3/1 file */
   FILE *outfp = stdout;  /* Output Hangul Syllables file  */

   /* Print a help message */
   void print_help ();

   /* Read the file containing Hangul base glyphs. */
   unsigned hangul_read_base8 (FILE *infp, unsigned char hangul_base[][32]);

   /* Given a Hangul Syllables code point, determine component glyphs. */
   void hangul_decompose (unsigned codept, int *, int *, int *);

   /* Given letters in a Hangul syllable, return a glyph. */
   void hangul_syllable (int choseong, int jungseong, int jongseong,
                         unsigned char hangul_base[][32],
                         unsigned char *syllable);


   /*
      If there are command line arguments, parse them.
   */
   arg_count = 1;

   while (arg_count < argc) {
      /* If input file is specified, open it for read access. */
      if (strncmp (argv [arg_count], "-i", 2) == 0) {
         arg_count++;
         if (arg_count < argc) {
            infp = fopen (argv [arg_count], "r");
            if (infp == NULL) {
               fprintf (stderr, "\n*** ERROR: Cannot open %s for input.\n\n",
                        argv [arg_count]);
               exit (EXIT_FAILURE);
            }
         }
      }
      /* If output file is specified, open it for write access. */
      else if (strncmp (argv [arg_count], "-o", 2) == 0) {
         arg_count++;
         if (arg_count < argc) {
            outfp = fopen (argv [arg_count], "w");
            if (outfp == NULL) {
               fprintf (stderr, "\n*** ERROR: Cannot open %s for output.\n\n",
                        argv [arg_count]);
               exit (EXIT_FAILURE);
            }
         }
      }
      /* If help is requested, print help message and exit. */
      else if (strncmp (argv [arg_count], "-h",     2) == 0 ||
               strncmp (argv [arg_count], "--help", 6) == 0) {
         print_help ();
         exit (EXIT_SUCCESS);
      }

      arg_count++;
   }


   /*
      Initialize entire glyph array to zeroes in case the input
      file skips over some code points.
   */
   for (codept = 0; codept < MAX_GLYPHS; codept++) {
      for (i = 0; i < 32; i++) hangul_base[codept][i] = 0;
   }

   /*
      Read the entire "hangul-base.hex" file into an array
      organized as hangul_base [code_point][glyph_byte].
      The Hangul glyphs are 16 columns wide, which is
      two bytes, by 16 rows, for a total of 2 * 16 = 32 bytes.
   */
   max_codept = hangul_read_base8 (infp, hangul_base);
   if (max_codept > 0x8FF) {
      fprintf (stderr, "\nWARNING: Hangul glyph range exceeds PUA space.\n\n");
   }

   /*
      For each glyph in the Unicode Hangul Syllables block,
      form a composite glyph of choseong + jungseong +
      optional jongseong and output it in Unifont .hex format.
   */
   for (codept = 0xAC00; codept < 0xAC00 + 19 * 21 * 28; codept++) {
      hangul_decompose (codept, &initial, &medial, &final);

      hangul_syllable (initial, medial, final, hangul_base, syllable);

      fprintf (outfp, "%04X:", codept);

      for (i = 0; i < 32; i++) {
         fprintf (outfp, "%02X", syllable[i]);
      }
      fputc ('\n', outfp);
   }

   exit (EXIT_SUCCESS);
}


/**
   @brief Print a help message.
*/
void
print_help () {

   printf ("\ngen-hangul [options]\n\n");
   printf ("     Generates Hangul syllables from an input Unifont .hex file encoded\n");
   printf ("     in Johab 6/3/1 format.  The output is the Unicode Hangul Syllables\n");
   printf ("     range, U+AC00..U+D7A3.\n\n");
   printf ("     This program demonstrates forming Hangul syllables without shifting\n");
   printf ("     the final consonant (jongseong) when combined with a vowel having\n");
   printf ("     a long double vertical stroke.  For a program that demonstrtes\n");
   printf ("     shifting jongseong in those cases, see unigen-hangul, which is what\n");
   printf ("     creates the Unifont Hangul Syllables block.\n\n");

   printf ("     This program may be invoked with the following command line options:\n\n");

   printf ("     Option    Parameters    Function\n");
   printf ("     ------    ----------    --------\n");
   printf ("     -h, --help              Print this message and exit.\n\n");
   printf ("     -i        input_file    Unifont hangul-base.hex formatted input file.\n\n");
   printf ("     -o        output_file   Unifont .hex format output file.\n\n");
   printf ("      Example:\n\n");
   printf ("         johab2syllables -i hangul-base.hex -o hangul-syllables.hex\n\n");

   return;
}

