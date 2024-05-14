/**
    @file hex2otf.c

    @brief hex2otf - Convert GNU Unifont .hex file to OpenType font

    This program reads a Unifont .hex format file and a file containing
    combining mark offset information, and produces an OpenType font file.

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
    to delete or override license and copyright information contained
    in the hex2otf.h file if creating a font derived from Unifont glyphs.
    Fonts derived from Unifont can add names to the copyright notice
    for creators of new or modified glyphs.
*/

#include <assert.h>
#include <ctype.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hex2otf.h"

#define VERSION "1.0.1"  ///< Program version, for "--version" option.

// This program assumes the execution character set is compatible with ASCII.

#define U16MAX 0xffff		///< Maximum UTF-16 code point value.
#define U32MAX 0xffffffff	///< Maximum UTF-32 code point value.

#define PRI_CP "U+%.4"PRIXFAST32 ///< Format string to print Unicode code point.

#ifndef static_assert
#define static_assert(a, b) (assert(a)) ///< If "a" is true, return string "b".
#endif

// Set or clear a particular bit.
#define BX(shift, x) ((uintmax_t)(!!(x)) << (shift)) ///< Truncate & shift word.
#define B0(shift) BX((shift), 0)	///< Clear a given bit in a word.
#define B1(shift) BX((shift), 1)	///< Set   a given bit in a word.

#define GLYPH_MAX_WIDTH 16	///< Maximum glyph width, in pixels.
#define GLYPH_HEIGHT 16		///< Maximum glyph height, in pixels.

/// Number of bytes to represent one bitmap glyph as a binary array.
#define GLYPH_MAX_BYTE_COUNT (GLYPH_HEIGHT * GLYPH_MAX_WIDTH / 8)

/// Count of pixels below baseline.
#define DESCENDER 2

/// Count of pixels above baseline.
#define ASCENDER (GLYPH_HEIGHT - DESCENDER)

/// Font units per em.
#define FUPEM 64

/// An OpenType font has at most 65536 glyphs.
#define MAX_GLYPHS 65536

/// Name IDs 0-255 are used for standard names.
#define MAX_NAME_IDS 256

/// Convert pixels to font units.
#define FU(x) ((x) * FUPEM / GLYPH_HEIGHT)

/// Convert glyph byte count to pixel width.
#define PW(x) ((x) / (GLYPH_HEIGHT / 8))

/// Definition of "byte" type as an unsigned char.
typedef unsigned char byte;

/// This type must be able to represent max(GLYPH_MAX_WIDTH, GLYPH_HEIGHT).
typedef int_least8_t pixels_t;

/**
    @brief Print an error message on stderr, then exit.

    This function prints the provided error string and optional
    following arguments to stderr, and then exits with a status
    of EXIT_FAILURE.

    @param[in] reason The output string to describe the error.
    @param[in] ... Optional following arguments to output.
*/
void
fail (const char *reason, ...)
{
    fputs ("ERROR: ", stderr);
    va_list args;
    va_start (args, reason);
    vfprintf (stderr, reason, args);
    va_end (args);
    putc ('\n', stderr);
    exit (EXIT_FAILURE);
}

/**
    @brief Generic data structure for a linked list of buffer elements.

    A buffer can act as a vector (when filled with 'store*' functions),
    or a temporary output area (when filled with 'cache*' functions).
    The 'store*' functions use native endian.
    The 'cache*' functions use big endian or other formats in OpenType.
    Beware of memory alignment.
*/
typedef struct Buffer
{
    size_t capacity; // = 0 iff this buffer is free
    byte *begin, *next, *end;
} Buffer;

Buffer *allBuffers; ///< Initial allocation of empty array of buffer pointers.
size_t bufferCount;	///< Number of buffers in a Buffer * array.
size_t nextBufferIndex; ///< Index number to tail element of Buffer * array.

/**
    @brief Initialize an array of buffer pointers to all zeroes.

    This function initializes the "allBuffers" array of buffer
    pointers to all zeroes.

    @param[in] count The number of buffer array pointers to allocate.
*/
void
initBuffers (size_t count)
{
    assert (count > 0);
    assert (bufferCount == 0); // uninitialized
    allBuffers = calloc (count, sizeof *allBuffers);
    if (!allBuffers)
        fail ("Failed to initialize buffers.");
    bufferCount = count;
    nextBufferIndex = 0;
}

/**
    @brief Free all allocated buffer pointers.

    This function frees all buffer pointers previously allocated
    in the initBuffers function.
*/
void
cleanBuffers ()
{
    for (size_t i = 0; i < bufferCount; i++)
        if (allBuffers[i].capacity)
            free (allBuffers[i].begin);
    free (allBuffers);
    bufferCount = 0;
}

/**
    @brief Create a new buffer.

    This function creates a new buffer array of type Buffer,
    with an initial size of initialCapacity elements.

    @param[in] initialCapacity The initial number of elements in the buffer.
*/
Buffer *
newBuffer (size_t initialCapacity)
{
    assert (initialCapacity > 0);
    Buffer *buf = NULL;
    size_t sentinel = nextBufferIndex;
    do
    {
        if (nextBufferIndex == bufferCount)
            nextBufferIndex = 0;
        if (allBuffers[nextBufferIndex].capacity == 0)
        {
            buf = &allBuffers[nextBufferIndex++];
            break;
        }
    } while (++nextBufferIndex != sentinel);
    if (!buf) // no existing buffer available
    {
        size_t newSize = sizeof (Buffer) * bufferCount * 2;
        void *extended = realloc (allBuffers, newSize);
        if (!extended)
            fail ("Failed to create new buffers.");
        allBuffers = extended;
        memset (allBuffers + bufferCount, 0, sizeof (Buffer) * bufferCount);
        buf = &allBuffers[bufferCount];
        nextBufferIndex = bufferCount + 1;
        bufferCount *= 2;
    }
    buf->begin = malloc (initialCapacity);
    if (!buf->begin)
        fail ("Failed to allocate %zu bytes of memory.", initialCapacity);
    buf->capacity = initialCapacity;
    buf->next = buf->begin;
    buf->end = buf->begin + initialCapacity;
    return buf;
}

/**
    @brief Ensure that the buffer has at least the specified minimum size.

    This function takes a buffer array of type Buffer and the
    necessary minimum number of elements as inputs, and attempts
    to increase the size of the buffer if it must be larger.

    If the buffer is too small and cannot be resized, the program
    will terminate with an error message and an exit status of
    EXIT_FAILURE.

    @param[in,out] buf The buffer to check.
    @param[in] needed The required minimum number of elements in the buffer.
*/
void
ensureBuffer (Buffer *buf, size_t needed)
{
    if (buf->end - buf->next >= needed)
        return;
    ptrdiff_t occupied = buf->next - buf->begin;
    size_t required = occupied + needed;
    if (required < needed) // overflow
        fail ("Cannot allocate %zu + %zu bytes of memory.", occupied, needed);
    if (required > SIZE_MAX / 2)
        buf->capacity = required;
    else while (buf->capacity < required)
        buf->capacity *= 2;
    void *extended = realloc (buf->begin, buf->capacity);
    if (!extended)
        fail ("Failed to allocate %zu bytes of memory.", buf->capacity);
    buf->begin = extended;
    buf->next = buf->begin + occupied;
    buf->end = buf->begin + buf->capacity;
}

/**
    @brief Count the number of elements in a buffer.

    @param[in] buf The buffer to be examined.
    @return The number of elements in the buffer.
*/
static inline size_t
countBufferedBytes (const Buffer *buf)
{
    return buf->next - buf->begin;
}

/**
    @brief Get the start of the buffer array.

    @param[in] buf The buffer to be examined.
    @return A pointer of type Buffer * to the start of the buffer.
*/
static inline void *
getBufferHead (const Buffer *buf)
{
    return buf->begin;
}

/**
    @brief Get the end of the buffer array.

    @param[in] buf The buffer to be examined.
    @return A pointer of type Buffer * to the end of the buffer.
*/
static inline void *
getBufferTail (const Buffer *buf)
{
    return buf->next;
}

/**
    @brief Add a slot to the end of a buffer.

    This function ensures that the buffer can grow by one slot,
    and then returns a pointer to the new slot within the buffer.

    @param[in] buf The pointer to an array of type Buffer *.
    @param[in] slotSize The new slot number.
    @return A pointer to the new slot within the buffer.
*/
static inline void *
getBufferSlot (Buffer *buf, size_t slotSize)
{
    ensureBuffer (buf, slotSize);
    void *slot = buf->next;
    buf->next += slotSize;
    return slot;
}

/**
    @brief Reset a buffer pointer to the buffer's beginning.

    This function resets an array of type Buffer * to point
    its tail to the start of the array.

    @param[in] buf The pointer to an array of type Buffer *.
*/
static inline void
resetBuffer (Buffer *buf)
{
    buf->next = buf->begin;
}

/**
    @brief Free the memory previously allocated for a buffer.

    This function frees the memory allocated to an array
    of type Buffer *.

    @param[in] buf The pointer to an array of type Buffer *.
*/
void
freeBuffer (Buffer *buf)
{
    free (buf->begin);
    buf->capacity = 0;
}

/**
    @brief Temporary define to look up an element in an array of given type.

    This defintion is used to create lookup functions to return
    a given element in unsigned arrays of size 8, 16, and 32 bytes,
    and in an array of pixels.
*/
#define defineStore(name, type) \
void name (Buffer *buf, type value) \
{ \
    type *slot = getBufferSlot (buf, sizeof value); \
    *slot = value; \
}
defineStore (storeU8, uint_least8_t)
defineStore (storeU16, uint_least16_t)
defineStore (storeU32, uint_least32_t)
defineStore (storePixels, pixels_t)
#undef defineStore

/**
    @brief Cache bytes in a big-endian format.

    This function adds from 1, 2, 3, or 4 bytes to the end of
    a byte array in big-endian order.  The buffer is updated
    to account for the newly-added bytes.

    @param[in,out] buf The array of bytes to which to append new bytes.
    @param[in] value The bytes to add, passed as a 32-bit unsigned word.
    @param[in] bytes The number of bytes to append to the buffer.
*/
void
cacheU (Buffer *buf, uint_fast32_t value, int bytes)
{
    assert (1 <= bytes && bytes <= 4);
    ensureBuffer (buf, bytes);
    switch (bytes)
    {
        case 4: *buf->next++ = value >> 24 & 0xff; // fall through
        case 3: *buf->next++ = value >> 16 & 0xff; // fall through
        case 2: *buf->next++ = value >> 8  & 0xff; // fall through
        case 1: *buf->next++ = value       & 0xff;
    }
}

/**
    @brief Append one unsigned byte to the end of a byte array.

    This function adds one byte to the end of a byte array.
    The buffer is updated to account for the newly-added byte.

    @param[in,out] buf The array of bytes to which to append a new byte.
    @param[in] value The 8-bit unsigned value to append to the buf array.
*/
void
cacheU8 (Buffer *buf, uint_fast8_t value)
{
    storeU8 (buf, value & 0xff);
}

/**
    @brief Append two unsigned bytes to the end of a byte array.

    This function adds two bytes to the end of a byte array.
    The buffer is updated to account for the newly-added bytes.

    @param[in,out] buf The array of bytes to which to append two new bytes.
    @param[in] value The 16-bit unsigned value to append to the buf array.
*/
void
cacheU16 (Buffer *buf, uint_fast16_t value)
{
    cacheU (buf, value, 2);
}

/**
    @brief Append four unsigned bytes to the end of a byte array.

    This function adds four bytes to the end of a byte array.
    The buffer is updated to account for the newly-added bytes.

    @param[in,out] buf The array of bytes to which to append four new bytes.
    @param[in] value The 32-bit unsigned value to append to the buf array.
*/
void
cacheU32 (Buffer *buf, uint_fast32_t value)
{
    cacheU (buf, value, 4);
}

/**
    @brief Cache charstring number encoding in a CFF buffer.

    This function caches two's complement 8-, 16-, and 32-bit
    words as per Adobe's Type 2 Charstring encoding for operands.
    These operands are used in Compact Font Format data structures.

    Byte values can have offsets, for which this function
    compensates, optionally followed by additional bytes:

        Byte Range   Offset   Bytes   Adjusted Range
        ----------   ------   -----   --------------
          0 to 11        0      1     0 to 11 (operators)
            12           0      2     Next byte is 8-bit op code
         13 to 18        0      1     13 to 18 (operators)
         19 to 20        0      2+    hintmask and cntrmask operators
         21 to 27        0      1     21 to 27 (operators)
            28           0      3     16-bit 2's complement number
         29 to 31        0      1     29 to 31 (operators)
         32 to 246    -139      1     -107 to +107
        247 to 250    +108      2     +108 to +1131
        251 to 254    -108      2     -108 to -1131
           255           0      5     16-bit integer and 16-bit fraction

    @param[in,out] buf The buffer to which the operand value is appended.
    @param[in] value The operand value.
*/
void
cacheCFFOperand (Buffer *buf, int_fast32_t value)
{
    if (-107 <= value && value <= 107)
        cacheU8 (buf, value + 139);
    else if (108 <= value && value <= 1131)
    {
        cacheU8 (buf, (value - 108) / 256 + 247);
        cacheU8 (buf, (value - 108) % 256);
    }
    else if (-32768 <= value && value <= 32767)
    {
        cacheU8 (buf, 28);
        cacheU16 (buf, value);
    }
    else if (-2147483647 <= value && value <= 2147483647)
    {
        cacheU8 (buf, 29);
        cacheU32 (buf, value);
    }
    else
        assert (false); // other encodings are not used and omitted
    static_assert (GLYPH_MAX_WIDTH <= 107, "More encodings are needed.");
}

/**
    @brief Append 1 to 4 bytes of zeroes to a buffer, for padding.

    @param[in,out] buf The buffer to which the operand value is appended.
    @param[in] count The number of bytes containing zeroes to append.
*/
void
cacheZeros (Buffer *buf, size_t count)
{
    ensureBuffer (buf, count);
    memset (buf->next, 0, count);
    buf->next += count;
}

/**
    @brief Append a string of bytes to a buffer.

    This function appends an array of 1 to 4 bytes to the end of 
    a buffer.

    @param[in,out] buf The buffer to which the bytes are appended.
    @param[in] src The array of bytes to append to the buffer.
    @param[in] count The number of bytes containing zeroes to append.
*/
void
cacheBytes (Buffer *restrict buf, const void *restrict src, size_t count)
{
    ensureBuffer (buf, count);
    memcpy (buf->next, src, count);
    buf->next += count;
}

/**
    @brief Append bytes of a table to a byte buffer.

    @param[in,out] bufDest The buffer to which the new bytes are appended.
    @param[in] bufSrc The bytes to append to the buffer array.
*/
void
cacheBuffer (Buffer *restrict bufDest, const Buffer *restrict bufSrc)
{
    size_t length = countBufferedBytes (bufSrc);
    ensureBuffer (bufDest, length);
    memcpy (bufDest->next, bufSrc->begin, length);
    bufDest->next += length;
}

/**
    @brief Write an array of bytes to an output file.

    @param[in] bytes An array of unsigned bytes to write.
    @param[in] file The file pointer for writing, of type FILE *.
*/
void
writeBytes (const byte bytes[], size_t count, FILE *file)
{
    if (fwrite (bytes, count, 1, file) != 1 && count != 0)
        fail ("Failed to write %zu bytes to output file.", count);
}

/**
    @brief Write an unsigned 16-bit value to an output file.

    This function writes a 16-bit unsigned value in big-endian order
    to an output file specified with a file pointer.

    @param[in] value The 16-bit value to write.
    @param[in] file The file pointer for writing, of type FILE *.
*/
void
writeU16 (uint_fast16_t value, FILE *file)
{
    byte bytes[] =
    {
        (value >> 8) & 0xff,
        (value     ) & 0xff,
    };
    writeBytes (bytes, sizeof bytes, file);
}

/**
    @brief Write an unsigned 32-bit value to an output file.

    This function writes a 32-bit unsigned value in big-endian order
    to an output file specified with a file pointer.

    @param[in] value The 32-bit value to write.
    @param[in] file The file pointer for writing, of type FILE *.
*/
void
writeU32 (uint_fast32_t value, FILE *file)
{
    byte bytes[] =
    {
        (value >> 24) & 0xff,
        (value >> 16) & 0xff,
        (value >>  8) & 0xff,
        (value      ) & 0xff,
    };
    writeBytes (bytes, sizeof bytes, file);
}

/**
    @brief Write an entire buffer array of bytes to an output file.

    This function determines the size of a buffer of bytes and
    writes that number of bytes to an output file specified with
    a file pointer.  The number of bytes is determined from the
    length information stored as part of the Buffer * data structure.

    @param[in] buf An array containing unsigned bytes to write.
    @param[in] file The file pointer for writing, of type FILE *.
*/
static inline void
writeBuffer (const Buffer *buf, FILE *file)
{
    writeBytes (getBufferHead (buf), countBufferedBytes (buf), file);
}

/// Array of OpenType names indexed directly by Name IDs.
typedef const char *NameStrings[MAX_NAME_IDS];

/**
   @brief Data structure to hold data for one bitmap glyph.

   This data structure holds data to represent one Unifont bitmap
   glyph: Unicode code point, number of bytes in its bitmap array,
   whether or not it is a combining character, and an offset from
   the glyph origin to the start of the bitmap.
*/
typedef struct Glyph
{
    uint_least32_t codePoint; ///< undefined for glyph 0
    byte bitmap[GLYPH_MAX_BYTE_COUNT]; ///< hexadecimal bitmap character array
    uint_least8_t byteCount; ///< length of bitmap data
    bool combining; ///< whether this is a combining glyph
    pixels_t pos; ///< number of pixels the glyph should be moved to the right
                  ///< (negative number means moving to the left)
    pixels_t lsb; ///< left side bearing (x position of leftmost contour point)
} Glyph;

/**
   @brief Data structure to hold information for one font.
*/
typedef struct Font
{
    Buffer *tables;
    Buffer *glyphs;
    uint_fast32_t glyphCount;
    pixels_t maxWidth;
} Font;

/**
   @brief Data structure for an OpenType table.

   This data structure contains a table tag and a pointer to the
   start of the buffer that holds data for this OpenType table.

   For information on the OpenType tables and their structure, see
   https://docs.microsoft.com/en-us/typography/opentype/spec/otff#font-tables.
*/
typedef struct Table
{
    uint_fast32_t tag;
    Buffer *content;
} Table;

/**
    @brief Index to Location ("loca") offset information.

    This enumerated type encodes the type of offset to locations
    in a table.  It denotes Offset16 (16-bit) and Offset32 (32-bit)
    offset types.
*/
enum LocaFormat {
    LOCA_OFFSET16 = 0,    ///< Offset to location is a 16-bit Offset16 value
    LOCA_OFFSET32 = 1     ///< Offset to location is a 32-bit Offset32 value
};

/**
    @brief Convert a 4-byte array to the machine's native 32-bit endian order.

    This function takes an array of 4 bytes in big-endian order and
    converts it to a 32-bit word in the endian order of the native machine.

    @param[in] tag The array of 4 bytes in big-endian order.
    @return The 32-bit unsigned word in a machine's native endian order.
*/
static inline uint_fast32_t tagAsU32 (const char tag[static 4])
{
    uint_fast32_t r = 0;
    r |= (tag[0] & 0xff) << 24;
    r |= (tag[1] & 0xff) << 16;
    r |= (tag[2] & 0xff) << 8;
    r |= (tag[3] & 0xff);
    return r;
}

/**
    @brief Add a TrueType or OpenType table to the font.

    This function adds a TrueType or OpenType table to a font.
    The 4-byte table tag is passed as an unsigned 32-bit integer
    in big-endian format.

    @param[in,out] font The font to which a font table will be added.
    @param[in] tag The 4-byte table name.
    @param[in] content The table bytes to add, of type Buffer *.
*/
void
addTable (Font *font, const char tag[static 4], Buffer *content)
{
    Table *table = getBufferSlot (font->tables, sizeof (Table));
    table->tag = tagAsU32 (tag);
    table->content = content;
}

/**
    @brief  Sort tables according to OpenType recommendations.

    The various tables in a font are sorted in an order recommended
    for TrueType font files.

    @param[in,out] font The font in which to sort tables.
    @param[in] isCFF True iff Compact Font Format (CFF) is being used.
*/
void
organizeTables (Font *font, bool isCFF)
{
    const char *const cffOrder[] = {"head","hhea","maxp","OS/2","name",
        "cmap","post","CFF ",NULL};
    const char *const truetypeOrder[] = {"head","hhea","maxp","OS/2",
        "hmtx","LTSH","VDMX","hdmx","cmap","fpgm","prep","cvt ","loca",
        "glyf","kern","name","post","gasp","PCLT","DSIG",NULL};
    const char *const *const order = isCFF ? cffOrder : truetypeOrder;
    Table *unordered = getBufferHead (font->tables);
    const Table *const tablesEnd = getBufferTail (font->tables);
    for (const char *const *p = order; *p; p++)
    {
        uint_fast32_t tag = tagAsU32 (*p);
        for (Table *t = unordered; t < tablesEnd; t++)
        {
            if (t->tag != tag)
                continue;
            if (t != unordered)
            {
                Table temp = *unordered;
                *unordered = *t;
                *t = temp;
            }
            unordered++;
            break;
        }
    }
}

/**
   @brief Data structure for data associated with one OpenType table.

   This data structure contains an OpenType table's tag, start within
   an OpenType font file, length in bytes, and checksum at the end of
   the table.
*/
struct TableRecord
{
    uint_least32_t tag, offset, length, checksum;
};

/**
    @brief Compare tables by 4-byte unsigned table tag value.

    This function takes two pointers to a TableRecord data structure
    and extracts the four-byte tag structure element for each.  The
    two 32-bit numbers are then compared.  If the first tag is greater
    than the first, then gt = 1 and lt = 0, and so 1 - 0 = 1 is
    returned.  If the first is less than the second, then gt = 0 and
    lt = 1, and so 0 - 1 = -1 is returned.

    @param[in] a Pointer to the first TableRecord structure.
    @param[in] b Pointer to the second TableRecord structure.
    @return 1 if the tag in "a" is greater, -1 if less, 0 if equal.
*/
int
byTableTag (const void *a, const void *b)
{
    const struct TableRecord *const ra = a, *const rb = b;
    int gt = ra->tag > rb->tag;
    int lt = ra->tag < rb->tag;
    return gt - lt;
}

/**
   @brief Write OpenType font to output file.

   This function writes the constructed OpenType font to the
   output file named "filename".

   @param[in] font Pointer to the font, of type Font *.
   @param[in] isCFF Boolean indicating whether the font has CFF data.
   @param[in] filename The name of the font file to create.
*/
void
writeFont (Font *font, bool isCFF, const char *fileName)
{
    FILE *file = fopen (fileName, "wb");
    if (!file)
        fail ("Failed to open file '%s'.", fileName);
    const Table *const tables = getBufferHead (font->tables);
    const Table *const tablesEnd = getBufferTail (font->tables);
    size_t tableCount = tablesEnd - tables;
    assert (0 < tableCount && tableCount <= U16MAX);
    size_t offset = 12 + 16 * tableCount;
    uint_fast32_t totalChecksum = 0;
    Buffer *tableRecords =
        newBuffer (sizeof (struct TableRecord) * tableCount);
    for (size_t i = 0; i < tableCount; i++)
    {
        struct TableRecord *record =
            getBufferSlot (tableRecords, sizeof *record);
        record->tag = tables[i].tag;
        size_t length = countBufferedBytes (tables[i].content);
        #if SIZE_MAX > U32MAX
            if (offset > U32MAX)
                fail ("Table offset exceeded 4 GiB.");
            if (length > U32MAX)
                fail ("Table size exceeded 4 GiB.");
        #endif
        record->length = length;
        record->checksum = 0;
        const byte *p = getBufferHead (tables[i].content);
        const byte *const end = getBufferTail (tables[i].content);

        /// Add a byte shifted by 24, 16, 8, or 0 bits.
        #define addByte(shift) \
            if (p == end) \
                break; \
            record->checksum += (uint_fast32_t)*p++ << (shift);

        for (;;)
        {
            addByte (24)
            addByte (16)
            addByte (8)
            addByte (0)
        }
        #undef addByte
        cacheZeros (tables[i].content, (~length + 1U) & 3U);
        record->offset = offset;
        offset += countBufferedBytes (tables[i].content);
        totalChecksum += record->checksum;
    }
    struct TableRecord *records = getBufferHead (tableRecords);
    qsort (records, tableCount, sizeof *records, byTableTag);
    // Offset Table
    uint_fast32_t sfntVersion = isCFF ? 0x4f54544f : 0x00010000;
    writeU32 (sfntVersion, file); // sfntVersion
    totalChecksum += sfntVersion;
    uint_fast16_t entrySelector = 0;
    for (size_t k = tableCount; k != 1; k >>= 1)
        entrySelector++;
    uint_fast16_t searchRange = 1 << (entrySelector + 4);
    uint_fast16_t rangeShift = (tableCount - (1 << entrySelector)) << 4;
    writeU16 (tableCount, file); // numTables
    writeU16 (searchRange, file); // searchRange
    writeU16 (entrySelector, file); // entrySelector
    writeU16 (rangeShift, file); // rangeShift
    totalChecksum += (uint_fast32_t)tableCount << 16;
    totalChecksum += searchRange;
    totalChecksum += (uint_fast32_t)entrySelector << 16;
    totalChecksum += rangeShift;
    // Table Records (always sorted by table tags)
    for (size_t i = 0; i < tableCount; i++)
    {
        // Table Record
        writeU32 (records[i].tag, file); // tableTag
        writeU32 (records[i].checksum, file); // checkSum
        writeU32 (records[i].offset, file); // offset
        writeU32 (records[i].length, file); // length
        totalChecksum += records[i].tag;
        totalChecksum += records[i].checksum;
        totalChecksum += records[i].offset;
        totalChecksum += records[i].length;
    }
    freeBuffer (tableRecords);
    for (const Table *table = tables; table < tablesEnd; table++)
    {
        if (table->tag == 0x68656164) // 'head' table
        {
            byte *begin = getBufferHead (table->content);
            byte *end = getBufferTail (table->content);
            writeBytes (begin, 8, file);
            writeU32 (0xb1b0afbaU - totalChecksum, file); // checksumAdjustment
            writeBytes (begin + 12, end - (begin + 12), file);
            continue;
        }
        writeBuffer (table->content, file);
    }
    fclose (file);
}

/**
    @brief Convert a hexadecimal digit character to a 4-bit number.

    This function takes a character that contains one hexadecimal digit
    and returns the 4-bit value (as an unsigned 8-bit value) corresponding
    to the hexadecimal digit.

    @param[in] nibble The character containing one hexadecimal digit.
    @return The hexadecimal digit value, 0 through 15, inclusive.
*/
static inline byte
nibbleValue (char nibble)
{
    if (isdigit (nibble))
        return nibble - '0';
    nibble = toupper (nibble);
    return nibble - 'A' + 10;
}

/**
    @brief Read up to 6 hexadecimal digits and a colon from file.

    This function reads up to 6 hexadecimal digits followed by
    a colon from a file.

    If the end of the file is reached, the function returns true.
    The file name is provided to include in an error message if
    the end of file was reached unexpectedly.

    @param[out] codePoint The Unicode code point.
    @param[in] fileName The name of the input file.
    @param[in] file Pointer to the input file stream.
    @return true if at end of file, false otherwise.
*/
bool
readCodePoint (uint_fast32_t *codePoint, const char *fileName, FILE *file)
{
    *codePoint = 0;
    uint_fast8_t digitCount = 0;
    for (;;)
    {
        int c = getc (file);
        if (isxdigit (c) && ++digitCount <= 6)
        {
            *codePoint = (*codePoint << 4) | nibbleValue (c);
            continue;
        }
        if (c == ':' && digitCount > 0)
            return false;
        if (c == EOF)
        {
            if (digitCount == 0)
                return true;
            if (feof (file))
                fail ("%s: Unexpected end of file.", fileName);
            else
                fail ("%s: Read error.", fileName);
        }
        fail ("%s: Unexpected character: %#.2x.", fileName, (unsigned)c);
    }
}

/**
    @brief Read glyph definitions from a Unifont .hex format file.

    This function reads in the glyph bitmaps contained in a Unifont
    .hex format file.  These input files contain one glyph bitmap
    per line.  Each line is of the form

        <hexadecimal code point> ':' <hexadecimal bitmap sequence>

    The code point field typically consists of 4 hexadecimal digits
    for a code point in Unicode Plane 0, and 6 hexadecimal digits for
    code points above Plane 0.  The hexadecimal bitmap sequence is
    32 hexadecimal digits long for a glyph that is 8 pixels wide by
    16 pixels high, and 64 hexadecimal digits long for a glyph that
    is 16 pixels wide by 16 pixels high.

    @param[in,out] font The font data structure to update with new glyphs.
    @param[in] fileName The name of the Unifont .hex format input file.
*/
void
readGlyphs (Font *font, const char *fileName)
{
    FILE *file = fopen (fileName, "r");
    if (!file)
        fail ("Failed to open file '%s'.", fileName);
    uint_fast32_t glyphCount = 1; // for glyph 0
    uint_fast8_t maxByteCount = 0;
    { // Hard code the .notdef glyph.
        const byte bitmap[] = "\0\0\0~fZZzvv~vv~\0\0"; // same as U+FFFD
        const size_t byteCount = sizeof bitmap - 1;
        assert (byteCount <= GLYPH_MAX_BYTE_COUNT);
        assert (byteCount % GLYPH_HEIGHT == 0);
        Glyph *notdef = getBufferSlot (font->glyphs, sizeof (Glyph));
        memcpy (notdef->bitmap, bitmap, byteCount);
        notdef->byteCount = maxByteCount = byteCount;
        notdef->combining = false;
        notdef->pos = 0;
        notdef->lsb = 0;
    }
    for (;;)
    {
        uint_fast32_t codePoint;
        if (readCodePoint (&codePoint, fileName, file))
            break;
        if (++glyphCount > MAX_GLYPHS)
            fail ("OpenType does not support more than %lu glyphs.",
                MAX_GLYPHS);
        Glyph *glyph = getBufferSlot (font->glyphs, sizeof (Glyph));
        glyph->codePoint = codePoint;
        glyph->byteCount = 0;
        glyph->combining = false;
        glyph->pos = 0;
        glyph->lsb = 0;
        for (byte *p = glyph->bitmap;; p++)
        {
            int h, l;
            if (isxdigit (h = getc (file)) && isxdigit (l = getc (file)))
            {
                if (++glyph->byteCount > GLYPH_MAX_BYTE_COUNT)
                    fail ("Hex stream of "PRI_CP" is too long.", codePoint);
                *p = nibbleValue (h) << 4 | nibbleValue (l);
            }
            else if (h == '\n' || (h == EOF && feof (file)))
                break;
            else if (ferror (file))
                fail ("%s: Read error.", fileName);
            else
                fail ("Hex stream of "PRI_CP" is invalid.", codePoint);
        }
        if (glyph->byteCount % GLYPH_HEIGHT != 0)
            fail ("Hex length of "PRI_CP" is indivisible by glyph height %d.",
                codePoint, GLYPH_HEIGHT);
        if (glyph->byteCount > maxByteCount)
            maxByteCount = glyph->byteCount;
    }
    if (glyphCount == 1)
        fail ("No glyph is specified.");
    font->glyphCount = glyphCount;
    font->maxWidth = PW (maxByteCount);
    fclose (file);
}

/**
    @brief Compare two Unicode code points to determine which is greater.

    This function compares the Unicode code points contained within
    two Glyph data structures.  The function returns 1 if the first
    code point is greater, and -1 if the second is greater.

    @param[in] a A Glyph data structure containing the first code point.
    @param[in] b A Glyph data structure containing the second code point.
    @return 1 if the code point a is greater, -1 if less, 0 if equal.
*/
int
byCodePoint (const void *a, const void *b)
{
    const Glyph *const ga = a, *const gb = b;
    int gt = ga->codePoint > gb->codePoint;
    int lt = ga->codePoint < gb->codePoint;
    return gt - lt;
}

/**
    @brief Position a glyph within a 16-by-16 pixel bounding box.

    Position a glyph within the 16-by-16 pixel drawing area and
    note whether or not the glyph is a combining character.

    N.B.: Glyphs must be sorted by code point before calling this function.

    @param[in,out] font Font data structure pointer to store glyphs.
    @param[in] fileName Name of glyph file to read.
    @param[in] xMin Minimum x-axis value (for left side bearing).
*/
void
positionGlyphs (Font *font, const char *fileName, pixels_t *xMin)
{
    *xMin = 0;
    FILE *file = fopen (fileName, "r");
    if (!file)
        fail ("Failed to open file '%s'.", fileName);
    Glyph *glyphs = getBufferHead (font->glyphs);
    const Glyph *const endGlyph = glyphs + font->glyphCount;
    Glyph *nextGlyph = &glyphs[1]; // predict and avoid search
    for (;;)
    {
        uint_fast32_t codePoint;
        if (readCodePoint (&codePoint, fileName, file))
            break;
        Glyph *glyph = nextGlyph;
        if (glyph == endGlyph || glyph->codePoint != codePoint)
        {
            // Prediction failed. Search.
            const Glyph key = { .codePoint = codePoint };
            glyph = bsearch (&key, glyphs + 1, font->glyphCount - 1,
                sizeof key, byCodePoint);
            if (!glyph)
                fail ("Glyph "PRI_CP" is positioned but not defined.",
                    codePoint);
        }
        nextGlyph = glyph + 1;
        char s[8];
        if (!fgets (s, sizeof s, file))
            fail ("%s: Read error.", fileName);
        char *end;
        const long value = strtol (s, &end, 10);
        if (*end != '\n' && *end != '\0')
            fail ("Position of glyph "PRI_CP" is invalid.", codePoint);
        // Currently no glyph is moved to the right,
        // so positive position is considered out of range.
        // If this limit is to be lifted,
        // 'xMax' of bounding box in 'head' table shall also be updated.
        if (value < -GLYPH_MAX_WIDTH || value > 0)
            fail ("Position of glyph "PRI_CP" is out of range.", codePoint);
        glyph->combining = true;
        glyph->pos = value;
        glyph->lsb = value; // updated during outline generation
        if (value < *xMin)
            *xMin = value;
    }
    fclose (file);
}

/**
    @brief Sort the glyphs in a font by Unicode code point.

    This function reads in an array of glyphs and sorts them
    by Unicode code point.  If a duplicate code point is encountered,
    that will result in a fatal error with an error message to stderr.

    @param[in,out] font Pointer to a Font structure with glyphs to sort.
*/
void
sortGlyphs (Font *font)
{
    Glyph *glyphs = getBufferHead (font->glyphs);
    const Glyph *const glyphsEnd = getBufferTail (font->glyphs);
    glyphs++; // glyph 0 does not need sorting
    qsort (glyphs, glyphsEnd - glyphs, sizeof *glyphs, byCodePoint);
    for (const Glyph *glyph = glyphs; glyph < glyphsEnd - 1; glyph++)
    {
        if (glyph[0].codePoint == glyph[1].codePoint)
            fail ("Duplicate code point: "PRI_CP".", glyph[0].codePoint);
        assert (glyph[0].codePoint < glyph[1].codePoint);
    }
}

/**
    @brief Specify the current contour drawing operation.
*/
enum ContourOp {
    OP_CLOSE,    ///< Close the current contour path that was being drawn.
    OP_POINT     ///< Add one more (x,y) point to the contor being drawn.
};

/**
    @brief Fill to the left side (CFF) or right side (TrueType) of a contour.
*/
enum FillSide {
    FILL_LEFT,    ///< Draw outline counter-clockwise (CFF, PostScript).
    FILL_RIGHT    ///< Draw outline clockwise (TrueType).
};

/**
    @brief Build a glyph outline.

    This function builds a glyph outline from a Unifont glyph bitmap.

    @param[out] result The resulting glyph outline.
    @param[in] bitmap A bitmap array.
    @param[in] byteCount the number of bytes in the input bitmap array.
    @param[in] fillSide Enumerated indicator to fill left or right side.
*/
void
buildOutline (Buffer *result, const byte bitmap[], const size_t byteCount,
    const enum FillSide fillSide)
{
    enum Direction {RIGHT, LEFT, DOWN, UP}; // order is significant

    // respective coordinate deltas
    const pixels_t dx[] = {1, -1, 0, 0}, dy[] = {0, 0, -1, 1};

    assert (byteCount % GLYPH_HEIGHT == 0);
    const uint_fast8_t bytesPerRow = byteCount / GLYPH_HEIGHT;
    const pixels_t glyphWidth = bytesPerRow * 8;
    assert (glyphWidth <= GLYPH_MAX_WIDTH);

    #if GLYPH_MAX_WIDTH < 32
        typedef uint_fast32_t row_t;
    #elif GLYPH_MAX_WIDTH < 64
        typedef uint_fast64_t row_t;
    #else
        #error GLYPH_MAX_WIDTH is too large.
    #endif

    row_t pixels[GLYPH_HEIGHT + 2] = {0};
    for (pixels_t row = GLYPH_HEIGHT; row > 0; row--)
        for (pixels_t b = 0; b < bytesPerRow; b++)
            pixels[row] = pixels[row] << 8 | *bitmap++;
    typedef row_t graph_t[GLYPH_HEIGHT + 1];
    graph_t vectors[4];
    const row_t *lower = pixels, *upper = pixels + 1;
    for (pixels_t row = 0; row <= GLYPH_HEIGHT; row++)
    {
        const row_t m = (fillSide == FILL_RIGHT) - 1;
        vectors[RIGHT][row] = (m ^ (*lower << 1)) & (~m ^ (*upper << 1));
        vectors[LEFT ][row] = (m ^ (*upper     )) & (~m ^ (*lower     ));
        vectors[DOWN ][row] = (m ^ (*lower     )) & (~m ^ (*lower << 1));
        vectors[UP   ][row] = (m ^ (*upper << 1)) & (~m ^ (*upper     ));
        lower++;
        upper++;
    }
    graph_t selection = {0};
    const row_t x0 = (row_t)1 << glyphWidth;

    /// Get the value of a given bit that is in a given row.
    #define getRowBit(rows, x, y)  ((rows)[(y)] &  x0 >> (x))

    /// Invert the value of a given bit that is in a given row.
    #define flipRowBit(rows, x, y) ((rows)[(y)] ^= x0 >> (x))

    for (pixels_t y = GLYPH_HEIGHT; y >= 0; y--)
    {
        for (pixels_t x = 0; x <= glyphWidth; x++)
        {
            assert (!getRowBit (vectors[LEFT], x, y));
            assert (!getRowBit (vectors[UP], x, y));
            enum Direction initial;

            if (getRowBit (vectors[RIGHT], x, y))
                initial = RIGHT;
            else if (getRowBit (vectors[DOWN], x, y))
                initial = DOWN;
            else
                continue;

            static_assert ((GLYPH_MAX_WIDTH + 1) * (GLYPH_HEIGHT + 1) * 2 <=
                U16MAX, "potential overflow");

            uint_fast16_t lastPointCount = 0;
            for (bool converged = false;;)
            {
                uint_fast16_t pointCount = 0;
                enum Direction heading = initial;
                for (pixels_t tx = x, ty = y;;)
                {
                    if (converged)
                    {
                        storePixels (result, OP_POINT);
                        storePixels (result, tx);
                        storePixels (result, ty);
                    }
                    do
                    {
                        if (converged)
                            flipRowBit (vectors[heading], tx, ty);
                        tx += dx[heading];
                        ty += dy[heading];
                    } while (getRowBit (vectors[heading], tx, ty));
                    if (tx == x && ty == y)
                        break;
                    static_assert ((UP ^ DOWN) == 1 && (LEFT ^ RIGHT) == 1,
                        "wrong enums");
                    heading = (heading & 2) ^ 2;
                    heading |= !!getRowBit (selection, tx, ty);
                    heading ^= !getRowBit (vectors[heading], tx, ty);
                    assert (getRowBit (vectors[heading], tx, ty));
                    flipRowBit (selection, tx, ty);
                    pointCount++;
                }
                if (converged)
                    break;
                converged = pointCount == lastPointCount;
                lastPointCount = pointCount;
            }

            storePixels (result, OP_CLOSE);
        }
    }
    #undef getRowBit
    #undef flipRowBit
}

/**
    @brief Prepare 32-bit glyph offsets in a font table.

    @param[in] sizes Array of glyph sizes, for offset calculations.
*/
void
prepareOffsets (size_t *sizes)
{
    size_t *p = sizes;
    for (size_t *i = sizes + 1; *i; i++)
        *i += *p++;
    if (*p > 2147483647U) // offset not representable
        fail ("CFF table is too large.");
}

/**
    @brief Prepare a font name string index.

    @param[in] names List of name strings.
    @return Pointer to a Buffer struct containing the string names.
*/
Buffer *
prepareStringIndex (const NameStrings names)
{
    Buffer *buf = newBuffer (256);
    assert (names[6]);
    const char *strings[] = {"Adobe", "Identity", names[6]};
    /// Get the number of elements in array char *strings[].
    #define stringCount (sizeof strings / sizeof *strings)
    static_assert (stringCount <= U16MAX, "too many strings");
    size_t offset = 1;
    size_t lengths[stringCount];
    for (size_t i = 0; i < stringCount; i++)
    {
        assert (strings[i]);
        lengths[i] = strlen (strings[i]);
        offset += lengths[i];
    }
    int offsetSize = 1 + (offset > 0xff)
                       + (offset > 0xffff)
                       + (offset > 0xffffff);
    cacheU16 (buf, stringCount); // count
    cacheU8 (buf, offsetSize); // offSize
    cacheU (buf, offset = 1, offsetSize); // offset[0]
    for (size_t i = 0; i < stringCount; i++)
        cacheU (buf, offset += lengths[i], offsetSize); // offset[i + 1]
    for (size_t i = 0; i < stringCount; i++)
        cacheBytes (buf, strings[i], lengths[i]);
    #undef stringCount
    return buf;
}

/**
    @brief Add a CFF table to a font.

    @param[in,out] font Pointer to a Font struct to contain the CFF table.
    @param[in] version Version of CFF table, with value 1 or 2.
    @param[in] names List of NameStrings.
*/
void
fillCFF (Font *font, int version, const NameStrings names)
{
    // HACK: For convenience, CFF data structures are hard coded.
    assert (0 < version && version <= 2);
    Buffer *cff = newBuffer (65536);
    addTable (font, version == 1 ? "CFF " : "CFF2", cff);

    /// Use fixed width integer for variables to simplify offset calculation.
    #define cacheCFF32(buf, x) (cacheU8 ((buf), 29), cacheU32 ((buf), (x)))

    // In Unifont, 16px glyphs are more common. This is used by CFF1 only.
    const pixels_t defaultWidth = 16, nominalWidth = 8;
    if (version == 1)
    {
        Buffer *strings = prepareStringIndex (names);
        size_t stringsSize = countBufferedBytes (strings);
        const char *cffName = names[6];
        assert (cffName);
        size_t nameLength = strlen (cffName);
        size_t namesSize = nameLength + 5;
        // These sizes must be updated together with the data below.
        size_t offsets[] = {4, namesSize, 45, stringsSize, 2, 5, 8, 32, 4, 0};
        prepareOffsets (offsets);
        { // Header
            cacheU8 (cff, 1); // major
            cacheU8 (cff, 0); // minor
            cacheU8 (cff, 4); // hdrSize
            cacheU8 (cff, 1); // offSize
        }
        assert (countBufferedBytes (cff) == offsets[0]);
        { // Name INDEX (should not be used by OpenType readers)
            cacheU16 (cff, 1); // count
            cacheU8 (cff, 1); // offSize
            cacheU8 (cff, 1); // offset[0]
            if (nameLength + 1 > 255) // must be too long; spec limit is 63
                fail ("PostScript name is too long.");
            cacheU8 (cff, nameLength + 1); // offset[1]
            cacheBytes (cff, cffName, nameLength);
        }
        assert (countBufferedBytes (cff) == offsets[1]);
        { // Top DICT INDEX
            cacheU16 (cff, 1); // count
            cacheU8 (cff, 1); // offSize
            cacheU8 (cff, 1); // offset[0]
            cacheU8 (cff, 41); // offset[1]
            cacheCFFOperand (cff, 391); // "Adobe"
            cacheCFFOperand (cff, 392); // "Identity"
            cacheCFFOperand (cff, 0);
            cacheBytes (cff, (byte[]){12, 30}, 2); // ROS
            cacheCFF32 (cff, font->glyphCount);
            cacheBytes (cff, (byte[]){12, 34}, 2); // CIDCount
            cacheCFF32 (cff, offsets[6]);
            cacheBytes (cff, (byte[]){12, 36}, 2); // FDArray
            cacheCFF32 (cff, offsets[5]);
            cacheBytes (cff, (byte[]){12, 37}, 2); // FDSelect
            cacheCFF32 (cff, offsets[4]);
            cacheU8 (cff, 15); // charset
            cacheCFF32 (cff, offsets[8]);
            cacheU8 (cff, 17); // CharStrings
        }
        assert (countBufferedBytes (cff) == offsets[2]);
        { // String INDEX
            cacheBuffer (cff, strings);
            freeBuffer (strings);
        }
        assert (countBufferedBytes (cff) == offsets[3]);
        cacheU16 (cff, 0); // Global Subr INDEX
        assert (countBufferedBytes (cff) == offsets[4]);
        { // Charsets
            cacheU8 (cff, 2); // format
            { // Range2[0]
                cacheU16 (cff, 1); // first
                cacheU16 (cff, font->glyphCount - 2); // nLeft
            }
        }
        assert (countBufferedBytes (cff) == offsets[5]);
        { // FDSelect
            cacheU8 (cff, 3); // format
            cacheU16 (cff, 1); // nRanges
            cacheU16 (cff, 0); // first
            cacheU8 (cff, 0); // fd
            cacheU16 (cff, font->glyphCount); // sentinel
        }
        assert (countBufferedBytes (cff) == offsets[6]);
        { // FDArray
            cacheU16 (cff, 1); // count
            cacheU8 (cff, 1); // offSize
            cacheU8 (cff, 1); // offset[0]
            cacheU8 (cff, 28); // offset[1]
            cacheCFFOperand (cff, 393);
            cacheBytes (cff, (byte[]){12, 38}, 2); // FontName
            // Windows requires FontMatrix in Font DICT.
            const byte unit[] = {0x1e,0x15,0x62,0x5c,0x6f}; // 1/64 (0.015625)
            cacheBytes (cff, unit, sizeof unit);
            cacheCFFOperand (cff, 0);
            cacheCFFOperand (cff, 0);
            cacheBytes (cff, unit, sizeof unit);
            cacheCFFOperand (cff, 0);
            cacheCFFOperand (cff, 0);
            cacheBytes (cff, (byte[]){12, 7}, 2); // FontMatrix
            cacheCFFOperand (cff, offsets[8] - offsets[7]); // size
            cacheCFF32 (cff, offsets[7]); // offset
            cacheU8 (cff, 18); // Private
        }
        assert (countBufferedBytes (cff) == offsets[7]);
        { // Private
            cacheCFFOperand (cff, FU (defaultWidth));
            cacheU8 (cff, 20); // defaultWidthX
            cacheCFFOperand (cff, FU (nominalWidth));
            cacheU8 (cff, 21); // nominalWidthX
        }
        assert (countBufferedBytes (cff) == offsets[8]);
    }
    else
    {
        assert (version == 2);
        // These sizes must be updated together with the data below.
        size_t offsets[] = {5, 21, 4, 10, 0};
        prepareOffsets (offsets);
        { // Header
            cacheU8 (cff, 2); // majorVersion
            cacheU8 (cff, 0); // minorVersion
            cacheU8 (cff, 5); // headerSize
            cacheU16 (cff, offsets[1] - offsets[0]); // topDictLength
        }
        assert (countBufferedBytes (cff) == offsets[0]);
        { // Top DICT
            const byte unit[] = {0x1e,0x15,0x62,0x5c,0x6f}; // 1/64 (0.015625)
            cacheBytes (cff, unit, sizeof unit);
            cacheCFFOperand (cff, 0);
            cacheCFFOperand (cff, 0);
            cacheBytes (cff, unit, sizeof unit);
            cacheCFFOperand (cff, 0);
            cacheCFFOperand (cff, 0);
            cacheBytes (cff, (byte[]){12, 7}, 2); // FontMatrix
            cacheCFFOperand (cff, offsets[2]);
            cacheBytes (cff, (byte[]){12, 36}, 2); // FDArray
            cacheCFFOperand (cff, offsets[3]);
            cacheU8 (cff, 17); // CharStrings
        }
        assert (countBufferedBytes (cff) == offsets[1]);
        cacheU32 (cff, 0); // Global Subr INDEX
        assert (countBufferedBytes (cff) == offsets[2]);
        { // Font DICT INDEX
            cacheU32 (cff, 1); // count
            cacheU8 (cff, 1); // offSize
            cacheU8 (cff, 1); // offset[0]
            cacheU8 (cff, 4); // offset[1]
            cacheCFFOperand (cff, 0);
            cacheCFFOperand (cff, 0);
            cacheU8 (cff, 18); // Private
        }
        assert (countBufferedBytes (cff) == offsets[3]);
    }
    { // CharStrings INDEX
        Buffer *offsets = newBuffer (4096);
        Buffer *charstrings = newBuffer (4096);
        Buffer *outline = newBuffer (1024);
        const Glyph *glyph = getBufferHead (font->glyphs);
        const Glyph *const endGlyph = glyph + font->glyphCount;
        for (; glyph < endGlyph; glyph++)
        {
            // CFF offsets start at 1
            storeU32 (offsets, countBufferedBytes (charstrings) + 1);

            pixels_t rx = -glyph->pos;
            pixels_t ry = DESCENDER;
            resetBuffer (outline);
            buildOutline (outline, glyph->bitmap, glyph->byteCount, FILL_LEFT);
            enum CFFOp {rmoveto=21, hmoveto=22, vmoveto=4, hlineto=6,
                vlineto=7, endchar=14};
            enum CFFOp pendingOp = 0;
            const int STACK_LIMIT = version == 1 ? 48 : 513;
            int stackSize = 0;
            bool isDrawing = false;
            pixels_t width = glyph->combining ? 0 : PW (glyph->byteCount);
            if (version == 1 && width != defaultWidth)
            {
                cacheCFFOperand (charstrings, FU (width - nominalWidth));
                stackSize++;
            }
            for (const pixels_t *p = getBufferHead (outline),
                 *const end = getBufferTail (outline); p < end;)
            {
                int s = 0;
                const enum ContourOp op = *p++;
                if (op == OP_POINT)
                {
                    const pixels_t x = *p++, y = *p++;
                    if (x != rx)
                    {
                        cacheCFFOperand (charstrings, FU (x - rx));
                        rx = x;
                        stackSize++;
                        s |= 1;
                    }
                    if (y != ry)
                    {
                        cacheCFFOperand (charstrings, FU (y - ry));
                        ry = y;
                        stackSize++;
                        s |= 2;
                    }
                    assert (!(isDrawing && s == 3));
                }
                if (s)
                {
                    if (!isDrawing)
                    {
                        const enum CFFOp moves[] = {0, hmoveto, vmoveto,
                            rmoveto};
                        cacheU8 (charstrings, moves[s]);
                        stackSize = 0;
                    }
                    else if (!pendingOp)
                        pendingOp = (enum CFFOp[]){0, hlineto, vlineto}[s];
                }
                else if (!isDrawing)
                {
                    // only when the first point happens to be (0, 0)
                    cacheCFFOperand (charstrings, FU (0));
                    cacheU8 (charstrings, hmoveto);
                    stackSize = 0;
                }
                if (op == OP_CLOSE || stackSize >= STACK_LIMIT)
                {
                    assert (stackSize <= STACK_LIMIT);
                    cacheU8 (charstrings, pendingOp);
                    pendingOp = 0;
                    stackSize = 0;
                }
                isDrawing = op != OP_CLOSE;
            }
            if (version == 1)
                cacheU8 (charstrings, endchar);
        }
        size_t lastOffset = countBufferedBytes (charstrings) + 1;
        #if SIZE_MAX > U32MAX
            if (lastOffset > U32MAX)
                fail ("CFF data exceeded size limit.");
        #endif
        storeU32 (offsets, lastOffset);
        int offsetSize = 1 + (lastOffset > 0xff)
                           + (lastOffset > 0xffff)
                           + (lastOffset > 0xffffff);
        // count (must match 'numGlyphs' in 'maxp' table)
        cacheU (cff, font->glyphCount, version * 2);
        cacheU8 (cff, offsetSize); // offSize
        const uint_least32_t *p = getBufferHead (offsets);
        const uint_least32_t *const end = getBufferTail (offsets);
        for (; p < end; p++)
            cacheU (cff, *p, offsetSize); // offsets
        cacheBuffer (cff, charstrings); // data
        freeBuffer (offsets);
        freeBuffer (charstrings);
        freeBuffer (outline);
    }
    #undef cacheCFF32
}

/**
    @brief Add a TrueType table to a font.

    @param[in,out] font Pointer to a Font struct to contain the TrueType table.
    @param[in] format The TrueType "loca" table format, Offset16 or Offset32.
    @param[in] names List of NameStrings.
*/
void
fillTrueType (Font *font, enum LocaFormat *format,
    uint_fast16_t *maxPoints, uint_fast16_t *maxContours)
{
    Buffer *glyf = newBuffer (65536);
    addTable (font, "glyf", glyf);
    Buffer *loca = newBuffer (4 * (font->glyphCount + 1));
    addTable (font, "loca", loca);
    *format = LOCA_OFFSET32;
    Buffer *endPoints = newBuffer (256);
    Buffer *flags = newBuffer (256);
    Buffer *xs = newBuffer (256);
    Buffer *ys = newBuffer (256);
    Buffer *outline = newBuffer (1024);
    Glyph *const glyphs = getBufferHead (font->glyphs);
    const Glyph *const glyphsEnd = getBufferTail (font->glyphs);
    for (Glyph *glyph = glyphs; glyph < glyphsEnd; glyph++)
    {
        cacheU32 (loca, countBufferedBytes (glyf));
        pixels_t rx = -glyph->pos;
        pixels_t ry = DESCENDER;
        pixels_t xMin = GLYPH_MAX_WIDTH, xMax = 0;
        pixels_t yMin = ASCENDER, yMax = -DESCENDER;
        resetBuffer (endPoints);
        resetBuffer (flags);
        resetBuffer (xs);
        resetBuffer (ys);
        resetBuffer (outline);
        buildOutline (outline, glyph->bitmap, glyph->byteCount, FILL_RIGHT);
        uint_fast32_t pointCount = 0, contourCount = 0;
        for (const pixels_t *p = getBufferHead (outline),
             *const end = getBufferTail (outline); p < end;)
        {
            const enum ContourOp op = *p++;
            if (op == OP_CLOSE)
            {
                contourCount++;
                assert (contourCount <= U16MAX);
                cacheU16 (endPoints, pointCount - 1);
                continue;
            }
            assert (op == OP_POINT);
            pointCount++;
            assert (pointCount <= U16MAX);
            const pixels_t x = *p++, y = *p++;
            uint_fast8_t pointFlags =
                + B1 (0) // point is on curve
                + BX (1, x != rx) // x coordinate is 1 byte instead of 2
                + BX (2, y != ry) // y coordinate is 1 byte instead of 2
                + B0 (3) // repeat
                + BX (4, x >= rx) // when x is 1 byte: x is positive;
                                  // when x is 2 bytes: x unchanged and omitted
                + BX (5, y >= ry) // when y is 1 byte: y is positive;
                                  // when y is 2 bytes: y unchanged and omitted
                + B1 (6) // contours may overlap
                + B0 (7) // reserved
            ;
            cacheU8 (flags, pointFlags);
            if (x != rx)
                cacheU8 (xs, FU (x > rx ? x - rx : rx - x));
            if (y != ry)
                cacheU8 (ys, FU (y > ry ? y - ry : ry - y));
            if (x < xMin) xMin = x;
            if (y < yMin) yMin = y;
            if (x > xMax) xMax = x;
            if (y > yMax) yMax = y;
            rx = x;
            ry = y;
        }
        if (contourCount == 0)
            continue; // blank glyph is indicated by the 'loca' table
        glyph->lsb = glyph->pos + xMin;
        cacheU16 (glyf, contourCount); // numberOfContours
        cacheU16 (glyf, FU (glyph->pos + xMin)); // xMin
        cacheU16 (glyf, FU (yMin)); // yMin
        cacheU16 (glyf, FU (glyph->pos + xMax)); // xMax
        cacheU16 (glyf, FU (yMax)); // yMax
        cacheBuffer (glyf, endPoints); // endPtsOfContours[]
        cacheU16 (glyf, 0); // instructionLength
        cacheBuffer (glyf, flags); // flags[]
        cacheBuffer (glyf, xs); // xCoordinates[]
        cacheBuffer (glyf, ys); // yCoordinates[]
        if (pointCount > *maxPoints)
            *maxPoints = pointCount;
        if (contourCount > *maxContours)
            *maxContours = contourCount;
    }
    cacheU32 (loca, countBufferedBytes (glyf));
    freeBuffer (endPoints);
    freeBuffer (flags);
    freeBuffer (xs);
    freeBuffer (ys);
    freeBuffer (outline);
}

/**
    @brief Create a dummy blank outline in a font table.

    @param[in,out] font Pointer to a Font struct to insert a blank outline.
*/
void
fillBlankOutline (Font *font)
{
    Buffer *glyf = newBuffer (12);
    addTable (font, "glyf", glyf);
    // Empty table is not allowed, but an empty outline for glyph 0 suffices.
    cacheU16 (glyf, 0); // numberOfContours
    cacheU16 (glyf, FU (0)); // xMin
    cacheU16 (glyf, FU (0)); // yMin
    cacheU16 (glyf, FU (0)); // xMax
    cacheU16 (glyf, FU (0)); // yMax
    cacheU16 (glyf, 0); // instructionLength
    Buffer *loca = newBuffer (2 * (font->glyphCount + 1));
    addTable (font, "loca", loca);
    cacheU16 (loca, 0); // offsets[0]
    assert (countBufferedBytes (glyf) % 2 == 0);
    for (uint_fast32_t i = 1; i <= font->glyphCount; i++)
        cacheU16 (loca, countBufferedBytes (glyf) / 2); // offsets[i]
}

/**
    @brief Fill OpenType bitmap data and location tables.

    This function fills an Embedded Bitmap Data (EBDT) Table
    and an Embedded Bitmap Location (EBLC) Table with glyph
    bitmap information.  These tables enable embedding bitmaps
    in OpenType fonts.  No Embedded Bitmap Scaling (EBSC) table
    is used for the bitmap glyphs, only EBDT and EBLC.

    @param[in,out] font Pointer to a Font struct in which to add bitmaps.
*/
void
fillBitmap (Font *font)
{
    const Glyph *const glyphs = getBufferHead (font->glyphs);
    const Glyph *const glyphsEnd = getBufferTail (font->glyphs);
    size_t bitmapsSize = 0;
    for (const Glyph *glyph = glyphs; glyph < glyphsEnd; glyph++)
        bitmapsSize += glyph->byteCount;
    Buffer *ebdt = newBuffer (4 + bitmapsSize);
    addTable (font, "EBDT", ebdt);
    cacheU16 (ebdt, 2); // majorVersion
    cacheU16 (ebdt, 0); // minorVersion
    uint_fast8_t byteCount = 0; // unequal to any glyph
    pixels_t pos = 0;
    bool combining = false;
    Buffer *rangeHeads = newBuffer (32);
    Buffer *offsets = newBuffer (64);
    for (const Glyph *glyph = glyphs; glyph < glyphsEnd; glyph++)
    {
        if (glyph->byteCount != byteCount || glyph->pos != pos ||
            glyph->combining != combining)
        {
            storeU16 (rangeHeads, glyph - glyphs);
            storeU32 (offsets, countBufferedBytes (ebdt));
            byteCount = glyph->byteCount;
            pos = glyph->pos;
            combining = glyph->combining;
        }
        cacheBytes (ebdt, glyph->bitmap, byteCount);
    }
    const uint_least16_t *ranges = getBufferHead (rangeHeads);
    const uint_least16_t *rangesEnd = getBufferTail (rangeHeads);
    uint_fast32_t rangeCount = rangesEnd - ranges;
    storeU16 (rangeHeads, font->glyphCount);
    Buffer *eblc = newBuffer (4096);
    addTable (font, "EBLC", eblc);
    cacheU16 (eblc, 2); // majorVersion
    cacheU16 (eblc, 0); // minorVersion
    cacheU32 (eblc, 1); // numSizes
    { // bitmapSizes[0]
        cacheU32 (eblc, 56); // indexSubTableArrayOffset
        cacheU32 (eblc, (8 + 20) * rangeCount); // indexTablesSize
        cacheU32 (eblc, rangeCount); // numberOfIndexSubTables
        cacheU32 (eblc, 0); // colorRef
        { // hori
            cacheU8 (eblc, ASCENDER); // ascender
            cacheU8 (eblc, -DESCENDER); // descender
            cacheU8 (eblc, font->maxWidth); // widthMax
            cacheU8 (eblc, 1); // caretSlopeNumerator
            cacheU8 (eblc, 0); // caretSlopeDenominator
            cacheU8 (eblc, 0); // caretOffset
            cacheU8 (eblc, 0); // minOriginSB
            cacheU8 (eblc, 0); // minAdvanceSB
            cacheU8 (eblc, ASCENDER); // maxBeforeBL
            cacheU8 (eblc, -DESCENDER); // minAfterBL
            cacheU8 (eblc, 0); // pad1
            cacheU8 (eblc, 0); // pad2
        }
        { // vert
            cacheU8 (eblc, ASCENDER); // ascender
            cacheU8 (eblc, -DESCENDER); // descender
            cacheU8 (eblc, font->maxWidth); // widthMax
            cacheU8 (eblc, 1); // caretSlopeNumerator
            cacheU8 (eblc, 0); // caretSlopeDenominator
            cacheU8 (eblc, 0); // caretOffset
            cacheU8 (eblc, 0); // minOriginSB
            cacheU8 (eblc, 0); // minAdvanceSB
            cacheU8 (eblc, ASCENDER); // maxBeforeBL
            cacheU8 (eblc, -DESCENDER); // minAfterBL
            cacheU8 (eblc, 0); // pad1
            cacheU8 (eblc, 0); // pad2
        }
        cacheU16 (eblc, 0); // startGlyphIndex
        cacheU16 (eblc, font->glyphCount - 1); // endGlyphIndex
        cacheU8 (eblc, 16); // ppemX
        cacheU8 (eblc, 16); // ppemY
        cacheU8 (eblc, 1); // bitDepth
        cacheU8 (eblc, 1); // flags = Horizontal
    }
    { // IndexSubTableArray
        uint_fast32_t offset = rangeCount * 8;
        for (const uint_least16_t *p = ranges; p < rangesEnd; p++)
        {
            cacheU16 (eblc, *p); // firstGlyphIndex
            cacheU16 (eblc, p[1] - 1); // lastGlyphIndex
            cacheU32 (eblc, offset); // additionalOffsetToIndexSubtable
            offset += 20;
        }
    }
    { // IndexSubTables
        const uint_least32_t *offset = getBufferHead (offsets);
        for (const uint_least16_t *p = ranges; p < rangesEnd; p++)
        {
            const Glyph *glyph = &glyphs[*p];
            cacheU16 (eblc, 2); // indexFormat
            cacheU16 (eblc, 5); // imageFormat
            cacheU32 (eblc, *offset++); // imageDataOffset
            cacheU32 (eblc, glyph->byteCount); // imageSize
            { // bigMetrics
                cacheU8 (eblc, GLYPH_HEIGHT); // height
                const uint_fast8_t width = PW (glyph->byteCount);
                cacheU8 (eblc, width); // width
                cacheU8 (eblc, glyph->pos); // horiBearingX
                cacheU8 (eblc, ASCENDER); // horiBearingY
                cacheU8 (eblc, glyph->combining ? 0 : width); // horiAdvance
                cacheU8 (eblc, 0); // vertBearingX
                cacheU8 (eblc, 0); // vertBearingY
                cacheU8 (eblc, GLYPH_HEIGHT); // vertAdvance
            }
        }
    }
    freeBuffer (rangeHeads);
    freeBuffer (offsets);
}

/**
    @brief Fill a "head" font table.

    The "head" table contains font header information common to the
    whole font.

    @param[in,out] font The Font struct to which to add the table.
    @param[in] locaFormat The "loca" offset index location table.
    @param[in] xMin The minimum x-coordinate for a glyph.
*/
void
fillHeadTable (Font *font, enum LocaFormat locaFormat, pixels_t xMin)
{
    Buffer *head = newBuffer (56);
    addTable (font, "head", head);
    cacheU16 (head, 1); // majorVersion
    cacheU16 (head, 0); // minorVersion
    cacheZeros (head, 4); // fontRevision (unused)
    // The 'checksumAdjustment' field is a checksum of the entire file.
    // It is later calculated and written directly in the 'writeFont' function.
    cacheU32 (head, 0); // checksumAdjustment (placeholder)
    cacheU32 (head, 0x5f0f3cf5); // magicNumber
    const uint_fast16_t flags =
        + B1 ( 0) // baseline at y=0
        + B1 ( 1) // LSB at x=0 (doubtful; probably should be LSB=xMin)
        + B0 ( 2) // instructions may depend on point size
        + B0 ( 3) // force internal ppem to integers
        + B0 ( 4) // instructions may alter advance width
        + B0 ( 5) // not used in OpenType
        + B0 ( 6) // not used in OpenType
        + B0 ( 7) // not used in OpenType
        + B0 ( 8) // not used in OpenType
        + B0 ( 9) // not used in OpenType
        + B0 (10) // not used in OpenType
        + B0 (11) // font transformed
        + B0 (12) // font converted
        + B0 (13) // font optimized for ClearType
        + B0 (14) // last resort font
        + B0 (15) // reserved
    ;
    cacheU16 (head, flags); // flags
    cacheU16 (head, FUPEM); // unitsPerEm
    cacheZeros (head, 8); // created (unused)
    cacheZeros (head, 8); // modified (unused)
    cacheU16 (head, FU (xMin)); // xMin
    cacheU16 (head, FU (-DESCENDER)); // yMin
    cacheU16 (head, FU (font->maxWidth)); // xMax
    cacheU16 (head, FU (ASCENDER)); // yMax
    // macStyle (must agree with 'fsSelection' in 'OS/2' table)
    const uint_fast16_t macStyle =
        + B0 (0) // bold
        + B0 (1) // italic
        + B0 (2) // underline
        + B0 (3) // outline
        + B0 (4) // shadow
        + B0 (5) // condensed
        + B0 (6) // extended
        //    7-15  reserved
    ;
    cacheU16 (head, macStyle);
    cacheU16 (head, GLYPH_HEIGHT); // lowestRecPPEM
    cacheU16 (head, 2); // fontDirectionHint
    cacheU16 (head, locaFormat); // indexToLocFormat
    cacheU16 (head, 0); // glyphDataFormat
}

/**
    @brief Fill a "hhea" font table.

    The "hhea" table contains horizontal header information,
    for example left and right side bearings.

    @param[in,out] font The Font struct to which to add the table.
    @param[in] xMin The minimum x-coordinate for a glyph.
*/
void
fillHheaTable (Font *font, pixels_t xMin)
{
    Buffer *hhea = newBuffer (36);
    addTable (font, "hhea", hhea);
    cacheU16 (hhea, 1); // majorVersion
    cacheU16 (hhea, 0); // minorVersion
    cacheU16 (hhea, FU (ASCENDER)); // ascender
    cacheU16 (hhea, FU (-DESCENDER)); // descender
    cacheU16 (hhea, FU (0)); // lineGap
    cacheU16 (hhea, FU (font->maxWidth)); // advanceWidthMax
    cacheU16 (hhea, FU (xMin)); // minLeftSideBearing
    cacheU16 (hhea, FU (0)); // minRightSideBearing (unused)
    cacheU16 (hhea, FU (font->maxWidth)); // xMaxExtent
    cacheU16 (hhea, 1); // caretSlopeRise
    cacheU16 (hhea, 0); // caretSlopeRun
    cacheU16 (hhea, 0); // caretOffset
    cacheU16 (hhea, 0); // reserved
    cacheU16 (hhea, 0); // reserved
    cacheU16 (hhea, 0); // reserved
    cacheU16 (hhea, 0); // reserved
    cacheU16 (hhea, 0); // metricDataFormat
    cacheU16 (hhea, font->glyphCount); // numberOfHMetrics
}

/**
    @brief Fill a "maxp" font table.

    The "maxp" table contains maximum profile information,
    such as the memory required to contain the font.

    @param[in,out] font The Font struct to which to add the table.
    @param[in] isCFF true if a CFF font is included, false otherwise.
    @param[in] maxPoints Maximum points in a non-composite glyph.
    @param[in] maxContours Maximum contours in a non-composite glyph.
*/
void
fillMaxpTable (Font *font, bool isCFF, uint_fast16_t maxPoints,
    uint_fast16_t maxContours)
{
    Buffer *maxp = newBuffer (32);
    addTable (font, "maxp", maxp);
    cacheU32 (maxp, isCFF ? 0x00005000 : 0x00010000); // version
    cacheU16 (maxp, font->glyphCount); // numGlyphs
    if (isCFF)
        return;
    cacheU16 (maxp, maxPoints); // maxPoints
    cacheU16 (maxp, maxContours); // maxContours
    cacheU16 (maxp, 0); // maxCompositePoints
    cacheU16 (maxp, 0); // maxCompositeContours
    cacheU16 (maxp, 0); // maxZones
    cacheU16 (maxp, 0); // maxTwilightPoints
    cacheU16 (maxp, 0); // maxStorage
    cacheU16 (maxp, 0); // maxFunctionDefs
    cacheU16 (maxp, 0); // maxInstructionDefs
    cacheU16 (maxp, 0); // maxStackElements
    cacheU16 (maxp, 0); // maxSizeOfInstructions
    cacheU16 (maxp, 0); // maxComponentElements
    cacheU16 (maxp, 0); // maxComponentDepth
}

/**
    @brief Fill an "OS/2" font table.

    The "OS/2" table contains OS/2 and Windows font metrics information.

    @param[in,out] font The Font struct to which to add the table.
*/
void
fillOS2Table (Font *font)
{
    Buffer *os2 = newBuffer (100);
    addTable (font, "OS/2", os2);
    cacheU16 (os2, 5); // version
    // HACK: Average glyph width is not actually calculated.
    cacheU16 (os2, FU (font->maxWidth)); // xAvgCharWidth
    cacheU16 (os2, 400); // usWeightClass = Normal
    cacheU16 (os2, 5); // usWidthClass = Medium
    const uint_fast16_t typeFlags =
        + B0 (0) // reserved
        // usage permissions, one of:
            // Default: Installable embedding
            + B0 (1) // Restricted License embedding
            + B0 (2) // Preview & Print embedding
            + B0 (3) // Editable embedding
        //    4-7   reserved
        + B0 (8) // no subsetting
        + B0 (9) // bitmap embedding only
        //    10-15 reserved
    ;
    cacheU16 (os2, typeFlags); // fsType
    cacheU16 (os2, FU (5)); // ySubscriptXSize
    cacheU16 (os2, FU (7)); // ySubscriptYSize
    cacheU16 (os2, FU (0)); // ySubscriptXOffset
    cacheU16 (os2, FU (1)); // ySubscriptYOffset
    cacheU16 (os2, FU (5)); // ySuperscriptXSize
    cacheU16 (os2, FU (7)); // ySuperscriptYSize
    cacheU16 (os2, FU (0)); // ySuperscriptXOffset
    cacheU16 (os2, FU (4)); // ySuperscriptYOffset
    cacheU16 (os2, FU (1)); // yStrikeoutSize
    cacheU16 (os2, FU (5)); // yStrikeoutPosition
    cacheU16 (os2, 0x080a); // sFamilyClass = Sans Serif, Matrix
    const byte panose[] =
    {
        2, // Family Kind = Latin Text
        11, // Serif Style = Normal Sans
        4, // Weight = Thin
        // Windows would render all glyphs to the same width,
        // if 'Proportion' is set to 'Monospaced' (as Unifont should be).
        // 'Condensed' is the best alternative according to metrics.
        6, // Proportion = Condensed
        2, // Contrast = None
        2, // Stroke = No Variation
        2, // Arm Style = Straight Arms
        8, // Letterform = Normal/Square
        2, // Midline = Standard/Trimmed
        4, // X-height = Constant/Large
    };
    cacheBytes (os2, panose, sizeof panose); // panose
    // HACK: All defined Unicode ranges are marked functional for convenience.
    cacheU32 (os2, 0xffffffff); // ulUnicodeRange1
    cacheU32 (os2, 0xffffffff); // ulUnicodeRange2
    cacheU32 (os2, 0xffffffff); // ulUnicodeRange3
    cacheU32 (os2, 0x0effffff); // ulUnicodeRange4
    cacheBytes (os2, "GNU ", 4); // achVendID
    // fsSelection (must agree with 'macStyle' in 'head' table)
    const uint_fast16_t selection =
        + B0 (0) // italic
        + B0 (1) // underscored
        + B0 (2) // negative
        + B0 (3) // outlined
        + B0 (4) // strikeout
        + B0 (5) // bold
        + B1 (6) // regular
        + B1 (7) // use sTypo* metrics in this table
        + B1 (8) // font name conforms to WWS model
        + B0 (9) // oblique
        //    10-15 reserved
    ;
    cacheU16 (os2, selection);
    const Glyph *glyphs = getBufferHead (font->glyphs);
    uint_fast32_t first = glyphs[1].codePoint;
    uint_fast32_t last = glyphs[font->glyphCount - 1].codePoint;
    cacheU16 (os2, first < U16MAX ? first : U16MAX); // usFirstCharIndex
    cacheU16 (os2, last  < U16MAX ? last  : U16MAX); // usLastCharIndex
    cacheU16 (os2, FU (ASCENDER)); // sTypoAscender
    cacheU16 (os2, FU (-DESCENDER)); // sTypoDescender
    cacheU16 (os2, FU (0)); // sTypoLineGap
    cacheU16 (os2, FU (ASCENDER)); // usWinAscent
    cacheU16 (os2, FU (DESCENDER)); // usWinDescent
    // HACK: All reasonable code pages are marked functional for convenience.
    cacheU32 (os2, 0x603f01ff); // ulCodePageRange1
    cacheU32 (os2, 0xffff0000); // ulCodePageRange2
    cacheU16 (os2, FU (8)); // sxHeight
    cacheU16 (os2, FU (10)); // sCapHeight
    cacheU16 (os2, 0); // usDefaultChar
    cacheU16 (os2, 0x20); // usBreakChar
    cacheU16 (os2, 0); // usMaxContext
    cacheU16 (os2, 0); // usLowerOpticalPointSize
    cacheU16 (os2, 0xffff); // usUpperOpticalPointSize
}

/**
    @brief Fill an "hmtx" font table.

    The "hmtx" table contains horizontal metrics information.

    @param[in,out] font The Font struct to which to add the table.
*/
void
fillHmtxTable (Font *font)
{
    Buffer *hmtx = newBuffer (4 * font->glyphCount);
    addTable (font, "hmtx", hmtx);
    const Glyph *const glyphs = getBufferHead (font->glyphs);
    const Glyph *const glyphsEnd = getBufferTail (font->glyphs);
    for (const Glyph *glyph = glyphs; glyph < glyphsEnd; glyph++)
    {
        int_fast16_t aw = glyph->combining ? 0 : PW (glyph->byteCount);
        cacheU16 (hmtx, FU (aw)); // advanceWidth
        cacheU16 (hmtx, FU (glyph->lsb)); // lsb
    }
}

/**
    @brief Fill a "cmap" font table.

    The "cmap" table contains character to glyph index mapping information.

    @param[in,out] font The Font struct to which to add the table.
*/
void
fillCmapTable (Font *font)
{
    Glyph *const glyphs = getBufferHead (font->glyphs);
    Buffer *rangeHeads = newBuffer (16);
    uint_fast32_t rangeCount = 0;
    uint_fast32_t bmpRangeCount = 1; // 1 for the last 0xffff-0xffff range
    glyphs[0].codePoint = glyphs[1].codePoint; // to start a range at glyph 1
    for (uint_fast16_t i = 1; i < font->glyphCount; i++)
    {
        if (glyphs[i].codePoint != glyphs[i - 1].codePoint + 1)
        {
            storeU16 (rangeHeads, i);
            rangeCount++;
            bmpRangeCount += glyphs[i].codePoint < 0xffff;
        }
    }
    Buffer *cmap = newBuffer (256);
    addTable (font, "cmap", cmap);
    // Format 4 table is always generated for compatibility.
    bool hasFormat12 = glyphs[font->glyphCount - 1].codePoint > 0xffff;
    cacheU16 (cmap, 0); // version
    cacheU16 (cmap, 1 + hasFormat12); // numTables
    { // encodingRecords[0]
        cacheU16 (cmap, 3); // platformID
        cacheU16 (cmap, 1); // encodingID
        cacheU32 (cmap, 12 + 8 * hasFormat12); // subtableOffset
    }
    if (hasFormat12) // encodingRecords[1]
    {
        cacheU16 (cmap, 3); // platformID
        cacheU16 (cmap, 10); // encodingID
        cacheU32 (cmap, 36 + 8 * bmpRangeCount); // subtableOffset
    }
    const uint_least16_t *ranges = getBufferHead (rangeHeads);
    const uint_least16_t *const rangesEnd = getBufferTail (rangeHeads);
    storeU16 (rangeHeads, font->glyphCount);
    { // format 4 table
        cacheU16 (cmap, 4); // format
        cacheU16 (cmap, 16 + 8 * bmpRangeCount); // length
        cacheU16 (cmap, 0); // language
        if (bmpRangeCount * 2 > U16MAX)
            fail ("Too many ranges in 'cmap' table.");
        cacheU16 (cmap, bmpRangeCount * 2); // segCountX2
        uint_fast16_t searchRange = 1, entrySelector = -1;
        while (searchRange <= bmpRangeCount)
        {
            searchRange <<= 1;
            entrySelector++;
        }
        cacheU16 (cmap, searchRange); // searchRange
        cacheU16 (cmap, entrySelector); // entrySelector
        cacheU16 (cmap, bmpRangeCount * 2 - searchRange); // rangeShift
        { // endCode[]
            const uint_least16_t *p = ranges;
            for (p++; p < rangesEnd && glyphs[*p].codePoint < 0xffff; p++)
                cacheU16 (cmap, glyphs[*p - 1].codePoint);
            uint_fast32_t cp = glyphs[*p - 1].codePoint;
            if (cp > 0xfffe)
                cp = 0xfffe;
            cacheU16 (cmap, cp);
            cacheU16 (cmap, 0xffff);
        }
        cacheU16 (cmap, 0); // reservedPad
        { // startCode[]
            for (uint_fast32_t i = 0; i < bmpRangeCount - 1; i++)
                cacheU16 (cmap, glyphs[ranges[i]].codePoint);
            cacheU16 (cmap, 0xffff);
        }
        { // idDelta[]
            const uint_least16_t *p = ranges;
            for (; p < rangesEnd && glyphs[*p].codePoint < 0xffff; p++)
                cacheU16 (cmap, *p - glyphs[*p].codePoint);
            uint_fast16_t delta = 1;
            if (p < rangesEnd && *p == 0xffff)
                delta = *p - glyphs[*p].codePoint;
            cacheU16 (cmap, delta);
        }
        { // idRangeOffsets[]
            for (uint_least16_t i = 0; i < bmpRangeCount; i++)
                cacheU16 (cmap, 0);
        }
    }
    if (hasFormat12) // format 12 table
    {
        cacheU16 (cmap, 12); // format
        cacheU16 (cmap, 0); // reserved
        cacheU32 (cmap, 16 + 12 * rangeCount); // length
        cacheU32 (cmap, 0); // language
        cacheU32 (cmap, rangeCount); // numGroups

        // groups[]
        for (const uint_least16_t *p = ranges; p < rangesEnd; p++)
        {
            cacheU32 (cmap, glyphs[*p].codePoint); // startCharCode
            cacheU32 (cmap, glyphs[p[1] - 1].codePoint); // endCharCode
            cacheU32 (cmap, *p); // startGlyphID
        }
    }
    freeBuffer (rangeHeads);
}

/**
    @brief Fill a "post" font table.

    The "post" table contains information for PostScript printers.

    @param[in,out] font The Font struct to which to add the table.
*/
void
fillPostTable (Font *font)
{
    Buffer *post = newBuffer (32);
    addTable (font, "post", post);
    cacheU32 (post, 0x00030000); // version = 3.0
    cacheU32 (post, 0); // italicAngle
    cacheU16 (post, 0); // underlinePosition
    cacheU16 (post, 1); // underlineThickness
    cacheU32 (post, 1); // isFixedPitch
    cacheU32 (post, 0); // minMemType42
    cacheU32 (post, 0); // maxMemType42
    cacheU32 (post, 0); // minMemType1
    cacheU32 (post, 0); // maxMemType1
}

/**
    @brief Fill a "GPOS" font table.

    The "GPOS" table contains information for glyph positioning.

    @param[in,out] font The Font struct to which to add the table.
*/
void
fillGposTable (Font *font)
{
    Buffer *gpos = newBuffer (16);
    addTable (font, "GPOS", gpos);
    cacheU16 (gpos, 1); // majorVersion
    cacheU16 (gpos, 0); // minorVersion
    cacheU16 (gpos, 10); // scriptListOffset
    cacheU16 (gpos, 12); // featureListOffset
    cacheU16 (gpos, 14); // lookupListOffset
    { // ScriptList table
        cacheU16 (gpos, 0); // scriptCount
    }
    { // Feature List table
        cacheU16 (gpos, 0); // featureCount
    }
    { // Lookup List Table
        cacheU16 (gpos, 0); // lookupCount
    }
}

/**
    @brief Fill a "GSUB" font table.

    The "GSUB" table contains information for glyph substitution.

    @param[in,out] font The Font struct to which to add the table.
*/
void
fillGsubTable (Font *font)
{
    Buffer *gsub = newBuffer (38);
    addTable (font, "GSUB", gsub);
    cacheU16 (gsub, 1); // majorVersion
    cacheU16 (gsub, 0); // minorVersion
    cacheU16 (gsub, 10); // scriptListOffset
    cacheU16 (gsub, 34); // featureListOffset
    cacheU16 (gsub, 36); // lookupListOffset
    { // ScriptList table
        cacheU16 (gsub, 2); // scriptCount
        { // scriptRecords[0]
            cacheBytes (gsub, "DFLT", 4); // scriptTag
            cacheU16 (gsub, 14); // scriptOffset
        }
        { // scriptRecords[1]
            cacheBytes (gsub, "thai", 4); // scriptTag
            cacheU16 (gsub, 14); // scriptOffset
        }
        { // Script table
            cacheU16 (gsub, 4); // defaultLangSysOffset
            cacheU16 (gsub, 0); // langSysCount
            { // Default Language System table
                cacheU16 (gsub, 0); // lookupOrderOffset
                cacheU16 (gsub, 0); // requiredFeatureIndex
                cacheU16 (gsub, 0); // featureIndexCount
            }
        }
    }
    { // Feature List table
        cacheU16 (gsub, 0); // featureCount
    }
    { // Lookup List Table
        cacheU16 (gsub, 0); // lookupCount
    }
}

/**
    @brief Cache a string as a big-ending UTF-16 surrogate pair.

    This function encodes a UTF-8 string as a big-endian UTF-16
    surrogate pair.

    @param[in,out] buf Pointer to a Buffer struct to update.
    @param[in] str The character array to encode.
*/
void
cacheStringAsUTF16BE (Buffer *buf, const char *str)
{
    for (const char *p = str; *p; p++)
    {
        byte c = *p;
        if (c < 0x80)
        {
            cacheU16 (buf, c);
            continue;
        }
        int length = 1;
        byte mask = 0x40;
        for (; c & mask; mask >>= 1)
            length++;
        if (length == 1 || length > 4)
            fail ("Ill-formed UTF-8 sequence.");
        uint_fast32_t codePoint = c & (mask - 1);
        for (int i = 1; i < length; i++)
        {
            c = *++p;
            if ((c & 0xc0) != 0x80) // NUL checked here
                fail ("Ill-formed UTF-8 sequence.");
            codePoint = (codePoint << 6) | (c & 0x3f);
        }
        const int lowerBits = length==2 ? 7 : length==3 ? 11 : 16;
        if (codePoint >> lowerBits == 0)
            fail ("Ill-formed UTF-8 sequence."); // sequence should be shorter
        if (codePoint >= 0xd800 && codePoint <= 0xdfff)
            fail ("Ill-formed UTF-8 sequence.");
        if (codePoint > 0x10ffff)
            fail ("Ill-formed UTF-8 sequence.");
        if (codePoint > 0xffff)
        {
            cacheU16 (buf, 0xd800 | (codePoint - 0x10000) >> 10);
            cacheU16 (buf, 0xdc00 | (codePoint & 0x3ff));
        }
        else
            cacheU16 (buf, codePoint);
    }
}

/**
    @brief Fill a "name" font table.

    The "name" table contains name information, for example for Name IDs.

    @param[in,out] font The Font struct to which to add the table.
    @param[in] names List of NameStrings.
*/
void
fillNameTable (Font *font, NameStrings nameStrings)
{
    Buffer *name = newBuffer (2048);
    addTable (font, "name", name);
    size_t nameStringCount = 0;
    for (size_t i = 0; i < MAX_NAME_IDS; i++)
        nameStringCount += !!nameStrings[i];
    cacheU16 (name, 0); // version
    cacheU16 (name, nameStringCount); // count
    cacheU16 (name, 2 * 3 + 12 * nameStringCount); // storageOffset
    Buffer *stringData = newBuffer (1024);
    // nameRecord[]
    for (size_t i = 0; i < MAX_NAME_IDS; i++)
    {
        if (!nameStrings[i])
            continue;
        size_t offset = countBufferedBytes (stringData);
        cacheStringAsUTF16BE (stringData, nameStrings[i]);
        size_t length = countBufferedBytes (stringData) - offset;
        if (offset > U16MAX || length > U16MAX)
            fail ("Name strings are too long.");
        // Platform ID 0 (Unicode) is not well supported.
        // ID 3 (Windows) seems to be the best for compatibility.
        cacheU16 (name, 3); // platformID = Windows
        cacheU16 (name, 1); // encodingID = Unicode BMP
        cacheU16 (name, 0x0409); // languageID = en-US
        cacheU16 (name, i); // nameID
        cacheU16 (name, length); // length
        cacheU16 (name, offset); // stringOffset
    }
    cacheBuffer (name, stringData);
    freeBuffer (stringData);
}

/**
    @brief Print program version string on stdout.

    Print program version if invoked with the "--version" option,
    and then exit successfully.
*/
void
printVersion () {
    printf ("hex2otf (GNU Unifont) %s\n", VERSION);
    printf ("Copyright \u00A9 2022 \u4F55\u5FD7\u7FD4 (He Zhixiang)\n");
    printf ("License GPLv2+: GNU GPL version 2 or later\n");
    printf ("<https://gnu.org/licenses/gpl.html>\n");
    printf ("This is free software: you are free to change and\n");
    printf ("redistribute it.  There is NO WARRANTY, to the extent\n");
    printf ("permitted by law.\n");

    exit (EXIT_SUCCESS);
}

/**
    @brief Print help message to stdout and then exit.

    Print help message if invoked with the "--help" option,
    and then exit successfully.
*/
void
printHelp () {
    printf ("Synopsis: hex2otf <options>:\n\n");
    printf ("    hex=<filename>        Specify Unifont .hex input file.\n");
    printf ("    pos=<filename>        Specify combining file. (Optional)\n");
    printf ("    out=<filename>        Specify output font file.\n");
    printf ("    format=<f1>,<f2>,...  Specify font format(s); values:\n");
    printf ("                             cff\n");
    printf ("                             cff2\n");
    printf ("                             truetype\n");
    printf ("                             blank\n");
    printf ("                             bitmap\n");
    printf ("                             gpos\n");
    printf ("                             gsub\n");
    printf ("\nExample:\n\n");
    printf ("    hex2otf hex=Myfont.hex out=Myfont.otf format=cff\n\n");
    printf ("For more information, consult the hex2otf(1) man page.\n\n");

    exit (EXIT_SUCCESS);
}

/**
   @brief Data structure to hold options for OpenType font output.

   This data structure holds the status of options that can be
   specified as command line arguments for creating the output
   OpenType font file.
*/
typedef struct Options
{
    bool truetype, blankOutline, bitmap, gpos, gsub;
    int cff; // 0 = no CFF outline; 1 = use 'CFF' table; 2 = use 'CFF2' table
    const char *hex, *pos, *out; // file names
    NameStrings nameStrings; // indexed directly by Name IDs
} Options;

/**
    @brief Match a command line option with its key for enabling.

    @param[in] operand A pointer to the specified operand.
    @param[in] key Pointer to the option structure.
    @param[in] delimeter The delimiter to end searching.
    @return Pointer to the first character of the desired option.
*/
const char *
matchToken (const char *operand, const char *key, char delimiter)
{
    while (*key)
        if (*operand++ != *key++)
            return NULL;
    if (!*operand || *operand++ == delimiter)
        return operand;
    return NULL;
}

/**
    @brief Parse command line options.

        Option         Data Type      Description
        ------         ---------      -----------
        truetype       bool           Generate TrueType outlines
        blankOutline   bool           Generate blank outlines
        bitmap         bool           Generate embedded bitmap
        gpos           bool           Generate a dummy GPOS table
        gsub           bool           Generate a dummy GSUB table
        cff            int            Generate CFF 1 or CFF 2 outlines
        hex            const char *   Name of Unifont .hex file
        pos            const char *   Name of Unifont combining data file
        out            const char *   Name of output font file
        nameStrings    NameStrings    Array of TrueType font Name IDs

    @param[in] argv Pointer to array of command line options.
    @return Data structure to hold requested command line options.
*/
Options
parseOptions (char *const argv[const])
{
    Options opt = {0}; // all options default to 0, false and NULL
    const char *format = NULL;
    struct StringArg
    {
        const char *const key;
        const char **const value;
    } strArgs[] =
    {
        {"hex", &opt.hex},
        {"pos", &opt.pos},
        {"out", &opt.out},
        {"format", &format},
        {NULL, NULL} // sentinel
    };
    for (char *const *argp = argv + 1; *argp; argp++)
    {
        const char *const arg = *argp;
        struct StringArg *p;
        const char *value = NULL;
        if (strcmp (arg, "--help") == 0)
            printHelp ();
        if (strcmp (arg, "--version") == 0)
            printVersion ();
        for (p = strArgs; p->key; p++)
            if ((value = matchToken (arg, p->key, '=')))
                break;
        if (p->key)
        {
            if (!*value)
                fail ("Empty argument: '%s'.", p->key);
            if (*p->value)
                fail ("Duplicate argument: '%s'.", p->key);
            *p->value = value;
        }
        else // shall be a name string
        {
            char *endptr;
            unsigned long id = strtoul (arg, &endptr, 10);
            if (endptr == arg || id >= MAX_NAME_IDS || *endptr != '=')
                fail ("Invalid argument: '%s'.", arg);
            endptr++; // skip '='
            if (opt.nameStrings[id])
                fail ("Duplicate name ID: %lu.", id);
            opt.nameStrings[id] = endptr;
        }
    }
    if (!opt.hex)
        fail ("Hex file is not specified.");
    if (opt.pos && opt.pos[0] == '\0')
        opt.pos = NULL; // Position file is optional. Empty path means none.
    if (!opt.out)
        fail ("Output file is not specified.");
    if (!format)
        fail ("Format is not specified.");
    for (const NamePair *p = defaultNames; p->str; p++)
        if (!opt.nameStrings[p->id])
            opt.nameStrings[p->id] = p->str;
    bool cff = false, cff2 = false;
    struct Symbol
    {
        const char *const key;
        bool *const found;
    } symbols[] =
    {
        {"cff", &cff},
        {"cff2", &cff2},
        {"truetype", &opt.truetype},
        {"blank", &opt.blankOutline},
        {"bitmap", &opt.bitmap},
        {"gpos", &opt.gpos},
        {"gsub", &opt.gsub},
        {NULL, NULL} // sentinel
    };
    while (*format)
    {
        const struct Symbol *p;
        const char *next = NULL;
        for (p = symbols; p->key; p++)
            if ((next = matchToken (format, p->key, ',')))
                break;
        if (!p->key)
            fail ("Invalid format.");
        *p->found = true;
        format = next;
    }
    if (cff + cff2 + opt.truetype + opt.blankOutline > 1)
        fail ("At most one outline format can be accepted.");
    if (!(cff || cff2 || opt.truetype || opt.bitmap))
        fail ("Invalid format.");
    opt.cff = cff + cff2 * 2;
    return opt;
}

/**
   @brief The main function.

   @param[in] argc The number of command-line arguments.
   @param[in] argv The array of command-line arguments.
   @return EXIT_FAILURE upon fatal error, EXIT_SUCCESS otherwise.
*/
int
main (int argc, char *argv[])
{
    initBuffers (16);
    atexit (cleanBuffers);
    Options opt = parseOptions (argv);
    Font font;
    font.tables = newBuffer (sizeof (Table) * 16);
    font.glyphs = newBuffer (sizeof (Glyph) * MAX_GLYPHS);
    readGlyphs (&font, opt.hex);
    sortGlyphs (&font);
    enum LocaFormat loca = LOCA_OFFSET16;
    uint_fast16_t maxPoints = 0, maxContours = 0;
    pixels_t xMin = 0;
    if (opt.pos)
        positionGlyphs (&font, opt.pos, &xMin);
    if (opt.gpos)
        fillGposTable (&font);
    if (opt.gsub)
        fillGsubTable (&font);
    if (opt.cff)
        fillCFF (&font, opt.cff, opt.nameStrings);
    if (opt.truetype)
        fillTrueType (&font, &loca, &maxPoints, &maxContours);
    if (opt.blankOutline)
        fillBlankOutline (&font);
    if (opt.bitmap)
        fillBitmap (&font);
    fillHeadTable (&font, loca, xMin);
    fillHheaTable (&font, xMin);
    fillMaxpTable (&font, opt.cff, maxPoints, maxContours);
    fillOS2Table (&font);
    fillNameTable (&font, opt.nameStrings);
    fillHmtxTable (&font);
    fillCmapTable (&font);
    fillPostTable (&font);
    organizeTables (&font, opt.cff);
    writeFont (&font, opt.cff, opt.out);
    return EXIT_SUCCESS;
}
