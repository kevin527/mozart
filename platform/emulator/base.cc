/*
 *  Authors:
 *    Michael Mehl (mehl@dfki.de)
 *
 *  Contributors:
 *    Ralf Scheidhauer (Ralf.Scheidhauer@ps.uni-sb.de)
 *    Tobias Mueller (tmueller@ps.uni-sb.de)
 *
 *  Copyright:
 *    Organization or Person (Year(s))
 *
 *  Last change:
 *    $Date$ by $Author$
 *    $Revision$
 *
 *  This file is part of Mozart, an implementation
 *  of Oz 3:
 *     http://mozart.ps.uni-sb.de
 *
 *  See the file "LICENSE" or
 *     http://mozart.ps.uni-sb.de/LICENSE.html
 *  for information on usage and redistribution
 *  of this file, and for a DISCLAIMER OF ALL
 *  WARRANTIES.
 *
 */

#if defined(INTERFACE) && !defined(PEANUTS)
#pragma implementation "base.hh"
#endif

#include "base.hh"

#include "ozconfig.hh"
#include "os.hh"

#include <stdarg.h>
#include <errno.h>
#include <stdio.h>

void prefixError()
{
  if (ozconf.runningUnderEmacs)
    fputc(MSG_ERROR,stderr);
}

void prefixWarning()
{
  if (ozconf.runningUnderEmacs)
    fputc(MSG_WARN,stderr);
}

extern char *AMVersion, *AMDate, *ozplatform;

void OZ_error(const char *format, ...)
{
  va_list ap;

  va_start(ap,format);

  if (ozconf.runningUnderEmacs)
    prefixError();
  else
    fprintf(stderr, "\a");

#ifdef DEBUG_CHECK
  fprintf(stderr, "\n(going to report an error in pid %d)", osgetpid());
#endif
  fprintf(stderr, "\n*** Internal Error: "
#ifndef DEBUG_CHECK
          "Please send a bug report to oz-bugs@ps.uni-sb.de ***\n"
          "*** with the following information:\n"
          "*** version:  %s\n"
          "*** platform: %s\n"
          "*** date:     %s\n\n",
          AMVersion,ozplatform,AMDate
#endif
           );
  vfprintf(stderr,format,ap);
  fprintf( stderr, "\n");

  va_end(ap);

  osStackDump();

  DebugCheckT(osUnblockSignals());

  //
  // send a signal to all forked processes, including the emulator itself
  DebugCode(sleep(30));
  oskill(0,ozconf.dumpCore?SIGQUIT:SIGUSR1);
}

void OZ_warning(const char *format, ...)
{
  va_list ap;

  va_start(ap,format);

  prefixWarning();

  fprintf( stderr, "*** Warning: ");
  vfprintf(stderr,format,ap);
  fprintf( stderr, "\n");

  va_end(ap);
}

void ozperror(const char *msg)
{
  OZ_error("UNIX ERROR: %s: %s",msg,OZ_unixError(errno));
}

void ozpwarning(const char *msg)
{
  OZ_warning("UNIX ERROR: %s: %s",msg,OZ_unixError(errno));
}


void message(const char *format, ...)
{
  va_list ap;

  va_start(ap,format);

  fprintf( stdout,"*** ");
  vfprintf(stdout,format,ap);
  fflush(  stdout);

  va_end(ap);
}

//
// kost@ : very quick hack to kill emulators that are lost...
Bool isDeadSTDOUT()
{
  char *buf = "\n";
  fflush(stdout);
  if (write(fileno(stdout), buf, 1) == -1)
    return (TRUE);
  else
    return (FALSE);
}


void errorTrailer()
{
  message("----------------------------------------\n\n");
  fflush(stderr);
}

void errorHeader()
{
  printf("\n");
  prefixError();
  message("****************************************\n");
}

/*
 * destructively replace 'from' with 'to' in 's'
 */
char *replChar(char *s,char from,char to) {
  for (char *help = s; *help != '\0'; help++) {
    if (*help == from) {
      *help = to;
    }
  }
  return s;
}

/*
 * destructively delete a character from string
 */
char *delChar(char *s,char c) {
  char *p = s;
  char *help;
  for (help = s; *help != '\0'; help++) {
    if (*help != c) {
      *p++=*help;
    }
  }
  *p=*help; /* don't forget the \0 */
  return s;
}
