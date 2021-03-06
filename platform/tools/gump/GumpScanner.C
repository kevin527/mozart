//
// Author:
//   Leif Kornstaedt <kornstae@ps.uni-sb.de>
//
// Copyright:
//   Leif Kornstaedt, 1996-1998
//
// Last change:
//   $Date$ by $Author$
//   $Revision$
//
// This file is part of Mozart, an implementation of Oz 3:
//   http://www.mozart-oz.org
//
// See the file "LICENSE" or
//   http://www.mozart-oz.org/LICENSE.html
// for information on usage and redistribution
// of this file, and for a DISCLAIMER OF ALL
// WARRANTIES.
//

// For use with C++ scanner classes generated by GNU Flex Version 2.5.3

#include <FlexLexer.h>
#include <string.h>

#include "mozart.h"

#define YY_BUF_SIZE 16384
#define YY_END_OF_BUFFER_CHAR 0

typedef unsigned int yy_size_t;

struct yy_buffer_state {
  FILE *yy_input_file;
  char *yy_ch_buf;
  char *yy_buf_pos;
  yy_size_t yy_buf_size;
  int yy_n_chars;
  int yy_is_our_buffer;
  int yy_is_interactive;
  int yy_at_bol;
  int yy_fill_buffer;

  int yy_buffer_status;
#define YY_BUFFER_NEW 0
#define YY_BUFFER_NORMAL 1
#define YY_BUFFER_EOF_PENDING 2
};

#define OZ_declareBufferIN(ARG,VAR)			\
 OZ_declareForeignPointer(ARG,_Tmp);			\
 yy_buffer_state *VAR = (yy_buffer_state *) _Tmp;

static void init_buffer(yy_buffer_state *p) {
  p->yy_buf_pos = &p->yy_ch_buf[0];
  p->yy_is_our_buffer = 1;
  p->yy_is_interactive = 0;
  p->yy_at_bol = 1;
  p->yy_buffer_status = YY_BUFFER_NEW;
}

OZ_BI_define(gump_createFromFile, 1,1)
{
  OZ_declareVirtualString(0, file);

  FILE *f = fopen(file, "rb");
  if (f == NULL) {
    return OZ_raiseErrorC("gump", 2, OZ_atom("fileNotFound"), OZ_in(0));
  }

  yy_buffer_state *p = new yy_buffer_state;
  p->yy_input_file = f;
  p->yy_ch_buf = new char[YY_BUF_SIZE + 2];
  p->yy_ch_buf[0] = YY_END_OF_BUFFER_CHAR;
  p->yy_ch_buf[1] = YY_END_OF_BUFFER_CHAR;
  p->yy_buf_size = YY_BUF_SIZE;
  p->yy_n_chars = 0;
  p->yy_fill_buffer = 1;
  init_buffer(p);

  OZ_RETURN(OZ_makeForeignPointer(p));
}
OZ_BI_end

OZ_BI_define(gump_createFromVirtualString, 1,1)
{
  OZ_declareVirtualString(0, s);

  yy_size_t size = strlen(s);

  yy_buffer_state *p = new yy_buffer_state;
  p->yy_input_file = 0;
  p->yy_ch_buf = new char[size + 2];
  strcpy(p->yy_ch_buf, s);
  p->yy_ch_buf[size] = YY_END_OF_BUFFER_CHAR;
  p->yy_ch_buf[size + 1] = YY_END_OF_BUFFER_CHAR;
  p->yy_buf_size = size;
  p->yy_n_chars = size;
  p->yy_fill_buffer = 0;
  init_buffer(p);

  OZ_RETURN(OZ_makeForeignPointer(p));
}
OZ_BI_end

OZ_BI_define(gump_setInteractive, 2,0)
{
  OZ_declareBufferIN(0, p);
  OZ_declareInt(1, b);
  p->yy_is_interactive = b;
  return PROCEED;
}
OZ_BI_end

OZ_BI_define(gump_getInteractive, 1,1)
{
  OZ_declareBufferIN(0, p);
  OZ_RETURN_INT(p->yy_is_interactive);
}
OZ_BI_end

OZ_BI_define(gump_setBOL, 2,0)
{
  OZ_declareBufferIN(0, p);
  OZ_declareInt(1, b);
  p->yy_at_bol = b;
  return PROCEED;
}
OZ_BI_end

OZ_BI_define(gump_getBOL, 1,1)
{
  OZ_declareBufferIN(0, p);
  OZ_RETURN_INT(p->yy_at_bol);
}
OZ_BI_end

OZ_BI_define(gump_close, 1,0)
{
  // Must never be invoked twice on the same foreign pointer!
#if 0
  // Makes the emulator crash under Linux??
  OZ_declareBufferIN(0, p);

  if (p->yy_input_file)
    fclose(p->yy_input_file);
  if (p->yy_is_our_buffer)
    delete[] p->yy_ch_buf;
  delete p;
#endif

  return PROCEED;
}
OZ_BI_end

extern "C" OZ_C_proc_interface *oz_init_module(void);

char oz_module_name[] = "GumpScanner";

OZ_C_proc_interface *oz_init_module(void) {
  static OZ_C_proc_interface oz_interface[] = {
    {"createFromFile",1,1,gump_createFromFile},
    {"createFromVirtualString",1,1,gump_createFromVirtualString},
    {"setInteractive",2,0,gump_setInteractive},
    {"getInteractive",1,1,gump_getInteractive},
    {"setBOL",2,0,gump_setBOL},
    {"getBOL",1,1,gump_getBOL},
    {"close",1,0,gump_close},
    {0,0,0,0}
  };

  return oz_interface;
}
