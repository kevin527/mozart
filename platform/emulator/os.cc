/*
 *  Authors:
 *    Michael Mehl (mehl@dfki.de)
 *    Ralf Scheidhauer (Ralf.Scheidhauer@ps.uni-sb.de)
 *
 *  Contributors:
 *    Christian Schulte (schulte@dfki.de)
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

// Unix/Posix functions

#if defined(INTERFACE)
#pragma implementation "os.hh"
#endif

#include "wsock.hh"

#include "os.hh"
#include "value.hh"
#include "ozconfig.hh"

#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef HAVE_DLOPEN

#ifdef HAVE_DLFCN_H
#include <dlfcn.h>
#else
extern "C" void * dlopen(char *, int);
extern "C" char * dlerror(void);
extern "C" void * dlsym(void *, char *);
extern "C" int dlclose(void *);
#endif

#if !defined(RTLD_NOW)
#define RTLD_NOW 1
#endif

#if !defined(RTLD_GLOBAL)
#define RTLD_GLOBAL 0
#endif

#endif

#ifdef IRIX
#include <bstring.h>
#include <sys/time.h>
#endif

#ifdef HPUX_700
#include <dl.h>
#endif

#if defined(FOPEN_MAX) && !defined(OPEN_MAX)
#define OPEN_MAX  FOPEN_MAX
#endif

#ifdef MAX_OPEN
#define OPEN_MAX  MAX_OPEN
#endif

#ifdef WINDOWS
#include <time.h>
#include <sys/time.h>
#include <process.h>

#ifndef __MINGW32__
extern "C" int _fmode;
extern "C" void setmode(int,mode_t);
#endif

#else
#include <sys/times.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/utsname.h>
#endif

#include <fcntl.h>

#if !defined(ultrix) && !defined(WINDOWS)
# include <sys/socket.h>
#endif

#ifdef AIX3_RS6000
#include <sys/select.h>
#endif

static long emulatorStartTime = 0;

fd_set socketFDs;
static int maxSocket = 0;

inline
Bool isSocket(int fd)
{
  return (FD_ISSET(fd,&socketFDs));
}

#ifdef WINDOWS
int runningUnderNT()
{
  static int underNT = -1;
  if (underNT==-1) {
    OSVERSIONINFO vi;
    vi.dwOSVersionInfoSize = sizeof(vi);
    BOOL b = GetVersionExA(&vi);
    Assert(b==TRUE);
    underNT = (vi.dwPlatformId==VER_PLATFORM_WIN32_NT);
  }
  return underNT;
}


typedef long long verylong;

verylong fileTimeToMS(FILETIME *ft)
{
  verylong x1 = ((verylong)(unsigned int)ft->dwHighDateTime)<<32;
  verylong x2 = x1 + (unsigned int)ft->dwLowDateTime;
  verylong ret = x2 / 10000;
  return ret;
}


FILETIME emuStartTime;

/* return time since start in milli seconds */
unsigned int getTime()
{
  SYSTEMTIME st;
  GetSystemTime(&st);
  FILETIME ft;
  SystemTimeToFileTime(&st,&ft);
  return fileTimeToMS(&ft)-fileTimeToMS(&emuStartTime);
}


#endif


// return current usertime in milliseconds
unsigned int osUserTime()
{
#ifdef WINDOWS
  if (!runningUnderNT()) {
    return getTime();
  }

  /* only NT supports this */
  FILETIME ct,et,kt,ut;
  GetProcessTimes(GetCurrentProcess(),&ct,&et,&kt,&ut);
  return (unsigned int) fileTimeToMS(&ut);

#else
  struct tms buffer;

  times(&buffer);
  return (unsigned int)(buffer.tms_utime*1000.0/(double)sysconf(_SC_CLK_TCK));
#endif
}

// return current systemtime in milliseconds
unsigned int osSystemTime()
{
#ifdef WINDOWS
  if (!runningUnderNT()) {
    return 0;
  }

  /* only NT supports this */
  FILETIME ct,et,kt,ut;
  GetProcessTimes(GetCurrentProcess(),&ct,&et,&kt,&ut);
  return (unsigned int) fileTimeToMS(&kt);

#else
  struct tms buffer;

  times(&buffer);
  return (unsigned int)(buffer.tms_stime*1000.0/(double)sysconf(_SC_CLK_TCK));
#endif
}



unsigned int osTotalTime()
{
#if defined(WINDOWS)
  return getTime();
#else


#ifdef SUNOS_SPARC

  struct timeval tp;

  (void) gettimeofday(&tp, NULL);

  return (unsigned int) ((tp.tv_sec - emulatorStartTime) * 1000 +
                         tp.tv_usec / 1000);


#else

  struct tms buffer;
  int t = times(&buffer) - emulatorStartTime;
  return (unsigned int) (t*1000.0/(double)sysconf(_SC_CLK_TCK));

#endif

#endif
}

#ifdef WINDOWS
class TimerThread {
public:
  HANDLE thrd;
  int wait;
  TimerThread(int w);
};


static TimerThread *timerthread = NULL;

#endif


void osBlockSignals(Bool check)
{
#ifdef WINDOWS
  if (timerthread)
    SuspendThread(timerthread->thrd);
#else
  sigset_t s,sOld;
  sigfillset(&s);

  /* some signals should not be blocked */
  sigdelset(&s,SIGINT);
  sigdelset(&s,SIGHUP);
  sigdelset(&s,SIGTERM);

  sigprocmask(SIG_SETMASK,&s,&sOld);

#ifdef DEBUG_CHECK
  if (check) {
    sigemptyset(&s);
    if (memcmp(&s,&sOld,sizeof(sigset_t)) != 0) {
      OZ_warning("blockSignals: there are blocked signals");
    }
  }
#endif
#endif
}

void osUnblockSignals()
{
#ifdef WINDOWS
  if (timerthread)
    ResumeThread(timerthread->thrd);
#else
  sigset_t s;
  sigemptyset(&s);
  sigprocmask(SIG_SETMASK,&s,NULL);
#endif
}


/* Oz version of signal(2)
   NOTE: (mm 13.10.94)
    Linux & Solaris are not POSIX compatible:
     therefor we need casts to (OsSigFun *). look at HERE.
    */

typedef void OsSigFun(void);

static
OsSigFun *osSignal(int signo, OsSigFun *fun)
{
#ifdef WINDOWS
  signal(signo,(void(*)(int))fun);
  return NULL;
#else
  struct sigaction act, oact;

  /* type of act.sa_handler ist not the same on all platforms,
   * therefore this quite intricateness */
  OsSigFun **f = (OsSigFun**)&act.sa_handler;
  *f = fun;
  sigemptyset(&act.sa_mask);
  act.sa_flags = 0;

  /* The following piece of code is from Stevens: Advanced UNIX Programming */
  if (signo == SIGALRM || signo == SIGUSR2) {
#ifdef SA_INTERUPT /* SunOS */
    act.sa_flags |= SA_INTERUPT;
#endif
  } else {
#ifdef SA_RESTART /* Sys V */
    act.sa_flags |= SA_RESTART;
#endif
  }
  if (sigaction(signo,&act,&oact) <0) {
    /* HERE */
    return (OsSigFun *) SIG_ERR;
  }
  /* HERE */
  return (OsSigFun *) oact.sa_handler;
#endif
}



/* Oz version of system(3)
 * Posix 2 requires this call not to be interuptible by a signal
 * however some OS (like Solaris 2.3) do return EINTR, so we define our
 * own one, stolen from Stevens.0
 */


int osSystem(char *cmd)
{
#ifdef WINDOWS
  return system(cmd);
#else
  if (cmd == NULL) {
    return 1;
  }

  pid_t pid;
  if ((pid = fork()) < 0) {
    return -1;
  }

  if (pid == 0) {
    execl("/bin/sh","sh","-c",cmd, (char*) NULL);
    _exit(127);     /* execl error */
  }

  int status;
  while(waitpid(pid,&status,0) < 0) {
    if (errno != EINTR) {
      return -1;
    }
  }

  return status;
#endif
}


#if !defined(__GNUC__) || defined(CCMALLOC)
// Linking object files compiled with gcc needs these:

/* void * operator new (size_t sz) */
extern "C" void * __builtin_new (size_t sz)
{
  void *p;

  /* malloc (0) is unpredictable; avoid it.  */
  if (sz == 0)
    sz = 1;
  p = (void *) malloc (sz);
  return p;
}

extern "C" void * __builtin_vec_new (size_t sz)
{
  return __builtin_new(sz);
}

/* void operator delete (void *ptr) */
extern "C" void __builtin_delete (void *ptr)
{
  if (ptr)
    free (ptr);
}

extern "C" void __builtin_vec_delete (void *ptr)
{
  __builtin_delete(ptr);
}

#endif  /* __GNUC__ */



#ifdef HPUX_700
#include <time.h>
#endif


#ifdef WINDOWS

const int wrappedHDStart = 1000; /* hope that's enough (RS) */

class WrappedHandle {
public:
  static int nextno;
  static WrappedHandle *allHandles;
  HANDLE hd;
  int fd;
  WrappedHandle *next;
  WrappedHandle(HANDLE h)
  {
    hd = h;
    fd = nextno++;
    next = allHandles;
    allHandles = this;
  }

  static WrappedHandle *find(int f)
  {
    WrappedHandle *aux = allHandles;
    while(aux) {
      if (f == aux->fd)
        return aux;
      aux = aux->next;
    }
    return 0;
  }

  static WrappedHandle *getHandle(HANDLE hd)
  {
    WrappedHandle *aux = allHandles;
    while (aux) {
      if (aux->hd==0) {
        aux->hd = hd;
        return aux;
      }
      aux = aux->next;
    }
    return new WrappedHandle(hd);
  }

  void close()
  {
    CloseHandle(hd);
    hd = 0;
  }
};

int WrappedHandle::nextno = wrappedHDStart;

WrappedHandle *WrappedHandle::allHandles = NULL;


int rawread(int fd, void *buf, int sz)
{
  if (isSocket(fd))
    return recv(fd,((char*)buf),sz,0);

  if (fd < wrappedHDStart)
    return read(fd,buf,sz);


  HANDLE hd = WrappedHandle::find(fd)->hd;
  Assert(hd!=0);
  unsigned int ret;
  if (ReadFile(hd,buf,sz,&ret,0)==FALSE)
    return -1;

  return ret;
}


int rawwrite(int fd, void *buf, int sz)
{
  if (isSocket(fd))
    return send(fd, (char *)buf, sz, 0);

  if (fd < wrappedHDStart)
    return write(fd,buf,sz);

  HANDLE hd = WrappedHandle::find(fd)->hd;
  Assert(hd!=0);

  unsigned int ret;
  if (WriteFile(hd,buf,sz,&ret,0)==FALSE)
    return -1;
  return ret;
}

Bool createReader(int fd);

int _hdopen(int handle, int flags)
{
  WrappedHandle *wh = WrappedHandle::getHandle((HANDLE)handle);
  if ((flags&O_WRONLY)==0) {
    createReader(wh->fd);
  }
  return wh->fd;
}

#else

#define rawwrite(fd,buf,sz) write(fd,buf,sz)
#define rawread(fd,buf,sz) read(fd,buf,sz)

#endif

static
int nonBlockSelect(int nfds, fd_set *readfds, fd_set *writefds)
{
  struct timeval timeout;
  timeout.tv_sec = 0;
  timeout.tv_usec = 0;
#ifdef HPUX_700
  return select(nfds,(int*)readfds,(int*)writefds,NULL,&timeout);
#else
  return select(nfds,readfds,writefds,NULL,&timeout);
#endif
}



#include "winselect.cc"

void registerSocket(int fd)
{
  OZ_FD_SET(fd,&socketFDs);
  maxSocket = max(fd,maxSocket);
}



#ifdef WINDOWS

unsigned __stdcall timerFun(void *p)
{
  TimerThread *ti = (TimerThread*) p;
  while(1) {
    Sleep(ti->wait);
    handlerALRM();
  }
  delete ti;
  ExitThread(1);
  return 1;
}

TimerThread::TimerThread(int w)
{
  wait = w;
  unsigned tid;
  thrd = CreateThread(NULL,10000,&timerFun,this,0,&tid);
  if (thrd==NULL) {
    ozpwarning("osSetAlarmTimer(start thread)");
  }
}
#endif


// 't' is in miliseconds;
void osSetAlarmTimer(int t)
{
#ifdef DEBUG_DET
    return;
#endif

#ifdef WINDOWS

  if (timerthread==NULL) {
    unsigned tid;
    timerthread = new TimerThread(t);
  }

  if (t==0) {
    SuspendThread(timerthread->thrd);
  } else {
    timerthread->wait = t;
    ResumeThread(timerthread->thrd);
  }
#else
  struct itimerval newT;

  int sec  = t/1000;
  int usec = (t*1000)%1000000;
  newT.it_interval.tv_sec  = sec;
  newT.it_interval.tv_usec = usec;
  newT.it_value.tv_sec     = sec;
  newT.it_value.tv_usec    = usec;

  if (setitimer(ITIMER_REAL,&newT,NULL) < 0) {
    ozpwarning("setitimer");
  }
#endif
}

static long openMax;


#ifndef WINDOWS


int osGetAlarmTimer()
{
#ifdef DEBUG_DET
  return 0;
#else
  struct itimerval timer;
  if (getitimer(ITIMER_REAL,&timer) < 0) {
    ozpwarning("getitimer");
    return -1;
  }

  return (timer.it_value.tv_sec * 1000) + (timer.it_value.tv_usec/1000);
#endif
}
#endif


/*
 * Wait *timeout msecs on given fds. If 'ptimeout' is WAIT_NULL,
 * return immediately.
 * Returns number of fds ready, and writes in *timeout #msecs really
 * spent in waiting;
 */
static
int osSelect(fd_set *readfds, fd_set *writefds, unsigned int *ptimeout)
{
#ifdef WINDOWS

  return win32Select(readfds,writefds,ptimeout);

#else

  struct timeval timeoutstruct, *timeoutptr;
  unsigned int currentSystemTime;

  if (ptimeout == (unsigned int*) WAIT_NULL) {
    timeoutstruct.tv_sec = 0;
    timeoutstruct.tv_usec = 0;
    timeoutptr = &timeoutstruct;
  } else {
    int timeout = *ptimeout;
    if (timeout == 0) {
      timeoutptr = NULL;        // indefinitely;
    } else {
      timeoutstruct.tv_sec = timeout/1000;
      timeoutstruct.tv_usec = (timeout*1000)%1000000;
      timeoutptr = &timeoutstruct;
    }

    //
    currentSystemTime = osTotalTime();
    // note that the alarm clock is untouched here now;
    osUnblockSignals();
  }

  /* The prototypes for select are wrong on HP-UX 9.x */
#ifdef HPUX_700
  int ret = select(openMax,(int*)readfds,(int*)writefds,NULL,timeoutptr);
#else
  int ret = select(openMax,readfds,writefds,NULL,timeoutptr);
#endif

  if (ptimeout != (unsigned int*) WAIT_NULL) {
    // kost@ : Note that effectively the time spent in wait
    // may be greater than specified;
    *ptimeout = (int) (osTotalTime() - currentSystemTime);
    //    Assert(*ptimeout >= 0); // mm2: *ptimeout is unsigned!!!
    osBlockSignals();
  }

  return ret;
#endif  /* WINDOWS */
}

void osInitSignals()
{
#ifndef WINDOWS
  osSignal(SIGALRM,handlerALRM);
  // 'SIGUSR2' notifies about presence of tasks. Right now these are
  // only virtual site messages;
  osSignal(SIGUSR1,handlerUSR1);
  osSignal(SIGUSR2,handlerUSR2);
#endif
  // kost@ : virtual sites need to be cleaned up - otherwise some
  // shared memory pages will get stuck in the system;
#if !defined(DEBUG_DET) || defined(VIRTUALSITES)
  osSignal(SIGINT,handlerINT);
  osSignal(SIGTERM,handlerTERM);
  osSignal(SIGSEGV,handlerSEGV);
  osSignal(SIGFPE,handlerFPE);
#ifndef WINDOWS
  osSignal(SIGBUS,handlerBUS);
  osSignal(SIGPIPE,handlerPIPE);
  osSignal(SIGCHLD,handlerCHLD);
#endif
#endif
}


/*********************************************************
 *       Sockets                                         *
 *********************************************************/

static fd_set globalFDs[2];     // mask of active read/write FDs
static fd_set copyFDs[2];       // ... its copy for 'select()';

int osOpenMax()
{
#ifdef WINDOWS
  /* socket numbers may grow very large on Windows */
  return 100000;
#else
  int ret = sysconf(_SC_OPEN_MAX);
  if (ret == -1) {
    ret = _POSIX_OPEN_MAX;
  }
  return ret;
#endif
}



char *oslocalhostname()
{
#ifdef WINDOWS
  char buf[1000];
  int aux = gethostname(buf,sizeof(buf));
  if (aux < 0) {
    OZ_warning("cannot determine hostname\n");
    sprintf(buf,"%s","localhost");
  }
  return strdup(buf);
#else
  struct utsname unp;
  int n = uname(&unp);
  if(0 > n) /* braindead Solaris, returns >0 if OK. POSIX says 0 */
    return 0;
  return strdup(unp.nodename);
#endif
}


#ifdef WINDOWS


char *ostmpnam(char *s)
{
  /* make sure that temporary files are allways located under C: */
  if (s) {
    s[0] = 'C';
    s[1] = ':';
    char *aux = tmpnam(s+2);
    return aux==0 ? 0 : s;
  }

  static char tn[L_tmpnam+2];
  tn[0] = 'C';
  tn[1] = ':';
  tn[2] = 0;
  char *aux = tmpnam(NULL);
  if (aux==0)
    return 0;
  Assert(aux[1] != ':');
  strcat(tn,aux);
  return tn;
}

static int wrappedStdin = -1;

int osdup(int fd)
{
  // no dup yet: conflicts with reader threads
  return fd==STDIN_FILENO ? wrappedStdin : fd;
}

#else

char *ostmpnam(char *s) { return tmpnam(s); }

int osdup(int fd) { return dup(fd); }

#endif


void osInit()
{
  DebugCheck(CLOCK_TICK < 1000, error("CLOCK_TICK must be greater than 1 ms"));

  openMax=osOpenMax();

  FD_ZERO(&globalFDs[SEL_READ]);
  FD_ZERO(&globalFDs[SEL_WRITE]);

  FD_ZERO(&socketFDs);

#ifdef SUNOS_SPARC

  struct timeval tp;

  (void) gettimeofday(&tp, NULL);

  emulatorStartTime = tp.tv_sec;

#else
#ifdef WINDOWS

  /* make sure everything is opened in binary mode */
  setmode(fileno(stdin),O_BINARY);  // otherwise input blocks!!
  _fmode = O_BINARY;

  SYSTEMTIME st;
  GetSystemTime(&st);
  SystemTimeToFileTime(&st,&emuStartTime);

  /* allow select on stdin */
  wrappedStdin = _hdopen(STD_INPUT_HANDLE, O_RDONLY);

  /* init sockets */
  WSADATA wsa_data;
  WORD req_version = MAKEWORD(1,1);

  int ret = WSAStartup(req_version, &wsa_data);
  if (ret != 0 && ret != WSASYSNOTREADY)
    OZ_warning("Initialization of socket interface failed\n");

  //  fprintf(stderr, "szDescription = \"%s\"", wsa_data.szDescription);
  //  fprintf(stderr, "szSystemStatus = \"%s\"", wsa_data.szSystemStatus);

#else

  struct tms buffer;
  emulatorStartTime = times(&buffer);;

#endif
#endif
}

#define CheckMode(mode) Assert(mode==SEL_READ || mode==SEL_WRITE)

static
void osWatchFDInternal(int fd, int mode)
{
  CheckMode(mode);
  OZ_FD_SET(fd,&globalFDs[mode]);
  // kost@ : in the case of preventing blocking in 'select()' by
  // waiting for write permission into 'stderr' (used by task
  // checkers&handlers, see am.cc), the copy must be updated as well:
  OZ_FD_SET(fd,&copyFDs[mode]);
}


void osWatchFD(int fd, int mode)
{
  osWatchFDInternal(fd,mode);
}

void osWatchAccept(int fd)
{
  osWatchFDInternal(fd,SEL_READ);
}


void osClrWatchedFD(int fd, int mode)
{
  CheckMode(mode);
  FD_CLR(fd,&globalFDs[mode]);
}


Bool osIsWatchedFD(int fd, int mode)
{
  CheckMode(mode);
  return (FD_ISSET(fd,&globalFDs[mode]));
}


/*
 * do a select, that waits "ms".
 * if "ms" <= 0 do a blocking select
 */
void osBlockSelect(unsigned int &ms)
{
  copyFDs[SEL_READ]  = globalFDs[SEL_READ];
  copyFDs[SEL_WRITE] = globalFDs[SEL_WRITE];
  (void) osSelect(&copyFDs[SEL_READ],&copyFDs[SEL_WRITE],&ms);
}

/* osClearSocketErrors
 * remove the closed/failed descriptors from the fd_sets
 */
void osClearSocketErrors()
{
  for (int i = 0; i < openMax; i++) {
    for(int mode=SEL_READ; mode <= SEL_WRITE; mode++) {
      if (FD_ISSET(i,&globalFDs[mode]) && osTestSelect(i,mode) < 0) {
        osClrWatchedFD(i,mode);
      }
    }
  }
}

/* returns: 1 if select succeeded on fd
 *          0 did not succeed
 *         -1 on error
 */
int osTestSelect(int fd, int mode)
{
  CheckMode(mode);
#ifdef WINDOWS
  IOChannel *ch = lookupChannel(fd);
  /* no select on write */
  if (ch)
    return (mode==SEL_WRITE || ch->status==ST_AVAIL);

  if (!isSocket(fd))
    return OK;
#endif

  while(1) {
    fd_set fdset, *readFDs=NULL, *writeFDs=NULL;
    FD_ZERO(&fdset);
    OZ_FD_SET(fd,&fdset);

    if (mode==SEL_READ) {
      readFDs = &fdset;
    } else {
      writeFDs = &fdset;
    }

    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 0;
    int ret = nonBlockSelect(fd+1, readFDs, writeFDs);
    if (ret >= 0 || ossockerrno() != EINTR) {
      return ret;
    }
  }
}

/* -------------------------------------------------------------------------
 * subroutines for AM::handleIO
 * ------------------------------------------------------------------------- */

// another copy since it must be preserved between 'osFirstSelect()'
// and 'osNextSelect()';
static fd_set tmpFDs[2];

/* signals are blocked */
int osFirstSelect()
{
 loop:
  tmpFDs[SEL_READ]  = globalFDs[SEL_READ];
  tmpFDs[SEL_WRITE] = globalFDs[SEL_WRITE];

  int numbOfFDs = osSelect(&tmpFDs[SEL_READ], &tmpFDs[SEL_WRITE],
                           (unsigned int *) WAIT_NULL);

  if (numbOfFDs < 0) {
    if (ossockerrno() == EINTR) {
      goto loop;
    }

    if (ossockerrno() != EBADF) {  /* some pipes may have been closed */
      ozpwarning("select failed");
    }
    osClearSocketErrors();
  }

  return numbOfFDs;
}

Bool osNextSelect(int fd, int mode)
{
  Assert(fd<openMax);
  CheckMode(mode);

  if (FD_ISSET(fd,&tmpFDs[mode])) {
    FD_CLR(fd,&tmpFDs[mode]);
    return OK;
  }
  return NO;
}

/* -------------------------------------------------------------------------
 * subroutine for AM::checkIO
 * ------------------------------------------------------------------------- */


// called from AM::checkIO
int osCheckIO()
{
 loop:
  copyFDs[SEL_READ]  = globalFDs[SEL_READ];
  copyFDs[SEL_WRITE] = globalFDs[SEL_WRITE];

  int numbOfFDs = osSelect(&copyFDs[SEL_READ], &copyFDs[SEL_WRITE],
                           (unsigned int *) WAIT_NULL);
  if (numbOfFDs < 0) {
    if (ossockerrno() == EINTR) goto loop;
    if (ossockerrno() != EBADF) { /* some pipes may have been closed */
      ozpwarning("checkIO: select failed");
    }
    osClearSocketErrors();
  }
  return numbOfFDs;
}





/*
 *  we want to kill all our children on exit *
 */


class ChildProc {
public:
  pid_t pid;
  ChildProc *next;
  ChildProc(pid_t p, ChildProc *nxt) : next(nxt), pid(p) {}
  static ChildProc *allchildren;
  static void add(pid_t p)
  {
    allchildren = new ChildProc(p,allchildren);
  }
};

ChildProc *ChildProc::allchildren = NULL;


void addChildProc(pid_t pid)
{
  ChildProc::add(pid);
}


void osExit(int status)
{
  /* terminate all our children */
  ChildProc *aux = ChildProc::allchildren;
  while(aux) {
#ifdef WINDOWS
    TerminateProcess((HANDLE)aux->pid,0);
#else
    (void) oskill(aux->pid,SIGTERM);
#endif
    aux = aux->next;
  }

#ifdef WINDOWS
  ExitProcess(status);
#else
  exit(status);
#endif
}




int osread(int fd, void *buf, unsigned int len)
{
#ifdef WINDOWS
  IOChannel *ch = lookupChannel(fd);
  if (ch!=NULL) {
    Assert(ch->thrd);
    WaitForSingleObject(ch->char_avail, INFINITE);

    if (ch->status==ST_ERROR) return -1;
    if (ch->status==ST_EOF)   return 0;
    Assert(ch->status==ST_AVAIL);
    ch->status=ST_NOTAVAIL;
    *(char*)buf = ch->chr;
    int ret = rawread(fd,((char*)buf)+1,len-1);
    if (ret<0)
      return ret;
    ResetEvent(ch->char_avail);
    SetEvent(ch->char_consumed);
    return ret+1;
  }
#endif

  return rawread(fd, buf, len);
}

int ossaferead(int fd, char *buf, unsigned int len)
{
 loop:
  int ret = osread(fd,buf,len);
  if (ret < 0 && ossockerrno()==EINTR)
    goto loop;
  return ret;
}


/* currently no wrapping for write */
int oswrite(int fd, void *buf, unsigned int len)
{
  return rawwrite(fd, buf, len);
}

int ossafewrite(int fd, char *buf, unsigned int len)
{
 loop:
  int written = oswrite(fd,buf,len);
  if (written < 0) {
    if (ossockerrno()==EINTR) goto loop;
    return written;
  }
  if ((unsigned)written<len) {
    buf += written;
    len -= written;
    goto loop;
  }
  return written;
}

int osclose(int fd)
{
#ifdef WINDOWS
  // never close stdin on Windows, leads to problems
  if (fd == wrappedStdin)
    return 0;

  Assert(fd!=STDIN_FILENO);

  if (isSocket(fd)) {
    FD_CLR((unsigned int)fd,&socketFDs);
    return closesocket(fd);
  }

  IOChannel *ch = lookupChannel(fd);
  if (ch) { ch->close(); }
  WrappedHandle *wh = WrappedHandle::find(fd);
  if (wh) { wh->close(); }
#endif

  FD_CLR((unsigned int)fd,&globalFDs[SEL_READ]);
  FD_CLR((unsigned int)fd,&globalFDs[SEL_WRITE]);
  FD_CLR((unsigned int)fd,&socketFDs);

#ifdef WINDOWS
  if (wh) return 0;
#endif
  return close(fd);
}

int osopen(const char *path, int flags, int mode)
{
  int ret = open(path,flags,mode);
  if (ret >= 0)
    FD_CLR((unsigned int)ret,&socketFDs);
  return ret;
}

int ossocket(int domain, int type, int protocol)
{
  int ret = socket(domain,type,protocol);
  if (ret >= 0)
    registerSocket(ret);
  return ret;
}

int osaccept(int s, struct sockaddr *addr, int *addrlen)
{
#if __GLIBC__ == 2
  int ret = accept(s,addr,(unsigned int*)addrlen);
#else
  int ret = accept(s,addr,addrlen);
#endif
  if (ret >= 0)
    registerSocket(ret);
  return ret;
}


int osconnect(int s, struct sockaddr *addr, int namelen)
{
  osBlockSignals();
  int ret = connect(s,addr,namelen);
  osUnblockSignals();
  return ret;
}


int ossockerrno()
{
#ifdef WINDOWS
  int ret = WSAGetLastError();
  return ret != 0 ? ret : errno;
#else
  return errno;
#endif
}

void ossleep(int secs)
{
#ifdef WINDOWS
  Sleep(secs*1000);
#else
  sleep(secs);
#endif
}

int osgetpid()
{
  int pid = getpid();
  return pid>0 ? pid : -pid;  // fucking Windows 95 returns negative pids
}

/* fgets may return NULL under Solaris if
 * interupted by the timer signal for example
 */
char *osfgets(char *s, int n, FILE *stream)
{
  osBlockSignals();
  char *ret = fgets(s,n,stream);
  osUnblockSignals();

  return ret;
}


#ifdef XXWINDOWS

// execution of C++ initializers

extern "C" {

extern void (*__CTOR_LIST__)();

int WINAPI dll_entry(int a,int b,int c)
{
  void (**pfunc)() = &__CTOR_LIST__;

  for (int i = 1; pfunc[i]; i++) {
    (pfunc[i])();
  }
  return 1;
}

}

#endif

/* -----------------------------------------------------------------
   stack dump
   ----------------------------------------------------------------- */

/* This code "inspired" by ccmalloc */

#define MAXCALLCHAINLENGTH 20

#if defined(__GNUC__) && !defined(WINDOWS)

#define RA(a) case a: return (caddr_t) __builtin_return_address(a);

static caddr_t return_address(unsigned i)
{
  switch(i)
    {
RA(0);RA(1);RA(2);RA(3);RA(4);RA(5);RA(6);RA(7);RA(8);RA(9);RA(10);
RA(11);RA(12);RA(13);RA(14);RA(15);RA(16);RA(17);RA(18);RA(19);RA(20);
      default: return 0;
    }
}

#include <setjmp.h>

static jmp_buf backtrace_jump;

static void handlerIgnore()
{
  longjmp(backtrace_jump, 1);
}


void osStackDump()
{
  osSignal(SIGSEGV,handlerIgnore);
  osSignal(SIGBUS,handlerIgnore);

  fprintf(stderr,"Stack Dump:\n");

  char *tmpfile = ostmpnam(NULL);
  FILE *tmpout = fopen(tmpfile,"w");
  if (tmpout==NULL)
    tmpout = stdout;

  if(setjmp(backtrace_jump) == 0) {
    int i=1;
    while(i<MAXCALLCHAINLENGTH) {
      caddr_t pc = return_address(i);
      i++;
      if(pc==NULL) break;
      fprintf(tmpout,"info line *%p\n",pc);
    }
  }


  if (tmpout != stdout) {
    fclose(tmpout);
    char buf[1000];
    sprintf(buf,"gdb -batch -n -x %s %s %d",tmpfile,ozconf.emuexe,getpid());
    osSystem(buf);
  }

  unlink(tmpfile);
}

#else

void osStackDump() {}

#endif


/* -----------------------------------------------------------------
   dynamic link objects files
   ----------------------------------------------------------------- */

#ifdef _MSC_VER
#define F_OK 00
#endif

TaggedRef osDlopen(char *filename, OZ_Term& ret)
{
  OZ_Term err=NameUnit;

#ifdef HAVE_DLOPEN
#ifdef HPUX_700
  {
    shl_t handle;
    handle = shl_load(filename,
                      BIND_IMMEDIATE | BIND_NONFATAL |
                      BIND_NOSTART | BIND_VERBOSE, 0L);

    if (handle == NULL) {
      goto raise;
    }
    ret = OZ_makeForeignPointer((void*)handle);
  }
#else
  {
    // RTLD_GLOBAL is important: it serves the same purpose
    // as -rdynamic for executables: newly loaded objects
    // are linked against symbols coming from already
    // liked objects (example: libfd and libschedule)

    void *handle=dlopen(filename, RTLD_NOW | RTLD_GLOBAL);

    if (!handle) {
      err=oz_atom(dlerror());
      goto raise;
    }
    ret = OZ_makeForeignPointer(handle);
  }
#endif

#elif defined(WINDOWS)
  {
    void *handle = (void *)LoadLibrary(filename);
    if (!handle) {
      err=oz_int(GetLastError());
      goto raise;
    }

    FARPROC linkff = (FARPROC) osDlsym(handle, "OZ_linkFF");
    if (linkff==0) {
      OZ_warning("OZ_linkFF not found, maybe not exported from DLL?");
      goto raise;
    }
    extern void **ffuns;
    if (linkff(OZVERSION,ffuns)==0) {
      OZ_warning("call of 'OZ_linkFF' failed, maybe recompilation needed?");
      goto raise;
    }

    ret = OZ_makeForeignPointer(handle);
  }
#endif

  return 0;
  // return oz_unify(out,ret);

raise:
  return err;
}

int osDlclose(void* handle)
{
#ifdef HAVE_DLOPEN
  if (dlclose(handle)) {
    return -1;
  }
#endif

#ifdef WINDOWS
  FreeLibrary(handle);
#endif

  return 0;
}

#if defined(HAVE_DLOPEN)
void *osDlsym(void *handle,const char *name) { return dlsym(handle,name); }
#elif defined(HPUX_700)
void *osDlsym(void *handle,const char *name)
{
  void *symaddr;

  int symbol = shl_findsym((shl_t*)&handle, name, TYPE_PROCEDURE, &symaddr);
  if (symbol != 0) {
    return NULL;
  }

  return symaddr;
}
#elif defined(WINDOWS)
void *osDlsym(void *h, const char *name)
{
  HMODULE handle = (HMODULE) h;
  FARPROC ret = GetProcAddress(handle,name);
  if (ret == NULL) {
    // try prepending a "_"
    char buf[1000];
    sprintf(buf,"_%s",name);
    ret = GetProcAddress(handle,buf);
  }
  return ret;
}
#endif
