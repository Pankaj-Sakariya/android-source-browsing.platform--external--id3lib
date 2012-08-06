// $Id: field_string_unicode.cpp,v 1.33 2003/03/02 14:23:58 t1mpy Exp $

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

#include "field_impl.h"
#include "id3/utils.h" // has <config.h> "id3/id3lib_streams.h" "id3/globals.h" "id3/id3lib_strings.h"
#include "io_helpers.h"

using namespace dami;

/** \fn ID3_Field& ID3_Field::operator=(const unicode_t*)
 ** \brief Shortcut for the Set operator.
 ** Performs similarly as operator=(const char*), taking a unicode_t
 ** string as a parameter rather than an ascii string.
 ** \sa Set(const unicode_t*)
 ** \param string The string to assign to the field
 **/

/** \brief Copies the supplied unicode string to the field.
 **
 ** Performs similarly as the ASCII Set() method, taking a unicode_t string
 ** as a parameter rather than an ascii string.
 **
 ** \param string The unicode string to set this field to.
 ** \sa Add(const unicode_t*)
 **/
size_t ID3_FieldImpl::Set(const unicode_t* data)
{
  if (GetType() == ID3FTY_TEXTSTRING &&
      ID3TE_IS_DOUBLE_BYTE_ENC(GetEncoding()) &&
      data)
  {
    String str((const char*) data, ucslen(data) * 2);
    Clear();
    return SetText( str, 0, GetEncoding() );
  }
  else
    return 0;
}

size_t ID3_FieldImpl::Add(const unicode_t* data)
{
  if (GetType() == ID3FTY_TEXTSTRING &&
      ID3TE_IS_DOUBLE_BYTE_ENC(GetEncoding()) &&
      data)
  {
    String str((const char*) data, ucslen(data) * 2);
    return SetText( str, GetNumTextItems(), GetEncoding() );
  }
  else
    return 0;
}



const unicode_t* ID3_FieldImpl::GetRawUnicodeText() const
{
  return (unicode_t *)GetRawTextItem(0);
}


const unicode_t* ID3_FieldImpl::GetRawUnicodeTextItem(size_t index) const
{
  const unicode_t* text = NULL;
  if (GetType() == ID3FTY_TEXTSTRING &&
      GetEncoding() == ID3TE_UNICODE &&
      index < this->GetNumTextItems())
  {
    String unicode = _text + '\0' + '\0';
    text = (unicode_t *) unicode.data();
    for (size_t i = 0; i < index; ++i)
    {
      text += ucslen(text) + 1;
    }
  }
  return text;
}


/** Returns the text of this field (if it really is a textfield) in the
 ** requested encoding.
 **
 **
 ** \return The text of this field converted to the requested encoding.
 **         If the field is not a textfield or the text cannot be converted
 **         the function returns a nullpointer.
 **         Warning: in case of a unicode text the string, c_str is only terminated with one null char.
 **         use string.size.
 **
 ** \param enc   The requested encoding.
 **
 ** TODO: a return value of an empty string can mean different things, e.g. emtpy string, no such item, unable to convert, ...
 **/
String ID3_FieldImpl::GetText( size_t index, ID3_TextEnc enc ) const
{
  const char* text = GetRawTextItem(index);
  if( !text )
    return String("");

  size_t len = GetRawTextItemLen(index);

  String sText( text, len );
  return convert( sText, GetEncoding(), enc );
}


/** Set the text of this field (if it really is a textfield)
 **
 ** \param data  The new text to be set. Note that unicode strings should just be put into the (char) string.
 ** \param enc   The source encoding.
 ** \return      The number of bytes set to the text
 **
 **/
size_t ID3_FieldImpl::SetText( String data, size_t index, ID3_TextEnc enc )
{
  if( GetType() != ID3FTY_TEXTSTRING )
    return 0;

  if( index > _num_items )
    return 0;

  // there are no fields with fixed size and multiple fields or fancy encoding.
  if( _fixed_size != 0 &&
      (index>0 || GetEncoding()!=ID3TE_ISO8859_1) )
    return 0;

  String str = convert( data, enc, GetEncoding() );

  // fixed size (always first item, always ISO8859_1)
  if( _fixed_size != 0 ) {
    _text = String(str, 0, _fixed_size);
    if( str.size()<_fixed_size )
      _text.append( _fixed_size - str.size(), '\0' );

  // no fixed size
  } else {

    // printf("Debug: SetText0: numItems: %d, index: %d %d, %s\n", _num_items, index, _text.size(), _text.c_str() );
    String newText;

    // add old items in front
    for( size_t i=0; i<index; i++ ) {
      if( i>0 ) {
        newText += '\0';
        if ( ID3TE_IS_DOUBLE_BYTE_ENC(this->GetEncoding()) )
          newText += '\0';
      }
      newText.append( GetRawTextItem(i) );
    }

    // printf("Debug: SetText1: %d, %s\n", newText.size(), newText.c_str() );
    // add new item
    if( index>0 ) {
      newText += '\0';
      if ( ID3TE_IS_DOUBLE_BYTE_ENC(this->GetEncoding()) )
        newText += '\0';
    }
    newText.append( str );
    // printf("Debug: SetText2: %d, %s\n", newText.size(), newText.c_str() );

    // add old items in back (if any)
    for( size_t i=index+1; i<_num_items; i++ ) {
      if( i>0 ) {
        newText += '\0';
        if ( ID3TE_IS_DOUBLE_BYTE_ENC(this->GetEncoding()) )
          newText += '\0';
      }
      newText.append( GetRawTextItem(i) );
    }

    _text = newText;
  }

  _changed = true;
  if( index >= _num_items )
    _num_items++;

  // printf("Debug: SetText end: items: %d\n", _num_items );

  return GetRawTextItemLen( index );
}

size_t ID3_FieldImpl::Get(unicode_t* buffer, size_t maxLength) const
{
  return Get( buffer, maxLength, 0 );
}

/** Copies the contents of the field into the supplied buffer, up to the
 ** number of characters specified; for fields with multiple entries, the
 ** optional third parameter indicates which of the fields to retrieve.
 **
 ** Performs similarly as the ASCII Get(char *, size_t, size_t) method, taking
 ** a unicode_t string as a parameter rather than an ascii string.  The
 ** maxChars parameter still represents the maximum number of characters, not
 ** bytes.
 **
 ** \code
 **   unicode_t myBuffer[1024];
 **   size_t charsUsed = myFrame.GetField(ID3FN_UNICODE)->Get(buffer, 1024);
 ** \endcode
 **
 ** \param buffer   Where the field's data is copied to
 ** \param maxChars The maximum number of characters to copy to the buffer.
 ** \param itemNum  For fields with multiple items (such as the involved
 **                 people frame, the item number to retrieve.
 ** \sa Get(char *, size_t, size_t)
 **/
size_t ID3_FieldImpl::Get(unicode_t *buffer, size_t maxLength, size_t index) const
{
  size_t length = 0;

  if ( ID3TE_IS_DOUBLE_BYTE_ENC(GetEncoding()) &&
       buffer != NULL &&
       maxLength > 0 )
  {
    const unicode_t* text = (unicode_t*)GetRawTextItem(index);
    if( text )
    {
      size_t itemLen = GetRawTextItemLen(index)/2;
      length = dami::min(maxLength, itemLen);

      ::memcpy(buffer, text, length * 2);
      if (length < maxLength)
        buffer[length] = NULL_UNICODE;
    }
  }

  return length;
}


