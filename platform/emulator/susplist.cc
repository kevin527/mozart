/*
  Hydra Project, DFKI Saarbruecken,
  Stuhlsatzenhausweg 3, D-66123 Saarbruecken, Phone (+49) 681 302-5312
  Author: mehl,tmueller,popow
  Last modified: $Date$ from $Author$
  Version: $Revision$
  State: $State$

  ------------------------------------------------------------------------
*/


#if defined(INTERFACE) && !defined(PEANUTS)
#pragma implementation "susplist.hh"
#endif

#include "am.hh"

#include "genvar.hh"
#include "fdprofil.hh"

#ifdef OUTLINE
#define inline
#endif

//-----------------------------------------------------------------------------
//                          class SuspList

SuspList * SuspList::stable_wake(void) {
  Assert(this);

  SuspList * sl = this;

  do {
    Thread *thr = sl->getElem ();

    Assert(thr->isPropagator () || thr->isNewPropagator());
    Assert(thr->isPropagated ());

    Board *b = thr->getBoardFast ();

    if (b == am.currentBoard) {
#ifndef TM_LP
      if (localPropStore.isUseIt ()) {
	localPropStore.push (thr);
      } else {
	// 
	//  Note that these threads might be converted into the 
	// runnable state only now, because of the counter of runnable 
	// threads in the solve actor: otherwise, if they would be converted
	// earlier, they will affect stability, so that no stability 
	// could be reached at all!
	DebugCode (thr->unmarkPropagated ()); // otherwise assertion hits;
	thr->cContToRunnable (); 

	//
	am.scheduleThread (thr);
      }
#else
      localPropStore.push (thr);
#endif      
    } else {
      am.scheduleThread (thr);
    }

  } while ((sl = sl->getNextAndDispose ()));

  return NULL;
}


int SuspList::length(void)
{
  int i=0;
  for(SuspList * aux = this; aux != NULL; aux = aux->next) {
    if (!aux->getElem()->isDeadThread () &&
	!aux->getElem()->isPropagated () &&
	aux->getElem()->getBoardFast ()) {
      i++;
    }
  }
  return i;
} 

int SuspList::lengthProp(void)
{
  int i=0;
  for(SuspList * aux = this; aux != NULL; aux = aux->next) {
    if (!aux->getElem()->isDeadThread () &&
	aux->getElem()->isPropagated () &&
	aux->getElem()->getBoardFast ()) {
      i++;
    }
  }
  return i;
}

//-----------------------------------------------------------------------------

SuspList * installPropagators(SuspList * local_list, SuspList * glob_list,
			      Board * glob_home)
{
  SuspList * aux = local_list, * ret_list = local_list;

  
  // mark up local suspensions
  while (aux) {
    aux->getElem()->markTagged();
    aux = aux->getNext();
  }

  // create references to suspensions of global variable
  aux = glob_list;
  while (aux) {
    Thread *thr = aux->getElem();
    
    if (!(thr->isDeadThread ()) && 
	(thr->isPropagator () || thr->isNewPropagator()) &&
	!(thr->isTagged ()) && 
	am.isBetween (thr->getBoardFast (), glob_home ) == B_BETWEEN) {
      ret_list = new SuspList (thr, ret_list);
    }
    
    aux = aux->getNext();
  }

  aux = local_list;
  while (aux) {
    aux->getElem()->unmarkTagged();
    aux = aux->getNext();
  }
  
  return ret_list;
}

#ifdef OUTLINE
#define inline
#include "susplist.icc"
#undef inline
#endif
