/*
  Hydra Project, DFKI Saarbruecken,
  Stuhlsatzenhausweg 3, D-66123 Saarbruecken, Phone (+49) 681 302-5312
  Author: mehl
  Last modified: $Date$ from $Author$
  Version: $Revision$
  State: $State$

  interface of threads and queues of threads with priorities
  ------------------------------------------------------------------------
*/



#ifndef __THREADHH
#define __THREADHH


#ifdef __GNUC__
#pragma interface
#endif

#include "types.hh"

/* enum ThreadFlags:
   Normal: thread has a taskStack and is scheduled
   SuspCont: thread has no taskStack,
             but 'suspCont' contains a SuspContinuation
   SuspCCont: thread has no taskStack,
             but 'suspCCont' contains a CFuncContinuation
   Nervous: thread has no taskStack, but 'board' contains the board to visit
   */

enum ThreadFlags
{
  T_Normal =      0x01,
  T_SuspCont =    0x02,
  T_SuspCCont =   0x04,
  T_Nervous =     0x08
};

class Toplevel;

class Thread : public ConstTerm
{
friend void engine();
private:
  static Thread *Head;
  static Thread *Tail;
  static Toplevel *ToplevelQueue;

public:
  static void Init();
  static void GC();
  static void Print();
  static Bool CheckSwitch();
  static Thread *GetHead();
  static Thread *GetTail();
  static Bool QueueIsEmpty();
  static Thread *GetFirst();
  static void ScheduleSuspCont(SuspContinuation *c, Bool wasExtSusp);
  static void ScheduleSuspCCont(CFuncContinuation *c, Bool wasExtSusp,
				Suspension *s = NULL);
  static void ScheduleWakeup(Board *n, Bool wasExtSusp);
  static void ScheduleSolve (Board *b); 

private:
  Thread *next;
  Thread *prev;
  int flags;
  union {
    TaskStack *taskStack;
    SuspContinuation *suspCont;
    CFuncContinuation *suspCCont;
    Board *board;
  } u;
  int priority;
  Board *notificationBoard; // for search capabilities; 
  Suspension *resSusp;      // resident suspension (mainly for finite domains)
public:
  Thread(int prio);

  USEFREELISTMEMORY;
  OZPRINT;
  OZPRINTLONG;
  Thread *gc();
  void gcRecurse(void);

public:
  int getPriority();
  TaskStack *getTaskStack();
  Suspension *getResSusp();
  Bool isNormal();
  Bool isNervous();
  Bool isSuspCont();
  Bool isSuspCCont();
  Bool isSolve () { return ((notificationBoard == (Board *) NULL) ? NO : OK); }
  void setNotificationBoard (Board *b) { notificationBoard = b; }
  Board* getNotificationBoard () { return (notificationBoard); }
  TaskStack *makeTaskStack();
  Board *popBoard();
  SuspContinuation *popSuspCont();
  CFuncContinuation *popSuspCCont();
  void pushTask(Board *n,ProgramCounter pc,
		       RefsArray y,RefsArray g,RefsArray x=NULL,int i=0);
  void pushTask (Board *n, OZ_CFun f, RefsArray x=NULL, int i=0);
  void schedule();
  void setPriority(int prio);
  void checkToplevel();
  void addToplevel(ProgramCounter pc);
  void pushToplevel(ProgramCounter pc);

private:
  Thread() : ConstTerm(Co_Thread) { init(); resSusp = NULL; }
  void init();
  Bool isScheduled();
  void insertFromTail();
  void insertFromHead();
  void insertAfter(Thread *here);
  void insertBefore(Thread *here);
  Thread *unlink();
  void dispose();
};


#ifndef OUTLINE
#include "thread.icc"
#endif

#endif
