/*
  Hydra Project, DFKI Saarbruecken,
  Stuhlsatzenhausweg 3, D-66123 Saarbruecken, Phone (+49) 681 302-5312
  Author: tmueller
  Last modified: $Date$ from $Author$
  Version: $Revision$
  State: $State$

  ------------------------------------------------------------------------
*/


#if defined(__GNUC__) && !defined(NOPRAGMA)
#pragma implementation "genvar.hh"
#endif


#include "tagged.hh"
#include "constter.hh"
#include "board.hh"

#include "suspensi.hh"
#include "variable.hh"
#include "genvar.hh"

GenCVariable::GenCVariable(TypeOfGenCVariable t, Board * n) :
SVariable(n == NULL ? am.currentBoard : n)
{
  Assert(n!=0);
  setType(t);
}


void GenCVariable::propagate(TaggedRef var, SuspList * &sl, TaggedRef term,
                             PropCaller unifyVars)
{
  sl = am.checkSuspensionList(tagged2SuspVar(var), var, sl, term, unifyVars);
}


Bool GenCVariable::unifyOutline(TaggedRef *tptr1, TaggedRef term1,
                                TaggedRef *tptr2, TaggedRef term2,
                                Bool prop)
{
  return unify(tptr1,term1,tptr2,term2,prop);
}


#ifdef OUTLINE
#define inline
#include "genvar.icc"
#undef inline
#endif
