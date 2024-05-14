/**
   @file unicoverage.c

   @brief unicoverage - Show the coverage of Unicode plane scripts
                        for a GNU Unifont hex glyph file

   @author Paul Hardy, unifoundry <at> unifoundry.com, 6 January 2008
   
   @copyright Copyright (C) 2008, 2013 Paul Hardy

   Synopsis: unicoverage [-ifont_file.hex] [-ocoverage_file.txt]

   This program requires the file "coverage.dat" to be present
   in the directory from which it is run.
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

/*
   2016 (Paul Hardy): Modified in Unifont 9.0.01 release to remove non-existent
   "-p" option and empty example from help printout.

   2018 (Paul Hardy): Modified to cover entire Unicode range, not just Plane 0.

   11 May 2019: [Paul Hardy] changed strcpy function call to strlcpy
   for better error handling.

   31 May 2019: [Paul Hardy] replaced strlcpy call with strncpy
   for compilation on more systems.

   4 June 2022: [Paul Hardy] Adjusted column spacing for better alignment
   of Unicode Plane 1-15 scripts.  Added "-n" option to print number of
   glyphs in each range instead of percent coverage.

   18 September 2022: [Paul Hardy] in nextrange function, initialize retval.

   21 October 2023: [Paul Hardy]
   Added full function prototype for nextrange function in main function.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#define MAXBUF 256   ///< Maximum input line length - 1


/**
   @brief The main function.

   @param[in] argc The count of command line arguments.
   @param[in] argv Pointer to array of command line arguments.
   @return This program exits with status 0.
*/
int
main (int argc, char *argv[])
{

   int      print_n=0;        /* print # of glyphs, not percentage   */
   unsigned i;                /* loop variable                       */
   unsigned slen;             /* string length of coverage file line */
   char     inbuf[256];       /* input buffer                        */
   unsigned thischar;         /* the current character               */

   char *infile="", *outfile="";  /* names of input and output files        */
   FILE *infp, *outfp;        /* file pointers of input and output files    */
   FILE *coveragefp;          /* file pointer to coverage.dat file          */
   int cstart, cend;          /* current coverage start and end code points */
   char coverstring[MAXBUF];  /* description of current coverage range      */
   int nglyphs;               /* number of glyphs in this section           */

   /* to get next range & name of Unicode glyphs */
   int nextrange (FILE *coveragefp, int *cstart, int *cend, char *coverstring);

   void print_subtotal (FILE *outfp, int print_n, int nglyphs,
                        int cstart, int cend, char *coverstring);

   if ((coveragefp = fopen ("coverage.dat", "r")) == NULL) {
      fprintf (stderr, "\nError: data file \"coverage.dat\" not found.\n\n");
      exit (0);
   }

   if (argc > 1) {
      for (i = 1; i < argc; i++) {
         if (argv[i][0] == '-') {  /* this is an option argument */
            switch (argv[i][1]) {
               case 'i':  /* name of input file */
                  infile = &argv[i][2];
                  break;
               case 'n':  /* print number of glyphs instead of percentage */
                  print_n = 1;
               case 'o':  /* name of output file */
                  outfile = &argv[i][2];
                  break;
               default:   /* if unrecognized option, print list and exit */
                  fprintf (stderr, "\nSyntax:\n\n");
                  fprintf (stderr, "   %s -p<Unicode_Page> ", argv[0]);
                  fprintf (stderr, "-i<Input_File> -o<Output_File> -w\n\n");
                  exit (1);
            }
         }
      }
   }
   /*
      Make sure we can open any I/O files that were specified before
      doing anything else.
   */
   if (strlen (infile) > 0) {
      if ((infp = fopen (infile, "r")) == NULL) {
         fprintf (stderr, "Error: can't open %s for input.\n", infile);
         exit (1);
      }
   }
   else {
      infp = stdin;
   }
   if (strlen (outfile) > 0) {
      if ((outfp = fopen (outfile, "w")) == NULL) {
         fprintf (stderr, "Error: can't open %s for output.\n", outfile);
         exit (1);
      }
   }
   else {
      outfp = stdout;
   }

   /*
      Print header row.
   */
   if (print_n) {
      fprintf (outfp, "# Glyphs      Range        Script\n");
      fprintf (outfp, "--------      -----        ------\n");
   }
   else {
      fprintf (outfp, "Covered      Range        Script\n");
      fprintf (outfp, "-------      -----        ------\n\n");
   }

   slen = nextrange (coveragefp, &cstart, &cend, coverstring);
   nglyphs = 0;

   /*
      Read in the glyphs in the file
   */
   while (slen != 0 && fgets (inbuf, MAXBUF-1, infp) != NULL) {
      sscanf (inbuf, "%x", &thischar);

      /* Read a character beyond end of current script. */
      while (cend < thischar && slen != 0) {
         print_subtotal (outfp, print_n, nglyphs, cstart, cend, coverstring);

         /* start new range total */
         slen = nextrange (coveragefp, &cstart, &cend, coverstring);
         nglyphs = 0;
      }
      nglyphs++;
   }

   print_subtotal (outfp, print_n, nglyphs, cstart, cend, coverstring);

   exit (0);
}

/**
   @brief Get next Unicode range.

   This function reads the next Unicode script range to count its
   glyph coverage.

   @param[in] coveragefp File pointer to Unicode script range data file.
   @param[in] cstart Starting code point in current Unicode script range.
   @param[in] cend Ending code point in current Unicode script range.
   @param[out] coverstring String containing <cstart>-<cend> substring.
   @return Length of the last string read, or 0 for end of file.
*/
int
nextrange (FILE *coveragefp,
              int *cstart, int *cend,
              char *coverstring)
{
   int i;
   static char inbuf[MAXBUF];
   int retval;         /* the return value */

   retval = 0;

   do {
      if (fgets (inbuf, MAXBUF-1, coveragefp) != NULL) {
         retval = strlen (inbuf);
         if ((inbuf[0] >= '0' && inbuf[0] <= '9') ||
             (inbuf[0] >= 'A' && inbuf[0] <= 'F') ||
             (inbuf[0] >= 'a' && inbuf[0] <= 'f')) {
            sscanf (inbuf, "%x-%x", cstart, cend);
            i = 0;
            while (inbuf[i] != ' ') i++;  /* find first blank */
            while (inbuf[i] == ' ') i++;  /* find next non-blank */
            strncpy (coverstring, &inbuf[i], MAXBUF);
         }
         else retval = 0;
      }
      else retval = 0;
   } while (retval == 0 && !feof (coveragefp));

   return (retval);
}


/**
   @brief Print the subtotal for one Unicode script range.

   @param[in] outfp Pointer to output file.
   @param[in] print_n 1 = print number of glyphs, 0 = print percentage.
   @param[in] nglyphs Number of glyphs in current range.
   @param[in] cstart Starting code point for current range.
   @param[in] cend Ending code point for current range.
   @param[in] coverstring Character string of "<cstart>-<cend>".
*/
void print_subtotal (FILE *outfp, int print_n, int nglyphs,
                     int cstart, int cend, char *coverstring) {

   /* print old range total */
   if (print_n) {  /* Print number of glyphs, not percentage */
      fprintf (outfp, " %6d ", nglyphs);
   }
   else {
      fprintf (outfp, " %5.1f%%", 100.0*nglyphs/(1+cend-cstart));
   }

   if (cend < 0x10000)
      fprintf (outfp, "  U+%04X..U+%04X   %s",
               cstart, cend, coverstring);
   else
      fprintf (outfp, " U+%05X..U+%05X  %s",
               cstart, cend, coverstring);

   return;
}
