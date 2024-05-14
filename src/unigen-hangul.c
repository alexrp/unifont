/**
   @file unigen-hangul.c

   @brief Generate arbitrary hangul syllables.

   Input is a Unifont .hex file such as the "hangul-base.hex" file that
   is included in the Unifont package.

   The default program parameters will generate the Unicode
   Hangul Syllables range of U+AC00..U+D7A3.  The syllables
   will appear in this order:

        For each modern choseong {
           For each modern jungseong {
              Output syllable of choseong and jungseong
              For each modern jongseong {
                 Output syllable of choseong + jungseong + jongseong
              }
           }
        }

   By starting the jongseong code point at one before the first
   valid jongseong, the first inner loop iteration will add a
   blank glyph for the jongseong portion of the syllable, so
   only the current choseong and jungseong will be output first.

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
#include "hangul.h"

// #define DEBUG


struct PARAMS {
   unsigned starting_codept;      /* First output Unicode code point. */
   unsigned cho_start,  cho_end;  /* Choseong  start and end code points. */
   unsigned jung_start, jung_end; /* Jungseong start and end code points. */
   unsigned jong_start, jong_end; /* Jongseong start and end code points. */
   FILE *infp;
   FILE *outfp;
};


/**
   @brief Program entry point.
*/
int
main (int argc, char *argv[]) {

   int i;  /* loop variable */
   unsigned codept;
   unsigned max_codept;
   unsigned glyph[MAX_GLYPHS][16];
   unsigned tmp_glyph [16];  /* To build one combined glyph at a time. */
   int cho, jung, jong;      /* The 3 components in a Hangul syllable. */

   /// Default parameters for Hangul syllable generation.
   struct PARAMS params = { 0xAC00,  /* Starting output Unicode code point */
                            0x1100,  /* First modern choseong              */
                            0x1112,  /* Last modern choseong               */
                            0x1161,  /* First modern jungseong             */
                            0x1175,  /* Last modern jungseong              */
                            0x11A7,  /* One before first modern jongseong  */
                            0x11C2,  /* Last modern jongseong              */
                            stdin,   /* Default input file pointer         */
                            stdout   /* Default output file pointer        */
                          };

   void parse_args (int argc, char *argv[], struct PARAMS *params);

   unsigned hangul_read_base16 (FILE *infp, unsigned glyph[][16]);

   void print_glyph_hex (FILE *fp, unsigned codept, unsigned *this_glyph);

   void combined_jamo (unsigned glyph [MAX_GLYPHS][16],
                       unsigned cho, unsigned jung, unsigned jong,
                       unsigned *combined_glyph);


   if (argc > 1) {
      parse_args (argc, argv, &params);

#ifdef DEBUG
      fprintf (stderr,
               "Range: (U+%04X, U+%04X, U+%04X) to (U+%04X, U+%04X, U+%04X)\n",
               params.cho_start, params.jung_start, params.jong_start,
               params.cho_end,   params.jung_end,   params.jong_end);
#endif
   }

   /*
      Initialize glyph array to all zeroes.
   */
   for (codept = 0; codept < MAX_GLYPHS; codept++) {
      for (i = 0; i < 16; i++) glyph[codept][i] = 0x0000;
   }

   /*
      Read Hangul base glyph file.
   */
   max_codept = hangul_read_base16 (params.infp, glyph);
   if (max_codept > 0x8FF) {
      fprintf (stderr, "\nWARNING: Hangul glyph range exceeds PUA space.\n\n");
   }

   codept = params.starting_codept;  /* First code point to output */

   for (cho = params.cho_start; cho <= params.cho_end; cho++) {
      for (jung = params.jung_start; jung <= params.jung_end; jung++) {
         for (jong = params.jong_start; jong <= params.jong_end; jong++) {

#ifdef DEBUG
            fprintf (params.outfp,
                     "(U+%04X, U+%04X, U+%04X)\n",
                     cho, jung, jong);
#endif
            combined_jamo (glyph, cho, jung, jong, tmp_glyph);
            print_glyph_hex (params.outfp, codept, tmp_glyph);
            codept++;
            if (jong == JONG_UNICODE_END)
               jong = JONG_EXTB_UNICODE_START - 1; /* Start Extended-B range */
         }
         if (jung == JUNG_UNICODE_END)
            jung = JUNG_EXTB_UNICODE_START - 1;  /* Start Extended-B range */
      }
      if (cho == CHO_UNICODE_END)
         cho = CHO_EXTA_UNICODE_START - 1;  /* Start Extended-A range */
   }

   if (params.infp  != stdin)  fclose (params.infp);
   if (params.outfp != stdout) fclose (params.outfp);

   exit (EXIT_SUCCESS);
}


/**
   @brief Parse command line arguments.

*/
void
parse_args (int argc, char *argv[], struct PARAMS *params) {
   int arg_count;  /* Current index into argv[]. */

   void get_hex_range (char *instring, unsigned *start, unsigned *end);

   int strncmp (const char *s1, const char *s2, size_t n);


   arg_count = 1;

   while (arg_count < argc) {
      /* If all 600,000+ Hangul syllables are requested. */
      if (strncmp (argv [arg_count], "-all", 4) == 0) {
         params->starting_codept = 0x0001;
         params->cho_start  = CHO_UNICODE_START;       /*            First modern choseong  */
         params->cho_end    = CHO_EXTA_UNICODE_END;    /*            Last ancient choseong  */
         params->jung_start = JUNG_UNICODE_START;      /*            First modern jungseong */
         params->jung_end   = JUNG_EXTB_UNICODE_END;   /*            Last ancient jungseong */
         params->jong_start = JONG_UNICODE_START - 1;  /* One before first modern jongseong */
         params->jong_end   = JONG_EXTB_UNICODE_END;   /*            Last andient jongseong */
      }
      /* If starting code point for output Unifont hex file is specified. */
      else if (strncmp (argv [arg_count], "-c", 2) == 0) {
         arg_count++;
         if (arg_count < argc) {
            sscanf (argv [arg_count], "%X", &params->starting_codept);
         }
      }
      /* If initial consonant (choseong) range, "jamo 1", get range.  */
      else if (strncmp (argv [arg_count], "-j1", 3) == 0) {
         arg_count++;
         if (arg_count < argc) {
            get_hex_range (argv [arg_count],
                           &params->cho_start, &params->cho_end);
            /*
               Allow one initial blank glyph at start of a loop, none at end.
            */
            if (params->cho_start < CHO_UNICODE_START) {
               params->cho_start = CHO_UNICODE_START - 1;
            }
            else if (params->cho_start > CHO_UNICODE_END &&
                     params->cho_start < CHO_EXTA_UNICODE_START) {
               params->cho_start = CHO_EXTA_UNICODE_START - 1;
            }
            /*
               Do not go past desired Hangul choseong range,
               Hangul Jamo or Hangul Jamo Extended-A choseong.
            */
            if (params->cho_end > CHO_EXTA_UNICODE_END) {
               params->cho_end = CHO_EXTA_UNICODE_END;
            }
            else if (params->cho_end > CHO_UNICODE_END &&
                     params->cho_end < CHO_EXTA_UNICODE_START) {
               params->cho_end = CHO_UNICODE_END;
            }
         }
      }
      /* If medial vowel (jungseong) range, "jamo 2", get range.  */
      else if (strncmp (argv [arg_count], "-j2", 3) == 0) {
         arg_count++;
         if (arg_count < argc) {
            get_hex_range (argv [arg_count],
                           &params->jung_start, &params->jung_end);
            /*
               Allow one initial blank glyph at start of a loop, none at end.
            */
            if (params->jung_start < JUNG_UNICODE_START) {
               params->jung_start = JUNG_UNICODE_START - 1; 
            }
            else if (params->jung_start > JUNG_UNICODE_END &&
                     params->jung_start < JUNG_EXTB_UNICODE_START) {
               params->jung_start = JUNG_EXTB_UNICODE_START - 1;
            }
            /*
               Do not go past desired Hangul jungseong range,
               Hangul Jamo or Hangul Jamo Extended-B jungseong.
            */
            if (params->jung_end > JUNG_EXTB_UNICODE_END) {
               params->jung_end = JUNG_EXTB_UNICODE_END;
            }
            else if (params->jung_end > JUNG_UNICODE_END &&
                     params->jung_end < JUNG_EXTB_UNICODE_START) {
               params->jung_end = JUNG_UNICODE_END;
            }
         }
      }
      /* If final consonant (jongseong) range, "jamo 3", get range.  */
      else if (strncmp (argv [arg_count], "-j3", 3) == 0) {
         arg_count++;
         if (arg_count < argc) {
            get_hex_range (argv [arg_count],
                           &params->jong_start, &params->jong_end);
            /*
               Allow one initial blank glyph at start of a loop, none at end.
            */
            if (params->jong_start < JONG_UNICODE_START) {
               params->jong_start = JONG_UNICODE_START - 1; 
            }
            else if (params->jong_start > JONG_UNICODE_END &&
                     params->jong_start < JONG_EXTB_UNICODE_START) {
               params->jong_start = JONG_EXTB_UNICODE_START - 1;
            }
            /*
               Do not go past desired Hangul jongseong range,
               Hangul Jamo or Hangul Jamo Extended-B jongseong.
            */
            if (params->jong_end > JONG_EXTB_UNICODE_END) {
               params->jong_end = JONG_EXTB_UNICODE_END;
            }
            else if (params->jong_end > JONG_UNICODE_END &&
                     params->jong_end < JONG_EXTB_UNICODE_START) {
               params->jong_end = JONG_UNICODE_END;
            }
         }
      }
      /* If input file is specified, open it for read access. */
      else if (strncmp (argv [arg_count], "-i", 2) == 0) {
         arg_count++;
         if (arg_count < argc) {
            params->infp = fopen (argv [arg_count], "r");
            if (params->infp == NULL) {
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
            params->outfp = fopen (argv [arg_count], "w");
            if (params->outfp == NULL) {
               fprintf (stderr, "\n*** ERROR: Cannot open %s for output.\n\n",
                        argv [arg_count]);
               exit (EXIT_FAILURE);
            }
         }
      }
      /* If help is requested, print help message and exit. */
      else if (strncmp (argv [arg_count], "-h",     2) == 0 ||
               strncmp (argv [arg_count], "--help", 6) == 0) {
         printf ("\nunigen-hangul [options]\n\n");
         printf ("     Generates Hangul syllables from an input Unifont .hex file encoded\n");
         printf ("     in Johab 6/3/1 format.  By default, the output is the Unicode Hangul\n");
         printf ("     Syllables range, U+AC00..U+D7A3.  Options allow the user to specify\n");
         printf ("     a starting code point for the output Unifont .hex file, and ranges\n");
         printf ("     in hexadecimal of the starting and ending Hangul Jamo code points:\n\n");

         printf ("          * 1100-115E Initial consonants (choseong)\n");
         printf ("          * 1161-11A7 Medial vowels (jungseong)\n");
         printf ("          * 11A8-11FF Final consonants (jongseong).\n\n");

         printf ("     A single code point or 0 to omit can be specified instead of a range.\n\n");

         printf ("   Option    Parameters    Function\n");
         printf ("   ------    ----------    --------\n");
         printf ("   -h, --help              Print this message and exit.\n\n");
         printf ("   -all                    Generate all Hangul syllables, using all modern and\n");
         printf ("                           ancient Hangul in the Unicode range U+1100..U+11FF,\n");
         printf ("                           U+A960..U+A97C, and U+D7B0..U+D7FB.\n");
         printf ("                           WARNING: this will generate over 1,600,000 syllables\n");
         printf ("                           in a 115 megabyte Unifont .hex format file.  The\n");
         printf ("                           default is to only output modern Hangul syllables.\n\n");
         printf ("   -c        code_point    Starting code point in hexadecimal for output file.\n\n");
         printf ("   -j1       start-end     Choseong (jamo 1) start-end range in hexadecimal.\n\n");
         printf ("   -j2       start-end     Jungseong (jamo 2) start-end range in hexadecimal.\n\n");
         printf ("   -j3       start-end     Jongseong (jamo 3) start-end range in hexadecimal.\n\n");
         printf ("   -i        input_file    Unifont hangul-base.hex formatted input file.\n\n");
         printf ("   -o        output_file   Unifont .hex format output file.\n\n");
         printf ("      Example:\n\n");
         printf ("         unigen-hangul -c 1 -j3 11AB-11AB -i hangul-base.hex -o nieun-only.hex\n\n");
         printf ("      Generates Hangul syllables using all modern choseong and jungseong,\n");
         printf ("      and only the jongseong nieun (Unicode code point U+11AB).  The output\n");
         printf ("      Unifont .hex file will contain code points starting at 1.  Instead of\n");
         printf ("      specifying \"-j3 11AB-11AB\", simply using \"-j3 11AB\" will also suffice.\n\n");

         exit (EXIT_SUCCESS);
      }

      arg_count++;
   }

   return;
}


/**
   @brief Scan a hexadecimal range from a character string.
*/
void
get_hex_range (char *instring, unsigned *start, unsigned *end) {

   int i;  /* String index variable. */

   /* Get first number in range. */
   sscanf (instring, "%X", start);
   for (i = 0;
        instring [i] != '\0' && instring [i] != '-';
        i++);
   /* Get last number in range. */
   if (instring [i] == '-') {
      i++;
      sscanf (&instring [i], "%X", end);
   }
   else {
      *end = *start;
   }

   return;
}
