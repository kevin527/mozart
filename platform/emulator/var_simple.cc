/*
 *  Authors:
 *    Michael Mehl (mehl@dfki.de)
 *
 *  Contributors:
 *    optional, Contributor's name (Contributor's email address)
 *
 *  Copyright:
 *    Michael Mehl (1998)
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

#if defined(INTERFACE) && !defined(VAR_ALL)
#pragma implementation "var_simple.hh"
#endif

#include "var_simple.hh"
#include "unify.hh"

OZ_Return SimpleVar::bind(TaggedRef* vPtr, TaggedRef t, ByteCode* scp)
{
  oz_bindVar(this,vPtr, t);
  return PROCEED;
}

// from marshaler.cc
OZ_Return export(OZ_Term t);

OZ_Return SimpleVar::unify(TaggedRef* vPtr, TaggedRef *tPtr, ByteCode* scp)
{
  // mm2
  if (isExported()) {
    OZ_Return aux = export(makeTaggedRef(tPtr));
    if (aux!=PROCEED) return aux;
  }

  OzVariable *tv=tagged2CVar(*tPtr);
  if (tv->getType()==OZ_VAR_SIMPLE
      && oz_isBelow(GETBOARD(tv),GETBOARD(this))
#ifdef VAR_BIND_NEWER
      // if both are local, then check heap
      && (!oz_isLocalVar(this) || heapNewer(tPtr,vPtr))
#endif
      ) {

    if (tv->isExported())
      markExported();

    oz_bindVar(tv,tPtr, makeTaggedRef(vPtr));
  } else {
    oz_bindVar(this,vPtr, makeTaggedRef(tPtr));
  }
  return PROCEED;
}

OzVariable *uvar2SimpleVar(TaggedRef *v)
{
  OzVariable *sv = new SimpleVar(tagged2VarHome(*v));
  *v = makeTaggedCVar(sv);
  return sv;
}

void addSuspUVar(TaggedRef *v, Suspension susp, int unstable)
{
  uvar2SimpleVar(v)->addSuspSVar(susp, unstable);
}
