###################################################
#
# Makefile for libid3tag.so
#
###################################################

C_SRCS =
C_OPTIONS=

CPP_SRCS= \
	../src/writers.cpp \
	../src/utils.cpp \
	../src/tag_render.cpp \
	../src/tag_parse_v1.cpp \
	../src/tag_parse_musicmatch.cpp \
	../src/tag_parse_lyrics3.cpp \
	../src/tag_parse.cpp \
	../src/tag_impl.cpp \
	../src/tag_find.cpp \
	../src/tag_file.cpp \
	../src/tag.cpp \
	../src/spec.cpp \
	../src/readers.cpp \
	../src/mp3_parse.cpp \
	../src/misc_support.cpp \
	../src/io_helpers.cpp \
	../src/io_decorators.cpp \
	../src/io.cpp \
	../src/helpers.cpp \
	../src/header_tag.cpp \
	../src/header_frame.cpp \
	../src/header.cpp \
	../src/globals.cpp \
	../src/frame_render.cpp \
	../src/frame_parse.cpp \
	../src/frame_impl.cpp \
	../src/frame.cpp \
	../src/field_string_unicode.cpp \
	../src/field_string_ascii.cpp \
	../src/field_integer.cpp \
	../src/field_binary.cpp \
	../src/field.cpp \
	../src/c_wrapper.cpp

CPP_OPTIONS=-DHAVE_CONFIG_H -shared -fPIC

INCLUDE = . .. ../src ../include ../include/id3 ../../../source/Shared

# when cross-compiling target does not have Zlib
ifneq ($(XTARGET),)
	INCLUDE+= \
		../../zlib-1.2.3/$(XTARGET)/include 
endif

DYNLIB=libid3tag.so

all: ../config.h $(LIBRARY)

include ../../../jumper/source/common.mak


#
# Build the parts of id3lib
#

../config.h: ../config_Linux32.h
	cp ../config_Linux32.h ../config.h


