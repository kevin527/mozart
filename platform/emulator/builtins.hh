/*
  Hydra Project, DFKI Saarbruecken,
  Stuhlsatzenhausweg 3, D-66123 Saarbruecken, Phone (+49) 681 302-5312
  Author: mehl
  Last modified: $Date$ from $Author$
  Version: $Revision$
  State: $State$
  */

#ifndef __BUILTINSH
#define __BUILTINSH

#ifdef INTERFACE
#pragma interface
#endif

// special builtins known in emulate
enum BIType {
  BIDefault,
  BIsolve,
  BIsolveEatWait,
  BIsolveDebug,
  BIsolveDebugEatWait,
  BIsolveCont,
  BIsolved,
  BIraise
};

// specification for builtins
struct BIspec {
  char *name;
  int arity;
  OZ_CFun fun;
  Bool yps;
  IFOR ifun;
};


// add specification to builtin table
void BIaddSpec(BIspec *spec);

#define NONVAR(X,term,tag)  					              \
TaggedRef term = X;							      \
TypeOfTerm tag;								      \
{ DEREF(term,_myTermPtr,myTag);  	                                      \
  tag = myTag;		    		                                      \
  if (isAnyVar(tag)) return SUSPEND;					      \
}

#define DECLAREBI_USEINLINEREL1(Name,InlineName)			      \
OZ_C_proc_begin(Name,1)						      \
{									      \
  OZ_Term arg1 = OZ_getCArg(0);						      \
  State state = InlineName(arg1);				      	      \
  if (state == SUSPEND)	{						      \
    return OZ_suspendOnVar(arg1);					      \
  } else {								      \
    return state;							      \
  }									      \
}									      \
OZ_C_proc_end


#define DECLAREBI_USEINLINEREL2(Name,InlineName)			      \
OZ_C_proc_begin(Name,2)						      \
{									      \
  OZ_Term arg0 = OZ_getCArg(0);						      \
  OZ_Term arg1 = OZ_getCArg(1);						      \
  State state = InlineName(arg0,arg1);					      \
  if (state == SUSPEND) {						      \
    return OZ_suspendOnVar2(arg0,arg1);					      \
  } else {								      \
    return state;							      \
  }									      \
}									      \
OZ_C_proc_end


#define DECLAREBI_USEINLINEFUN1(Name,InlineName)			      \
OZ_C_proc_begin(Name,2)						      \
{									      \
  OZ_Term help;								      \
  									      \
  OZ_Term arg1 = OZ_getCArg(0);						      \
  State state = InlineName(arg1,help);					      \
  switch (state) {							      \
  case SUSPEND: 							      \
    return OZ_suspendOnVar(arg1);					      \
  case FAILED:								      \
    return FAILED;							      \
  case PROCEED:								      \
  default:								      \
    return(OZ_unify(help,OZ_getCArg(1)));				      \
  }									      \
									      \
}									      \
OZ_C_proc_end



#define DECLAREBI_USEINLINEFUN2(Name,InlineName)			      \
OZ_C_proc_begin(Name,3)						      \
{									      \
  OZ_Term help;								      \
  									      \
  OZ_Term arg0 = OZ_getCArg(0);						      \
  OZ_Term arg1 = OZ_getCArg(1);						      \
  State state=InlineName(arg0,arg1,help);		  		      \
  switch (state) {							      \
  case SUSPEND:								      \
    return OZ_suspendOnVar2(arg0,arg1);					      \
  case FAILED:								      \
    return state;							      \
  case PROCEED:								      \
  default:								      \
    return(OZ_unify(help,OZ_getCArg(2)));				      \
  }									      \
									      \
}									      \
OZ_C_proc_end


#define DECLAREBI_USEINLINEFUN3(Name,InlineName)			      \
OZ_C_proc_begin(Name,4)						      \
{									      \
  OZ_Term help;								      \
  									      \
  OZ_Term arg0 = OZ_getCArg(0);						      \
  OZ_Term arg1 = OZ_getCArg(1);						      \
  OZ_Term arg2 = OZ_getCArg(2);						      \
  State state=InlineName(arg0,arg1,arg2,help);		  		      \
  switch (state) {							      \
  case SUSPEND:								      \
    return OZ_suspendOnVar3(arg0,arg1,arg2);				      \
  case FAILED:								      \
    return state;							      \
  case PROCEED:								      \
  default:								      \
    return(OZ_unify(help,OZ_getCArg(3)));				      \
  }									      \
									      \
}									      \
OZ_C_proc_end

#define DECLAREBOOLFUN1(BIfun,BIifun,BIirel) 				      \
State BIifun(TaggedRef val, TaggedRef &out)			      \
{									      \
  State state = BIirel(val);						      \
  switch(state) {							      \
  case PROCEED: out = NameTrue;  return PROCEED;			      \
  case FAILED:  out = NameFalse; return PROCEED;			      \
  default: return state;						      \
  }									      \
}									      \
DECLAREBI_USEINLINEFUN1(BIfun,BIifun)

#define DECLAREBOOLFUN2(BIfun,BIifun,BIirel) 				      \
State BIifun(TaggedRef val1, TaggedRef val2, TaggedRef &out)	      \
{									      \
  State state = BIirel(val1,val2);					      \
  switch(state) {							      \
  case PROCEED: out = NameTrue;  return PROCEED;			      \
  case FAILED:  out = NameFalse; return PROCEED;			      \
  default: return state;						      \
  }									      \
}									      \
DECLAREBI_USEINLINEFUN2(BIfun,BIifun)

BuiltinTabEntry *BIinit();
BuiltinTabEntry *BIadd(char *name,int arity,OZ_CFun fun,
		       Bool replace = NO, IFOR infun=(IFOR) NULL);
BuiltinTabEntry *BIaddSpecial(char *name,int arity,BIType t,
			      Bool replace = NO);

class BuiltinTabEntry {
  friend class Debugger;
public:
  BuiltinTabEntry (Literal *name,int arty,OZ_CFun fn,
		   IFOR infun=NULL)
  : printname(makeTaggedLiteral(name)), arity(arty),fun(fn),
    inlineFun(infun), type(BIDefault)
  {
    Assert(isAtom(printname));
  }
  BuiltinTabEntry (char *s,int arty,OZ_CFun fn,
		   IFOR infun=NULL)
  : arity(arty),fun(fn), inlineFun(infun), type(BIDefault)
  {
    printname = makeTaggedAtom(s);
    Assert(isAtom(printname));
  }
  BuiltinTabEntry (char *s,int arty,OZ_CFun fn,BIType t,
		   IFOR infun=NULL)
    : arity(arty),fun(fn), inlineFun(infun), type(t) {
      printname = makeTaggedAtom(s);
    }
  BuiltinTabEntry (char *s,int arty,BIType t, IFOR infun=(IFOR)NULL)
    : arity(arty),fun((OZ_CFun)NULL), inlineFun(infun), type(t)
  {
    printname = makeTaggedAtom(s);
    Assert(isAtom(printname));
  }

  ~BuiltinTabEntry () {}

  OZPRINT;
  OZ_CFun getFun() { return fun; }
  int getArity() { return arity; }
  char *getPrintName() { return tagged2Literal(printname)->getPrintName(); }
  TaggedRef getName() { return printname; }
  IFOR getInlineFun() { return inlineFun; } 
  BIType getType() { return type; } 

private:

  TaggedRef printname; //must be atom
  int arity;
  OZ_CFun fun;
  IFOR inlineFun;
  BIType type;
};



class Builtin: public ConstTerm {
friend void ConstTerm::gcConstRecurse(void);
private:
  BuiltinTabEntry *fun;
  TaggedRef suspHandler; // this one is called, when it must suspend
protected:
  RefsArray gRegs;       // context;
public:
  Builtin(BuiltinTabEntry *fn, TaggedRef handler, RefsArray gregs = NULL)
    : suspHandler(handler), fun(fn), ConstTerm(Co_Builtin),
    gRegs (gregs) {}

  Builtin *clone() { return new Builtin(fun,suspHandler,gRegs); }

  OZPRINT;
  OZPRINTLONG;

  int getArity()       { return fun->getArity(); }
  OZ_CFun getFun()     { return fun->getFun(); }
  char *getPrintName() { return fun->getPrintName(); }
  TaggedRef getName()  { return fun->getName(); }
  BIType getType()     { return fun->getType(); } 

  TaggedRef getSuspHandler() { 
    return suspHandler;
  }
  TaggedRef getDBGHandler() { 
    return suspHandler == makeTaggedNULL() ? nil() : suspHandler;
  }
  BuiltinTabEntry *getBITabEntry() {return fun;}
};


// -----------------------------------------------------------------------
// tables

class BuiltinTab : public HashTable {
public:
  BuiltinTab(int sz) : HashTable(CHARTYPE,sz) {};
  ~BuiltinTab() {};
  unsigned memRequired(void) {
    return HashTable::memRequired(sizeof(BuiltinTabEntry));
  }
  char * getName(void * fp) {
    HashNode * hn = getFirst();
    for (; hn != NULL; hn = getNext(hn)) {
      BuiltinTabEntry * abit = (BuiltinTabEntry *) hn->value;
      if (abit->getInlineFun() == (IFOR) fp ||
	  abit->getFun() == (OZ_CFun) fp)
	return hn->key.fstr;
    }
    return "???";
  }
};

extern BuiltinTab builtinTab;

/*
 * Essential Note:
 *  If gregs == NULL, *that* builtin was already applied,
 *  and 'isSeen' says 'OK'!
 *  'hasSeen' removes simply the gregs;
 */
class OneCallBuiltin: public Builtin {
public:
  USEHEAPMEMORY;

  OneCallBuiltin (BuiltinTabEntry *fn, RefsArray gregs)
    : Builtin (fn, (TaggedRef) 0, gregs) {}

  inline Bool isSeen () { return (gRegs == NULL); }
  inline RefsArray &getGRegs() { return(gRegs); }
  inline void hasSeen () { gRegs = (RefsArray) NULL; }
};

class SolvedBuiltin: public Builtin {
public:
  USEHEAPMEMORY;

  SolvedBuiltin(BuiltinTabEntry *fn, RefsArray gregs)
    : Builtin (fn, (TaggedRef) 0, gregs) {}

  inline RefsArray &getGRegs() { return(gRegs); }
};


extern State dotInline(TaggedRef term, TaggedRef fea, TaggedRef &out);
extern State uparrowInline(TaggedRef term, TaggedRef fea, TaggedRef &out);


#endif

