/*
  Hydra Project, DFKI Saarbruecken,
  Stuhlsatzenhausweg 3, D-W-6600 Saarbruecken 11, Phone (+49) 681 302-5312
  Author: scheidhr
  Last modified: $Date$ from $Author$
  Version: $Revision$
  State: $State$
  ------------------------------------------------------------------------
  Unix/Posix functions
  ------------------------------------------------------------------------
*/

#if defined(INTERFACE) && !defined(PEANUTS)
#pragma implementation "os.hh"
#endif

#include "wsock.hh"

#include <errno.h>
#include <limits.h>
#include <malloc.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef WINDOWS
#include <time.h>
#else
#include <sys/times.h>
#include <sys/wait.h>
#include <sys/time.h>
#endif

#if !defined(ultrix) && !defined(WINDOWS)
# include <sys/socket.h>
#endif

#ifdef AIX3_RS6000
#include <sys/select.h>
#endif

#include "tagged.hh"
#include "os.hh"





// return current usertime in milliseconds
unsigned int osUserTime()
{
#ifdef WINDOWS
  return ((1000*clock())/CLOCKS_PER_SEC);
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
  return 0;
#else
  struct tms buffer;

  times(&buffer);
  return (unsigned int)(buffer.tms_stime*1000.0/(double)sysconf(_SC_CLK_TCK));
#endif
}


void osBlockSignals(Bool check)
{
#ifndef WINDOWS
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
      warning("blockSignals: there are blocked signals");
    }
  }
#endif
#endif
}

void osUnblockSignals()
{
#ifndef WINDOWS
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
OsSigFun *osSignal(int signo, OsSigFun *fun)
{
#ifdef WINDOWS
  signal(signo,fun);
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
  if (signo == SIGALRM) {
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


#ifndef __GNUC__
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

/* void operator delete (void *ptr) */
extern "C" void __builtin_delete (void *ptr)
{
  if (ptr)
    free (ptr);
}

#endif  /* __GNUC__ */



#ifdef HPUX_700
#include <time.h>
#endif




/* abstract timeout values */
#define WAIT_INFINITE (int*) -1
#define WAIT_NULL     (int*) -2



#ifdef WINDOWS

class StreamReaders {
public:
  int fd;
  HANDLE char_avail;
  HANDLE char_consumed;
  char chr;
  Bool status;  /* true iff input is available */
  int thrd;     /* reader thread */
};

static StreamReader *readers;


void readerThread(void *arg)
{
  StreamReader *sr = (StreamReader *)arg;

  while(1) {
    sr->status=NO;
    ResetEvent(sr->char_avail);
    int ret = read(sr->fd, &sr->chr, sizeof(char));
    sr->status = (ret == sizeof(char));
    if (ret<0) {
      perror("readerThread: read");
      break;
    }
    SetEvent(sr->char_avail);

    /* Wait until our input is acknowledged before reading again */
    if (WaitForSingleObject(sr->char_consumed, INFINITE) != WAIT_OBJECT_0)
      break;
  }
  sr->status = NO;
  sr->thrd = NULL;
  _endthread();
}

static
Bool createReader(int fd)
{
  StreamReader *sr = &readers[fd];

  if (sr->thrd != NULL)
    return OK;

  sr->char_avail = CreateEvent(NULL, FALSE, FALSE, NULL);
  sr->char_consumed = CreateEvent(NULL, FALSE, FALSE, NULL);

  if ((sr->char_consumed==NULL) || (sr->char_avail == NULL)) {
    goto err;
  }

  sr->thrd = _beginthread(readerThread, NULL, 4096, sr);
  if (sr->thrd == -1) {
    sr->thrd = NULL;
    goto err;
  }

  return OK;

err:
  int id = GetLastError();
  CloseHandle(sr->char_consumed);
  CloseHandle(sr->char_avail);

  return NO;
}


static
int getAvailFDs(int nfds, fd_set *readfds)
{
  FD_ZERO(readfds);
  int ret=0, i;
  for (i=0; i<nfds; i++) {
    if (readers[i].status==OK) {
      FD_SET(i,readfds);
      ret++;
    }
  }
  return ret;
}


static
Bool win32Select(int maxfd, fd_set *fds, int *timeout)
{
  if (timeout == WAIT_NULL)
    return getAvailFDs(readfds);

  int wait = (timeout==WAIT_INFINITE) ? INFINITE : *timeout;

  HANDLE wait_hnd[OPEN_MAX];

  int nh = 0;
  int i;
  for (i=0; i< maxfd; i++) {
    if (FD_ISSET(i,fds) && readers[i].thrd!=NULL) {
      wait_hnd[nh++] = readers[i].char_avail;
    }
  }

  if (nh == 0) {
    /* Nothing to wait on, so fail */
    errno = ECHILD;
    return NO;
  }

  DWORD active = WaitForMultipleObjects(nh, wait_hnd, FALSE, wait);

  if (active == WAIT_FAILED) {
    errno = EBADF;
    return NO;
  }
  if (active == WAIT_TIMEOUT) {
    return 0;
  }

  if (timeout!=WAIT_INFINITE) {
    *timeout = 0;  /* must be corrected !!!!! */
  }

  return getAvailFDs(maxfd, fds);
}

#endif

void osSetAlarmTimer(int t, Bool interval)
{
#ifdef DEBUG_DET
  return;
#else
  struct itimerval newT;

  int sec  = t/1000;
  int usec = (t*1000)%1000000;
  newT.it_interval.tv_sec  = (interval ? sec : 0);
  newT.it_interval.tv_usec = (interval ? usec : 0);
  newT.it_value.tv_sec     = sec;
  newT.it_value.tv_usec    = usec;

  if (setitimer(ITIMER_REAL,&newT,NULL) < 0) {
    ozpwarning("setitimer");
  }
#endif
}

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



/* wait *timeout msecs on given fds
 * return number of fds ready and return in *timeout msecs left
 */
static
int osSelect(int nfds, fd_set *readfds, fd_set *writefds, int *timeout)
{
#ifdef WINDOWS

  return win32Select(nfds,readfds,timeout);

#else

  struct timeval timeoutstruct, *timeoutptr;
  if (timeout == WAIT_INFINITE) {
    timeoutptr=NULL;
  } else if (timeout == WAIT_NULL) {
    timeoutstruct.tv_sec = 0;
    timeoutstruct.tv_usec = 0;
    timeoutptr = &timeoutstruct;
  } else {
    timeoutptr=NULL;
    osSetAlarmTimer(*timeout,NO);
    osUnblockSignals();
  }

/* The prototypes for select are wrong on HP-UX 9.x */
#ifdef HPUX_700
  int ret = select(nfds,(int*)readfds,(int*)writefds,NULL,timeoutptr);
#else
  int ret = select(nfds,readfds,writefds,NULL,timeoutptr);
#endif

  if (timeout!=WAIT_INFINITE && timeout!=WAIT_NULL) {
    *timeout = osGetAlarmTimer();
    osBlockSignals();
  }
  return ret;
#endif  /* WINDOWS */
}

void osInitSignals()
{
#ifndef DEBUG_DET
  osSignal(SIGINT,handlerINT);
  osSignal(SIGTERM,handlerTERM);
  osSignal(SIGSEGV,handlerSEGV);
  osSignal(SIGBUS,handlerBUS);
  osSignal(SIGPIPE,handlerPIPE);
  osSignal(SIGUSR1,handlerUSR1);
  osSignal(SIGCHLD,handlerCHLD);
  osSignal(SIGFPE,handlerFPE);
  osSignal(SIGALRM,handlerALRM);
#endif
}

Bool osHasJobControl()
{
#if defined(_POSIX_JOB_CONTROL)
  return OK;
#elif defined(_SC_JOB_CONTROL)
  return sysconf(_SC_JOB_CONTROL)==-1:NO:OK;
#else
  return NO;
#endif
}



/*********************************************************
 *       Sockets                                         *
 *********************************************************/

static fd_set globalFDs[2];     // mask of active read/write FDs
static long openMax;

int osOpenMax()
{
#ifdef WINDOWS
  return OPEN_MAX;
#else
  int ret = sysconf(_SC_OPEN_MAX);
  if (ret == -1) {
    ret = _POSIX_OPEN_MAX;
  }
  return ret;
#endif
}

void osInit()
{
#ifdef MAKEANEWPGRP // mm2
  // create a new process group, so that we can
  // easily terminate all our children
  if (setpgrp(getpid(),getpid()) < 0) {
    ozperror("setpgrp");
  }
#endif

#ifndef WINDOWS
  if (osHasJobControl()) {
    /* start a new process group */
    if (setpgid(0,0)<0) {
      DebugCheckT(ozpwarning("setpgid(0,0)"));
    }
  } else {
    DebugCheckT(warning("OS does not support POSIX job control\n"));
  }
#endif

  DebugCheck(CLOCK_TICK < 1000, error("CLOCK_TICK must be greater than 1 ms"));

  openMax=osOpenMax();
  DebugCheckT(printf("openMax: %d\n",openMax));

  FD_ZERO(&globalFDs[SEL_READ]);
  FD_ZERO(&globalFDs[SEL_WRITE]);

#ifdef WINDOWS
  readers = new StreamReader[openMax];
  for(int i=0; i<=openMax; i++) {
    readers[i].fd     = i;
    readers[i].thrd   = NULL;
    readers[i].status = NO;
  }
#endif
}

#define CheckMode(mode) Assert(mode==SEL_READ || mode==SEL_WRITE)


void osWatchFD(int fd, int mode)
{
  CheckMode(mode);
  FD_SET(fd,&globalFDs[mode]);
#ifdef WINDOWS
  Assert(mode==SEL_READ);
  createReader(fd);
#endif
}

void osClrWatchedFD(int fd, int mode)
{
  CheckMode(mode);
  FD_CLR(fd,&globalFDs[mode]);
#ifdef WINDOWS
  Assert(mode==SEL_READ);
  deleteReader(fd);
#endif
}

Bool osIsWatchedFD(int fd, int mode)
{
  CheckMode(mode);
  return (FD_ISSET(fd,&globalFDs[mode]));
}


/* do a select, that waits "ticks" ticks.
 * if "ticks" <= 0 do a blocking select
 * return number of ticks left
 */
int osBlockSelect(int ticks)
{
  fd_set copyFDs[2];
  copyFDs[SEL_READ]  = globalFDs[SEL_READ];
  copyFDs[SEL_WRITE] = globalFDs[SEL_WRITE];
  int wait = osClockTickToMs(ticks);
  int ret = osSelect(openMax,&copyFDs[SEL_READ],&copyFDs[SEL_WRITE],&wait);
  int ticksleft = osMsToClockTick(wait);
  return ticksleft;
}

/* osClearSocketErrors
 * remove the closed/failed descriptors from the fd_sets
 */
void osClearSocketErrors()
{
  for (int i = 0; i < openMax; i++) {
    for(int mode=SEL_READ; mode <= SEL_WRITE; mode++) {
      if (FD_ISSET(i,&globalFDs[mode]) && osTestSelect(i,mode) < 0) {
        FD_CLR(i,&globalFDs[mode]);
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
  /* no select on write */
  if (mode==SEL_WRITE)
    return 1;

  /* hopefully a disk file, so input will not block */
  HANDLE h = (HANDLE)_os_handle(fd);
  if (GetFileType(h) != FILE_TYPE_PIPE)
    return 1;

  if (readers[fd].thrd==NULL)
    return -1;
  return (readers[fd].status==OK) ? 1 : 0;
#else
  while(1) {
    fd_set fdset, *readFDs=NULL, *writeFDs=NULL;
    FD_ZERO(&fdset);
    FD_SET(fd,&fdset);

    if (mode==SEL_READ) {
      readFDs = &fdset;
    } else {
      writeFDs = &fdset;
    }
    int ret = osSelect(fd+1, readFDs, writeFDs, WAIT_NULL);
    if (ret >= 0 || errno != EINTR) {
      return ret;
    }
  }
#endif
}

/* -------------------------------------------------------------------------
 * subroutines for AM::handleIO
 * ------------------------------------------------------------------------- */

static fd_set tmpFDs[2];

/* signals are blocked */
int osFirstSelect()
{
 loop:
  tmpFDs[SEL_READ]  = globalFDs[SEL_READ];
  tmpFDs[SEL_WRITE] = globalFDs[SEL_WRITE];

  int numbOfFDs = osSelect(openMax,&tmpFDs[SEL_READ],&tmpFDs[SEL_WRITE], WAIT_NULL);

  if (numbOfFDs < 0) {
    if (errno == EINTR) {
      goto loop;
    }

    if (errno != EBADF) {  /* the compiler may have terminated */
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
  fd_set copyFDs[2];
  copyFDs[SEL_READ]  = globalFDs[SEL_READ];
  copyFDs[SEL_WRITE] = globalFDs[SEL_WRITE];

  int numbOfFDs = osSelect(openMax,&copyFDs[SEL_READ],&copyFDs[SEL_WRITE],WAIT_NULL);
  if (numbOfFDs < 0) {
    if (errno == EINTR) goto loop;
    if (errno != EBADF) { /* the compiler may have terminated (rs) */
      ozpwarning("checkIO: select failed");
    }
    osClearSocketErrors();
  }
  return numbOfFDs;
}


void osKillChildren()
{
#ifndef WINDOWS
  if (osHasJobControl()) {
    // terminate all our children
    OsSigFun *save=osSignal(SIGTERM,(OsSigFun*)SIG_IGN);
    if (oskill(-getpid(),SIGTERM) < 0) {
      ozpwarning("kill");
    }
    osSignal(SIGTERM,save);
  }
#endif
}


int osread(int fd, void *buf, unsigned int len)
{
#ifdef WINDOWS
  if (readers[fd].thrd) {
    if (readers[fd].status==NO) {
      WaitForSingleObject(readers[fd].char_avail, INFINITE);
    }
    *(char*)buf = readers[fd].chr;
    int ret = read(fd,((char*)buf)+1,len-1);
    if (ret<0)
      return ret;
    readers[fd].status=NO;
    PulseEvent(readers[fd].char_consumed);
    return ret+1;
  }
#endif
  return read(fd, buf, len);
}


/* currently no wrapping for write */
int oswrite(int fd, void *buf, unsigned int len)
{
  return write(fd, buf, len);
}
