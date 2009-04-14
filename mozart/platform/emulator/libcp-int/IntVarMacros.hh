/*
 *  Main authors:
 *     Gustavo Gutierrez <ggutierrez@cic.puj.edu.co>
 *     Alberto Delgado <adelgado@cic.puj.edu.co> 
 *     Alejandro Arbelaez <aarbelaez@cic.puj.edu.co>
 *     Andres Felipe Barco <anfelbar@univalle.edu.co>
 *
 *  Contributors:
 *     Gustavo A. Gomez Farhat <gafarhat@univalle.edu.co>
 *
 *  Copyright:
 *    Alberto Delgado, 2006-2007
 *    Alejandro Arbelaez, 2006-2007
 *    Gustavo Gutierrez, 2006-2007
 *    Andres Felipe Barco, 2008
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

 
 
#ifndef __GFD_INT_DECLARE_MACROS_HH__
#define __GFD_INT_DECLARE_MACROS_HH__

#include "GeIntVar.hh"
#include "GeSpace-builtins.hh"
#include "builtins.hh"
#include "value.hh"
#include "../GeVar.hh"
#include "../libcp-bool/BoolVarMacros.hh"


/**
   ############################## Variable declaration macros ##############################
*/

/**
 * \brief Inline for intvar declaration inside a space. 
 * @param p The new variable.
 * @param s The space where the variable is declared.
 */
inline
IntVar* declareIV(OZ_Term p, GenericSpace *s) {
  IntVar *v = NULL;
  if (OZ_isInt(p)) {
    int val = OZ_intToC(p);
    v = new IntVar(s,val,val);
  } else if (OZ_isGeIntVar(p)) {
    v = get_IntVarPtr(p);
  } 
  return v;
}

/**
 * \brief Inline for intvar declaration.
 * @param p The new variable.
 */
inline
IntVar* declareIV(OZ_Term p) {
  IntVar *v = NULL;
  if (OZ_isInt(p)) {
    int val = OZ_intToC(p);
    v = new IntVar(oz_currentBoard()->getGenericSpace(),val,val);
  } else if (OZ_isGeIntVar(p)) {
    v = get_IntVarPtr(p);
  } 
  return v;
} 

/**
 * \brief Macros for variable declaration inside propagators posting built-ins. 
 * Space stability is affected as a side effect.
 * @param p The position in OZ_in
 * @param v Name of the new variable
 * @param sp The space where the variable is declared.
 */
#define DeclareGeIntVar(p,v,sp)					\
  declareInTerm(p,v##x);					\
  IntVar v;							\
  { IntVar *vp = declareIV(v##x,sp);				\
    if (vp == NULL) return OZ_typeError(p,"IntVar or Int");	\
    v = *vp;							\
  }
  
/**
 * \brief Declares a variable form OZ_Term argument at position p in the
 * input to be an IntVar variable. This declaration does not affect space 
 * stability so it can not be used in 
 * propagator's built ins.
 * @param p The position in OZ
 * @param v Name of the new variable
 */
#define DeclareGeIntVar1(p,v)					\
  declareInTerm(p,v##x);					\
  IntVar v;							\
  { IntVar *vp = declareIV(v##x);				\
    if (vp == NULL) return OZ_typeError(p,"IntVar or Int");	\
    v = *vp;							\
  }
  
/**
 * \brief Declares a GeIntVar inside a var array. Space stability is affected as a side effect.
 * @param val integer value of the new variable
 * @param ar array of variables where the new variable is posted
 * @param i position in the array assigned to the variable
 * @param sp Space where the array belongs to
 */
#define DeclareGeIntVarVA(val,ar,i,sp)		\
  {  declareTerm(val,x);			\
    if(OZ_isInt(val)) {				\
      int domain=OZ_intToC(val);		\
      Gecode::IntVar v(sp,domain,domain);	\
      ar[i] = v;				\
    }						\
    else if(OZ_isGeIntVar(val)) {		\
      IntVar *v = get_IntVarPtr(val);		\
      ar[i]=(*v);				\
      delete v;					\
    }						\
  }
  
/**
 * \brief Declares a GeIntVarArgs. Space stability is affected as a side effect.
 * @param tIn Values of the variables in the new array
 * @param array Array of variables
 * @param sp Space where the array is declared
 */
#define DECLARE_INTVARARGS(tIn,array,sp) DECLARE_VARARGS(tIn,array,sp,IntVarArgs,DeclareGeIntVarVA)

/**
 * \brief Return a IntVar from a integer or a GeIntVar
 * @param value integer or GeIntVar representing a IntVar
 */
inline
Gecode::IntVar * intOrIntVar(TaggedRef value){
  if(OZ_isInt(value)){
    return new IntVar(oz_currentBoard()->getGenericSpace(), OZ_intToC(value), OZ_intToC(value));
  }else {
    return get_IntVarPtr(value);
  }
}


//the next inline replace the macro DECLARE_INTVARARGS
/**
 * \brief Return a IntVarArgs. Space stability is affected as a side effect.
 * @param vaar Array of GeIntVars and/or integers
 */
inline
Gecode::IntVarArgs getIntVarArgs(TaggedRef vaar){
  int sz;
  TaggedRef t = vaar;

  Assert(OZ_isIntVarArgs(vaar));

  if(OZ_isLiteral(OZ_deref(t))) {
    sz=0;
    IntVarArgs array(sz);
    return array;
  } else
    if(OZ_isCons(t)) {
      sz = OZ_length(t);
      IntVarArgs array(sz);
      for(int i=0; OZ_isCons(t); t=OZ_tail(t),i++){
	IntVar *iv = intOrIntVar(OZ_deref(OZ_head(t)));
	array[i] = *iv;
	delete iv;
      }
      return array;
    } else 
      if(OZ_isTuple(t)) {
	sz=OZ_width(t);
	IntVarArgs array(sz);
	for(int i=0; i<sz; i++) {
	  IntVar *iv = intOrIntVar(OZ_getArg(t,i));
	  array[i] = *iv;
	  delete iv;
	}
	return array;
      } else {
	Assert(OZ_isRecord(t));
	OZ_Term al = OZ_arityList(t);
	sz = OZ_width(t);
	IntVarArgs array(sz);
	for(int i=0; OZ_isCons(al); al=OZ_tail(al),i++) {
	  IntVar *iv = intOrIntVar(OZ_subtree(t,OZ_head(al)));
	  array[i] = *iv;
	  delete iv;
	}
	return array;
      }  
}


/**
   ############################## Domain declaration ##########################
*/

/** 
 * \brief Declares a Gecode::IntSet from an Oz domain description
 * 
 * @param _t Mozart domain specification 
 * @param ds The domain description in terms of list and tuples
 * 
 */
#define DECLARE_INT_SET(_t,ds)					\
  OZ_Term _l = (OZ_isCons(_t) ? _t : OZ_cons(_t, OZ_nil()));	\
  int _length = OZ_length(_l);					\
  int _pairs[_length][2];					\
  for (int i = 0; OZ_isCons(_l); _l=OZ_tail(_l), i++) {		\
    OZ_Term _val = OZ_head(_l);					\
    								\
    if (OZ_isInt(_val)) {					\
      _pairs[i][0] = OZ_intToC(_val);				\
      _pairs[i][1] = OZ_intToC(_val);				\
    }								\
    else if (OZ_isTuple(_val)) {				\
      _pairs[i][0] = OZ_intToC(OZ_getArg(_val,0));		\
      _pairs[i][1] = OZ_intToC(OZ_getArg(_val,1));		\
    }								\
    else {							\
      return OZ_typeError(0,"malformed domain description");	\
    }								\
  }								\
  Gecode::IntSet ds(_pairs, _length);


// this inline replace DECLARE_INT_SET
/**
 * \brief Return a IntSet. 
 * @param is List, tuple or record of domain description
 */
inline
Gecode::IntSet getIntSet(TaggedRef is){

  Assert(OZ_isIntSet(is));

  OZ_Term list = (OZ_isCons(is) ? is : OZ_cons( is, OZ_nil()));
  int length = OZ_length(list);
  int pairs[length][2];
  
  for(int i=0; OZ_isCons(list); list = OZ_tail(list), i++){
    OZ_Term val = OZ_head(list);
    
    if(OZ_isInt(val)){
      pairs[i][0] = OZ_intToC(val);
      pairs[i][1] = OZ_intToC(val);
    } else
      if (OZ_isTuple(val)){
	pairs[i][0] = OZ_intToC(OZ_getArg(val,0));
	pairs[i][1] = OZ_intToC(OZ_getArg(val,1));
      }
    else {							
      OZ_typeError(0,"malformed domain description");	
    }								
  }
  return IntSet(pairs, length);
  
}

//TODO: what is this for? DECLARE_INT_SET is used by GFS and DECLARE_INT_SET2
//is used by GFD, probe gfd and fix this madness!
#define DECLARE_INT_SET2(_p,_v) DECLARE_INT_SET(OZ_in(_p),_v)

#define DECLARE_INT_SET3(ds,val,arg)					\
  IntSet *tmp##val;							\
  if(OZ_isNil(OZ_in(arg))){						\
    tmp##val = new Gecode::IntSet(IntSet::empty);			\
  }else{								\
    OZ_declareDetTerm(arg,val);						\
    OZ_Term val##arg = (OZ_isCons(val) ? val : OZ_cons(val, OZ_nil()));	\
    int length##val = OZ_length(val##arg);				\
    int _pairs##val[length##val][2];					\
    for (int i = 0; OZ_isCons(val##arg); val##arg=OZ_tail(val##arg), i++) { \
      OZ_Term _val = OZ_head(val##arg);					\
      if (OZ_isInt(_val)) {						\
	_pairs##val[i][0] = OZ_intToC(_val);				\
	_pairs##val[i][1] = OZ_intToC(_val);				\
      }									\
      else if (OZ_isTuple(_val)) {					\
	_pairs##val[i][0] = OZ_intToC(OZ_getArg(_val,0));		\
	_pairs##val[i][1] = OZ_intToC(OZ_getArg(_val,1));		\
      }									\
      else {								\
	OZ_typeError(arg,"Int domain");					\
      }									\
    }									\
    tmp##val = new Gecode::IntSet(_pairs##val, length##val);		\
  }									\
  Gecode::IntSet ds(*tmp##val); 

/** 
 * \brief Declares a Gecode::Int::IntSetArgs from an Oz domain description
 * 
 * @param ds The domain description int terms of list and tuples
 * @param _t Mozart domain specification
 * TODO: where is this macro used?
 */
#define DECLARE_INT_SET_ARGS(_t,ds)				\
  OZ_Term l = (OZ_isCons(_t) ? _t : OZ_cons(_t, OZ_nil()));	\
  int length = OZ_length(l);					\
  int _pairs[length][2];					\
  std::vector<int> ar;						\
								\
  for (int i = 0; OZ_isCons(l); l=OZ_tail(l), i++) {		\
    OZ_Term _val = OZ_head(l);					\
    if (OZ_isIntSet(_val)) {					\
      _pairs[i][0] = OZ_intToC(OZ_getArg(_val,0));		\
      _pairs[i][1] = OZ_intToC(OZ_getArg(_val,1));		\
      ar[i] = OZ_intToC(OZ_getArg(_val,0));			\
    }								\
    else {							\
      return OZ_typeError(0,"domain type unknown");		\
    }								\
  }								\
  Gecode::PrimArgArray<IntSet> ds(length);


/**
 * \brief Return a IntSetArgs. 
 * @param is List, tuple or record of domain IntSets
 */
inline
Gecode::IntSetArgs getIntSetArgs(TaggedRef isa){
  int sz;
  TaggedRef t = isa;
    
  Assert(OZ_isIntSetArgs(isa));

  if(OZ_isLiteral(OZ_deref(t))) {
    sz=0;
    IntSetArgs array(sz);
    return array;
  } else
    if(OZ_isCons(t)) {
      sz = OZ_length(t);
      IntSetArgs array(sz);
      for(int i=0; OZ_isCons(t); t=OZ_tail(t),i++){
	array[i] = getIntSet(OZ_deref(OZ_head(t)));
      }
      return array;
    } else 
      if(OZ_isTuple(t)) {
	sz=OZ_width(t);
	IntSetArgs array(sz);
	for(int i=0; i<sz; i++) {
	  array[i] = getIntSet(OZ_getArg(t,i));
	}
	return array;
      } else {
	Assert(OZ_isRecord(t));
	OZ_Term al = OZ_arityList(t);
	sz = OZ_width(t);
	IntSetArgs array(sz);
	for(int i=0; OZ_isCons(al); al=OZ_tail(al),i++) {
	  array[i] = getIntSet(OZ_subtree(t,OZ_head(al)));
	}
	return array;
      }  
}

/**
 * \brief Declares a Gecode::Int:IntArgs from a list of int values.
 * @param array the resulting IntArgs
 * @param t possition in OZ_in
 */
#define DECLARE_INTARGS1(array,t)					\
  OZ_declareTerm(t,_termTmp_args);					\
  int __length_args=1;							\
  __length_args=OZ_length(_termTmp_args);				\
  IntArgs array(__length_args);						\
  if( __length_args != -1 ){						\
    for(int i = 0; OZ_isCons(_termTmp_args); _termTmp_args=OZ_tail(_termTmp_args),i++) \
      {									\
	OZ_Term val = OZ_head(_termTmp_args);				\
	if(true){							\
	  array[i]=OZ_intToC(val);					\
	}								\
	else{								\
	  std::cout<<"Error,  Variable type is not GDF"<<std::endl;	\
	}								\
      }									\
  }                 

/**
 * \brief Declares a Gecode::Int:IntArgs from a literal, list, tuple or record of int values.
 * @param tIn possition in OZ_in
 * @param array the resulting IntArgs
 */

#define DECLARE_INTARGS(tIn,array)				\
  IntArgs array(0);						\
  {								\
    int sz;							\
    OZ_declareTerm(tIn,t);					\
    if(OZ_isLiteral(t)) {					\
      sz = 0;							\
      IntArgs _array(sz);					\
      array=_array;						\
    }								\
    else if(OZ_isCons(t)) {					\
      sz = OZ_length(t);					\
      IntArgs _array(sz);					\
      for(int i=0; OZ_isCons(t); t=OZ_tail(t))			\
	_array[i++] = OZ_intToC(OZ_head(t));			\
      array=_array;						\
    }								\
    else if(OZ_isTuple(t)) {					\
      sz=OZ_width(t);						\
      IntArgs _array(sz);					\
      for(int i=0; i < sz; i++) {				\
	OZ_Term _tmp = OZ_getArg(t,i);				\
	_array[i] = OZ_intToC(_tmp);				\
      }								\
      array=_array;						\
    }								\
    else {							\
      assert(OZ_isRecord(t));					\
      OZ_Term al = OZ_arityList(t);				\
      sz = OZ_width(t);						\
      IntArgs _array(sz);					\
      for(int i = 0; OZ_isCons(al); al=OZ_tail(al))		\
	_array[i++]=OZ_intToC(OZ_subtree(t,OZ_head(al)));	\
      array=_array;						\
    }								\
  }


//the next inline replace the macro DECLARE_INTARGS
/**
 * \brief Return a IntArgs 
 * @param is List, tuple or record of integers
 */
inline
Gecode::IntArgs getIntArgs(TaggedRef inar){
  int sz;

  Assert(OZ_isIntArgs(inar));
  
  if(OZ_isLiteral(inar)) {
    sz = 0;
    IntArgs array(sz);
    return array;
  } else 
    if(OZ_isCons(inar)){
      sz = OZ_length(inar);
      IntArgs array(sz);
      for(int i=0; OZ_isCons(inar); inar=OZ_tail(inar)){
	array[i++] = OZ_intToC(OZ_head(inar));
      }
      return array;
    } else
      if(OZ_isTuple(inar)) {
	sz=OZ_width(inar);
	IntArgs array(sz);
	for(int i=0; i < sz; i++) {
	  array[i] = OZ_intToC(OZ_getArg(inar,i));
	}
	return array;
      } else {
	Assert(OZ_isRecord(inar));
	OZ_Term al = OZ_arityList(inar);
	sz = OZ_width(inar);
	IntArgs array(sz);
	for(int i = 0; OZ_isCons(al); al=OZ_tail(al)){
	  array[i++] = OZ_intToC(OZ_subtree(inar,OZ_head(al)));
	}
	return array;
      }
}
  
/**
 * \brief Declare a Gecode::TupleSet from a domain description.
 * @param _t tuple with IntArgs as values
 * @param ds domain specification
 */
#define DeclareTupleSet(_t, ds)					\
  OZ_Term l = (OZ_isCons(_t) ? _t : OZ_cons(_t, OZ_nil()));	\
  Gecode::TupleSet aa;						\
  int ta = OZ_length(l);					\
  IntArgs array(ta);						\
  for (int i = 0; OZ_isCons(l); l=OZ_tail(l), i++) {		\
    OZ_Term _val = OZ_head(l);					\
    if (OZ_isIntArgs(_val)) {					\
      aa.add((IntArgs)_val);					\
    }								\
    else {							\
      return OZ_typeError(0,"domain type unknown");		\
    }								\
  }								\
  Gecode::TupleSet ds(aa);

/**
   ############################## New variables from Gecode declare macros ##############################
*/

/**
 * \brief Declares a Gecode::IntConLevel
 * @param arg A integer defining the IntConLevel
 * @param var the variable name of the IntConLevel
 */
#define DeclareIntConLevel(arg,var)					\
  IntConLevel var;							\
  {									\
    OZ_TOC(arg,int,__vv,OZ_isInt,OZ_intToC,"Expected consistency level"); \
    var = (IntConLevel)__vv;						\
  }

// this inline replace de macri DeclareIntConLevel
/**
 * \brief Return a IntConLevel.
 * @param icl OZ integer representing the IntConLevel
 */
inline
Gecode::IntConLevel getIntConLevel(TaggedRef icl){
  Assert(OZ_isIntConLevel(icl));
  return (IntConLevel) OZ_intToC(icl);
}

/**
 * \brief Declares a Gecode::PropKind
 * @param arg An integer defining the PropKind
 * @param var the variable name of the PropKind
 */
#define DeclarePropKind(arg,var)					\
  PropKind var;								\
  {									\
    OZ_TOC(arg,int,__vv,OZ_isInt,OZ_intToC,"Expected propagator kind"); \
    var = (PropKind)__vv;						\
  }


// this inline replace de macro DeclarePropKind
/**
 * \brief Return a PropKind.
 * @param pk OZ integer representing the PropKind
 */
inline
Gecode::PropKind getPropKind(TaggedRef pk){
  Assert(OZ_isPropKind(pk));
  return (PropKind) OZ_intToC(pk);
}

/**
 * \brief Declares a Gecode::IntRelType
 * @param arg An integer defining the IntRelType
 * @param var the variable name of the IntRelType
 */
#define DeclareIntRelType(arg,var)					\
  IntRelType var;							\
  {									\
    OZ_TOC(arg,int,__vv,OZ_isInt,OZ_intToC,"Expected relation type") ;  \
    var = (IntRelType)__vv;						\
  }


//this inline replace de macro DeclareIntRelType
/**
 * \brief Return a IntRelType.
 * @param irt OZ integer representing the IntRelType
 */
inline
Gecode::IntRelType getIntRelType(TaggedRef irt){
  Assert(OZ_isIntRelType(irt));
  return (IntRelType) OZ_intToC(irt);
}

/**
 * \brief Declares a Gecode::DFA::Transition for DFA
 * @param val the name of the new transition
 * @param tr tuple with the values of transition
 */
#define TransitionS(val, tr)			\
  { Gecode::DFA::Transition _t;			\
    _t.i_state = OZ_intToC(OZ_getArg(tr,0));	\
    _t.symbol  = OZ_intToC(OZ_getArg(tr,1));	\
    _t.o_state = OZ_intToC(OZ_getArg(tr,2));	\
    val = _t;					\
  }
  
/**
 * \brief Declares a Gecode::DFA with the argument pos
 * @param pos the position in the OZ_in
 * @param dfa the name of the new declared DFA
 */
#define DeclareDFA(pos, dfa)						\
  OZ_Term _t = OZ_in(pos);						\
  OZ_Term _inputl = OZ_arityList(_t);					\
  OZ_Term _inputs = OZ_subtree(_t, OZ_head(_inputl));			\
  int _istate     = OZ_intToC(_inputs);					\
  OZ_Term _tl    = OZ_subtree(_t, OZ_head(OZ_tail(_inputl)));		\
  Gecode::DFA::Transition _trans[OZ_length(_tl)];			\
  for(int i=0; OZ_isCons(_tl); _tl=OZ_tail(_tl)) {			\
    TransitionS(_trans[i++], OZ_head(_tl));				\
  }									\
  OZ_Term _fstates = OZ_subtree(_t, OZ_head(OZ_tail(OZ_tail(_inputl)))); \
  _fstates = oz_deref(_fstates);					\
  int _fl[OZ_length(_fstates)];						\
  for(int i=0; OZ_isCons(_fstates); _fstates=OZ_tail(_fstates)) {	\
    _fl[i++] = OZ_intToC(OZ_head(_fstates));				\
  }									\
  Gecode::DFA dfa(_istate, _trans, _fl);
  
/**
   ############################## Miscelanious declare macros ##############################
*/

/**
 * \brief Declares a IntAssignType
 * @param arg the position in the OZ_in
 * @param var the name of the new IntAssignType
 */
#define DeclareIntAssignType(arg,var)					\
  IntAssign var;							\
  {									\
    OZ_declareInt(arg,op);						\
    switch(op) {							\
    case 0: var = INT_ASSIGN_MIN; break;				\
    case 1: var = INT_ASSIGN_MED; break;				\
    case 2: var = INT_ASSIGN_MAX; break;				\
    default: return OZ_typeError(arg,"Expecting atom with an integer assign type: min, med, max"); \
    }}  
  
/**
 * \brief Declare a new integer
 * @param arg value of the integer
 * @param var the variable declared as integer
 */
#define DeclareInt(arg,var,msg)			\
  OZ_TOC(arg,int,var,OZ_isInt,OZ_intToC,msg)
  
/**
 * \brief Declare a new integer, the same that DeclareInt but with only two arguments in its input
 * @param arg value of the integer
 * @param var the variable declared as integer
 */
#define DeclareInt2(arg,var)						\
  OZ_TOC(arg,int,var,OZ_isInt,OZ_intToC,"The value must be a number")
  

/**
 * \brief Turn a OZ type value in a C/C++ type value
 * @param arg the value to be transformed
 * @param type the type of the variable to be transformed
 * @param var the name of variable
 * @param check check wheter the value is of the corresponding type
 * @param conv funcion to turn the values
 */
#define OZ_TOC(arg,type,var,check,conv,msg)	\
  type var;					\
  {						\
    if (!check(OZ_in(arg)))			\
      return OZ_typeError(arg,msg);		\
    var=conv(OZ_in(arg));			\
  }

#endif
