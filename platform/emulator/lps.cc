/*
  Hydra Project, DFKI Saarbruecken,
  Stuhlsatzenhausweg 3, D-66123 Saarbruecken, Phone (+49) 681 302-5312
  Author: tmueller,popow
  Last modified: $Date$ from $Author$
  Version: $Revision$
  State: $State$

  ------------------------------------------------------------------------
*/


#if defined(INTERFACE) && !defined(PEANUTS)
#pragma implementation "lps.hh"
#endif

#include "am.hh"

#include "genvar.hh"
#include "fdprofil.hh"

#include <strstream.h>

#ifdef OUTLINE
#define inline
#endif

//-----------------------------------------------------------------------------

LocalPropagationStore localPropStore;

void LocalPropagationQueue::resize () {
  int new_maxsize = maxsize * 2;
  queue_t * new_queue = ::new queue_t[new_maxsize];
  int index = size - 1;
  int old_size = size;

  DebugCode(message("Resizing local propagation queue 0x%x --> 0x%x.\n",
		    maxsize, new_maxsize));
  
  while (size) {
    Thread *thr = dequeue ();
    new_queue[index].thr = thr;
    index -= 1;
  }

  Assert(index == -1);
  delete queue;
  queue = new_queue;
  head = 0;
  tail = (size = old_size) - 1;
  maxsize = new_maxsize;
}

// s must be power of 2
void LocalPropagationQueue::backupQueue (int s) {
  backup_head = head;
  backup_tail = tail;
  backup_size = size;
  backup_maxsize = maxsize;
  backup_queue = queue;

  DebugCode(int i; for (i = 1; i; i <<= 1) if (i == s) break;
	    if (!i) error("size expected to be power of 2."););
  
  tail = (maxsize = s) - 1;
  size = head = 0;
  queue = new queue_t[maxsize];
}

void LocalPropagationQueue::restoreQueue () {
  Assert(size == 0);

  delete queue;
  
  head = backup_head;
  tail = backup_tail;
  size = backup_size;
  maxsize = backup_maxsize;
  queue = backup_queue;
}

void LocalPropagationQueue::printDebug () {
  int psize = size, phead = head;

  for (; psize; psize --) {
    cout << "lpqueue[" << phead << "]="
	 << "@" << queue[phead].thr << "(";
    queue[phead].thr->print(cout);
    cout << ")" << endl;

    phead = (phead + 1) & (maxsize - 1);
  }
}

#ifdef DEBUG_CHECK
Bool LocalPropagationStore::checkIsPropagator (Thread *thr)
{
  return (thr->isPropagator () || thr->isNewPropagator());
}
#endif

Bool LocalPropagationStore::propagate_locally () {
  in_local_propagation = TRUE;
  Board *currentBoard = am.currentBoard;
  Thread *savedCurrentThread = am.currentThread;
  CFuncContinuation * c;
  RefsArray args;

  /*
   *  We save the actual 'am.currentThread' pointer and restore
   * it before returning. This is because propagators must have 
   * an access to themselves; this is provided by setting the 
   * (global) 'am.currentThread' to an actual propagator (thread);
   * 
   */
  // kost@ : --> let's try ...
  Assert (currentBoard->getSuspCount () >= getSize ());
	 
  while (!(isEmpty ())) {
    Thread *thr = am.currentThread = pop ();
    Assert (am.currentThread != (Thread *) NULL);
    Assert (am.currentBoard == thr->getBoardFast());
    //
    //  No 'propagated' threads are allowed here, 
    // because only true propagators are in the LPS;
    Assert (!(am.currentThread->isDeadThread ()));
    Assert (am.currentThread->isPropagator () ||
	    am.currentThread->isNewPropagator ());

    OZ_Boolean isNewPropagator = thr->isNewPropagator();
    OZ_Return ret_val;
    
    if (isNewPropagator) {
      ret_val = thr->runNewPropagator();
    } else {
      c = thr->getCCont ();
      args = c->getX ();
      Assert (args != (RefsArray) NULL);
      ret_val = (c->getCFunc())(getRefsArraySize (args), args);
    }

    switch (ret_val) {
    case FAILED:
      if (am.currentBoard->isRoot ()) {
	if (ozconf.errorVerbosity > 0) {
	  errorHeader();
	  if (isNewPropagator) {
            ostrstream buf; 
            buf << * thr->getNewPropagator() << '\0';
	    char *str = buf.str();
 	    message("Propagator %s failed\n", str);
	    delete str;
	  } else {
	    message("Propagator %s failed.\n",
		    builtinTab.getName((void *)(c->getCFunc())));
	    errorTrailer ();
	    allowTopLevelFailureMsg = FALSE;
	  }
	}
      }
      am.currentThread->closeDonePropagator ();
      am.currentThread = savedCurrentThread;
      return reset();

    case RAISE:
      error("propagators can't raise exceptions");

    case SUSPEND:
      error ("propagate_locally: 'SUSPEND' is returned?\n");

    case SLEEP:
      am.currentThread->suspendPropagator ();
      break;

    case SCHEDULED:
      am.currentThread->scheduledPropagator ();
      break;

    case PROCEED:
      am.currentThread->closeDonePropagator ();
      break;
    }
    
  }
  in_local_propagation = FALSE;
  am.currentThread = savedCurrentThread;
  return (TRUE);
}


#ifdef OUTLINE
#define inline
#include "lps.icc"
#undef inline
#endif
