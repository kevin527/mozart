/*
 *  Authors:
 *    Kostja Popov (kost@sics.se)
 * 
 *  Contributors:
 *    optional, Contributor's name (Contributor's email address)
 * 
 *  Copyright:
 *    Organization or Person (Year(s))
 * 
 *  Last change:
 *    $Date$
 *    $Revision$
 * 
 *  This file is part of Mozart, an implementation 
 *  of Oz 3:
 *     http://www.mozart-oz.org
 * 
 *  See the file "LICENSE" or
 *     http://www.mozart-oz.org/LICENSE.html
 *  for information on usage and redistribution 
 *  of this file, and for a DISCLAIMER OF ALL 
 *  WARRANTIES.
 *
 */

#if defined(INTERFACE)
#pragma implementation "gentraverser.hh"
#endif

#include "base.hh"
#include "gentraverser.hh"
#include "pickle.hh"

//
// A different one because of effeciency reasons. A 'ProcessNodeProc' 
// could well be supplied...
void GenTraverser::doit()
{
  while (!isEmpty()) {
    OZ_Term t = get();
    // a push-pop pair for the topmost entry is saved:
  bypass:
    CrazyDebug(incDebugNODES(););
    DEREF(t, tPtr, tTag);

    //
    switch(tTag) {

    case TAG_SMALLINT:
      processSmallInt(t);
      break;

    case TAG_FLOAT:
      processFloat(t);
      break;

    case TAG_LITERAL:
      {
	int ind = find(t);
	if (ind >= 0) {
	  (void) processRepetition(ind);
	  continue;
	}
	processLiteral(t);
	break;
      }

    case TAG_LTUPLE:
      {
	int ind = find(t);
	if (ind >= 0) {
	  (void) processRepetition(ind);
	  continue;
	}

	//
	if (!processLTuple(t)) {
	  LTuple *l = tagged2LTuple(t);
	  ensureFree(1);
	  put(l->getTail());
	  t = l->getHead();
	  goto bypass;
	}
	break;
      }

    case TAG_SRECORD:
      {
	int ind = find(t);
	if (ind >= 0) {
	  (void) processRepetition(ind);
	  continue;
	}

	//
	if (!processSRecord(t)) {
	  SRecord *rec = tagged2SRecord(t);
	  TaggedRef label = rec->getLabel();
	  int argno = rec->getWidth();

	  // 
	  // The order is: label, [arity], subtrees...
	  // Both tuple and record args appear in a reverse order;
	  ensureFree(argno+2);	// pessimistic approximation;
	  for(int i = 0; i < argno; i++)
	    put(rec->getArg(i));
	  if (!rec->isTuple()) {
	    putSync();		// will appear after the arity list;
	    put(rec->getArityList());
	  }
	  t = rec->getLabel();
	  goto bypass;
	}
	break;
      }

  case TAG_EXT:
    processExtension(t);
    break;

  case TAG_CONST:
    {
      int ind = find(t);
      if (ind >= 0) {
	(void) processRepetition(ind);
	continue;
      }

      //
      ConstTerm *ct = tagged2Const(t);
      switch (ct->getType()) {

      case Co_BigInt:
	processBigInt(t, ct);
	break;

      case Co_Dictionary:
	if (!processDictionary(t, ct)) {
	  OzDictionary *d = (OzDictionary *) ct;
	  // kost@ : what the hell is going on here???
	  int i = d->getFirst();
	  i = d->getNext(i);
	  // (pairs will be added on the receiver site in reverse order);
	  ensureFree(i+i);
	  while(i>=0) {
	    put(d->getValue(i));
	    put(d->getKey(i));
	    i = d->getNext(i);
	  }
	}
	break;

      case Co_Array:
	if (!processArray(t, ct)) {
	  OzArray *array = (OzArray *) ct;
	  ensureFree(array->getWidth());
	  for (int i = array->getHigh(); i >= array->getLow(); i--) {
	    put(array->getArg(i));
	  }
	}
	break;

      case Co_Builtin:
	processBuiltin(t, ct);
	break;

      case Co_Chunk:
	if (!processChunk(t, ct)) {
	  SChunk *ch = (SChunk *) ct;
	  t = ch->getValue();
	  goto bypass;
	}
	break;

      case Co_Class:
	if (!processClass(t, ct)) {
	  ObjectClass *cl = (ObjectClass *) ct;
	  SRecord *fs = cl->getFeatures();
	  if (fs)
	    t = makeTaggedSRecord(fs);
	  else
	    t = oz_nil();
	  goto bypass;
	}
	break;

      case Co_Abstraction:
	{
	  if (!processAbstraction(t, ct)) {
	    Abstraction *pp = (Abstraction *) ct;
	    int gs = pp->getPred()->getGSize();
	    //
	    // in the stream: file, name, registers, code area:
	    ensureFree(gs+2);
	    for (int i=0; i < gs; i++)
	      put(pp->getG(i));
	    //
	    put(pp->getName());
	    put(pp->getPred()->getFile());
	  }
	}
	break;

      case Co_Object:
	processObject(t, ct);
	break;

      case Co_Lock:
	processLock(t, (Tertiary *) ct);
	break;

      case Co_Cell:
	if (!processCell(t, (Tertiary *) ct) && ((Tertiary *) ct)->isLocal()) {
	  t = ((CellLocal *) ct)->getValue();
	  goto bypass;
	}
	break;

      case Co_Port:
	processPort(t, (Tertiary *) ct);
	break;
      case Co_Resource:
	processResource(t, (Tertiary *) ct);
	break;

      default:
	processNoGood(t,OK);
	break;
      }
      break;
    }

    case TAG_FSETVALUE:
      if (!processFSETValue(t)) {
	ensureFree(1);
	putSync();		// will appear after the list;
	t = tagged2FSetValue(t)->getKnownInList();
	goto bypass;
      }
      break;

    case TAG_UVAR:
      processUVar(tPtr);
      break;

    case TAG_CVAR:
      {
	int ind = find(t);
	if (ind >= 0) {
	  (void) processRepetition(ind);
	  continue;
	}

	//
	OZ_Term value;
	if ((value = processCVar(tPtr))) {
	  t = value;
	  goto bypass;
	}
	break;
      }

    case TAG_GCMARK:
      //
      switch(t) {
      case taggedBATask:
	{
	  // If the argument is zero then the task is empty:
	  void *arg = getPtr();

	  //
	  if (arg) {
	    MarshalerBinaryAreaProcessor proc =
	      (MarshalerBinaryAreaProcessor) lookupPtr();
	    Assert(proc > (MarshalerBinaryAreaProcessor) 0xf);
	    // 'proc' is preserved in stack;
	    StackEntry *se = putPtrSERef(0);
	    putInt(taggedBATask);	// pre-cooked;

	    //
	    if (!(*proc)(this, arg)) {
	      // not yet done - restore the argument back;
	      updateSEPtr(se, arg);
	    }
	    // ... otherwise do nothing: the empty task will be
	    // discarded later - 
	  } else {
	    CrazyDebug(decDebugNODES(););
	    // - here, to be exact:
	    dropEntry();		// 'proc';
	  }
	  break;
	}

      case taggedSyncTask:
	processSync();
	break;
      }
      break;

    default:
      processNoGood(t,NO);
      break;
    }
  }
}

//
// Code fragments that create particular data structures should be
// factorized away, but this would make comparing this unmarshaler
// with the recursive one unfair (since it's not done for that one
// neither). 
// 
// An obvious factorization is to have 'constructXXX(...)' functions
// (or virtual methods, if more than one builder are anticipated) that
// take necessary arguments and do whatever necessary for creating the 
// structure. Arguments are (a) all the non-oz-terms, aka gnames, etc.
// and (b) non-proper subtrees, e.g. a record's arity list.
//
// Handling proper subtrees is done by the builder, symmetrically to the
// GenTraverser.

//
// kost@ : there is no 'fsetcore.hh';
extern void makeFSetValue(OZ_Term,OZ_Term*);

#ifndef USE_FAST_UNMARSHALER   
Bool isArityList(OZ_Term l)
{
  OZ_Term old = l;
  while (oz_isCons(l)) {
    OZ_Term h = oz_head(l);
    if(!oz_isFeature(h)) return NO;
    l = oz_tail(l);
    if (l==old) return NO; // cyclic
  }
  if (oz_isNil(l)) {
    return OK;
  } 
else {
    return NO;
  }
}
#endif

//
void
Builder::buildValueOutline(OZ_Term value, BTFrame *frame,
			   BuilderTaskType type)
{
  Assert(type != BT_spointer);
  Bool doMemo = NO;

  //
  // Iteration invariant: there are correct 'frame' and 'type', but 
  // no argument.
  // Procedure invariant: it gets frame but must get rid of it;
repeat:
  //
  switch (type) {

    //
    // Though it's handled inline, we can get here iteratively:
  case BT_spointer:
    {
      GetBTTaskPtr1(frame, OZ_Term*, spointer);
      DiscardBTFrame(frame);
      *spointer = value;
      break;
    }

  case BT_spointer_iterate:
    {
      GetBTTaskPtr1(frame, OZ_Term*, spointer);
      *spointer = value;
      CrazyDebug(incDebugNODES(););
      DiscardBTFrame(frame);
      DebugCode(value = (OZ_Term) -1;);	// 'value' is expired;
      GetBTTaskTypeNoDecl(frame, type);
      goto repeat;
    }

  case BT_buildValue:
    {
      GetBTTaskArg1NoDecl(frame, OZ_Term, value);
      DiscardBTFrame(frame);
      GetBTTaskTypeNoDecl(frame, type);
      goto repeat;
    }

  case BT_makeTupleMemo:
    doMemo = OK;
    // fall through;
  case BT_makeTuple:
    {
      GetBTTaskArg1(frame, int, arity);
      SRecord *rec = SRecord::newSRecord(value, arity);
      OZ_Term recTerm = makeTaggedSRecord(rec);
      if (doMemo) {
	GetBTTaskArg2(frame, int, memoIndex);
	set(recTerm, memoIndex);
	doMemo = NO;
      }

      //
      GetBTNextTaskType(frame, nt);
      if (nt == BT_spointer) {
	CrazyDebug(incDebugNODES(););
	GetBTNextTaskPtr1(frame, OZ_Term*, spointer);
	*spointer = recTerm;
	DiscardBT2Frames(frame);

	//
	OZ_Term *args = rec->getRef();
	// put tasks in reverse order (since subtrees will appear in
	// the normal order):
	EnsureBTSpace(frame, arity);
	while(arity-- > 0) {
	  PutBTTaskPtr(frame, BT_spointer, args++);
	}
      } else {
	//
	ReplaceBTTask1stArg(frame, BT_buildValue, recTerm);

	//
	OZ_Term *args = rec->getRef();
	EnsureBTSpace(frame, arity);
	arity--;
	PutBTTaskPtr(frame, BT_spointer_iterate, args++);
	while (arity-- > 0) {
	  PutBTTaskPtr(frame, BT_spointer, args++);
	}
      }
      break;
    }

  case BT_takeRecordLabel:
    ReplaceBTTask1stArg(frame, BT_takeRecordArity, value);
    break;

  case BT_takeRecordLabelMemo:
    ReplaceBTTask1stArg(frame, BT_takeRecordArityMemo, value);
    break;

  case BT_takeRecordArity:
    ReplaceBTTask2ndArg(frame, BT_makeRecordSync, value);
    break;

  case BT_takeRecordArityMemo:
    ReplaceBTTask2ndArg(frame, BT_makeRecordMemoSync, value);
    break;

  case BT_makeRecordMemoSync:
    doMemo = OK;
    // fall through;

  case BT_makeRecordSync:
    {
      Assert(value == (OZ_Term) 0);
      GetBTTaskArg1(frame, OZ_Term, label);
      GetBTTaskArg2(frame, OZ_Term, arity);
#ifndef USE_FAST_UNMARSHALER
      if(!OZ_isLiteral(label) || !isArityList(arity))
      	RAISE_UNMARSHAL_ERROR;
#endif
      //
      OZ_Term sortedArity = arity;
      if (!isSorted(arity)) {
	int arityLen;
	TaggedRef aux = duplist(arity, arityLen);
	sortedArity = sortlist(aux, arityLen);
      }
      //
      SRecord *rec =
	SRecord::newSRecord(label, aritytable.find(sortedArity));
      OZ_Term recTerm = makeTaggedSRecord(rec);
      if (doMemo) {
	GetNextBTFrameArg1(frame, int, memoIndex);
	set(recTerm, memoIndex);
	doMemo = NO;
      }
      DiscardBT2Frames(frame); 

      //
      GetBTTaskType(frame, nt);
      // An optimization (for the most frequent case?): 
      // if the record is just to be stored somewhere ('spointer'
      // task), then let's do it now:
      if (nt == BT_spointer) {
	CrazyDebug(incDebugNODES(););
	GetBTTaskPtr1(frame, OZ_Term*, spointer);
	*spointer = recTerm;
	DiscardBTFrame(frame);

	//
	while (oz_isCons(arity)) {
	  EnsureBTSpace1Frame(frame);
	  PutBTTaskPtrArg(frame, BT_recordArg, rec, oz_head(arity));
	  arity = oz_tail(arity);
	}
      } else {
	//
	// The last 'iterate' task will restore 'value' to the record
	// and iterate to the (non-spointer) task that handles it:
	EnsureBTSpace1Frame(frame);
	PutBTTaskPtrArg(frame, BT_recordArg_iterate, rec, oz_head(arity));
	arity = oz_tail(arity);
	while (oz_isCons(arity)) {
	  EnsureBTSpace1Frame(frame);
	  PutBTTaskPtrArg(frame, BT_recordArg, rec, oz_head(arity));
	  arity = oz_tail(arity);
	}
      }

      //
      break;
    }

  case BT_recordArg:
    {
      GetBTTaskPtr1(frame, SRecord*, rec);
      GetBTTaskArg2(frame, OZ_Term, fea);
      DiscardBTFrame(frame);
      rec->setFeature(fea, value);
      break;
    }

  case BT_recordArg_iterate:
    {
      GetBTTaskPtr1(frame, SRecord*, rec);
      GetBTTaskArg2(frame, OZ_Term, fea);
      DiscardBTFrame(frame);
      rec->setFeature(fea, value);
      //
      CrazyDebug(incDebugNODES(););
      value = makeTaggedSRecord(rec); // new value;
      GetBTTaskTypeNoDecl(frame, type);
      goto repeat;
    }

  case BT_dictKey:
    {
#ifndef USE_FAST_UNMARSHALER
      if(!oz_isFeature(value))
      	RAISE_UNMARSHAL_ERROR;
#endif
      // 'dict' remains in place:
      ReplaceBTTask2ndArg(frame, BT_dictVal, value);
      break;
    }

  case BT_dictVal:
    {
      GetBTTaskPtr1(frame, OzDictionary*, dict);
      GetBTTaskArg2(frame, OZ_Term, key);
      DiscardBTFrame(frame);
      dict->setArg(key, value);
      break;
    }

  case BT_fsetvalue:
    ReplaceBTTask2ndArg(frame, BT_fsetvalueSync, value);
    break;

  case BT_fsetvalueMemo:
    ReplaceBTTask2ndArg(frame, BT_fsetvalueMemoSync, value);
    break;

  case BT_fsetvalueMemoSync:
    doMemo = OK;

  case BT_fsetvalueSync:
    {
      Assert(value == (OZ_Term) 0);
      //
      GetBTTaskArg2(frame, OZ_Term, listRep);
      // will iterate to the task that handles value:
      makeFSetValue(listRep, &value);
      if (doMemo) {
	GetBTTaskArg1(frame, int, memoIndex);
	set(value, memoIndex);
	doMemo = NO;
      }
      DiscardBTFrame(frame);

      //
      // 'value' now is the 'fsetvalue' we've just built. Let's just
      // go and do whatever is supposed to happen with it:
      GetBTTaskTypeNoDecl(frame, type);
      goto repeat;
    }

  case BT_chunkMemo:
    doMemo = OK;
    // fall through;
  case BT_chunk:
    {
      Assert(oz_onToplevel());
#ifndef USE_FAST_UNMARSHALER
      if(!oz_onToplevel() || !OZ_isRecord(value))
      	RAISE_UNMARSHAL_ERROR;
#endif
      GetBTTaskPtr1(frame, GName*, gname);

      //
      OZ_Term chunkTerm;
      SChunk *sc = new SChunk(am.currentBoard(), 0);
      sc->setGName(gname);
      chunkTerm = makeTaggedConst(sc);
      addGName(gname, chunkTerm);
      sc->import(value);

      //
      value = chunkTerm;
      if (doMemo) {
	GetBTTaskArg2(frame, int, memoIndex);
	set(value, memoIndex);
	doMemo = NO;
      }
      DiscardBTFrame(frame);
      GetBTTaskTypeNoDecl(frame, type);
      goto repeat;
    }

  case BT_classFeatures:
    {
      Assert(oz_isSRecord(value));
#ifndef USE_FAST_UNMARSHALER
      if(!oz_isSRecord(value))
      	RAISE_UNMARSHAL_ERROR;
#endif
      GetBTTaskPtr1(frame, ObjectClass*, cl);
      GetBTTaskArg2(frame, int, flags);
      DiscardBTFrame(frame);

      //
      SRecord *feat = tagged2SRecord(value);
      TaggedRef ff = feat->getFeature(NameOoFeat);
      //
      cl->import(value,
		 feat->getFeature(NameOoFastMeth),
		 oz_isSRecord(ff) ? ff : makeTaggedNULL(),
		 feat->getFeature(NameOoDefaults),
		 flags);

      //
      value = makeTaggedConst(cl);
      GetBTTaskTypeNoDecl(frame, type);
      goto repeat;
    }

  case BT_procFile:
    ReplaceBTTask1stArg(frame, BT_proc, value);
    break;

  case BT_procFileMemo:
    ReplaceBTTask1stArg(frame, BT_procMemo, value);
    break;

  case BT_procMemo:
    doMemo = OK;
    // fall through;
  case BT_proc:
    {
      OZ_Term name = value;
      GetBTTaskArg1(frame, OZ_Term, file);
      DiscardBTFrame(frame);
      GetBTFramePtr1(frame, GName*, gname);
      GetBTFramePtr2(frame, ProgramCounter, pc);
      GetBTFrameArg3(frame, int, maybeMemoIndex);
      DiscardBTFrame(frame);
      GetBTFrameArg1(frame, int, maxX);
      GetBTFrameArg2(frame, int, line);
      GetBTFrameArg3(frame, int, column);
      DiscardBTFrame(frame);
      GetBTFrameArg1(frame, int, arity);
      GetBTFrameArg2(frame, int, gsize);
      DiscardBTFrame(frame);

      //
      Assert(gname);		// must be an unknown procedure here;
      OZ_Term procTerm;
      // kost@ : 'flags' are obviously not used (otherwise something
      // would not work: flags are not passed as e.g. 'file' is);
      PrTabEntry *pr = new PrTabEntry(name, mkTupleWidth(arity),
				      file, line, column,
				      oz_nil(), maxX);
      pr->PC = pc;
      pr->setGSize(gsize);
      Abstraction *pp = Abstraction::newAbstraction(pr, am.currentBoard());
      procTerm = makeTaggedConst(pp);
      pp->setGName(gname);
      addGName(gname, procTerm);

      //
      if (doMemo) {
	set(procTerm, maybeMemoIndex);
	doMemo = NO;
      }

      //
      if (gsize > 0) {
	// reverse order... and don't bother with 'spointer' tasks:
	// just issue an '_iterate' task;
	EnsureBTSpace(frame, gsize);
	PutBTTaskPtrArg(frame, BT_closureElem_iterate, pp, 0);
	for (int i = 1; i < gsize; i++) {
	  PutBTTaskPtrArg(frame, BT_closureElem, pp, i);
	}
	break;			// BT_proc:
      } else {
	value = makeTaggedConst(pp);
	GetBTTaskTypeNoDecl(frame, type);
	goto repeat;
      }

      //
      // (code area is done by the user himself;)
      Assert(0);
    }

  case BT_closureElem:
    {
      GetBTTaskPtr1(frame, Abstraction*, pp);
      GetBTTaskArg2(frame, int, ind);
      DiscardBTFrame(frame);
      pp->initG(ind, value);
      break;
    }

  case BT_closureElem_iterate:
    {
      GetBTTaskPtr1(frame, Abstraction*, pp);
      GetBTTaskArg2(frame, int, ind);
      DiscardBTFrame(frame);
      pp->initG(ind, value);
      //
      CrazyDebug(incDebugNODES(););
      value = makeTaggedConst(pp);
      GetBTTaskTypeNoDecl(frame, type);
      goto repeat;
    }

  //
  // 'BT_binary' is transient here: it must be either saved or
  // discarded if it's already done;
  case BT_binary:
    {
      GetBTTaskPtr1(frame, void*, arg);
      Assert(arg == 0);
      DiscardBTFrame(frame);
      GetBTTaskTypeNoDecl(frame, type);
      goto repeat;
    }

  case BT_binary_getValue:
    ReplaceBTTask1stArg(frame, BT_binary_getValueSync, value);
    break;

  case BT_binary_getValueSync:
    {
      Assert(value == (OZ_Term) 0);
      GetBTTaskArg1(frame, OZ_Term, ozValue);
      DiscardBTFrame(frame);
      GetBTFramePtr1(frame, OzValueProcessor, proc);
      GetBTFramePtr2(frame, void*, arg);
      DiscardBTFrame(frame);
      //
      (*proc)(arg, ozValue);

      //
      break;
    }

  case BT_binary_doGenAction_intermediate:
    {
      GetBTTaskPtr1(frame, BuilderGenAction, proc);
      GetBTTaskPtr2(frame, void*, arg);
      DiscardBTFrame(frame);
      //
      (*proc)(arg);

      //
      // Note that 'value' is preserved;
      GetBTTaskTypeNoDecl(frame, type);
      goto repeat;
    }

  case BT_nop:
    Assert(value == (OZ_Term) 0);
    DiscardBTFrame(frame);
    break;

  default:
#ifndef USE_FAST_UNMARSHALER
    RAISE_UNMARSHAL_ERROR;
#else
    OZD_error("Builder: unknown task!");
#endif
  }

  //
  SetBTFrame(frame);
}

//
BTFrame* Builder::liftTask(int sz)
{
  GetBTFrame(frame);
  BTFrame *newTop = frame + bsFrameSize*sz;
  SetBTFrame(newTop);

  //
  // Iterate until a non-'goto iterate' task is found:
 repeat:
  GetBTTaskType(frame, type);
  switch (type) {

    // single frame:
  case BT_spointer:
  case BT_makeTupleMemo:
  case BT_makeTuple:
  case BT_recordArg:
  case BT_dictKey:
  case BT_dictVal:
  case BT_fsetvalue:
  case BT_fsetvalueMemo:
  case BT_closureElem:
  case BT_nop:
    CopyBTFrame(frame ,newTop);
    break;

    // single frame iterate:
  case BT_binary:
    { 
      DebugCode(GetBTTaskPtr1(frame, void*, bp););
      Assert(bp == 0);
    }
  case BT_spointer_iterate:
  case BT_buildValue:
  case BT_recordArg_iterate:
  case BT_fsetvalueMemoSync:
  case BT_fsetvalueSync:
  case BT_chunkMemo:
  case BT_chunk:
  case BT_classFeatures:
  case BT_closureElem_iterate:
  case BT_binary_doGenAction_intermediate:
    CopyBTFrame(frame, newTop);
    goto repeat;

    // two frames:
  case BT_takeRecordLabel:
  case BT_takeRecordLabelMemo:
  case BT_takeRecordArity:
  case BT_takeRecordArityMemo:
  case BT_makeRecordMemoSync:
  case BT_makeRecordSync:
  case BT_binary_getValue:
  case BT_binary_getValueSync:
    CopyBTFrame(frame, newTop);
    CopyBTFrame(frame, newTop);
    break;

    // four frames:
  case BT_procFile:
  case BT_procFileMemo:
    CopyBTFrame(frame, newTop);
    CopyBTFrame(frame, newTop);
    CopyBTFrame(frame, newTop);
    CopyBTFrame(frame, newTop);
    break;

    // four frames a'la procMemo:
  case BT_procMemo:
  case BT_proc:
    {
      CopyBTFrame(frame, newTop);
      CopyBTFrame(frame, newTop);
      CopyBTFrame(frame, newTop);
      GetBTFrameArg2(frame, int, gsize);
      CopyBTFrame(frame, newTop);
      if (gsize > 0)
	break;
      else
	goto repeat;
    }

  default:
#ifndef USE_FAST_UNMARSHALER
    RAISE_UNMARSHAL_ERROR;
#else
    OZD_error("Builder: unknown task!");
#endif
  }

  //
  return (frame);
}

//
BTFrame* Builder::findBinary(BTFrame *frame)
{
  void *bp;
  //
  // Iterate until a non-'goto iterate' task is found:
 repeat:
  GetBTTaskType(frame, type);
  switch (type) {

    // found:
  case BT_binary:
    GetBTTaskPtr1NoDecl(frame, void*, bp);
    if (bp)
      break;
    // else fall through;

    // single frame:
  case BT_spointer:
  case BT_makeTupleMemo:
  case BT_makeTuple:
  case BT_recordArg:
  case BT_dictKey:
  case BT_dictVal:
  case BT_fsetvalue:
  case BT_fsetvalueMemo:
  case BT_closureElem:
  case BT_nop:
    // single frame iterate:
  case BT_spointer_iterate:
  case BT_buildValue:
  case BT_recordArg_iterate:
  case BT_fsetvalueMemoSync:
  case BT_fsetvalueSync:
  case BT_chunkMemo:
  case BT_chunk:
  case BT_classFeatures:
  case BT_closureElem_iterate:
  case BT_binary_doGenAction_intermediate:
    NextBTFrame(frame);
    goto repeat;

    // two frames:
  case BT_takeRecordLabel:
  case BT_takeRecordLabelMemo:
  case BT_takeRecordArity:
  case BT_takeRecordArityMemo:
  case BT_makeRecordMemoSync:
  case BT_makeRecordSync:
  case BT_binary_getValue:
  case BT_binary_getValueSync:
    NextBTFrame(frame);
    NextBTFrame(frame);
    goto repeat;

    // four frames:
  case BT_procFile:
  case BT_procFileMemo:
    // four frames a'la procMemo:
  case BT_procMemo:
  case BT_proc:
    NextBTFrame(frame);
    NextBTFrame(frame);
    NextBTFrame(frame);
    NextBTFrame(frame);
    goto repeat;

  default:
#ifndef USE_FAST_UNMARSHALER
    RAISE_UNMARSHAL_ERROR;
#else
    OZD_error("Builder: unknown task!");
#endif
  }

  //
  return (frame);
}
