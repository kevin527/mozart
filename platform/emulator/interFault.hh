/*
 *  Authors:
 *    Per Brand (perbrand@sics.se)
 * 
 *  Contributors:
 *    optional, Contributor's name (Contributor's email address)
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

#ifndef __INTERFAULTHH
#define __INTERFAULTHH

#ifdef INTERFACE  
#pragma interface
#endif

#include "base.hh"
#include "value.hh"
#include "genhashtbl.hh"

enum WatcherKind{
  WATCHER_RETRY      = 1,
  WATCHER_PERSISTENT = 2,
  WATCHER_SITE_BASED = 4,
  WATCHER_INJECTOR   = 8,
  WATCHER_CELL       = 16
};

enum EntityCondFlags{
  ENTITY_NORMAL = 0,
  PERM_BLOCKED  = 2,       
  TEMP_BLOCKED  = 1,
  PERM_ALL      = 4,
  TEMP_ALL      = 8,
  PERM_SOME     = 16,
  TEMP_SOME     = 32,
  PERM_ME       = 64,
  TEMP_ME       = 128,  
  UNREACHABLE   = 256,
  TEMP_FLOW     = 512
};

#define IncorrectFaultSpecification oz_raise(E_ERROR,E_SYSTEM,"incorrect fault specification",0)

#define DerefVarTest(tt) { \
  if(OZ_isVariable(tt)){OZ_suspendOn(tt);} \
  tt=oz_deref(tt);}

typedef unsigned int EntityCond;

class DeferWatcher{
public:
  TaggedRef proc;
  Thread* thread;
  TaggedRef entity;
  short kind;
  EntityCond watchcond;
  DeferWatcher* next;

  USEHEAPMEMORY;
  NO_DEFAULT_CONSTRUCTORS(DeferWatcher);

  DeferWatcher(short wk,EntityCond c,
	       Thread* th,TaggedRef e,TaggedRef p){
    proc=p;
    thread=th;
    entity=e;
    next=NULL;
    watchcond=c;
    kind=wk;}

  Bool isEqual(short,EntityCond,Thread *,TaggedRef,TaggedRef);

  Bool preventAdd(short,Thread *,TaggedRef);

  void gc();
};


extern DeferWatcher* deferWatchers;
extern Bool perdioInitialized;

void gcDeferWatchers();

Bool addDeferWatcher(short, EntityCond, Thread*,
		     TaggedRef, TaggedRef);

Bool remDeferWatcher(short, EntityCond, Thread*,
		     TaggedRef, TaggedRef);

OZ_Return distHandlerInstallHelp(SRecord*, EntityCond&, Thread* &,TaggedRef &,
				 short&);


/* __INTERFAULTHH */
#endif 


