// $Id: tag_file.cpp,v 1.43 2003/03/02 14:14:08 t1mpy Exp $

// id3lib: a C++ library for creating and manipulating id3v1/v2 tags
// Copyright 1999, 2000  Scott Thomas Haug
// Copyright 2002 Thijmen Klok (thijmen@id3lib.org)

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

#include <stdio.h>  //for BUFSIZ and functions remove & rename
#include "writers.h"
#include "io_strings.h"
#include "tag_impl.h" //has <stdio.h> "tag.h" "header_tag.h" "frame.h" "field.h" "spec.h" "id3lib_strings.h" "utils.h"

using namespace dami;

#if !defined HAVE_MKSTEMP
#  include <stdio.h>
#endif

#if defined HAVE_UNISTD_H
#  include <unistd.h>
#endif

#if defined HAVE_SYS_STAT_H
#  include <sys/stat.h>
#endif

#if defined WIN32 //Klenotic
#  include <io.h>
#endif
#if defined WIN32 && (!defined(WINCE))
#  include <windows.h>
static int truncate(const char *path, size_t length)
{
  int result = -1;
  HANDLE fh;

  fh = ::CreateFileA(path,
                    GENERIC_WRITE | GENERIC_READ,
                    0,
                    NULL,
                    OPEN_EXISTING,
                    FILE_ATTRIBUTE_NORMAL,
                    NULL);

  if(INVALID_HANDLE_VALUE != fh)
  {
    SetFilePointer(fh, length, NULL, FILE_BEGIN);
    SetEndOfFile(fh);
    CloseHandle(fh);
    result = 0;
  }

  return result;
}

// prevents a weird error I was getting compiling this under windows
#  if defined CreateFile
#    undef CreateFile
#  endif

#elif defined(WINCE)
// Createfile is apparently to defined to CreateFileW. (Bad Bad Bad), so we
// work around it by converting path to Unicode
#  include <windows.h>
static int truncate(const char *path, size_t length)
{
  int result = -1;
  wchar_t wcTempPath[256];
  mbstowcs(wcTempPath,path,255);
  HANDLE fh;
  fh = ::CreateFile(wcTempPath,
                    GENERIC_WRITE | GENERIC_READ,
                    0,
                    NULL,
                    OPEN_EXISTING,
                    FILE_ATTRIBUTE_NORMAL,
                    NULL);

  if (INVALID_HANDLE_VALUE != fh)
  {
    SetFilePointer(fh, length, NULL, FILE_BEGIN);
    SetEndOfFile(fh);
    CloseHandle(fh);
    result = 0;
  }

  return result;
}

#elif defined(macintosh)

static int truncate(const char *path, size_t length)
{
   /* not implemented on the Mac */
   return -1;
}

#endif

#ifdef WIN32
size_t ID3_TagImpl::Link(const wchar_t *fileInfo, bool parseID3v1, bool parseLyrics3)
#else
size_t ID3_TagImpl::Link(const char *fileInfo, bool parseID3v1, bool parseLyrics3)
#endif
{
  flags_t tt = ID3TT_NONE;
  if (parseID3v1)
  {
    tt |= ID3TT_ID3V1;
  }
  if (parseLyrics3)
  {
    tt |= ID3TT_LYRICS;
  }
  return this->Link(fileInfo, tt);
}

#ifdef WIN32
size_t ID3_TagImpl::Link(const wchar_t *fileInfo, flags_t tag_types)
#else
size_t ID3_TagImpl::Link(const char *fileInfo, flags_t tag_types)
#endif
{
  _tags_to_parse.set(tag_types);

  if (NULL == fileInfo)
  {
    return 0;
  }

  _file_name = fileInfo;
  _changed = true;

  this->ParseFile();

  return this->GetPrependedBytes();
}

// used for streaming:
size_t ID3_TagImpl::Link(ID3_Reader &reader, flags_t tag_types)
{
  _tags_to_parse.set(tag_types);

#ifdef WIN32
  _file_name = L"";
#else
  _file_name = "";
#endif
  _changed = true;

  this->ParseReader(reader);

  return this->GetPrependedBytes();
}

size_t RenderV1ToFile(ID3_TagImpl& tag, fstream& file)
{
  if (!file)
  {
    return 0;
  }

  // Heck no, this is stupid.  If we do not read in an initial V1(.1)
  // header then we are constantly appending new V1(.1) headers. Files
  // can get very big that way if we never overwrite the old ones.
  //  if (ID3_V1_LEN > tag.GetAppendedBytes())   - Daniel Hazelbaker
  if (ID3_V1_LEN > tag.GetFileSize())
  {
    file.seekp(0, ios::end);
  }
  else
  {
    // We want to check if there is already an id3v1 tag, so we can write over
    // it.  First, seek to the beginning of any possible id3v1 tag
    file.seekg(0-ID3_V1_LEN, ios::end);
    char sID[ID3_V1_LEN_ID];

    // Read in the TAG characters
    file.read(sID, ID3_V1_LEN_ID);

    // If those three characters are TAG, then there's a preexisting id3v1 tag,
    // so we should set the file cursor so we can overwrite it with a new tag.
    if (memcmp(sID, "TAG", ID3_V1_LEN_ID) == 0)
    {
      file.seekp(0-ID3_V1_LEN, ios::end);
    }
    // Otherwise, set the cursor to the end of the file so we can append on
    // the new tag.
    else
    {
      file.seekp(0, ios::end);
    }
  }

  ID3_IOStreamWriter out(file);

  id3::v1::render(out, tag);

  return ID3_V1_LEN;
}

//Klenotic: The RewriteFile() function assists RenderV2ToFile() and ID3_TagImpl::Strip().  It uses
//          C File IO for fast performance and adds code to properly copy file permissions on 
//          Windows systems.  
size_t RewriteFile(const ID3_TagImpl& tag, const char* tagData, const size_t tagSize)
{
	ID3D_NOTICE( "RewriteFile: starting" );

	bool bSuccess = false;

#ifdef WIN32
	_ASSERT(false);
	return false;
#else
	String filename = tag.GetFileName();
	String sTmpSuffix = ".XXXXXX";

	char* sTempFile = new char[filename.size() + sTmpSuffix.size() + 1];
	strcpy(sTempFile, filename.c_str());
	strcat(sTempFile, sTmpSuffix.c_str());

#if defined(WIN32)
	_mktemp(sTempFile);	
#elif defined(HAVE_MKTEMP)
	mktemp(sTempFile);
#endif

	FILE* fileIn = fopen(filename.c_str(), "r+b"); // Technically, we don't need to open this with write access.  However, to the user, this is supposed to be an in-place update.  To be consistent with that model, we'll open the source file with write access.
	if (fileIn)
	{
		FILE* tmpOut = fopen(sTempFile, "w+b");
		if (tmpOut)
		{
			// Begin Write Tag //
			size_t ioResult = 0;
			if ( (tagData) && (tagSize > 0) )
				ioResult = fwrite(tagData, sizeof(char), tagSize, tmpOut);
			// End Write Tag //

			// Begin Write File Data //
			if (ioResult == tagSize) // The tag was written correctly
			{
				unsigned char tmpBuffer[BUFSIZ] = {0};
				fseek(fileIn, tag.GetPrependedBytes(), SEEK_SET);
				while (!feof(fileIn))
				{
					size_t nBytes = fread(tmpBuffer, sizeof(unsigned char), BUFSIZ, fileIn);
					fwrite(tmpBuffer, sizeof(unsigned char), nBytes, tmpOut);
				}

				if (!ferror(fileIn) && !ferror(tmpOut))
					bSuccess = true;
			}
			// End Write File Data //

			fclose(tmpOut);
		}

		fclose(fileIn);
	}

	if (bSuccess)
	{
		bSuccess = false;

		// the following sets the permissions of the new file
		// to be the same as the original
#if defined(WIN32)
		DWORD dwLength = 0;
		GetFileSecurityA(filename.c_str(), DACL_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | OWNER_SECURITY_INFORMATION, NULL, 0, &dwLength);
		SECURITY_DESCRIPTOR* pSecurityDescriptor = reinterpret_cast<SECURITY_DESCRIPTOR*>(new char[dwLength + 1]);
		if (GetFileSecurityA(filename.c_str(), DACL_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | OWNER_SECURITY_INFORMATION, pSecurityDescriptor, dwLength, &dwLength))
			SetFileSecurityA(sTempFile, DACL_SECURITY_INFORMATION | GROUP_SECURITY_INFORMATION | OWNER_SECURITY_INFORMATION, pSecurityDescriptor);
		delete[] pSecurityDescriptor;
#elif defined(HAVE_SYS_STAT_H)
		struct stat fileStat;
		if(stat(filename.c_str(), &fileStat) == 0)
		{
#endif //defined(HAVE_SYS_STAT_H)

			// Begin File Replacement //
			if (remove(filename.c_str()) == 0)
			{
				rename(sTempFile, filename.c_str());
				bSuccess = true; // This must set this to true even if the rename fails.  The source file has already been deleted.  If the success flag is not set, the temp file will be removed as well.
			}
			// End File Replacement //

#if defined(HAVE_SYS_STAT_H)
	#if !defined(WIN32)
			chmod(filename.c_str(), fileStat.st_mode);
		}
	#endif
#endif //defined(HAVE_SYS_STAT_H)
	}
	
	if (!bSuccess)
		remove(sTempFile);

	delete[] sTempFile;

	return bSuccess ? tagSize : -1;
#endif // WIN32
}

//Klenotic: This is the modified version of the RenderV2ToFile function.
size_t RenderV2ToFile(const ID3_TagImpl& tag, fstream& file)
{
#ifdef WIN32
	_ASSERT(false);
	return false;
#else
	ID3D_NOTICE( "RenderV2ToFile: starting" );
	if (!file)
	{
		ID3D_WARNING( "RenderV2ToFile: error in file" );
		return 0;
	}

	String tagString;
	io::StringWriter writer(tagString);
	id3::v2::render(writer, tag);
	ID3D_NOTICE( "RenderV2ToFile: rendered v2" );

	const char* tagData = tagString.data();
	size_t tagSize = tagString.size();
	// if the new tag fits perfectly within the old and the old one
	// actually existed (ie this isn't the first tag this file has had)
	if ((!tag.GetPrependedBytes() && !ID3_GetDataSize(tag)) ||
		(tagSize == tag.GetPrependedBytes()))
	{
		file.seekp(0, ios::beg);
		file.write(tagData, tagSize);
	}
	else
	{
		file.close(); // We need to close the fstream file to gain access to the file.
		tagSize = RewriteFile(tag, tagData, tagSize);
		if (tagSize == -1)
			tagSize = 0;

		file.clear();//to clear the eof mark
		openWritableFile(tag.GetFileName(), file); // The code we're returning to expects the file to be open.
	}

	return tagSize;
#endif
}

flags_t ID3_TagImpl::Update(flags_t ulTagFlag)
{
#ifdef WIN32
	_ASSERT(false);
	return false;
#else
  flags_t tags = ID3TT_NONE;

  fstream file;
  String filename = this->GetFileName();
  ID3_Err err = openWritableFile(filename, file);
  _file_size = getFileSize(file);

  if (err == ID3E_NoFile)
  {
    err = createFile(filename, file);
  }
  if (err == ID3E_ReadOnly)
  {
    return tags;
  }

  if ((ulTagFlag & ID3TT_ID3V2) && this->HasChanged())
  {
    _prepended_bytes = RenderV2ToFile(*this, file);
    if (_prepended_bytes)
    {
      tags |= ID3TT_ID3V2;
    }
  }

  if ((ulTagFlag & ID3TT_ID3V1) &&
      (!this->HasTagType(ID3TT_ID3V1) || this->HasChanged()))
  {
    size_t tag_bytes = RenderV1ToFile(*this, file);
    if (tag_bytes)
    {
      // only add the tag_bytes if there wasn't an id3v1 tag before
      if (! _file_tags.test(ID3TT_ID3V1))
      {
        _appended_bytes += tag_bytes;
      }
      tags |= ID3TT_ID3V1;
    }
  }
  _changed = false;
  _file_tags.add(tags);
  _file_size = getFileSize(file);
  file.close();
  return tags;
#endif
}

//Klenotic: this is the modified version of ID3_TagImpl::Strip.
// returns 0 on failure or ulTagFlag on success.
flags_t ID3_TagImpl::Strip(flags_t ulTagFlag)
{
#ifdef WIN32
	_ASSERT(false);
	return false;
#else
	flags_t ulTags = ID3TT_NONE;
	const size_t data_size = ID3_GetDataSize(*this);

	// First remove the prepended tag(s), if requested.
	if ( (ulTagFlag & ID3TT_PREPENDED) && (_file_tags.get() & ID3TT_PREPENDED) )
	{
		size_t tagSize = RewriteFile(*this, NULL, 0);
		if (tagSize == -1)
			return 0;

		ulTags |= _file_tags.get() & ID3TT_PREPENDED;
	}

	// Then remove the appended tag(s), if requested.
	if ( (ulTagFlag & ID3TT_APPENDED) && (_file_tags.get() & ID3TT_APPENDED) )
	{
		size_t nNewFileSize = data_size;

		ulTags |= _file_tags.get() & ID3TT_APPENDED;

		if (!((ulTagFlag & ID3TT_PREPENDED) && (_file_tags.get() & ID3TT_PREPENDED)))
		{
			// add the original prepended tag size since we don't want to delete it,
			// and the new file size represents the file size _not_ counting the ID3v2
			// tag
			nNewFileSize += this->GetPrependedBytes();
		}
		//else
		//{
		//	// If we're stripping the ID3v2 tag, there's no need to adjust the new
		//	// file size, since it doesn't account for the ID3v2 tag size
		//}
		
		if (ulTags && (truncate(_file_name.c_str(), nNewFileSize) == -1))
		{
			// log this
			return 0;
			//ID3_THROW(ID3E_NoFile);
		}
	}

	_prepended_bytes = (ulTags & ID3TT_PREPENDED) ? 0 : _prepended_bytes;
	_appended_bytes  = (ulTags & ID3TT_APPENDED)  ? 0 : _appended_bytes;
	_file_size = data_size + _prepended_bytes + _appended_bytes;

	_changed = _file_tags.remove(ulTags) || _changed;

	return ulTagFlag;
#endif // WIN32
}
