/*
 *  Authors:
 *    Tobias M�ller (tmueller@ps.uni-sb.de)
 * 
 *  Contributors:
 *    Christian Schulte <schulte@ps.uni-sb.de>
 * 
 *  Copyright:
 *    Tobias M�ller, 1999
 *    Christian Schulte, 1999
 * 
 *  Last change:
 *    $Date$ by $Author$
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

#include "builtins.hh"
#include "space.hh"
#include "cpi.hh"
#include "var_fs.hh"

//*****************************************************************************

#define FSETDESCR_SYNTAX						\
"The syntax of a " OZ_EM_FSETDESCR " is:\n"				\
"   set_descr   ::= simpl_descr | compl(simpl_descr)\n"			\
"   simpl_descr ::= range_descr | nil | [range_descr+]\n"		\
"   range_descr ::= integer | integer#integer\n"			\
"   integer     ::= {" _OZ_EM_FSETINF ",...," _OZ_EM_FSETSUP "}"

//*****************************************************************************

OZ_BI_define(BIfsValueToString, 1,1)
{
  OZ_declareDetTerm(0,in);

  if (oz_isFSetValue(in)) {
    char *s = OZ_toC(in,100,100); // mm2
    OZ_RETURN_STRING(s);
  }
  oz_typeError(0,"FSetValue");
} OZ_BI_end

OZ_BI_define(BIfsIsVarB, 1,1)
{
  OZ_RETURN(oz_bool(isGenFSetVar(oz_deref(OZ_in(0)))));
} OZ_BI_end


OZ_BI_define(BIfsIsValueB, 1, 1)
{
  OZ_Term term = OZ_in(0);
  DEREF(term, term_ptr, term_tag);
  if (isVariableTag(term_tag)) 
    oz_suspendOnPtr(term_ptr);

  OZ_RETURN(oz_bool(oz_isFSetValue(term)));
} OZ_BI_end


OZ_BI_define(BIfsSetValue, 1, 1) 
{
  ExpectedTypes(OZ_EM_FSETDESCR "," OZ_EM_FSETVAL);

  ExpectOnly pe;
  
  EXPECT_BLOCK(pe, 0, expectFSetDescr, FSETDESCR_SYNTAX);

  OZ_RETURN(makeTaggedFSetValue(new FSetValue(OZ_in(0))));
} OZ_BI_end

OZ_BI_define(BIfsSet, 3, 0) 
{
  ExpectedTypes(OZ_EM_FSETDESCR "," OZ_EM_FSETDESCR "," OZ_EM_FSET);
  
  ExpectOnly pe;
  
  EXPECT_BLOCK(pe, 0, expectFSetDescr, FSETDESCR_SYNTAX);
  EXPECT_BLOCK(pe, 1, expectFSetDescr, FSETDESCR_SYNTAX);
  
  FSetConstraint fset(OZ_in(0), OZ_in(1));

  if (! fset.isValid()) {
    TypeError(2, "Invalid set description");
    return FAILED;
  }

  return tellBasicConstraint(OZ_in(2), &fset);
}
OZ_BI_end


OZ_BI_define(BIfsSup, 0, 1)
{
  OZ_RETURN_INT(fset_sup);
} OZ_BI_end


OZ_BI_define(BIfsClone, 2, 0) 
{
  ExpectedTypes(OZ_EM_FSET "," OZ_EM_FSET);
  
  ExpectOnly pe;
  
  EXPECT_BLOCK(pe, 0, expectFSetVar, "");
  
  DEREF(OZ_in(0), arg0ptr, arg0tag);

  return arg0tag == FSETVALUE ? OZ_unify(OZ_in(0), OZ_in(1))
    : tellBasicConstraint(OZ_in(1), 
			  (FSetConstraint *) &tagged2GenFSetVar(oz_deref(OZ_in(0)))->getSet());
} OZ_BI_end


//-----------------------------------------------------------------------------

OZ_BI_define(BIfsGetKnownIn, 1, 1) 
{
  ExpectedTypes(OZ_EM_FSET ","  OZ_EM_FSETDESCR);
  
  OZ_Term v = OZ_in(0);
  DEREF(v, vptr, vtag);

  if (isFSetValueTag(vtag)) {
    OZ_RETURN(tagged2FSetValue(v)->getKnownInList());
  } else if (isGenFSetVar(v, vtag)) {
    OZ_FSetConstraint * fsetconstr = &tagged2GenFSetVar(v)->getSet();
    OZ_RETURN(fsetconstr->getKnownInList());
  } else if (oz_isNonKinded(v)) {
    oz_suspendOnPtr(vptr);
  } 
  TypeError(0, "");
}
OZ_BI_end

OZ_BI_define(BIfsGetNumOfKnownIn, 1, 1) 
{
  ExpectedTypes(OZ_EM_FSET "," OZ_EM_INT);
  
  OZ_Term v = OZ_in(0);
  DEREF(v, vptr, vtag);

  if (isFSetValueTag(vtag)) {
    OZ_FSetValue * fsetval = tagged2FSetValue(v);
    OZ_RETURN(newSmallInt(fsetval->getCard()));
  } else if (isGenFSetVar(v, vtag)) {
    OZ_FSetConstraint * fsetconstr = &tagged2GenFSetVar(v)->getSet();
    OZ_RETURN(newSmallInt(fsetconstr->getKnownIn()));
  } else if (oz_isNonKinded(v)) {
    oz_suspendOnPtr(vptr);
  } 
  TypeError(0, "");
}
OZ_BI_end

//-----------------------------------------------------------------------------

OZ_BI_define(BIfsGetKnownNotIn, 1, 1) 
{
  ExpectedTypes(OZ_EM_FSET ","  OZ_EM_FSETDESCR);
  
  OZ_Term v = OZ_in(0);
  DEREF(v, vptr, vtag);

  if (isFSetValueTag(vtag)) {
    OZ_FSetValue * fsetval = tagged2FSetValue(v);
    OZ_RETURN(fsetval->getKnownNotInList());
  } else if (isGenFSetVar(v, vtag)) {
    OZ_FSetConstraint * fsetconstr = &tagged2GenFSetVar(v)->getSet();
    OZ_RETURN(fsetconstr->getKnownNotInList());
  } else if (oz_isNonKinded(v)) {
    oz_suspendOnPtr(vptr);
  } 
  TypeError(0, "");
}
OZ_BI_end

OZ_BI_define(BIfsGetNumOfKnownNotIn, 1, 1) 
{
  ExpectedTypes(OZ_EM_FSET "," OZ_EM_INT);
  
  OZ_Term v = OZ_in(0);
  DEREF(v, vptr, vtag);

  if (isFSetValueTag(vtag)) {
    OZ_FSetValue * fsetval = tagged2FSetValue(v);
    OZ_RETURN(newSmallInt(fsetval->getKnownNotIn()));
  } else if (isGenFSetVar(v, vtag)) {
    OZ_FSetConstraint * fsetconstr = &tagged2GenFSetVar(v)->getSet();
    OZ_RETURN(newSmallInt(fsetconstr->getKnownNotIn()));
  } else if (oz_isNonKinded(v)) {
    oz_suspendOnPtr(vptr);
  } 
  TypeError(0, "");
}
OZ_BI_end

//-----------------------------------------------------------------------------

OZ_BI_define(BIfsGetUnknown, 1, 1) 
{
  ExpectedTypes(OZ_EM_FSET "," OZ_EM_FSETDESCR);
  
  OZ_Term v = OZ_in(0);
  DEREF(v, vptr, vtag);

  if (isFSetValueTag(vtag)) {
    OZ_RETURN(oz_nil());
  } else if (isGenFSetVar(v, vtag)) {
    OZ_FSetConstraint * fsetconstr = &tagged2GenFSetVar(v)->getSet();
    OZ_RETURN(fsetconstr->getUnknownList());
  } else if (oz_isNonKinded(v)) {
    oz_suspendOnPtr(vptr);
  } 
  TypeError(0, "");
}
OZ_BI_end

OZ_BI_define(BIfsGetNumOfUnknown, 1, 1) 
{
  ExpectedTypes(OZ_EM_FSET "," OZ_EM_INT);
  
  OZ_Term v = OZ_in(0);
  DEREF(v, vptr, vtag);

  if (isFSetValueTag(vtag)) {
    OZ_RETURN(newSmallInt(0));
  } else if (isGenFSetVar(v, vtag)) {
    OZ_FSetConstraint * fsetconstr = &tagged2GenFSetVar(v)->getSet();
    OZ_RETURN(newSmallInt(fsetconstr->getUnknown()));
  } else if (oz_isNonKinded(v)) {
    oz_suspendOnPtr(vptr);
  } 
  TypeError(0, "");
}
OZ_BI_end

//-----------------------------------------------------------------------------

OZ_BI_define(BIfsGetLub, 1, 1) 
{
  ExpectedTypes(OZ_EM_FSET "," OZ_EM_FSETDESCR);
  
  OZ_Term v = OZ_in(0);
  DEREF(v, vptr, vtag);

  if (isFSetValueTag(vtag)) {
    OZ_FSetValue * fsetval = tagged2FSetValue(v);
    OZ_RETURN(fsetval->getKnownInList());
  } else if (isGenFSetVar(v, vtag)) {
    OZ_FSetConstraint * fsetconstr = &tagged2GenFSetVar(v)->getSet();
    OZ_RETURN(fsetconstr->getLubList());
  } else if (oz_isNonKinded(v)) {
    oz_suspendOnPtr(vptr);
  } 
  TypeError(0, "");
}
OZ_BI_end

OZ_BI_define(BIfsGetCard, 1, 1) 
{
  ExpectedTypes(OZ_EM_FSET "," OZ_EM_INT);
  
  OZ_Term v = OZ_in(0);
  DEREF(v, vptr, vtag);

  if (isFSetValueTag(vtag)) {
    OZ_FSetValue * fsetval = tagged2FSetValue(v);
    OZ_RETURN(newSmallInt(fsetval->getCard()));
  } else if (isGenFSetVar(v, vtag)) {
    OZ_FSetConstraint * fsetconstr = &tagged2GenFSetVar(v)->getSet();
    OZ_RETURN(fsetconstr->getCardTuple());
  } else if (oz_isNonKinded(v)) {
    oz_suspendOnPtr(vptr);
  } 
  TypeError(0, "");
}
OZ_BI_end

OZ_BI_define(BIfsCardRange, 3, 0) 
{
  ExpectedTypes(OZ_EM_INT "," OZ_EM_INT "," OZ_EM_FSET);
  
  int l = -1;
  {
    OZ_Term lt = OZ_in(0);
    DEREF(lt, ltptr, lttag);
    
    if (isSmallIntTag(lttag)) {
      l = smallIntValue(lt);
    } else if (isVariableTag(lttag)) {
      oz_suspendOnPtr(ltptr);
    } else {
      TypeError(0, "");
    }
  }

  int u = -1;
  {
    OZ_Term ut = OZ_in(1);
    DEREF(ut, utptr, uttag);
    
    if (isSmallIntTag(uttag)) {
      u = smallIntValue(ut);
    } else if (isVariableTag(uttag)) {
      oz_suspendOnPtr(utptr);
    } else {
      TypeError(1, "");
    }
  }

  if (l > u) 
    return FAILED;

  {
    OZ_Term v = OZ_in(2);
    DEREF(v, vptr, vtag);
    
    if (isFSetValueTag(vtag)) {
      int card = tagged2FSetValue(v)->getCard();
      return ((l <= card) && (card <= u)) ? PROCEED : FAILED;
    } else if (isGenFSetVar(v, vtag)) {
      OZ_FSetConstraint * fsetconstr = &tagged2GenFSetVar(v)->getSet();
      if (!fsetconstr->putCard(l, u)) 
	return FAILED;
      /* a variable might have become a fset value because of 
	 imposing a card constraints */
      if (fsetconstr->isValue())
	tagged2GenFSetVar(v)->becomesFSetValueAndPropagate(vptr);
      return PROCEED; 
    } else if (oz_isNonKinded(v)) {
      oz_suspendOnPtr(vptr);
    } 
  }
  TypeError(2, "");
}
OZ_BI_end


/*
 * The builtin table
 */

#ifndef MODULES_LINK_STATIC

#include "modFSB-if.cc"

#endif





