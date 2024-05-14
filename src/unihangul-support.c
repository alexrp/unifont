/**
   @file unihangul-support.c

   @brief Functions for converting Hangul letters into syllables

   This file contains functions for reading in Hangul letters
   arranged in a Johab 6/3/1 pattern and composing syllables
   with them.  One function maps an iniital letter (choseong),
   medial letter (jungseong), and final letter (jongseong)
   into the Hangul Syllables Unicode block, U+AC00..U+D7A3.
   Other functions allow formation of glyphs that include
   the ancient Hangul letters that Hanterm supported.  More
   can be added if desired, with appropriate changes to
   start positions and lengths defined in "hangul.h".

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
#include "hangul.h"


/**
   @brief Read hangul-base.hex file into a unsigned char array.

   Read a Hangul base .hex file with separate choseong, jungseong,
   and jongseong glyphs for syllable formation.  The order is:

      - Empty glyph in 0x0000 position.
      - Initial consonants (choseong).
      - Medial vowels and dipthongs (jungseong).
      - Final consonants (jongseong).
      - Individual letter forms in isolation, not for syllable formation.

   The letters are arranged with all variations for one letter
   before continuing to the next letter.  In the current
   encoding, there are 6 variations of choseong, 3 of jungseong,
   and 1 of jongseong per letter.

   @param[in] Input file pointer; can be stdin.
   @param[out] Array of bit patterns, with 32 8-bit values per letter.
   @return The maximum code point value read in the file.
*/
unsigned
hangul_read_base8 (FILE *infp, unsigned char base[][32]) {
   unsigned codept;
   unsigned max_codept;
   int      i, j;
   char     instring[MAXLINE];


   max_codept = 0;

   while (fgets (instring, MAXLINE, infp) != NULL) {
      sscanf (instring, "%X", &codept);
      codept -= PUA_START;
      /* If code point is within range, add it */
      if (codept < MAX_GLYPHS) {
         /* Find the start of the glyph bitmap. */
         for (i = 1; instring[i] != '\0' && instring[i] != ':'; i++);
         if (instring[i] == ':') {
            i++;  /* Skip over ':' to get to start of bitmap. */
            for (j = 0; j < 32; j++) {
               sscanf (&instring[i], "%2hhX", &base[codept][j]);
               i += 2;
            }
            if (codept > max_codept) max_codept = codept;
         }
      }
   }

   return max_codept;
}


/**
   @brief Read hangul-base.hex file into a unsigned array.

   Read a Hangul base .hex file with separate choseong, jungseong,
   and jongseong glyphs for syllable formation.  The order is:

      - Empty glyph in 0x0000 position.
      - Initial consonants (choseong).
      - Medial vowels and dipthongs (jungseong).
      - Final consonants (jongseong).
      - Individual letter forms in isolation, not for syllable formation.

   The letters are arranged with all variations for one letter
   before continuing to the next letter.  In the current
   encoding, there are 6 variations of choseong, 3 of jungseong,
   and 1 of jongseong per letter.

   @param[in] Input file pointer; can be stdin.
   @param[out] Array of bit patterns, with 16 16-bit values per letter.
   @return The maximum code point value read in the file.
*/
unsigned
hangul_read_base16 (FILE *infp, unsigned base[][16]) {
   unsigned codept;
   unsigned max_codept;
   int      i, j;
   char     instring[MAXLINE];


   max_codept = 0;

   while (fgets (instring, MAXLINE, infp) != NULL) {
      sscanf (instring, "%X", &codept);
      codept -= PUA_START;
      /* If code point is within range, add it */
      if (codept < MAX_GLYPHS) {
         /* Find the start of the glyph bitmap. */
         for (i = 1; instring[i] != '\0' && instring[i] != ':'; i++);
         if (instring[i] == ':') {
            i++;  /* Skip over ':' to get to start of bitmap. */
            for (j = 0; j < 16; j++) {
               sscanf (&instring[i], "%4X", &base[codept][j]);
               i += 4;
            }
            if (codept > max_codept) max_codept = codept;
         }
      }
   }

   return max_codept;
}


/**
   @brief Decompose a Hangul Syllables code point into three letters.

   Decompose a Hangul Syllables code point (U+AC00..U+D7A3) into:

      - Choseong    0-19
      - Jungseong   0-20
      - Jongseong   0-27 or -1 if no jongseong

   All letter values are  set to -1 if the letters do not
   form a syllable in the Hangul Syllables range.  This function
   only handles modern Hangul, because that is all that is in
   the Hangul Syllables range.

   @param[in] codept The Unicode code point to decode, from 0xAC00 to 0xD7A3.
   @param[out] initial The 1st letter (choseong)  in the syllable.
   @param[out] initial The 2nd letter (jungseong) in the syllable.
   @param[out] initial The 3rd letter (jongseong) in the syllable.
*/
void
hangul_decompose (unsigned codept, int *initial, int *medial, int *final) {

   if (codept < 0xAC00 || codept > 0xD7A3) {
      *initial = *medial = *final = -1;
   }
   else {
      codept  -= 0xAC00;
      *initial =  codept / (28  * 21);
      *medial  = (codept /  28) % 21;
      *final   =  codept %  28 - 1;
   }
   
   return;
}


/**
   @brief Compose a Hangul syllable into a code point, or 0 if none exists.

   This function takes three letters that can form a modern Hangul
   syllable and returns the corresponding Unicode Hangul Syllables
   code point in the range 0xAC00 to 0xD7A3.

   If a three-letter combination includes one or more archaic letters,
   it will not map into the Hangul Syllables range.  In that case,
   the returned code point will be 0 to indicate that no valid
   Hangul Syllables code point exists.

   @param[in] initial The first letter (choseong), 0 to 18.
   @param[in] medial  The second letter (jungseong), 0 to 20.
   @param[in] final   The third letter (jongseong), 0 to 26 or -1 if none.
   @return The Unicode Hangul Syllables code point, 0xAC00 to 0xD7A3.
*/
unsigned
hangul_compose (int initial, int medial, int final) {
   unsigned codept;


   if (initial >= 0 && initial <= 18 &&
       medial  >= 0 && medial  <= 20 &&
       final   >= 0 && final   <= 26) {

      codept  = 0xAC00;
      codept += initial * 21 * 28;
      codept += medial  * 28;
      codept += final + 1;
   }
   else {
      codept = 0;
   }

   return codept;
}


/**
   @brief Determine index values to the bitmaps for a syllable's components.

   This function reads these input values for modern and ancient Hangul letters:

      - Choseong  number (0 to the number of modern and archaic choseong  - 1.
      - Jungseong number (0 to the number of modern and archaic jungseong - 1.
      - Jongseong number (0 to the number of modern and archaic jongseong - 1, or -1 if none.

   It then determines the variation of each letter given the combination with
   the other two letters (or just choseong and jungseong if the jongseong value
   is -1).

   These variations are then converted into index locations within the
   glyph array that was read in from the hangul-base.hex file.  Those
   index locations can then be used to form a composite syllable.

   There is no restriction to only use the modern Hangul letters.

   @param[in] choseong  The 1st letter in the syllable.
   @param[in] jungseong The 2nd letter in the syllable.
   @param[in] jongseong The 3rd letter in the syllable, or -1 if none.
   @param[out] cho_index Index location to the 1st letter variation from the hangul-base.hex file.
   @param[out] jung_index Index location to the 2nd letter variation from the hangul-base.hex file.
   @param[out] jong_index Index location to the 3rd letter variation from the hangul-base.hex file.
*/
void
hangul_hex_indices (int choseong, int jungseong, int jongseong,
                   int *cho_index, int *jung_index, int *jong_index) {

   int cho_variation, jung_variation, jong_variation;  /* Letter variations */

   void hangul_variations (int choseong, int jungseong, int jongseong,
           int *cho_variation, int *jung_variation, int *jong_variation);


   hangul_variations (choseong, jungseong, jongseong,
                      &cho_variation, &jung_variation, &jong_variation);

    *cho_index = CHO_HEX +  choseong * CHO_VARIATIONS +  cho_variation;
   *jung_index = JUNG_HEX      + jungseong * JUNG_VARIATIONS      + jung_variation;;
   *jong_index = jongseong < 0 ? 0x0000 :
                 JONG_HEX + jongseong * JONG_VARIATIONS + jong_variation;

   return;
}


/**
   @brief Determine the variations of each letter in a Hangul syllable.

   Given the three letters that will form a syllable, return the variation
   of each letter used to form the composite glyph.

   This function can determine variations for both modern and archaic
   Hangul letters; it is not limited to only the letters combinations
   that comprise the Unicode Hangul Syllables range.

   This function reads these input values for modern and ancient Hangul letters:

      - Choseong  number (0 to the number of modern and archaic choseong  - 1.
      - Jungseong number (0 to the number of modern and archaic jungseong - 1.
      - Jongseong number (0 to the number of modern and archaic jongseong - 1, or -1 if none.

   It then determines the variation of each letter given the combination with
   the other two letters (or just choseong and jungseong if the jongseong value
   is -1).

   @param[in] choseong  The 1st letter in the syllable.
   @param[in] jungseong The 2nd letter in the syllable.
   @param[in] jongseong The 3rd letter in the syllable, or -1 if none.
   @param[out] cho_var  Variation of the 1st letter from the hangul-base.hex file.
   @param[out] jung_var Variation of the 2nd letter from the hangul-base.hex file.
   @param[out] jong_var Variation of the 3rd letter from the hangul-base.hex file.
*/
void
hangul_variations (int choseong, int jungseong, int jongseong,
                   int *cho_var, int *jung_var, int *jong_var) {

   int  cho_variation (int choseong, int jungseong, int jongseong);
   int jung_variation (int choseong, int jungseong, int jongseong);
   int jong_variation (int choseong, int jungseong, int jongseong);

   /*
      Find the variation for each letter component.
   */
    *cho_var =  cho_variation (choseong, jungseong, jongseong);
   *jung_var = jung_variation (choseong, jungseong, jongseong);
   *jong_var = jong_variation (choseong, jungseong, jongseong);


   return;
}


/**
   @brief Return the Johab 6/3/1 choseong variation for a syllable.

   This function takes the two or three (if jongseong is included)
   letters that comprise a syllable and determine the variation
   of the initial consonant (choseong).

   Each choseong has 6 variations:

      Variation   Occurrence
      ---------   ----------
          0       Choseong with a vertical vowel such as "A".
          1       Choseong with a horizontal vowel such as "O".
          2       Choseong with a vertical and horizontal vowel such as "WA".
          3       Same as variation 0, but with jongseong (final consonant).
          4       Same as variation 1, but with jongseong (final consonant).
                  Also a horizontal vowel pointing down, such as U and YU.
          5       Same as variation 2, but with jongseong (final consonant).
                  Also a horizontal vowel pointing down with vertical element,
                  such as WEO, WE, and WI.

   In addition, if the vowel is horizontal and a downward-pointing stroke
   as in the modern letters U, WEO, WE, WI, and YU, and in archaic
   letters YU-YEO, YU-YE, YU-I, araea, and araea-i, then 3 is added
   to the initial variation of 0 to 2, resulting in a choseong variation
   of 3 to 5, respectively.

   @param[in] choseong  The 1st letter in the syllable.
   @param[in] jungseong The 2nd letter in the syllable.
   @param[in] jongseong The 3rd letter in the syllable.
   @return The choseong variation, 0 to 5.
*/
int
cho_variation (int choseong, int jungseong, int jongseong) {
   int cho_variation;  /* Return value */

   /*
      The Choseong cho_var is determined by the
      21 modern + 50 ancient Jungseong, and whether
      or not the syllable contains a final consonant
      (Jongseong).
   */
   static int choseong_var [TOTAL_JUNG + 1] = {
          /*
             Modern Jungseong in positions 0..20.
          */
/* Location Variations  Unicode Range    Vowel #    Vowel Names  */
/* -------- ----------  --------------   -------    -----------  */
/* 0x2FB */ 0, 0, 0, // U+1161..U+1163-->[ 0.. 2]  A,   AE,  YA
/* 0x304 */ 0, 0, 0, // U+1164..U+1166-->[ 3.. 5]  YAE, EO,  E
/* 0x30D */ 0, 0,    // U+1167..U+1168-->[ 6.. 7]  YEO, YE
/* 0x313 */ 1,       // U+1169        -->[ 8]      O
/* 0x316 */ 2, 2, 2, // U+116A..U+116C-->[ 9..11]  WA,  WAE, WE
/* 0x31F */ 1, 4,    // U+116D..U+116E-->[12..13]  YO,  U
/* 0x325 */ 5, 5, 5, // U+116F..U+1171-->[14..16]  WEO, WE,  WI
/* 0x32E */ 4, 1,    // U+1172..U+1173-->[17..18]  YU,  EU
/* 0x334 */ 2,       // U+1174        -->[19]      YI
/* 0x337 */ 0,       // U+1175        -->[20]      I
          /*
             Ancient Jungseong in positions 21..70.
          */
/* Location  Variations  Unicode Range    Vowel #       Vowel Names  */
/* --------  ----------  --------------   -------       -----------  */
/* 0x33A: */ 2, 5, 2, // U+1176..U+1178-->[21..23]   A-O,    A-U,   YA-O
/* 0x343: */ 2, 2, 5, // U+1179..U+117B-->[24..26]  YA-YO,  EO-O,   EU-U
/* 0x34C: */ 2, 2, 5, // U+117C..U+117E-->[27..29]  EO-EU, YEO-O,  YEO-U
/* 0x355: */ 2, 5, 5, // U+117F..U+1181-->[30..32]   O-EO,   O-E,    O-YE,
/* 0x35E: */ 4, 4, 2, // U+1182..U+1184-->[33..35]   O-O,    O-U,   YO-YA,
/* 0x367: */ 2, 2, 5, // U+1185..U+1187-->[36..38]  YO-YAE, YO-YEO, YO-O,
/* 0x370: */ 2, 5, 5, // U+1188..U+118A-->[39..41]  YO-I,    U-A,    U-AE,
/* 0x379: */ 5, 5, 5, // U+118B..U+118D-->[42..44] U-EO-EU,  U-YE,   U-U,
/* 0x382: */ 5, 5, 5, // U+118E..U+1190-->[45..47]  YU-A,   YU-EO,  YU-E,
/* 0x38B: */ 5, 5, 2, // U+1191..U+1193-->[48..50]  YU-YEO, YU-YE,  YU-U,
/* 0x394: */ 5, 2, 2, // U+1194..U+1196-->[51..53]  YU-I,   EU-U,   EU-EU,
/* 0x39D: */ 2, 0, 0, // U+1197..U+1199-->[54..56]  YI-U,    I-A,    I-YA,
/* 0x3A6: */ 2, 5, 2, // U+119A..U+119C-->[57..59]   I-O,    I-U,    I-EU,
/* 0x3AF: */ 0, 1, 2, // U+119D..U+119F-->[60..62] I-ARAEA, ARAEA, ARAEA-EO,
/* 0x3B8: */ 1, 2, 1, // U+11A0..U+11A2-->[63..65] ARAEA-U, ARAEA-I,SSANGARAEA,
/* 0x3C1: */ 2, 5, 0, // U+11A3..U+11A5-->[66..68]   A-EU,  YA-U,  YEO-YA,
/* 0x3CA: */ 2, 2,    // U+11A6..U+11A7-->[69..70]   O-YA,   O-YAE,
#ifdef EXTENDED_HANGUL
/* 0x3D0: */ 2, 4, 5, // U+D7B0..U+D7B2-->[71..73]   O-YEO,  O-O-I, YO-A,
/* 0x3D9: */ 5, 2, 5, // U+D7B3..U+D7B5-->[74..76]  YO-AE,  YO-EO,   U-YEO,
/* 0x3E2: */ 5, 5, 4, // U+D7B6..U+D7B8-->[77..79]   U-I-I, YU-AE,  YU-O,
/* 0x3EB: */ 5, 2, 5, // U+D7B9..U+D7BB-->[80..82]  EU-A,   EU-EO,  EU-E,
/* 0x3F4: */ 4, 2, 3, // U+D7BC..U+D7BE-->[83..85]  EU-O,    I-YA-O, I-YAE,
/* 0x3FD: */ 3, 3, 2, // U+D7BF..U+D7C1-->[86..88]   I-YEO,  I-YE,   I-O-I,
/* 0x406: */ 2, 2, 0, // U+D7C2..U+D7C4-->[89..91]   I-YO,   I-YU,   I-I,
/* 0x40F: */ 2, 2,    // U+D7C5..U+D7C6-->[92..93] ARAEA-A, ARAEA-E,
/* 0x415: */ -1       // Mark end of list of vowels. 
#else
/* 0x310: */ -1       // Mark end of list of vowels.
#endif
   };


   if (jungseong < 0 || jungseong >= TOTAL_JUNG) {
      cho_variation = -1;
   }
   else {
      cho_variation = choseong_var [jungseong];
      if (choseong >= 0 && jongseong >= 0 && cho_variation < 3)
         cho_variation += 3;
   }


   return cho_variation;
}


/**
   @brief Whether vowel has rightmost vertical stroke to the right.

   @param[in] vowel Vowel number, from 0 to TOTAL_JUNG - 1.
   @return 1 if this vowel's vertical stroke is wide on the right side; else 0.
*/
int
is_wide_vowel (int vowel) {
   int retval;  /* Return value. */

   static int wide_vowel [TOTAL_JUNG + 1] = {
          /*
             Modern Jungseong in positions 0..20.
          */
/* Location Variations  Unicode Range    Vowel #    Vowel Names  */
/* -------- ----------  --------------   -------    -----------  */
/* 0x2FB */ 0, 1, 0, // U+1161..U+1163-->[ 0.. 2]  A,   AE,  YA
/* 0x304 */ 1, 0, 1, // U+1164..U+1166-->[ 3.. 5]  YAE, EO,  E
/* 0x30D */ 0, 1,    // U+1167..U+1168-->[ 6.. 7]  YEO, YE
/* 0x313 */ 0,       // U+1169        -->[ 8]      O
/* 0x316 */ 0, 1, 0, // U+116A..U+116C-->[ 9..11]  WA,  WAE, WE
/* 0x31F */ 0, 0,    // U+116D..U+116E-->[12..13]  YO,  U
/* 0x325 */ 0, 1, 0, // U+116F..U+1171-->[14..16]  WEO, WE,  WI
/* 0x32E */ 0, 0,    // U+1172..U+1173-->[17..18]  YU,  EU
/* 0x334 */ 0,       // U+1174        -->[19]      YI
/* 0x337 */ 0,       // U+1175        -->[20]      I
          /*
             Ancient Jungseong in positions 21..70.
          */
/* Location  Variations  Unicode Range    Vowel #       Vowel Names  */
/* --------  ----------  --------------   -------       -----------  */
/* 0x33A: */ 0, 0, 0, // U+1176..U+1178-->[21..23]   A-O,    A-U,   YA-O
/* 0x343: */ 0, 0, 0, // U+1179..U+117B-->[24..26]  YA-YO,  EO-O,   EU-U
/* 0x34C: */ 0, 0, 0, // U+117C..U+117E-->[27..29]  EO-EU, YEO-O,  YEO-U
/* 0x355: */ 0, 1, 1, // U+117F..U+1181-->[30..32]   O-EO,   O-E,    O-YE,
/* 0x35E: */ 0, 0, 0, // U+1182..U+1184-->[33..35]   O-O,    O-U,   YO-YA,
/* 0x367: */ 1, 0, 0, // U+1185..U+1187-->[36..38]  YO-YAE, YO-YEO, YO-O,
/* 0x370: */ 0, 0, 1, // U+1188..U+118A-->[39..41]  YO-I,    U-A,    U-AE,
/* 0x379: */ 0, 1, 0, // U+118B..U+118D-->[42..44] U-EO-EU,  U-YE,   U-U,
/* 0x382: */ 0, 0, 1, // U+118E..U+1190-->[45..47]  YU-A,   YU-EO,  YU-E,
/* 0x38B: */ 0, 1, 0, // U+1191..U+1193-->[48..50]  YU-YEO, YU-YE,  YU-U,
/* 0x394: */ 0, 0, 0, // U+1194..U+1196-->[51..53]  YU-I,   EU-U,   EU-EU,
/* 0x39D: */ 0, 0, 0, // U+1197..U+1199-->[54..56]  YI-U,    I-A,    I-YA,
/* 0x3A6: */ 0, 0, 0, // U+119A..U+119C-->[57..59]   I-O,    I-U,    I-EU,
/* 0x3AF: */ 0, 0, 0, // U+119D..U+119F-->[60..62] I-ARAEA, ARAEA, ARAEA-EO,
/* 0x3B8: */ 0, 0, 0, // U+11A0..U+11A2-->[63..65] ARAEA-U, ARAEA-I,SSANGARAEA,
/* 0x3C1: */ 0, 0, 0, // U+11A3..U+11A5-->[66..68]   A-EU,  YA-U,  YEO-YA,
/* 0x3CA: */ 0, 1,    // U+11A6..U+11A7-->[69..70]   O-YA,   O-YAE
#ifdef EXTENDED_HANGUL
/* 0x3D0: */ 0, 0, 0, // U+D7B0..U+D7B2-->[71..73]   O-YEO,  O-O-I, YO-A,
/* 0x3D9: */ 1, 0, 0, // U+D7B3..U+D7B5-->[74..76]  YO-AE,  YO-EO,   U-YEO,
/* 0x3E2: */ 1, 1, 0, // U+D7B6..U+D7B8-->[77..79]   U-I-I, YU-AE,  YU-O,
/* 0x3EB: */ 0, 0, 1, // U+D7B9..U+D7BB-->[80..82]  EU-A,   EU-EO,  EU-E,
/* 0x3F4: */ 0, 0, 1, // U+D7BC..U+D7BE-->[83..85]  EU-O,    I-YA-O, I-YAE,
/* 0x3FD: */ 0, 1, 0, // U+D7BF..U+D7C1-->[86..88]   I-YEO,  I-YE,   I-O-I,
/* 0x406: */ 0, 0, 1, // U+D7C2..U+D7C4-->[89..91]   I-YO,   I-YU,   I-I,
/* 0x40F: */ 0, 1,    // U+D7C5..U+D7C6-->[92..93] ARAEA-A, ARAEA-E,
/* 0x415: */ -1       // Mark end of list of vowels.
#else
/* 0x310: */ -1       // Mark end of list of vowels.
#endif
   };


   if (vowel >= 0 && vowel < TOTAL_JUNG) {
      retval = wide_vowel [vowel];
   }
   else {
      retval = 0;
   }


   return retval;
}


/**
   @brief Return the Johab 6/3/1 jungseong variation.

   This function takes the two or three (if jongseong is included)
   letters that comprise a syllable and determine the variation
   of the vowel (jungseong).

   Each jungseong has 3 variations:

      Variation   Occurrence
      ---------   ----------
          0       Jungseong with only chungseong (no jungseong).
          1       Jungseong with chungseong and jungseong (except nieun).
          2       Jungseong with chungseong and jungseong nieun.
   
   @param[in] choseong  The 1st letter in the syllable.
   @param[in] jungseong The 2nd letter in the syllable.
   @param[in] jongseong The 3rd letter in the syllable.
   @return The jungseong variation, 0 to 2.
*/
inline int
jung_variation (int choseong, int jungseong, int jongseong) {
   int jung_variation;  /* Return value */

   if (jungseong < 0) {
      jung_variation = -1;
   }
   else {
      jung_variation = 0;
      if (jongseong >= 0) {
         if (jongseong == 3)
            jung_variation = 2;  /* Vowel for final Nieun. */
         else
            jung_variation = 1;
      }
   }


   return jung_variation;
}


/**
   @brief Return the Johab 6/3/1 jongseong variation.

   There is only one jongseong variation, so this function
   always returns 0.  It is a placeholder function for
   possible future adaptation to other johab encodings.

   @param[in] choseong  The 1st letter in the syllable.
   @param[in] jungseong The 2nd letter in the syllable.
   @param[in] jongseong The 3rd letter in the syllable.
   @return The jongseong variation, always 0.
*/
inline int
jong_variation (int choseong, int jungseong, int jongseong) {

   return 0;  /* There is only one Jongseong variation. */
}


/**
   @brief Given letters in a Hangul syllable, return a glyph.

   This function returns a glyph bitmap comprising up to three
   Hangul letters that form a syllable.  It reads the three
   component letters (choseong, jungseong, and jungseong),
   then calls a function that determines the appropriate
   variation of each letter, returning the letter bitmap locations
   in the glyph array.  Then these letter bitmaps are combined
   with a logical OR operation to produce a final bitmap,
   which forms a 16 row by 16 column bitmap glyph.

   @param[in] choseong  The 1st letter in the composite glyph.
   @param[in] jungseong The 2nd letter in the composite glyph.
   @param[in] jongseong The 3rd letter in the composite glyph.
   @param[in] hangul_base The glyphs read from the "hangul_base.hex" file.
   @return syllable The composite syllable, as a 16 by 16 pixel bitmap.
*/
void
hangul_syllable (int choseong, int jungseong, int jongseong,
                 unsigned char hangul_base[][32], unsigned char *syllable) {

   int      i;  /* loop variable */
   int      cho_hex, jung_hex, jong_hex;
   unsigned char glyph_byte;


   hangul_hex_indices (choseong, jungseong, jongseong,
                      &cho_hex, &jung_hex, &jong_hex);

   for (i = 0; i < 32; i++) {
      glyph_byte  = hangul_base [cho_hex][i];
      glyph_byte |= hangul_base [jung_hex][i];
      if (jong_hex >= 0) glyph_byte |= hangul_base [jong_hex][i];
      syllable[i] = glyph_byte;
   }

   return;
}


/**
   @brief See if two glyphs overlap.

   @param[in] glyph1 The first glyph, as a 16-row bitmap.
   @param[in] glyph2 The second glyph, as a 16-row bitmap.
   @return 0 if no overlaps between glyphs, 1 otherwise.
*/
int
glyph_overlap (unsigned *glyph1, unsigned *glyph2) {
   int overlaps;  /* Return value; 0 if no overlaps, -1 if overlaps. */
   int i;

   /* Check for overlaps between the two glyphs. */

   i = 0;
   do {
      overlaps = (glyph1[i] & glyph2[i]) != 0;
      i++;
   }  while (i < 16 && overlaps == 0);

   return overlaps;
}


/**
   @brief Combine two glyphs into one glyph.

   @param[in] glyph1 The first glyph to overlap.
   @param[in] glyph2 The second glyph to overlap.
   @param[out] combined_glyph The returned combination glyph.
*/
void
combine_glyphs (unsigned *glyph1, unsigned *glyph2,
                unsigned *combined_glyph) {
   int i;

   for (i = 0; i < 16; i++)
      combined_glyph [i] = glyph1 [i] | glyph2 [i];

   return;
}


/**
   @brief Print one glyph in Unifont hexdraw plain text style.

   @param[in] fp         The file pointer for output.
   @param[in] codept     The Unicode code point to print with the glyph.
   @param[in] this_glyph The 16-row by 16-column glyph to print.
*/
void
print_glyph_txt (FILE *fp, unsigned codept, unsigned *this_glyph) {
   int i;
   unsigned mask;


   fprintf (fp, "%04X:", codept);

   /* for each this_glyph row */
   for (i = 0; i < 16; i++) {
      mask = 0x8000;
      fputc ('\t', fp);
      while (mask != 0x0000) {
         if (mask & this_glyph [i]) {
            fputc ('#', fp);
         }
         else {
            fputc ('-', fp);
         }
         mask >>= 1;  /* shift to next bit in this_glyph row */
      }
      fputc ('\n', fp);
   }
   fputc ('\n', fp);
   
   return;
}


/**
   @brief Print one glyph in Unifont hexdraw hexadecimal string style.

   @param[in] fp         The file pointer for output.
   @param[in] codept     The Unicode code point to print with the glyph.
   @param[in] this_glyph The 16-row by 16-column glyph to print.
*/
void
print_glyph_hex (FILE *fp, unsigned codept, unsigned *this_glyph) {

   int i;


   fprintf (fp, "%04X:", codept);

   /* for each this_glyph row */
   for (i = 0; i < 16; i++) {
      fprintf (fp, "%04X", this_glyph[i]);
   }
   fputc ('\n', fp);
   
   return;
}


/**
   @brief Convert Hangul Jamo choseong, jungseong, and jongseong into a glyph.

   @param[in]  glyph_table The collection of all jamo glyphs.
   @param[in]  jamo        The Unicode code point, 0 or 0x1100..0x115F.
   @param[out] jamo_glyph  The output glyph, 16 columns in each of 16 rows.
*/
void
one_jamo (unsigned glyph_table [MAX_GLYPHS][16],
          unsigned jamo, unsigned *jamo_glyph) {

   int i;  /* Loop variable */
   int glyph_index;  /* Location of glyph in "hangul-base.hex" array */


   /* If jamo is invalid range, use blank glyph, */
   if (jamo >= 0x1100 && jamo <= 0x11FF) {
      glyph_index = jamo - 0x1100 + JAMO_HEX;
   }
   else if (jamo >= 0xA960 && jamo <= 0xA97F) {
      glyph_index = jamo - 0xA960 + JAMO_EXTA_HEX;
   }
   else if (jamo >= 0xD7B0 && jamo <= 0xD7FF) {
      glyph_index = jamo - 0x1100 + JAMO_EXTB_HEX;
   }
   else {
      glyph_index = 0;
   }

   for (i = 0; i < 16; i++) {
      jamo_glyph [i] = glyph_table [glyph_index] [i];
   }

   return;
}


/**
   @brief Convert Hangul Jamo choseong, jungseong, and jongseong into a glyph.

   This function converts input Hangul choseong, jungseong, and jongseong
   Unicode code triplets into a Hangul syllable.  Any of those with an
   out of range code point are assigned a blank glyph for combining.

   This function performs the following steps:

        1) Determine the sequence number of choseong, jungseong,
           and jongseong, from 0 to the total number of choseong,
           jungseong, or jongseong, respectively, minus one.  The
           sequence for each is as follows:

           a) Choseong: Unicode code points of U+1100..U+115E
              and then U+A960..U+A97C.

           b) Jungseong: Unicode code points of U+1161..U+11A7
              and then U+D7B0..U+D7C6.

           c) Jongseong: Unicode code points of U+11A8..U+11FF
              and then U+D7CB..U+D7FB.

        2) From the choseong, jungseong, and jongseong sequence number,
           determine the variation of choseong and jungseong (there is
           only one jongseong variation, although it is shifted right
           by one column for some vowels with a pair of long vertical
           strokes on the right side).

        3) Convert the variation numbers for the three syllable
           components to index locations in the glyph array.

        4) Combine the glyph array glyphs into a syllable.

   @param[in] glyph_table The collection of all jamo glyphs.
   @param[in] cho The choseong Unicode code point, 0 or 0x1100..0x115F.
   @param[in] jung The jungseong Unicode code point, 0 or 0x1160..0x11A7.
   @param[in] jong The jongseong Unicode code point, 0 or 0x11A8..0x11FF.
   @param[out] combined_glyph The output glyph, 16 columns in each of 16 rows.
*/
void
combined_jamo (unsigned glyph_table [MAX_GLYPHS][16],
               unsigned cho, unsigned jung, unsigned jong,
               unsigned *combined_glyph) {

   int i;  /* Loop variable. */
   int cho_num,   jung_num,   jong_num;
   int cho_group, jung_group, jong_group;
   int cho_index, jung_index, jong_index;

   unsigned tmp_glyph[16]; /* Hold shifted jongsung for wide vertical vowel. */

   int  cho_variation (int choseong, int jungseong, int jongseong);

   void combine_glyphs (unsigned *glyph1, unsigned *glyph2,
                        unsigned *combined_glyph);


   /* Choose a blank glyph for each syllalbe by default. */
   cho_index = jung_index = jong_index = 0x000;

   /*
      Convert Unicode code points to jamo sequence number
      of each letter, or -1 if letter is not in valid range.
   */
   if (cho >= 0x1100 && cho <= 0x115E)
      cho_num = cho - CHO_UNICODE_START;
   else if (cho >= CHO_EXTA_UNICODE_START &&
            cho < (CHO_EXTA_UNICODE_START + NCHO_EXTA))
      cho_num = cho - CHO_EXTA_UNICODE_START + NCHO_MODERN + NJONG_ANCIENT;
   else
      cho_num = -1;

   if (jung >= 0x1161 && jung <= 0x11A7)
      jung_num = jung - JUNG_UNICODE_START;
   else if (jung >= JUNG_EXTB_UNICODE_START &&
            jung < (JUNG_EXTB_UNICODE_START + NJUNG_EXTB))
      jung_num = jung - JUNG_EXTB_UNICODE_START + NJUNG_MODERN + NJUNG_ANCIENT;
   else
      jung_num = -1;

   if (jong >= 0x11A8 && jong <= 0x11FF)
      jong_num = jong - JONG_UNICODE_START;
   else if (jong >= JONG_EXTB_UNICODE_START &&
            jong < (JONG_EXTB_UNICODE_START + NJONG_EXTB))
      jong_num = jong - JONG_EXTB_UNICODE_START + NJONG_MODERN + NJONG_ANCIENT;
   else
      jong_num = -1;

   /*
      Choose initial consonant (choseong) variation based upon
      the vowel (jungseong) if both are specified.
   */
   if (cho_num < 0) {
      cho_index = cho_group = 0;  /* Use blank glyph for choseong. */
   }
   else {
      if (jung_num < 0 && jong_num < 0) {  /* Choseong is by itself. */
         cho_group = 0;
         if (cho_index < (NCHO_MODERN + NCHO_ANCIENT))
            cho_index = cho_num + JAMO_HEX;
         else  /* Choseong is in Hangul Jamo Extended-A range. */
            cho_index = cho_num - (NCHO_MODERN + NCHO_ANCIENT)
                                + JAMO_EXTA_HEX;
      }
      else {
         if (jung_num >= 0) {  /* Valid jungseong with choseong. */
            cho_group = cho_variation (cho_num, jung_num, jong_num);
         }
         else {  /* Invalid vowel; see if final consonant is valid. */
            /*
               If initial consonant and final consonant are specified,
               set cho_group to 4, which is the group tha would apply
               to a horizontal-only vowel such as Hangul "O", so the
               consonant appears full-width.
            */
            cho_group = 0;
            if (jong_num >= 0) {
               cho_group = 4;
            }
         }
         cho_index = CHO_HEX + CHO_VARIATIONS * cho_num +
                     cho_group;
      }  /* Choseong combined with jungseong and/or jongseong. */
   }  /* Valid choseong. */

   /*
      Choose vowel (jungseong) variation based upon the choseong
      and jungseong.
   */
   jung_index = jung_group = 0;  /* Use blank glyph for jungseong. */

   if (jung_num >= 0) {
      if (cho_num < 0 && jong_num < 0) {  /* Jungseong is by itself. */
         jung_group = 0;
         jung_index = jung_num + JUNG_UNICODE_START;
      }
      else {
         if (jong_num >= 0) {  /* If there is a final consonant. */
            if (jong_num == 3) /* Nieun; choose variation 3. */
               jung_group = 2;
            else
               jung_group = 1;
         }  /* Valid jongseong. */
         /* If valid choseong but no jongseong, choose jungseong variation 0. */
         else if (cho_num >= 0)
            jung_group = 0;
      }
      jung_index = JUNG_HEX + JUNG_VARIATIONS * jung_num + jung_group;
   }

   /*
      Choose final consonant (jongseong) based upon whether choseong
      and/or jungseong are present.
   */
   if (jong_num < 0) {
      jong_index = jong_group = 0;  /* Use blank glyph for jongseong. */
   }
   else {  /* Valid jongseong. */
      if (cho_num < 0 && jung_num < 0) {  /* Jongseong is by itself. */
         jong_group = 0;
         jong_index = jung_num + 0x4A8;
      }
      else {  /* There is only one jongseong variation if combined. */
         jong_group = 0;
         jong_index = JONG_HEX + JONG_VARIATIONS * jong_num +
                      jong_group;
      }
   }

   /*
      Now that we know the index locations for choseong, jungseong, and
      jongseong glyphs, combine them into one glyph.
   */
   combine_glyphs (glyph_table [cho_index], glyph_table [jung_index],
                   combined_glyph);

   if (jong_index > 0) {
      /*
         If the vowel has a vertical stroke that is one column
         away from the right border, shift this jongseung right
         by one column to line up with the rightmost vertical
         stroke in the vowel.
      */
      if (is_wide_vowel (jung_num)) {
         for (i = 0; i < 16; i++) {
            tmp_glyph [i] = glyph_table [jong_index] [i] >> 1;
         }
         combine_glyphs (combined_glyph, tmp_glyph,
                         combined_glyph);
      }
      else {
         combine_glyphs (combined_glyph, glyph_table [jong_index],
                         combined_glyph);
      }
   }

   return;
}

