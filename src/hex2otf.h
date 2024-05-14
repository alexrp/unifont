/**
    @file hex2otf.h

    @brief hex2otf.h - Header file for hex2otf.c

    @copyright Copyright © 2022 何志翔 (He Zhixiang)

    @author 何志翔 (He Zhixiang)
*/

/*
    LICENSE:

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
    02110-1301, USA.

    NOTE: It is a violation of the license terms of this software
    to delete license and copyright information below if creating
    a font derived from Unifont glyphs.
*/
#ifndef _HEX2OTF_H_
#define _HEX2OTF_H_

#define UNIFONT_VERSION "15.1.05"	///< Current Unifont version.

/**
    Define default strings for some TrueType font NameID strings.

        NameID   Description
        ------   -----------
           0     Copyright Notice
           1     Font Family
           2     Font Subfamily
           5     Version of the Name Table
          11     URL of the Font Vendor
          13     License Description
          14     License Information URL

    Default NameID 0 string (Copyright Notice)
*/
#define DEFAULT_ID0 "Copyright © 1998-2022 Roman Czyborra, Paul Hardy, \
Qianqian Fang, Andrew Miller, Johnnie Weaver, David Corbett, \
Nils Moskopp, Rebecca Bettencourt, et al."

#define DEFAULT_ID1 "Unifont"	///< Default NameID 1 string (Font Family)
#define DEFAULT_ID2 "Regular"	///< Default NameID 2 string (Font Subfamily)

/// Default NameID 5 string (Version of the Name Table)
#define DEFAULT_ID5 "Version "UNIFONT_VERSION

/// Default NameID 11 string (Font Vendor URL)
#define DEFAULT_ID11 "https://unifoundry.com/unifont/"

/// Default NameID 13 string (License Description)
#define DEFAULT_ID13 "Dual license: SIL Open Font License version 1.1, \
and GNU GPL version 2 or later with the GNU Font Embedding Exception."

/// Default NameID 14 string (License Information URLs)
#define DEFAULT_ID14 "http://unifoundry.com/LICENSE.txt, \
https://scripts.sil.org/OFL"

/**
    @brief Data structure for a font ID number and name character string.
*/
typedef struct NamePair
{
    int id;
    const char *str;
} NamePair;

/// Macro to initialize name identifier codes to default values defined above.
#define NAMEPAIR(n) {(n), DEFAULT_ID##n}

/**
    @brief Allocate array of NameID codes with default values.

    This array contains the default values for several TrueType NameID
    strings, as defined above in this file.  Strings are assigned using
    the NAMEPAIR macro defined above.
*/
const NamePair defaultNames[] =
{
    NAMEPAIR (0),  // Copyright notice; required (used in CFF)
    NAMEPAIR (1),  // Font family; required (used in CFF)
    NAMEPAIR (2),  // Font subfamily
    NAMEPAIR (5),  // Version of the name table
    NAMEPAIR (11), // URL of font vendor
    NAMEPAIR (13), // License description
    NAMEPAIR (14), // License information URL
    {0, NULL}      // Sentinel
};

#undef NAMEPAIR

#endif
