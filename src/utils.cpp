/*$off*/
// $Id: utils.cpp,v 1.26 2002/07/02 22:04:36 t1mpy Exp $

// id3lib: a C++ library for creating and manipulating id3v1/v2 tags
// Copyright 1999, 2000  Scott Thomas Haug

// This library is free software; you can redistribute it and/or modify it
// under the terms of the GNU Library General Public License as published by
// the Free Software Foundation; either version 2 of the License, or (at your
// option) any later version.
//
// This library is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library General Public
// License for more details.
//
// You should have received a copy of the GNU Library General Public License
// along with this library; if not, write to the Free Software Foundation,
// Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

// The id3lib authors encourage improvements and optimisations to be sent to
// the id3lib coordinator.  Please see the README file for details on where to
// send such submissions.  See the AUTHORS file for a list of people who have
// contributed to id3lib.  See the ChangeLog file for a list of changes to
// id3lib.  These files are distributed with id3lib at
// http://download.sourceforge.net/id3lib/
/*$on*/
#include <ctype.h>
#include <errno.h>
#include "id3/utils.h"  // has <config.h> "id3/id3lib_streams.h" "id3/globals.h" "id3/id3lib_strings.h"

#if (defined(__GNUC__) && __GNUC__ == 2)
#define NOCREATE    ios::nocreate
#else
#define NOCREATE    ((std::ios_base::openmode) 0)
#endif
#ifdef WIN32
#include <windows.h>
#else
#define min(a, b)   (a < b ? a : b)
#endif
using namespace dami;

// ESL: The original code is complicated, but maybe supports a wider range of platforms? The changes hereafter
// leverage Unicode utilities and known shortcuts between encodings.
#include "../unicode.org/ConvertUTF.c"

/* =====================================================================================================================
 ======================================================================================================================= */
inline bool _legalTagChar(int c)
{
    // NULL, TAB, CR, LF and anything higher or equal to space is legal
    return !c || (c >= 0x20) || (c == 0x09) || (c == 0x0a) || (c == 0x0d);
}

/* =====================================================================================================================
 ======================================================================================================================= */
String Utf8FromLatin1String(const char *input, size_t src_sz)
{
    String              convertedString("");

    // check the input
    const unsigned char *src_buf = (const unsigned char *) input;
    if(!src_buf || !src_sz) {
        return convertedString;
    }

    // Allocate buffer and perform conversion. Since input is Latin1 it never expands to more than 2 octets
    size_t          dest_sz = src_sz * 2;
    unsigned char   *dest_buf = new unsigned char[dest_sz + 2];
    unsigned char   *dest_ptr = dest_buf;

    int             c = (int) *((const unsigned char *) src_buf);

    while(c && src_sz--) {
        if(c >= 128) {
            *dest_ptr++ = (char) (((c & 0x7c0) >> 6) | 0xc0);
            *dest_ptr++ = (char) ((c & 0x3f) | 0x80);
        }
        else if(!_legalTagChar(c)) {
            *dest_ptr++ = '?';
        }
        else {
            *dest_ptr++ = (char) c;
        }

        c = (int) *((const unsigned char *)++src_buf);
    }

    size_t  length = dest_ptr - dest_buf;
    if(length) {
        dest_buf[length] = dest_buf[length + 1] = '\0';
        convertedString = String((const char *) dest_buf);
    }

    delete[] dest_buf;
    return convertedString;
}

/* =====================================================================================================================
 ======================================================================================================================= */
String Utf8FromUtf16String(const unicode_t *input, size_t src_sz)
{
    String      convertedString("");

    // check the input
    const UTF16 *src_buf = (const UTF16 *) input;
    if(!src_buf || !src_sz || ((src_sz % 2) != 0)) {
        return convertedString;
    }

    // allocate buffer and perform conversion. At most each UCS2 expands to 3 octets in UTF8, but since this is
    // physical length in octet alreay
    size_t              dest_sz = (size_t) (((float) src_sz) * 1.5) + 1;
    unsigned char       *dest_buf = new unsigned char[dest_sz + 2];
    unsigned char       *dest_ptr = dest_buf;
    ConversionResult    result = ConvertUTF16toUTF8((const UTF16 **) &src_buf, ((const UTF16 *) src_buf) + (src_sz / sizeof(UTF16)), (UTF8 **) &dest_ptr,
                                                    ((UTF8 *) dest_buf) + dest_sz, strictConversion);

    // On success set the string in _data
    if(result == conversionOK) {
        size_t  length = dest_ptr - dest_buf;   // physical length
        dest_buf[length] = dest_buf[length + 1] = '\0';
        convertedString = String((const char *) dest_buf);
    }

    delete[] dest_buf;
    return convertedString;
}

/* =====================================================================================================================
 ======================================================================================================================= */
String Latin1FromUtf8String(const char *input, size_t src_sz)
{
    String              convertedString("");

    // check the input
    const unsigned char *src_buf = (const unsigned char *) input;
    if(!src_buf || !src_sz) {
        return convertedString;
    }

    // allocate buffer. The latin1 string will not be larger than the UTF-8 one
    unsigned char   *dest_buf = new unsigned char[src_sz + 2];
    unsigned char   *dest_ptr = dest_buf;

    // perform conversion
    int             c = (int) *src_buf;
    while(c && src_sz--) {
        if(c >= 128) {
            int c2 = (int) * ++src_buf;
            if(!c2) {
                break;  // We reached the end of string
            }

            // conversion rule: 110yyyyy(C2-DF) 10zzzzzz(80-BF);
            // However we really only care about 2 of the 'y'
            *dest_ptr++ = (unsigned char) (((c & 0x3) << 6) + (c2 & 0x3F));

            // Decrement counter since we consumed one more character
            src_sz--;
        }
        else if(!_legalTagChar(c)) {
            *dest_ptr++ = (unsigned char) '?';
        }
        else {
            *dest_ptr++ = (unsigned char) c;
        }

        c = (int) * ++src_buf;
    }

    size_t  length = dest_ptr - dest_buf;
    dest_buf[length] = dest_buf[length + 1] = '\0';
    convertedString = String((const char *) dest_buf);

    delete[] dest_buf;
    return convertedString;
}

/* =====================================================================================================================
 ======================================================================================================================= */
String Latin1FromUtf16String(const unicode_t *input, size_t src_sz)
{
    String      convertedString("");

    // check the input
    const UTF16 *src_buf = (const UTF16 *) input;
    if(!src_buf || !src_sz || ((src_sz % 2) != 0)) {
        return convertedString;
    }

    // Each UCS2 character hanles one Latin1 character. But takes half the physical space
    size_t          dest_sz = src_sz / sizeof(UTF16);
    size_t          length = src_sz / sizeof(UTF16);
    unsigned char   *dest_buf = new unsigned char[dest_sz + 2];
    unsigned char   *dest_ptr = dest_buf;
    while(dest_sz--) {
        unsigned char   c = (unsigned char) (0x00FF & (*src_buf++));
        if(!_legalTagChar(c)) {
            *dest_ptr++ = '?';
        }
        else {
            *dest_ptr++ = c;
        }
    }

    dest_buf[length] = dest_buf[length + 1] = '\0';
    convertedString = String((const char *) dest_buf);

    delete[] dest_buf;
    return convertedString;
}

/* =====================================================================================================================
 ======================================================================================================================= */
String Utf16FromLatin1String(const char *input, size_t src_sz)
{
    String              convertedString("");

    // check the input
    const unsigned char *src_buf = (const unsigned char *) input;
    if(!src_buf || !src_sz) {
        return convertedString;
    }

    // allocate the buffer
    size_t          length = src_sz * sizeof(UTF16);
    unsigned char   *dest_buf = new unsigned char[length + 2];
    UTF16           *dest_ptr = (UTF16 *) dest_buf;
    while(src_sz--) {
        if(!_legalTagChar(*src_buf)) {
            *dest_ptr++ = (UTF16) '?';
            src_buf++;
        }
        else {
            *dest_ptr++ = (UTF16) * src_buf++;
        }
    }

    dest_buf[length] = dest_buf[length + 1] = '\0';
    convertedString = String((const char *) dest_buf, length + 2);

    delete[] dest_buf;
    return convertedString;
}

/* =====================================================================================================================
 ======================================================================================================================= */
String Utf16FromUtf8String(const char *input, size_t src_sz)
{
    String              convertedString("");

    // check the input
    const unsigned char *src_buf = (const unsigned char *) input;
    if(!src_buf || !src_sz) {
        return convertedString;
    }

    // allocate buffer and perform conversion
    size_t              dest_sz = src_sz;
    unsigned char       *dest_buf = new unsigned char[(dest_sz + 1) * sizeof(UTF16)];
    unsigned char       *dest_ptr = dest_buf;
    ConversionResult    result = ConvertUTF8toUTF16((const UTF8 **) &src_buf, (const UTF8 *) (src_buf + src_sz), (UTF16 **) &dest_ptr,
                                                    ((UTF16 *) dest_buf) + dest_sz, strictConversion);

    // On success set the string in _data
    if(result == conversionOK) {
        size_t  length = dest_ptr - dest_buf;   // physical length
        dest_buf[length] = dest_buf[length + 1] = '\0';
        convertedString = String((const char *) dest_buf, length + 2);
    }

    delete[] dest_buf;
    return convertedString;
}

/* =====================================================================================================================
 ======================================================================================================================= */
size_t dami::renderNumber(uchar *buffer, uint32 val, size_t size)
{
    uint32  num = val;
    for(size_t i = 0; i < size; i++) {
        buffer[size - i - 1] = (uchar) (num & MASK8);
        num >>= 8;
    }

    return size;
}

/* =====================================================================================================================
 ======================================================================================================================= */
String dami::renderNumber(uint32 val, size_t size)
{
    String  str(size, '\0');
    uint32  num = val;
    for(size_t i = 0; i < size; i++) {
        str[size - i - 1] = (uchar) (num & MASK8);
        num >>= 8;
    }

    return str;
}

// =====================================================================================================================
//    this should be convert( const String&, ...) to be changed soon...
// =====================================================================================================================
String dami::convert(String data, ID3_TextEnc sourceEnc, ID3_TextEnc targetEnc)
{
    if((sourceEnc == targetEnc) || (data.size() == 0)) {
        return String(data);
    }

    switch(sourceEnc) {
        case ID3TE_ISO8859_1:
            {
                size_t  len = data.length();
                if(targetEnc == ID3TE_UTF8) {
                    return Utf8FromLatin1String(data.c_str(), len);
                }
                else if(targetEnc == ID3TE_UTF16) {
                    return Utf16FromLatin1String(data.c_str(), len);
                }
            }

            break;

        case ID3TE_UTF8:
            {
                size_t  len = data.length();
                if(targetEnc == ID3TE_ISO8859_1) {
                    return Latin1FromUtf8String(data.c_str(), len);
                }
                else if(targetEnc == ID3TE_UTF16) {
                    return Utf16FromUtf8String(data.c_str(), len);
                }
            }

            break;

        case ID3TE_UTF16:
            {
                size_t  len = ucslen((const unicode_t *) data.c_str()) * sizeof(unicode_t);
                if(targetEnc == ID3TE_ISO8859_1) {
                    return Latin1FromUtf16String((unicode_t *) data.c_str(), len);
                }
                else if(targetEnc == ID3TE_UTF8) {
                    return Utf8FromUtf16String((unicode_t *) data.c_str(), len);
                }
            }

            break;
    }

    return "";
}

/* =====================================================================================================================
 ======================================================================================================================= */
size_t dami::ucslen(const unicode_t *unicode)
{
    if(NULL != unicode) {
        for(size_t size = 0; true; size++) {
            if(NULL_UNICODE == unicode[size]) {
                return size;
            }
        }
    }

    return 0;
}

namespace
{

    /* =================================================================================================================
     =================================================================================================================== */
    bool exists(String name)
    {
        ifstream    file(name.c_str(), NOCREATE);
        return file.is_open() != 0;
    }
};

/* =====================================================================================================================
 ======================================================================================================================= */
ID3_Err dami::createFile(String name, fstream &file)
{
    if(file.is_open()) {
        file.close();
    }

    file.open(name.c_str(), ios::in | ios::out | ios::binary | ios::trunc);
    if(!file) {
        return ID3E_ReadOnly;
    }

    return ID3E_NoError;
}

/* =====================================================================================================================
 ======================================================================================================================= */
size_t dami::getFileSize(fstream &file)
{
    size_t  size = 0;
    if(file.is_open()) {
        streamoff   curpos = file.tellg();
        file.seekg(0, ios::end);
        size = file.tellg();
        file.seekg(curpos);
    }

    return size;
}

/* =====================================================================================================================
 ======================================================================================================================= */
size_t dami::getFileSize(ifstream &file)
{
    size_t  size = 0;
    if(file.is_open()) {
        streamoff   curpos = file.tellg();
        file.seekg(0, ios::end);
        size = file.tellg();
        file.seekg(curpos);
    }

    return size;
}

/* =====================================================================================================================
 ======================================================================================================================= */
size_t dami::getFileSize(ofstream &file)
{
    size_t  size = 0;
    if(file.is_open()) {
        streamoff   curpos = file.tellp();
        file.seekp(0, ios::end);
        size = file.tellp();
        file.seekp(curpos);
    }

    return size;
}

/* =====================================================================================================================
 ======================================================================================================================= */
ID3_Err dami::openWritableFile(String name, fstream &file)
{
    if(!exists(name)) {
        return ID3E_NoFile;
    }

    if(file.is_open()) {
        file.close();
    }

    file.open(name.c_str(), ios::in | ios::out | ios::binary | NOCREATE);
    if(!file) {
        return ID3E_ReadOnly;
    }

    return ID3E_NoError;
}

/* =====================================================================================================================
 ======================================================================================================================= */
ID3_Err dami::openWritableFile(String name, ofstream &file)
{
    if(!exists(name)) {
        return ID3E_NoFile;
    }

    if(file.is_open()) {
        file.close();
    }

    file.open(name.c_str(), ios::in | ios::out | ios::binary | NOCREATE);
    if(!file) {
        return ID3E_ReadOnly;
    }

    return ID3E_NoError;
}

/* =====================================================================================================================
 ======================================================================================================================= */
ID3_Err dami::openReadableFile(String name, fstream &file)
{
    if(file.is_open()) {
        file.close();
    }

    file.open(name.c_str(), ios::in | ios::binary | NOCREATE);
    if(!file) {
        return ID3E_NoFile;
    }

    return ID3E_NoError;
}

#ifdef WIN32
ID3_Err dami::  openReadableFile(std::wstring name, ifstream &file)
#else
ID3_Err dami::openReadableFile(String name, ifstream &file)
#endif
{
    if(file.is_open()) {
        file.close();
    }

    file.open(name.c_str(), ios::in | ios::binary | NOCREATE);

    if(!file) {
        return ID3E_NoFile;
    }

    return ID3E_NoError;
}

// =====================================================================================================================
//    wow, did we really had to implement string by ourself (ralf)
// =====================================================================================================================
String dami::toString(uint32 val)
{
    if(val == 0) {
        return "0";
    }

    String  text;
    while(val > 0) {
        String  tmp;
        char    ch = (val % 10) + '0';
        tmp += ch;
        text = tmp + text;
        val /= 10;
    }

    return text;
}
