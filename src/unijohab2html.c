/**
   @file unijohab2html.c

   @brief Display overalpped Hangul letter combinations in a grid.

   This displays overlapped letters that form Unicode Hangul Syllables
   combinations, as a tool to determine bounding boxes for all combinations.
   It works with both modern and archaic Hangul letters.

   Input is a Unifont .hex file such as the "hangul-base.hex" file that
   is part of the Unifont package.  Glyphs are all processed as being
   16 pixels wide and 16 pixels tall.

   Output is an HTML file containing 16 by 16 pixel grids shwoing
   overlaps in table format, arranged by variation of the initial
   consonant (choseong).

   Initial consonants (choseong) have 6 variations.  In general, the
   first three are for combining with vowels (jungseong) that are
   vertical, horizontal, or vertical and horizontal, respectively;
   the second set of three variations are for combinations with a final
   consonant.

   The output HTML file can be viewed in a web browser.

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

#define MAXFILENAME	1024

#define START_JUNG 0	///< Vowel index of first vowel with which to begin.
// #define START_JUNG 21  /* Use this #define for just ancient vowels */


/* (Red, Green, Blue) HTML color coordinates. */
#define RED	0xCC0000  ///< Color code for slightly unsaturated HTML red.
#define GREEN	0x00CC00  ///< Color code for slightly unsaturated HTML green.
#define BLUE	0x0000CC  ///< Color code for slightly unsaturated HTML blue.
#define BLACK	0x000000  ///< Color code for HTML black.
#define WHITE	0xFFFFFF  ///< Color code for HTML white.


/**
   @brief The main function.
*/
int
main (int argc, char *argv[]) {
   int i, j;  /* loop variables */
   unsigned codept;
   unsigned max_codept;
   int      modern_only = 0;  /* To just use modern Hangul */
   int      group, consonant1, vowel, consonant2;
   int      vowel_variation;
   unsigned glyph[MAX_GLYPHS][16];
   unsigned tmp_glyph [16];  /* To build one combined glyph at a time. */
   unsigned mask;
   unsigned overlapped;       /* To find overlaps */
   int      ancient_choseong; /* Flag when within ancient choseong range. */

   /*
      16x16 pixel grid for each Choseong group, for:

         Group 0 to Group 5 with no Jongseong
         Group 3 to Group 5 with Jongseong except Nieun
         Group 3 to Group 5 with Jongseong Nieun

      12 grids total.

      Each grid cell will hold a 32-bit HTML RGB color.
   */
   unsigned grid[12][16][16];

   /*
      Matrices to detect and report overlaps.  Identify vowel
      variations where an overlap occurred.  For most vowel
      variations, there will be no overlap.  Then go through
      choseong, and then jongseong to find the overlapping
      combinations.  This saves storage space as an alternative
      to storing large 2- or 3-dimensional overlap matrices.
   */
   // jungcho: Jungseong overlap with Choseong
   unsigned jungcho [TOTAL_JUNG * JUNG_VARIATIONS];
   // jongjung: Jongseong overlap with Jungseong -- for future expansion
   // unsigned jongjung [TOTAL_JUNG * JUNG_VARIATIONS];

   int glyphs_overlap;     /* If glyph pair being considered overlap. */
   int cho_overlaps  = 0;  /* Number of choseong+vowel overlaps.      */
   // int jongjung_overlaps = 0;  /* Number of vowel+jongseong overlaps. */

   int  inindex = 0;
   int  outindex = 0;
   FILE *infp, *outfp;     /* Input and output file pointers. */

   void     parse_args (int argc, char *argv[], int *inindex, int *outindex,
                        int *modern_only);
   int      cho_variation      (int cho, int jung, int jong);
   unsigned hangul_read_base16 (FILE *infp, unsigned glyph[][16]);
   int  glyph_overlap (unsigned *glyph1, unsigned *glyph2);

   void combine_glyphs (unsigned *glyph1, unsigned *glyph2,
                        unsigned *combined_glyph);
   void print_glyph_txt (FILE *fp, unsigned codept, unsigned *this_glyph);


   /*
      Parse command line arguments to open input & output files, if given.
   */
   if (argc > 1) {
      parse_args (argc, argv, &inindex, &outindex, &modern_only);
   }

   if (inindex == 0) {
      infp = stdin;
   }
   else {
      infp = fopen (argv[inindex], "r");
      if (infp == NULL) {
         fprintf (stderr, "\n*** ERROR: Cannot open %s for input.\n\n",
                  argv[inindex]);
         exit (EXIT_FAILURE);
      }
   }
   if (outindex == 0) {
      outfp = stdout;
   }
   else {
      outfp = fopen (argv[outindex], "w");
      if (outfp == NULL) {
         fprintf (stderr, "\n*** ERROR: Cannot open %s for output.\n\n",
                  argv[outindex]);
         exit (EXIT_FAILURE);
      }
   }

   /*
      Initialize glyph array to all zeroes.
   */
   for (codept = 0; codept < MAX_GLYPHS; codept++) {
      for (i = 0; i < 16; i++) glyph[codept][i] = 0x0000;
   }

   /*
      Initialize overlap matrices to all zeroes.
   */
   for (i = 0; i < TOTAL_JUNG * JUNG_VARIATIONS; i++) {
      jungcho  [i] = 0;
   }
   // jongjung is reserved for expansion.
   // for (i = 0; i < TOTAL_JONG * JONG_VARIATIONS; i++) {
   //    jongjung [i] = 0;
   // }

   /*
      Read Hangul base glyph file.
   */
   max_codept = hangul_read_base16 (infp, glyph);
   if (max_codept > 0x8FF) {
      fprintf (stderr, "\nWARNING: Hangul glyph range exceeds PUA space.\n\n");
   }

   /*
      If only examining modern Hangul, fill the ancient glyphs
      with blanks to guarantee they won't overlap.  This is
      not as efficient as ending loops sooner, but is easier
      to verify for correctness.
   */
   if (modern_only) {
      for (i = 0x0073; i < JUNG_HEX; i++) {
         for (j = 0; j < 16; j++) glyph[i][j] = 0x0000;
      }
      for (i = 0x027A; i < JONG_HEX; i++) {
         for (j = 0; j < 16; j++) glyph[i][j] = 0x0000;
      }
      for (i = 0x032B; i < 0x0400; i++) {
         for (j = 0; j < 16; j++) glyph[i][j] = 0x0000;
      }
   }

   /*
      Initialize grids to all black (no color) for each of
      the 12 Choseong groups.
   */
   for (group = 0; group < 12; group++) {
      for (i = 0; i < 16; i++) {
         for (j = 0; j < 16; j++) {
            grid[group][i][j] = BLACK;  /* No color at first */
         }
      }
   }

   /*
      Superimpose all Choseong glyphs according to group.
      Each grid spot with choseong will be blue.
   */
   for (group = 0; group < 6; group++) {
      for (consonant1 = CHO_HEX + group;
           consonant1 < CHO_HEX +
                        CHO_VARIATIONS * TOTAL_CHO;
           consonant1 += CHO_VARIATIONS) {
         for (i = 0; i < 16; i++) { /* For each glyph row */
            mask = 0x8000;
            for (j = 0; j < 16; j++) {
               if (glyph[consonant1][i] & mask) grid[group][i][j] |= BLUE;
               mask >>= 1;  /* Get next bit in glyph row */
            }
         }
      }
   }

   /*
      Fill with Choseong (initial consonant) to prepare
      for groups 3-5 with jongseong except niuen (group+3),
      then for groups 3-5 with jongseong nieun (group+6).
   */
   for (group = 3; group < 6; group++) {
      for (i = 0; i < 16; i++) {
         for (j = 0; j < 16; j++) {
            grid[group + 6][i][j] = grid[group + 3][i][j]
                                   = grid[group][i][j];
         }
      }
   }

   /*
      For each Jungseong, superimpose first variation on
      appropriate Choseong group for grids 0 to 5.
   */
   for (vowel = START_JUNG; vowel < TOTAL_JUNG; vowel++) {
      group = cho_variation (-1, vowel, -1);
      glyphs_overlap = 0;  /* Assume the 2 glyphs do not overlap. */

      for (i = 0; i < 16; i++) { /* For each glyph row */
         mask = 0x8000;
         for (j = 0; j < 16; j++) {
            if (glyph[JUNG_HEX + JUNG_VARIATIONS * vowel][i] & mask) {
               /*
                  If there was already blue in this grid cell,
                  mark this vowel variation as having overlap
                  with choseong (initial consonant) letter(s).
               */
               if (grid[group][i][j] & BLUE) glyphs_overlap = 1;

               /* Add green to grid cell color. */
               grid[group][i][j] |= GREEN;
            }
            mask >>= 1;  /* Mask for next bit in glyph row */
         }  /* for j */
      }  /* for i */
      if (glyphs_overlap) {
         jungcho [JUNG_VARIATIONS * vowel] = 1;
         cho_overlaps++;
      }
   }  /* for each vowel */

   /*
      For each Jungseong, superimpose second variation on
      appropriate Choseong group for grids 6 to 8.
   */
   for (vowel = START_JUNG; vowel < TOTAL_JUNG; vowel++) {
      /*
         The second vowel variation is for combination with
         a final consonant (Jongseong), with initial consonant
         (Choseong) variations (or "groups") 3 to 5.  Thus,
         if the vowel type returns an initial Choseong group
         of 0 to 2, add 3 to it.
      */
      group = cho_variation (-1, vowel, -1);
      /*
         Groups 0 to 2 don't use second vowel variation,
         so increment if group is below 2.
      */
      if (group < 3) group += 3;
      glyphs_overlap = 0;  /* Assume the 2 glyphs do not overlap. */

      for (i = 0; i < 16; i++) { /* For each glyph row */
         mask = 0x8000;  /* Start mask at leftmost glyph bit */
         for (j = 0; j < 16; j++) {  /* For each column in this row */
            /* "+ 1" is to get each vowel's second variation */
            if (glyph [JUNG_HEX +
                       JUNG_VARIATIONS * vowel + 1][i] & mask) {
               /* If this cell has blue already, mark as overlapped. */
               if (grid [group + 3][i][j] & BLUE) glyphs_overlap = 1;

               /* Superimpose green on current cell color. */
               grid [group + 3][i][j] |= GREEN;
            }
            mask >>= 1;  /* Get next bit in glyph row */
         }  /* for j */
      }  /* for i */
      if (glyphs_overlap) {
         jungcho [JUNG_VARIATIONS * vowel + 1] = 1;
         cho_overlaps++;
      }
   }  /* for each vowel */

   /*
      For each Jungseong, superimpose third variation on
      appropriate Choseong group for grids 9 to 11 for
      final consonant (Jongseong) of Nieun.
   */
   for (vowel = START_JUNG; vowel < TOTAL_JUNG; vowel++) {
      group = cho_variation (-1, vowel, -1);
      if (group < 3) group += 3;
      glyphs_overlap = 0;  /* Assume the 2 glyphs do not overlap. */

      for (i = 0; i < 16; i++) { /* For each glyph row */
         mask = 0x8000;
         for (j = 0; j < 16; j++) {
            if (glyph[JUNG_HEX +
                      JUNG_VARIATIONS * vowel + 2][i] & mask) {
               /* If this cell has blue already, mark as overlapped. */
               if (grid[group + 6][i][j] & BLUE) glyphs_overlap = 1;

               grid[group + 6][i][j] |= GREEN;
            }
            mask >>= 1;  /* Get next bit in glyph row */
         }  /* for j */
      }  /* for i */
      if (glyphs_overlap) {
         jungcho [JUNG_VARIATIONS * vowel + 2] = 1;
         cho_overlaps++;
      }
   }  /* for each vowel */


   /*
      Superimpose all final consonants except nieun for grids 6 to 8.
   */
   for (consonant2 = 0; consonant2 < TOTAL_JONG; consonant2++) {
      /*
         Skip over Jongseong Nieun, because it is covered in
         grids 9 to 11 after this loop.
      */
      if (consonant2 == 3) consonant2++;

      glyphs_overlap = 0;  /* Assume the 2 glyphs do not overlap. */
      for (i = 0; i < 16; i++) { /* For each glyph row */
         mask = 0x8000;
         for (j = 0; j < 16; j++) {
            if (glyph [JONG_HEX +
                       JONG_VARIATIONS * consonant2][i] & mask) {
               if (grid[6][i][j] & GREEN ||
                   grid[7][i][j] & GREEN ||
                   grid[8][i][j] & GREEN) glyphs_overlap = 1;

               grid[6][i][j] |= RED;
               grid[7][i][j] |= RED;
               grid[8][i][j] |= RED;
            }
            mask >>= 1;  /* Get next bit in glyph row */
         }  /* for j */
      }  /* for i */
      // jongjung is for expansion
      // if (glyphs_overlap) {
      //    jongjung [JONG_VARIATIONS * consonant2] = 1;
      //    jongjung_overlaps++;
      // }
   }  /* for each final consonant except nieun */

   /*
      Superimpose final consonant 3 (Jongseong Nieun) on
      groups 9 to 11.
   */
   codept = JONG_HEX + 3 * JONG_VARIATIONS;

   for (i = 0; i < 16; i++) { /* For each glyph row */
      mask = 0x8000;
      for (j = 0; j < 16; j++) {
         if (glyph[codept][i] & mask) {
            grid[ 9][i][j] |= RED;
            grid[10][i][j] |= RED;
            grid[11][i][j] |= RED;
         }
         mask >>= 1;  /* Get next bit in glyph row */
      }
   }


   /*
      Turn the black (uncolored) cells into white for better
      visibility of grid when displayed.
   */
   for (group = 0; group < 12; group++) {
      for (i = 0; i < 16; i++) {
         for (j = 0; j < 16; j++) {
            if (grid[group][i][j] == BLACK) grid[group][i][j] = WHITE;
         }
      }
   }


   /*
      Generate HTML output.
   */
   fprintf (outfp, "<html>\n");
   fprintf (outfp, "<head>\n");
   fprintf (outfp, "  <title>Johab 6/3/1 Overlaps</title>\n");
   fprintf (outfp, "</head>\n");
   fprintf (outfp, "<body bgcolor=\"#FFFFCC\">\n");

   fprintf (outfp, "<center>\n");
   fprintf (outfp, "  <h1>Unifont Hangul Jamo Syllable Components</h1>\n");
   fprintf (outfp, "  <h2>Johab 6/3/1 Overlap</h2><br><br>\n");

   /* Print the color code key for the table. */
   fprintf (outfp, "  <table border=\"1\" cellpadding=\"10\">\n");
   fprintf (outfp, "    <tr><th colspan=\"2\" align=\"center\" bgcolor=\"#FFCC80\">");
   fprintf (outfp, "<font size=\"+1\">Key</font></th></tr>\n");
   fprintf (outfp, "    <tr>\n");
   fprintf (outfp, "      <th align=\"center\" bgcolor=\"#FFFF80\">Color</th>\n");
   fprintf (outfp, "      <th align=\"center\" bgcolor=\"#FFFF80\">Letter(s)</th>\n");
   fprintf (outfp, "    </tr>\n");

   fprintf (outfp, "    <tr><td bgcolor=\"#%06X\">", BLUE);
   fprintf (outfp, "&nbsp;&nbsp;&nbsp;&nbsp;</td>");
   fprintf (outfp, "<td>Choseong (Initial Consonant)</td></tr>\n");

   fprintf (outfp, "    <tr><td bgcolor=\"#%06X\">", GREEN);
   fprintf (outfp, "&nbsp;&nbsp;&nbsp;&nbsp;</td>");
   fprintf (outfp, "<td>Jungseong (Medial Vowel/Diphthong)</td></tr>\n");

   fprintf (outfp, "    <tr><td bgcolor=\"#%06X\">", RED);
   fprintf (outfp, "&nbsp;&nbsp;&nbsp;&nbsp;</td>");
   fprintf (outfp, "<td>Jongseong (Final Consonant)</td></tr>\n");

   fprintf (outfp, "    <tr><td bgcolor=\"#%06X\">", BLUE | GREEN);
   fprintf (outfp, "&nbsp;&nbsp;&nbsp;&nbsp;</td>");
   fprintf (outfp, "<td>Choseong + Jungseong Overlap</td></tr>\n");

   fprintf (outfp, "    <tr><td bgcolor=\"#%06X\">", GREEN | RED);
   fprintf (outfp, "&nbsp;&nbsp;&nbsp;&nbsp;</td>");
   fprintf (outfp, "<td>Jungseong + Jongseong Overlap</td></tr>\n");

   fprintf (outfp, "    <tr><td bgcolor=\"#%06X\">", RED | BLUE);
   fprintf (outfp, "&nbsp;&nbsp;&nbsp;&nbsp;</td>");
   fprintf (outfp, "<td>Choseong + Jongseong Overlap</td></tr>\n");

   fprintf (outfp, "    <tr><td bgcolor=\"#%06X\">", RED | GREEN | BLUE);
   fprintf (outfp, "&nbsp;&nbsp;&nbsp;&nbsp;</td>");
   fprintf (outfp, "<td>Choseong + Jungseong + Jongseong Overlap</td></tr>\n");

   fprintf (outfp, "  </table>\n");
   fprintf (outfp, "  <br><br>\n");


   for (group = 0; group < 12; group++) {
      /* Arrange tables 3 across, 3 down. */
      if ((group % 3) == 0) {
         fprintf (outfp, "  <table border=\"0\" cellpadding=\"10\">\n");
         fprintf (outfp, "    <tr>\n");
      }

      fprintf (outfp, "      <td>\n");
      fprintf (outfp, "        <table border=\"3\" cellpadding=\"2\">\n");
      fprintf (outfp, "          <tr><th colspan=\"16\" bgcolor=\"#FFFF80\">");
      fprintf (outfp, "Choseong Group %d, %s %s</th></tr>\n",
               group < 6 ? group : (group > 8 ? group - 6 : group - 3),
               group < 6 ? (group < 3 ? "No" : "Without") : "With",
               group < 9 ? "Jongseong" : "Nieun");
   
      for (i = 0; i < 16; i++) {
         fprintf (outfp, "          <tr>\n");
         for (j = 0; j < 16; j++) {
            fprintf (outfp, "            <td bgcolor=\"#%06X\">",
                     grid[group][i][j]);
            fprintf (outfp, "&nbsp;&nbsp;&nbsp;&nbsp;</td>\n");
         }
         fprintf (outfp, "          </tr>\n");
      }
   
      fprintf (outfp, "            </td>\n");
      fprintf (outfp, "          </tr>\n");
      fprintf (outfp, "        </table>\n");
      fprintf (outfp, "      </td>\n");

      if ((group % 3) == 2) {
         fprintf (outfp, "    </tr>\n");
         fprintf (outfp, "  </table>\n  </br>\n");
      }
   }

   /* Wrap up HTML table output. */
   fprintf (outfp, "</center>\n");

   /*
      Print overlapping initial consonant + vowel combinations.
   */
   fprintf (outfp, "<h2>%d Vowel Overlaps with Initial Consonants Found</h2>",
            cho_overlaps);
   fprintf (outfp, "<font size=\"+1\"><pre>\n");

   for (i = JUNG_HEX;
        i < JUNG_HEX + TOTAL_JUNG * JUNG_VARIATIONS;
        i++) {
      /*
         If this vowel variation (Jungseong) had overlaps
         with one or more initial consonants (Choseong),
         find and print them.
      */
      if (jungcho [i - JUNG_HEX]) {
         ancient_choseong = 0;  /* Not within ancient choseong range yet. */
         fprintf (outfp, "<font color=\"#0000FF\"><b>");
         if (i >= JUNG_ANCIENT_HEX) {
            if (i >= JUNG_EXTB_HEX) fprintf (outfp, "Extended-B ");
            fprintf (outfp, "Ancient ");
         }
         fprintf (outfp, "Vowel at 0x%04X and&hellip;</b>", i + PUA_START);
         fprintf (outfp, "</font>\n\n");

         /*
            Get current vowel number, 0 to (TOTAL_JUNG - 1), and
            current vowel variation, 0 or 1, or 2 for final nieun.
         */
         vowel = (i - JUNG_HEX) / JUNG_VARIATIONS;
         vowel_variation = (i - JUNG_HEX) % JUNG_VARIATIONS;

         /* Get first Choseong group for this vowel, 0 to 5. */
         group = cho_variation (-1, vowel, -1);

         /*
            If this vowel variation is used with a final consonant
            (Jongseong) and the default initial consonant (Choseong)
            group for this vowel is < 3, add 3 to current Chosenong
            group.
         */
         if (vowel_variation > 0 && group < 3) group += 3;

         for (consonant1 = 0; consonant1 < TOTAL_CHO; consonant1++) {
            overlapped = glyph_overlap (glyph [i],
                             glyph [consonant1 * CHO_VARIATIONS
                                  + CHO_HEX + group]);

            /*
               If we just entered ancient choseong range, flag it.
            */
            if (overlapped && consonant1 >= 19 && ancient_choseong == 0) {
               fprintf (outfp, "<font color=\"#0000FF\"><b>");
               fprintf (outfp, "&hellip;Ancient Choseong&hellip;</b></font>\n");
               ancient_choseong = 1;
            }
            /*
               If overlapping choseong found, print combined glyph.
            */
            if (overlapped != 0) {

               combine_glyphs (glyph [i],
                               glyph [consonant1 * CHO_VARIATIONS
                                    + CHO_HEX + group],
                               tmp_glyph);

               print_glyph_txt (outfp,
                                PUA_START +
                                consonant1 * CHO_VARIATIONS +
                                CHO_HEX + group,
                                tmp_glyph);

            }  /* If overlapping pixels found. */
         }  /* For each initial consonant (Choseong) */
      }  /* Find the initial consonant that overlapped this vowel variation. */
   }  /* For each variation of each vowel (Jungseong) */

   fputc ('\n', outfp);

   fprintf (outfp, "</pre></font>\n");
   fprintf (outfp, "</body>\n");
   fprintf (outfp, "</html>\n");

   fclose (infp);
   fclose (outfp);


   exit (EXIT_SUCCESS);
}


/**
   @brief Parse command line arguments.

   @param[in] argc The argc parameter to the main function.
   @param[in] argv The argv command line arguments to the main function.
   @param[in,out] infile The input filename; defaults to NULL.
   @param[in,out] outfile The output filename; defaults to NULL.
*/
void
parse_args (int argc, char *argv[], int *inindex, int *outindex,
                 int *modern_only) {
   int arg_count;  /* Current index into argv[]. */

   int strncmp (const char *s1, const char *s2, size_t n);


   arg_count = 1;

   while (arg_count < argc) {
      /* If input file is specified, open it for read access. */
      if (strncmp (argv [arg_count], "-i", 2) == 0) {
         arg_count++;
         if (arg_count < argc) {
            *inindex = arg_count;
         }
      }
      /* If only modern Hangul is desired, set modern_only flag. */
      else if (strncmp (argv [arg_count], "-m", 2) == 0 ||
               strncmp (argv [arg_count], "--modern", 8) == 0) {
         *modern_only = 1;
      }
      /* If output file is specified, open it for write access. */
      else if (strncmp (argv [arg_count], "-o", 2) == 0) {
         arg_count++;
         if (arg_count < argc) {
            *outindex = arg_count;
         }
      }
      /* If help is requested, print help message and exit. */
      else if (strncmp (argv [arg_count], "-h",     2) == 0 ||
               strncmp (argv [arg_count], "--help", 6) == 0) {
         printf ("\nunijohab2html [options]\n\n");
         printf ("     Generates an HTML page of overlapping Hangul letters from an input\n");
         printf ("     Unifont .hex file encoded in Johab 6/3/1 format.\n\n");

         printf ("     Option        Parameters   Function\n");
         printf ("     ------        ----------   --------\n");
         printf ("     -h, --help                 Print this message and exit.\n\n");
         printf ("     -i            input_file   Unifont hangul-base.hex formatted input file.\n\n");
         printf ("     -o            output_file  HTML output file showing overlapping letters.\n\n");
         printf ("     -m, --modern               Only examine modern Hangul letters.\n\n");
         printf ("     Example:\n\n");
         printf ("          unijohab2html -i hangul-base.hex -o hangul-syllables.html\n\n");

         exit (EXIT_SUCCESS);
      }

      arg_count++;
   }

   return;
}

