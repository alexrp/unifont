/**
   @file hangul.h

   @brief Define constants and function prototypes for using Hangul glyphs.

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

#ifndef _HANGUL_H_
#define _HANGUL_H_

#include <stdlib.h>


#define MAXLINE		256	///< Length of maximum file input line.

#define EXTENDED_HANGUL	/* Use rare Hangul code points beyond U+1100 */

/* Definitions to move Hangul .hex file contents into the Private Use Area. */
#define PUA_START	0xE000
#define PUA_END		0xE8FF
#define MAX_GLYPHS	(PUA_END - PUA_START + 1) /* Maximum .hex file glyphs */

/*
   Unicode ranges for Hangul choseong, jungseong, and jongseong.

   U+1100..U+11FF is the main range of modern and ancient Hangul jamo.
   U+A960..U+A97C is the range for extended Hangul choseong.
   U+D7B0..U+D7C6 is the range for extended Hangul jungseong.
   U+D7CB..U+D7FB is the range for extended Hangul jongseong.
*/
#define CHO_UNICODE_START	0x1100	///< Modern Hangul choseong start
#define CHO_UNICODE_END		0x115E  ///< Hangul Jamo choseong end
#define CHO_EXTA_UNICODE_START	0xA960	///< Hangul Extended-A choseong start
#define CHO_EXTA_UNICODE_END	0xA97C	///< Hangul Extended-A choseong end

#define JUNG_UNICODE_START	0x1161	///< Modern Hangul jungseong start
#define JUNG_UNICODE_END	0x11A7	///< Modern Hangul jungseong end
#define JUNG_EXTB_UNICODE_START	0xD7B0	///< Hangul Extended-B jungseong start
#define JUNG_EXTB_UNICODE_END	0xD7C6	///< Hangul Extended-B jungseong end

#define JONG_UNICODE_START	0x11A8	///< Modern Hangul jongseong start
#define JONG_UNICODE_END	0x11FF	///< Modern Hangul jongseong end
#define JONG_EXTB_UNICODE_START	0xD7CB	///< Hangul Extended-B jongseong start
#define JONG_EXTB_UNICODE_END	0xD7FB	///< Hangul Extended-B jongseong end


/*
   Number of modern and ancient letters in hangul-base.hex file.
*/
#define NCHO_MODERN	 19	///< 19 modern Hangul Jamo choseong
#define NCHO_ANCIENT	 76	///< ancient Hangul Jamo choseong
#define NCHO_EXTA	 29	///< Hangul Extended-A choseong
#define NCHO_EXTA_RSRVD   3	///< Reserved at end of Extended-A choseong

#define NJUNG_MODERN	 21	///< 21 modern Hangul Jamo jungseong
#define NJUNG_ANCIENT	 50	///< ancient Hangul Jamo jungseong
#define NJUNG_EXTB	 23	///< Hangul Extended-B jungseong
#define NJUNG_EXTB_RSRVD  4	///< Reserved at end of Extended-B junseong

#define NJONG_MODERN	 27	///< 28 modern Hangul Jamo jongseong
#define NJONG_ANCIENT	 61	///< ancient Hangul Jamo jongseong
#define NJONG_EXTB	 49	///< Hangul Extended-B jongseong
#define NJONG_EXTB_RSRVD  4	///< Reserved at end of Extended-B jonseong


/*
   Number of variations of each component in a Johab 6/3/1 arrangement.
*/
#define CHO_VARIATIONS	6	///< 6 choseong variations
#define JUNG_VARIATIONS	3	///< 3 jungseong variations
#define JONG_VARIATIONS	1	///< 1 jongseong variation

/*
   Starting positions in the hangul-base.hex file for each component.
*/
/// Location of first choseong (location 0x0000 is a blank glyph)
#define CHO_HEX		 0x0001

/// Location of first ancient choseong
#define CHO_ANCIENT_HEX	(CHO_HEX         + CHO_VARIATIONS * NCHO_MODERN)

/// U+A960 Extended-A choseong
#define CHO_EXTA_HEX	(CHO_ANCIENT_HEX + CHO_VARIATIONS * NCHO_ANCIENT)

/// U+A97F Extended-A last location in .hex file, including reserved Unicode code points at end
#define CHO_LAST_HEX	(CHO_EXTA_HEX    + CHO_VARIATIONS * (NCHO_EXTA + NCHO_EXTA_RSRVD) - 1)

/// Location of first jungseong (will be 0x2FB)
#define JUNG_HEX	(CHO_LAST_HEX + 1)

/// Location of first ancient jungseong
#define JUNG_ANCIENT_HEX (JUNG_HEX         + JUNG_VARIATIONS * NJUNG_MODERN)

/// U+D7B0 Extended-B jungseong
#define JUNG_EXTB_HEX	 (JUNG_ANCIENT_HEX + JUNG_VARIATIONS * NJUNG_ANCIENT)

/// U+D7CA Extended-B last location in .hex file, including reserved Unicode code points at end
#define JUNG_LAST_HEX	(JUNG_EXTB_HEX     + JUNG_VARIATIONS * (NJUNG_EXTB + NJUNG_EXTB_RSRVD) - 1)

/// Location of first jongseong (will be 0x421)
#define JONG_HEX	 (JUNG_LAST_HEX + 1)

/// Location of first ancient jongseong
#define JONG_ANCIENT_HEX (JONG_HEX         + JONG_VARIATIONS * NJONG_MODERN)

/// U+D7CB Extended-B jongseong
#define JONG_EXTB_HEX	 (JONG_ANCIENT_HEX + JONG_VARIATIONS * NJONG_ANCIENT)

/// U+D7FF Extended-B last location in .hex file, including reserved Unicode code points at end
#define JONG_LAST_HEX	(JONG_EXTB_HEX     + JONG_VARIATIONS * (NJONG_EXTB + NJONG_EXTB_RSRVD) - 1)

/* Common modern and ancient Hangul Jamo range */
#define JAMO_HEX	0x0500	///< Start of U+1100..U+11FF glyphs
#define JAMO_END	0x05FF	///< End   of U+1100..U+11FF glyphs

/* Hangul Jamo Extended-A range */
#define JAMO_EXTA_HEX	0x0600	///< Start of U+A960..U+A97F glyphs
#define JAMO_EXTA_END	0x061F	///< End   of U+A960..U+A97F glyphs

/* Hangul Jamo Extended-B range */
#define JAMO_EXTB_HEX	0x0620	///< Start of U+D7B0..U+D7FF glyphs
#define JAMO_EXTB_END	0x066F	///< End   of U+D7B0..U+D7FF glyphs

/*
   These values allow enumeration of all modern and ancient letters.

   If RARE_HANGUL is defined, include Hangul code points above U+11FF.
*/
#ifdef EXTENDED_HANGUL

#define TOTAL_CHO	(NCHO_MODERN  + NCHO_ANCIENT  + NCHO_EXTA )
#define TOTAL_JUNG  	(NJUNG_MODERN + NJUNG_ANCIENT + NJUNG_EXTB)
#define TOTAL_JONG	(NJONG_MODERN + NJONG_ANCIENT + NJONG_EXTB)

#else

#define TOTAL_CHO	(NCHO_MODERN  + NCHO_ANCIENT )
#define TOTAL_JUNG  	(NJUNG_MODERN + NJUNG_ANCIENT)
#define TOTAL_JONG	(NJONG_MODERN + NJONG_ANCIENT)

#endif


/*
   Function Prototypes.
*/

unsigned hangul_read_base8  (FILE *infp,  unsigned char base[][32]);
unsigned hangul_read_base16 (FILE *infp, unsigned base[][16]);

void     hangul_decompose (unsigned codept,
                           int *initial, int *medial, int *final);
unsigned hangul_compose   (int initial, int medial, int final);

void hangul_hex_indices (int choseong, int jungseong, int jongseong,
                         int *cho_index, int *jung_index, int *jong_index);
void hangul_variations (int choseong, int jungseong, int jongseong,
                        int *cho_var, int *jung_var, int *jong_var);
int is_wide_vowel  (int vowel);
int  cho_variation (int choseong, int jungseong, int jongseong);
int jung_variation (int choseong, int jungseong, int jongseong);
int jong_variation (int choseong, int jungseong, int jongseong);

void hangul_syllable (int choseong, int jungseong, int jongseong,
                      unsigned char hangul_base[][32], unsigned char *syllable);
int glyph_overlap (unsigned *glyph1, unsigned *glyph2);
void combine_glyphs (unsigned *glyph1, unsigned *glyph2,
                     unsigned *combined_glyph);
void one_jamo (unsigned glyph_table [MAX_GLYPHS][16],
               unsigned jamo, unsigned *jamo_glyph);
void combined_jamo (unsigned glyph_table [MAX_GLYPHS][16],
                    unsigned cho, unsigned jung, unsigned jong,
                    unsigned *combined_glyph);
void print_glyph_txt (FILE *fp, unsigned codept, unsigned *this_glyph);
void print_glyph_hex (FILE *fp, unsigned codept, unsigned *this_glyph);


#endif
