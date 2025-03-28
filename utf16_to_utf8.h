/* 
 * The MIT License (MIT)
 *
 * Copyright (c) 2025 rppicomidi
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */
#pragma once
#include <stdint.h>
#include <stddef.h>
/// @brief convert the UTF-16 string from a USB string descriptor to a UTF-8 C-string
///
/// Only works with single 16-bit word src characters. U+0000 (NULL) is
/// treated as a string terminator in the src string. On return dest is NULL terminated.
/// See https://www.unicode.org/versions/Unicode16.0.0/core-spec/chapter-23/#G20365.
/// For conversion, see
/// See https://www.unicode.org/versions/Unicode16.0.0/core-spec/chapter-3/#G7404
/// UTF-16 encoding that is not well formed emits U+FFFD See
/// see https://www.unicode.org/versions/Unicode16.0.0/core-spec/chapter-3/#G2155
/// https://www.unicode.org/versions/Unicode16.0.0/core-spec/chapter-5/#G40630
///
/// For easier to understand references, see
/// https://en.wikipedia.org/wiki/UTF-8 and https://en.wikipedia.org/wiki/UTF-16
/// @param src points to an array of UTF-16 encoded code units; may be NULL terminated.
///        It is important that the byte order of the src array match the endian
///        encoding order of the machine using this function or else this function
///        will not work correctly. If there is a Byte Order Mark U+FEFF in src[0],
///        it will be skipped and not encoded.
/// @param maxsrc is at least as large as number of code units in the src array; if it is larger, then
///        the array must be NULL terminated
/// @param dest points to an array of bytes for storing the UTF-8 encoded string. It is NULL terminated
/// @param maxdest is the maximum number of bytes that can be stored in the dest buffer, including the NULL termination
/// @note if maxdest is set larger than the dest memory buffer size, bad things happen.
/// @todo This function is more generally useful than just this class. Make this a separate repository.
static void utf16ToUtf8(uint16_t* src, size_t maxsrc, uint8_t* dest, size_t maxdest) {
  size_t destidx = 0;
  size_t srcidx = 0; // The first word contains the string length in word and the descriptor type
  if (srcidx < maxsrc && src[srcidx] == 0xFEFF) {
    ++srcidx; // ignore Byte Order Mark
  }
  for(;;) {
    // assume a 0 word in the src array is a null termination
    if (srcidx >= maxsrc || src[srcidx] == 0 || (destidx+1) >= maxdest) {
      dest[destidx] = 0;
      break;
    }
    else if (src[srcidx] < 0x80) {
      dest[destidx++] = src[srcidx++] & 0x7f;
    }
    else if (src[srcidx] < 0x800) {
      if ((destidx+2) >= maxdest) {
        // no room for a 2-byte code
        dest[destidx] = 0;
        break;
      }
      else {
        dest[destidx++] = 0xC0 | ((src[srcidx] >> 6) & 0x1f);
        dest[destidx++] = 0x80 | (src[srcidx++] & 0x3f);
      }
    }
    else if (src[srcidx] < 0xD800 || src[srcidx] >= 0xE000) {
      if ((destidx+3) >= maxdest) {
        // no room for a 3-byte code
        dest[destidx] = 0;
        break;
      }
      else {
        dest[destidx++] = 0xE0 | ((src[srcidx] >> 12) & 0xf);
        dest[destidx++] = 0x80 | ((src[srcidx] >> 6) & 0x3f);
        dest[destidx++] = 0x80 | (src[srcidx++] & 0x3f);
      }
    }
    else if ((srcidx+1) < maxsrc) {
      // should be paired surrogate
      if ((destidx + 4) < maxdest) {
        // There is enough room to decode a paired surrogate
        if (src[srcidx] >= 0xD800 && src[srcidx] < 0xDC00 &&
            src[srcidx+1] >= 0xDC00 && src[srcidx+1] < 0xE000) {
          // There is a well-formed surrogate pair

          // compute the 32-bit UTF code point
          uint32_t upper = src[srcidx++] & 0x3FF;
          uint32_t lower = src[srcidx++] & 0x3FF;
          uint32_t code = ((upper << 10) | lower) + 0x10000;
          // Convert to UTF-8
          dest[destidx++] = 0xF0 | ((code >> 18) & 0x07);
          dest[destidx++] = 0x80 | ((code >> 12) & 0x3f);
          dest[destidx++] = 0x80 | ((code >> 6) & 0x3f);
          dest[destidx++] = 0x80 | (code & 0x3f);
        }
        else {
          // Unpaired surrogate value; encode with replacement character U+FFFD
          // and attempt to keep going
          dest[destidx++] = 0xEF;
          dest[destidx++] = 0xBF;
          dest[destidx++] = 0xBD;
          ++srcidx;
        }
      }
      else {
        // Not enough room to decode the surrogate pair. Give up
        dest[destidx] = 0;
        break;
      }
    }
    else {
      // Last code unit is not part of a paired surrogate, so it is not well formed UTF-16LE.
      if ((destidx + 3) < maxdest) {
        // There is space to encode the last code unit as U+FFFD
        dest[destidx++] = 0xEF;
        dest[destidx++] = 0xBF;
        dest[destidx++] = 0xBD;
      }
      // Last code unit, so done
      dest[destidx] = 0;
      break;
    }
  }
}
