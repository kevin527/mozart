/*
 *  Authors:
 *    Kostja Popow (popow@ps.uni-sb.de)
 *    Michael Mehl (mehl@dfki.de)
 *    Ralf Scheidhauer (Ralf.Scheidhauer@ps.uni-sb.de)
 *
 *  Contributors:
 *    Benjamin Lorenz (lorenz@ps.uni-sb.de)
 *    Leif Kornstaedt (kornstae@ps.uni-sb.de)
 *    Tobias Mueller (tmueller@ps.uni-sb.de)
 *    Christian Schulte <schulte@ps.uni-sb.de>
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
 *     http://www.mozart-oz.org
 *
 *  See the file "LICENSE" or
 *     http://www.mozart-oz.org/LICENSE.html
 *  for information on usage and redistribution
 *  of this file, and for a DISCLAIMER OF ALL
 *  WARRANTIES.
 *
 */

// The main engine

#include <stdarg.h>

#include "am.hh"
#include "thr_int.hh"
#include "debug.hh"
#include "prop_int.hh"
#include "var_of.hh"
#include "codearea.hh"
#include "builtins.hh"
#include "indexing.hh"

#include "boot-manager.hh"
#include "dictionary.hh"
#include "copycode.hh"
#include "trace.hh"
#include "os.hh"

#ifdef OUTLINE
#define inline
#endif

// -----------------------------------------------------------------------
// Object stuff
// -----------------------------------------------------------------------

inline
Abstraction *getSendMethod(Object *obj, TaggedRef label, SRecordArity arity,
                           InlineCache *cache, RefsArray X)
{
  Assert(oz_isFeature(label));
  return cache->lookup(obj->getClass(),label,arity,X);
}

// -----------------------------------------------------------------------
// *** EXCEPTION stuff
// -----------------------------------------------------------------------

#define RAISE_APPLY(fun,args)                           \
  (void) oz_raise(E_ERROR,E_KERNEL,"apply",2,fun,args); \
  RAISE_THREAD;


static
void enrichTypeException(TaggedRef value,const char *fun, OZ_Term args)
{
  OZ_Term e = OZ_subtree(value,newSmallInt(1));
  OZ_putArg(e,1,OZ_atom((OZ_CONST char*)fun));
  OZ_putArg(e,2,args);
}

static
TaggedRef formatError(TaggedRef info, TaggedRef val, TaggedRef traceBack) {
  OZ_Term d = OZ_record(AtomD,oz_mklist(AtomInfo,AtomStack));
  OZ_putSubtree(d, AtomStack, traceBack);
  OZ_putSubtree(d, AtomInfo,  info);

  return OZ_adjoinAt(val, AtomDebug, d);
}


#define RAISE_TYPE1(fun,args)                           \
  enrichTypeException(e->exception.value,fun,args);     \
  RAISE_THREAD;

#define RAISE_TYPE1_FUN(fun,args) \
  RAISE_TYPE1(fun, appendI(args,oz_mklist(oz_newVariable())));

#define RAISE_TYPE_NEW(bi,loc) \
  RAISE_TYPE1(bi->getPrintName(), biArgs(loc,X));

#define RAISE_TYPE(bi) \
  RAISE_TYPE1(bi->getPrintName(), OZ_toList(bi->getArity(),X));

/*
 * Handle Failure macros (HF)
 */

Bool AM::hf_raise_failure()
{
  if (!oz_onToplevel() && !oz_currentThread()->isCatch())
    return OK;

  exception.info  = NameUnit;
  exception.value = RecordFailure;
  exception.debug = ozconf.errorDebug;
  return NO;
}

// This macro is optimized such that the term T is only created
// when needed, so don't pass it as argument to functions.
#define HF_RAISE_FAILURE(T)                             \
   if (e->hf_raise_failure())                           \
     return T_FAILURE;                                  \
   if (ozconf.errorDebug) e->setExceptionInfo(T);       \
   RAISE_THREAD;


#define HF_EQ(X,Y)    HF_RAISE_FAILURE(OZ_mkTupleC("eq",2,X,Y))
#define HF_TELL(X,Y)  HF_RAISE_FAILURE(OZ_mkTupleC("tell",2,X,Y))
#define HF_APPLY(N,A) HF_RAISE_FAILURE(OZ_mkTupleC("apply",2,N,A))
#define HF_BI(bi)     HF_APPLY(bi->getName(),OZ_toList(bi->getArity(),X));
#define HF_BI_NEW(bi,loc)   HF_APPLY(bi->getName(),biArgs(loc,X));

#define CheckArity(arityExp,proc)                                          \
if (predArity != arityExp) {                                               \
  (void) oz_raise(E_ERROR,E_KERNEL,"arity",2,proc,OZ_toList(predArity,X)); \
  RAISE_THREAD;                                                    \
}

// -----------------------------------------------------------------------
// *** ???
// -----------------------------------------------------------------------

/*
 * make an record
 *  the subtrees are initialized with new variables
 */
static
TaggedRef mkRecord(TaggedRef label,SRecordArity ff)
{
  SRecord *srecord = SRecord::newSRecord(label,ff,getWidth(ff));
  srecord->initArgs();
  return makeTaggedSRecord(srecord);
}

// optimized RefsArray allocation
inline
RefsArray allocateY(int n)
{
  int sz = (n+1) * sizeof(TaggedRef);
  RefsArray a = (RefsArray) freeListMalloc(sz);
  a += 1;
  initRefsArray(a,n,OK);
  return a;
}

inline
void deallocateY(RefsArray a, int sz)
{
  Assert(getRefsArraySize(a)==sz);
  Assert(!isFreedRefsArray(a));
#ifdef DEBUG_CHECK
  markFreedRefsArray(a);
#else
  freeListDispose(a-1,(sz+1) * sizeof(TaggedRef));
#endif
}

inline
void deallocateY(RefsArray a)
{
  deallocateY(a,getRefsArraySize(a));
}

void buildRecord(ProgramCounter PC, RefsArray Y, Abstraction *CAP);

// -----------------------------------------------------------------------
// *** ???
// -----------------------------------------------------------------------

#define DoSwitchOnTerm(indexTerm,table)                                 \
      TaggedRef term = indexTerm;                                       \
      DEREF(term,termPtr,_2);                                           \
                                                                        \
      if (oz_isLTuple(term)) {                                          \
        int offset = table->listLabel;                                  \
        if (!offset) offset = table->elseLabel;                         \
        sPointer = tagged2LTuple(term)->getRef();                       \
        JUMPRELATIVE(offset);                                           \
      } else {                                                          \
        TaggedRef *sp = sPointer;                                       \
        int offset = switchOnTermOutline(term,termPtr,table,sp);        \
        sPointer = sp;                                                  \
        if (offset) {                                                   \
          JUMPRELATIVE(offset);                                         \
        } else {                                                        \
          SUSP_PC(termPtr,PC);                                          \
        }                                                               \
      }


// most probable case first: local UVar
// if (isUVar(var) && isCurrentBoard(tagged2VarHome(var))) {
// more efficient:
inline
void bindOPT(OZ_Term *varPtr, OZ_Term term)
{
  Assert(isUVar(*varPtr));
  if (!am.currentUVarPrototypeEq(*varPtr) && !oz_isLocalUVar(varPtr)) {
    DoBindAndTrail(varPtr,term);
  } else {
    doBind(varPtr,term);
  }
}

/* specially optimized unify: test two most probable cases first:
 *
 *     1. bind a unconstraint local variable to a non-var
 *     2. test two non-variables for equality
 */
inline
OZ_Return fastUnify(OZ_Term A, OZ_Term B) {
  OZ_Term term1 = A;
  DEREF0(term1,term1Ptr,_1);

  OZ_Term term2 = B;
  DEREF0(term2,term2Ptr,_2);

  if (!oz_isVariable(term2)) {
    if (am.currentUVarPrototypeEq(term1)) {
      doBind(term1Ptr,term2);
      goto exit;
    }
    if (term1==term2) {
      goto exit;
    }
  } else if (!oz_isVariable(term1) && am.currentUVarPrototypeEq(term2)) {
    doBind(term2Ptr,term1);
    goto exit;
  }

  return oz_unify(A,B);

 exit:
  return PROCEED;
}

/*
 * new builtins support
 */

static OZ_Term *savedX = NULL;

OZ_Return oz_bi_wrapper(Builtin *bi,OZ_Term *X)
{
  Assert(am.isEmptySuspendVarList());
  Assert(am.isEmptyPreparedCalls());

  const int inAr = bi->getInArity();
  const int outAr = bi->getOutArity();

  if (savedX)
    delete [] savedX;
  savedX = new OZ_Term[outAr];

  int i;
  for (i=outAr; i--; ) savedX[i]=X[inAr+i];

  OZ_Return ret1 = bi->getFun()(X,OZ_ID_MAP);
  if (ret1!=PROCEED) {
    switch (ret1) {
    case FAILED:
    case RAISE:
    case BI_TYPE_ERROR:
    case SUSPEND:
      {
        // restore X
        for (int j=outAr; j--; ) {
          X[inAr+j]=savedX[j];
        }
        return ret1;
      }
    case PROCEED:
    case BI_PREEMPT:
    case BI_REPLACEBICALL:
      break;
    default:
      OZ_error("Builtin: Unknown return value.\nDoes your builtin return a meaningful value?\nThis value is definitely unknown: %d",ret1);
      return FAILED;
    }
  }
  for (i=outAr;i--;) {
    OZ_Return ret2 = fastUnify(X[inAr+i],savedX[i]);
    if (ret2!=PROCEED) {
      switch (ret2) {
      case FAILED:
      case RAISE:
      case BI_TYPE_ERROR:
        {
          // restore X in case of error
          for (int j=outAr; j--; ) {
            X[inAr+j]=savedX[j];
          }
          return ret2;
        }
      case SUSPEND:
        am.emptySuspendVarList();
        am.prepareCall(BI_Unify,X[inAr+i],savedX[i]);
        ret1=BI_REPLACEBICALL;
        break;
      case BI_REPLACEBICALL:
        ret1=BI_REPLACEBICALL;
        break;
      default:
        Assert(0);
      }
    }
  }
  return ret1;
}

static
void set_exception_info_call(Builtin *bi,OZ_Term *X, int *map=OZ_ID_MAP)
{
  if (bi==bi_raise||bi==bi_raiseError) return;

  int iarity = bi->getInArity();
  int oarity = bi->getOutArity();

  OZ_Term args=oz_nil();
  for (int j = iarity; j--;) {
    args=oz_cons(X[map == OZ_ID_MAP? j : map[j]],args);
  }
  am.setExceptionInfo(OZ_mkTupleC("fapply",3,
                                  makeTaggedConst(bi),
                                  args,
                                  OZ_int(oarity)));
}

static
OZ_Term biArgs(OZ_Location *loc, OZ_Term *X) {
  OZ_Term out=oz_nil();
  int i;
  for (i=loc->getOutArity(); i--; ) {
    out=oz_cons(oz_newVariable(),out);
  }
  for (i=loc->getInArity(); i--; ) {
    out=oz_cons(X[loc->in(i)],out);
  }
  return out;
}

// -----------------------------------------------------------------------
// *** patchToFastCall: self modifying code!
// -----------------------------------------------------------------------


void patchToFastCall(Abstraction *abstr, ProgramCounter PC, Bool isTailCall)
{
  AbstractionEntry *entry = new AbstractionEntry(NO);
  entry->setPred(abstr);
  CodeArea *code = CodeArea::findBlock(PC);
  code->writeAbstractionEntry(entry, PC+1);
  CodeArea::writeOpcode(isTailCall ? FASTTAILCALL : FASTCALL, PC);
}




// -----------------------------------------------------------------------
// *** CALL HOOK
// -----------------------------------------------------------------------


/* the hook functions return:
     TRUE: must reschedule
     FALSE: can continue
   */

#define DET_COUNTER 10000
inline
Bool hookCheckNeeded()
{
#if defined(DEBUG_DET)
  static int counter = DET_COUNTER;
  if (--counter < 0) {
    am.handleAlarm(CLOCK_TICK/1000);   // simulate an alarm
    counter = DET_COUNTER;
  }
#endif

  return am.isSetSFlag();
}


// -----------------------------------------------------------------------
// ??? <- Bob, Justus und Peter
// -----------------------------------------------------------------------

#define RAISE_THREAD_NO_PC                      \
  e->exception.pc=NOCODE;                       \
  goto LBLraise;

#define RAISE_THREAD                            \
  e->exception.pc=PC;                           \
  e->exception.y=Y;                             \
  e->exception.cap=CAP;                         \
  goto LBLraise;


/* macros are faster ! */
#define emulateHookCall(e,Code)                 \
   if (hookCheckNeeded()) {                     \
       Code;                                    \
       return T_PREEMPT;                        \
   }

#define emulateHookPopTask(e) emulateHookCall(e,;)


#define ChangeSelf(obj)                         \
      e->changeSelf(obj);

#define PushCont(_PC)  CTS->pushCont(_PC,Y,CAP);
#define PushContX(_PC) pushContX(CTS,_PC,Y,CAP,X);

void pushContX(TaskStack *stk,
               ProgramCounter pc,RefsArray y,Abstraction *cap,
               RefsArray x);

/* NOTE:
 * in case we have call(x-N) and we have to switch process or do GC
 * we have to save as cont address Pred->getPC() and NOT PC
 */
#define CallDoChecks(Pred)                                                  \
     Y = NULL;                                                              \
     CAP = Pred;                                                            \
     emulateHookCall(e,PushContX(Pred->getPC()));


// -----------------------------------------------------------------------
// *** THREADED CODE
// -----------------------------------------------------------------------

#if defined(RECINSTRFETCH) && defined(THREADED)
 Error: RECINSTRFETCH requires THREADED == 0;
#endif

#define INCFPC(N) PC += N

#if !defined(DEBUG_EMULATOR) && !defined(DISABLE_INSTRPROFILE) && defined(__GNUC__)
#define asmLbl(INSTR) asm(" " #INSTR ":");
#else
#define asmLbl(INSTR)
#endif

#ifdef THREADED
#define Case(INSTR) INSTR##LBL : asmLbl(INSTR);

#ifdef DELAY_SLOT
// let gcc fill in the delay slot of the "jmp" instruction:
#define DISPATCH(INC) {                         \
  intlong aux = *(PC+INC);                      \
  INCFPC(INC);                                  \
  goto* (void*) (aux|textBase);                 \
}
#elif defined(_MSC_VER)
#define DISPATCH(INC) {                         \
   PC += INC;                                   \
   int aux = *PC;                               \
   __asm jmp aux                                \
}

#else
#define DISPATCH(INC) {                         \
  INCFPC(INC);                                  \
  goto* (void*) ((*PC)|textBase);               \
}
#endif

#else /* THREADED */

#define Case(INSTR)   case INSTR :  INSTR##LBL : asmLbl(INSTR);
#define DISPATCH(INC) INCFPC(INC); goto LBLdispatcher

#endif

#define JUMPRELATIVE(offset) Assert(offset!=0); DISPATCH(offset)
#define JUMPABSOLUTE(absaddr) PC=absaddr; DISPATCH(0)


#define SETTMPA(V) { TMPA = &(V); }
#define SETTMPB(V) { TMPB = &(V); }
#define GETTMPA()  (*(TMPA))
#define GETTMPB()  (*(TMPB))

#define SETAUX(V)   { auxTaggedA = (V); };


#ifdef FASTREGACCESS
#define RegAccess(Reg,Index) (*(RefsArray)((intlong) Reg + Index))
#else
#define RegAccess(Reg,Index) (Reg[Index])
#endif

#define XPC(N) RegAccess(X,getRegArg(PC+N))
#define YPC(N) RegAccess(Y,getRegArg(PC+N))
#define GPC(N) RegAccess((CAP->getGRef()),getRegArg(PC+N))

/* define REGOPT if you want the into register optimization for GCC */
#if defined(REGOPT) && __GNUC__ >= 2 && (defined(ARCH_I486) || defined(ARCH_MIPS) || defined(OSF1_ALPHA) || defined(ARCH_SPARC)) && !defined(DEBUG_CHECK)
#define Into(Reg) asm(#Reg)

#ifdef ARCH_I486
#define Reg1 asm("%esi")
#define Reg2
#define Reg3
#define Reg4
#define Reg5
#define Reg6
#define Reg7
#endif

#ifdef ARCH_SPARC
#define Reg1 asm("i0")
#define Reg2 asm("i1")
#define Reg3 asm("i2")
#define Reg4 asm("i3")
#define Reg5 asm("i4")
#define Reg6 asm("i5")
#define Reg7 asm("l0")
#endif

#ifdef OSF1_ALPHA
#define Reg1 asm("$9")
#define Reg2 asm("$10")
#define Reg3
#define Reg4
#define Reg5
#define Reg6
#define Reg7
#endif

#ifdef ARCH_MIPS
#define Reg1 asm("$16")
#define Reg2 asm("$17")
#define Reg3 asm("$18")
#define Reg4 asm("$19")
#define Reg5 asm("$20")
#define Reg6
#define Reg7
#endif

#else

#define Reg1
#define Reg2
#define Reg3
#define Reg4
#define Reg5
#define Reg6
#define Reg7

#endif

/*
 * Handling of the READ/WRITE mode bit:
 * last significant bit of sPointer set iff in WRITE mode
 */

#define SetReadMode  lastGetRecord = PC
#define SetWriteMode (sPointer = (TaggedRef *)((long)sPointer+1));

#define InWriteMode (((long)sPointer)&1)

#define GetSPointerWrite(ptr) (TaggedRef*)(((long)ptr)-1)


/* spointer also abused for return values of functions */
#define SetFunReturn(val) sPointer = (TaggedRef*) val
#define GetFunReturn()    (TaggedRef)sPointer


#ifdef DEBUG_LIVENESS
extern void checkLiveness(ProgramCounter PC,TaggedRef *X, int maxX);
#define CheckLiveness(PC) checkLiveness(PC,X,CAP->getPred()->getMaxX())
#else
#define CheckLiveness(PC)
#endif

// ------------------------------------------------------------------------
// ???
// ------------------------------------------------------------------------

#define MAGIC_RET goto LBLMagicRet;

#define SUSP_PC(TermPtr,PC) {                   \
   CheckLiveness(PC);                           \
   PushContX(PC);                               \
   tmpRet = oz_var_addSusp(TermPtr,CTT);        \
   MAGIC_RET;                                   \
}

/*
 * create the suspension for builtins returning SUSPEND
 *
 * PRE: no reference chains !!
 */


#define SUSPENDONVARLIST                        \
{                                               \
  tmpRet = e->suspendOnVarList(CTT);            \
  MAGIC_RET;                                    \
}

static
OZ_Return suspendInline(Thread *th, OZ_Term A,OZ_Term B=0,OZ_Term C=0)
{
  if (C) {
    DEREF (C, ptr, _1);
    if (oz_isVariable(C)) {
      OZ_Return ret = oz_var_addSusp(ptr, th);
      if (ret != SUSPEND) return ret;
    }
  }
  if (B) {
    DEREF (B, ptr, _1);
    if (oz_isVariable(B)) {
      OZ_Return ret = oz_var_addSusp(ptr, th);
      if (ret != SUSPEND) return ret;
    }
  }
  {
    DEREF (A, ptr, _1);
    if (oz_isVariable(A)) {
      OZ_Return ret = oz_var_addSusp(ptr, th);
      if (ret != SUSPEND) return ret;
    }
  }
  return SUSPEND;
}

// -----------------------------------------------------------------------
// outlined auxiliary functions
// -----------------------------------------------------------------------

static
TaggedRef makeMessage(SRecordArity srecArity, TaggedRef label, TaggedRef *X)
{
  int width = getWidth(srecArity);
  if (width == 0) {
    return label;
  }

  if (width == 2 && oz_eq(label,AtomCons))
    return makeTaggedLTuple(new LTuple(X[0],X[1]));

  SRecord *tt;
  if(sraIsTuple(srecArity)) {
    tt = SRecord::newSRecord(label,width);
  } else {
    tt = SRecord::newSRecord(label,getRecordArity(srecArity));
  }
  for (int i = width-1;i >= 0; i--) {
    tt->setArg(i,X[i]);
  }
  TaggedRef ret = makeTaggedSRecord(tt);

  return ret;
}


// ------------------------------------------------------------------------
// ------------------------------------------------------------------------
// ************************************************************************
// ************************************************************************
// THE BIG EMULATE LOOP STARTS
// ************************************************************************
// ************************************************************************
// ------------------------------------------------------------------------
// ------------------------------------------------------------------------


/* &Var prevent Var to be allocated to a register,
 * increases chances that variables declared as "register"
 * will be really allocated to registers
 */

#define NoReg(Var) { void *p = &Var; }

// short names
# define CBB (oz_currentBoard())
# define CTT (oz_currentThread())
# define CTS (e->cachedStack)

int engine(Bool init)
{
// ------------------------------------------------------------------------
// *** Global Variables
// ------------------------------------------------------------------------
  /* ordered by importance: first variables will go into machine registers
   * if -DREGOPT is set
   */
  register ProgramCounter PC   Reg1 = 0;
#ifdef MANY_REGISTERS
  register TaggedRef *X        Reg2 = am.xRegs;
#else
  register TaggedRef * const X Reg2 = am.xRegs;
#endif
  register RefsArray Y         Reg3 = NULL;
  register TaggedRef *sPointer Reg4 = NULL;
  register AM * const e        Reg5 = &am;
  register Abstraction * CAP   Reg6 = NULL;

  Bool isTailCall              = NO;                NoReg(isTailCall);

  // handling perdio unification
  ProgramCounter lastGetRecord;                     NoReg(lastGetRecord);

  ConstTerm *predicate;     NoReg(predicate);
  int predArity;            NoReg(predArity);

  TaggedRef * TMPA, * TMPB;

  // optimized arithmetic and special cases for unification
  OZ_Return tmpRet;  NoReg(tmpRet);

  TaggedRef auxTaggedA, auxTaggedB;
  int auxInt;
  char *auxString;

#ifdef THREADED
  if (init) {
#include "instrtab.hh"
    CodeArea::init(instrTable);
    return 0;
  }
#else
  if (init) {
    CodeArea::init(NULL);
    return 0;
  }
  Opcode op = (Opcode) -1;
#endif

  // no preemption required, because of exception handling impl.
  goto LBLpopTaskNoPreempt;

// ------------------------------------------------------------------------
// *** Emulator: execute instructions
// ------------------------------------------------------------------------

 LBLemulate:
  asmLbl(EMULATE);

  JUMPABSOLUTE( PC );

  asmLbl(END_EMULATE);
#ifndef THREADED
LBLdispatcher:
  asmLbl(DISPATCH);

#ifdef RECINSTRFETCH
  CodeArea::recordInstr(PC);
#endif

  DebugTrace( if (!ozd_trace("emulate",PC,Y,CAP)) {
    return T_FAILURE;
  });

  op = CodeArea::getOP(PC);
  // displayCode(PC,1);

#ifdef PROFILE_INSTR
  {
    static Opcode lastop = OZERROR;
    if (op < PROFILE_INSTR_MAX) {
      ozstat.instr[op]++;
      ozstat.instrCollapsable[lastop][op]++;
      lastop = op;
    }
  }
#endif

  Assert(am.isEmptySuspendVarList());
  Assert(am.isEmptyPreparedCalls());
  switch (op) {
#endif

// -------------------------------------------------------------------------
// INSTRUCTIONS: TERM: MOVE/UNIFY/CREATEVAR/...
// -------------------------------------------------------------------------

  Case(MOVEXX)
    {
      XPC(2) = XPC(1);
      DISPATCH(3);
    }
  Case(MOVEXY)
    {
      YPC(2) = XPC(1);
      DISPATCH(3);
    }

  Case(MOVEYX)
    {
      XPC(2) = YPC(1);
      DISPATCH(3);
    }
  Case(MOVEYY)
    {
      YPC(2) = YPC(1);
      DISPATCH(3);
    }

  Case(MOVEGX)
    {
      XPC(2) = GPC(1);
      DISPATCH(3);
    }
  Case(MOVEGY)
    {
      YPC(2) = GPC(1);
      DISPATCH(3);
    }

  Case(MOVEMOVEXYXY)
    {
      YPC(2) = XPC(1); YPC(4) = XPC(3);
      DISPATCH(5);
    }
  Case(MOVEMOVEYXYX)
    {
      XPC(2) = YPC(1); XPC(4) = YPC(3);
      DISPATCH(5);
    }
  Case(MOVEMOVEYXXY)
    {
      XPC(2) = YPC(1); YPC(4) = XPC(3);
      DISPATCH(5);
    }
  Case(MOVEMOVEXYYX)
    {
      YPC(2) = XPC(1); XPC(4) = YPC(3);
      DISPATCH(5);
    }

  Case(CLEARY)
    {
      YPC(1) = NameVoidRegister;
      DISPATCH(2);
    }

  Case(GETSELF)
    {
      XPC(1) = makeTaggedConst(e->getSelf());
      DISPATCH(2);
    }

  Case(SETSELFG)
    {
      TaggedRef term = GPC(1);
      if (oz_isRef(term)) {
        DEREF(term,termPtr,tag);
        if (oz_isVariable(term)) {
          SUSP_PC(termPtr,PC);
        }
      }
      ChangeSelf(tagged2Object(term));
      DISPATCH(2);
    }


  Case(GETRETURNX)
    {
      XPC(1) = GetFunReturn();
      DISPATCH(2);
    }
  Case(GETRETURNY)
    {
      YPC(1) = GetFunReturn();
      DISPATCH(2);
    }
  Case(GETRETURNG)
    {
      GPC(1) = GetFunReturn();
      DISPATCH(2);
    }

  Case(FUNRETURNX)
    {
      SetFunReturn(XPC(1));
      if (Y) { deallocateY(Y); Y = NULL; }
      goto LBLpopTaskNoPreempt;
    }
  Case(FUNRETURNY)
    {
      SetFunReturn(YPC(1));
      if (Y) { deallocateY(Y); Y = NULL; }
      goto LBLpopTaskNoPreempt;
    }
  Case(FUNRETURNG)
    {
      SetFunReturn(GPC(1));
      if (Y) { deallocateY(Y); Y = NULL; }
      goto LBLpopTaskNoPreempt;
    }


  Case(CREATEVARIABLEX)
    {
      XPC(1) = oz_newVariableOPT();
      DISPATCH(2);
    }
  Case(CREATEVARIABLEY)
    {
      YPC(1) = oz_newVariableOPT();
      DISPATCH(2);
    }

  Case(CREATEVARIABLEMOVEX)
    {
      XPC(1) = XPC(2) = oz_newVariableOPT();
      DISPATCH(3);
    }
  Case(CREATEVARIABLEMOVEY)
    {
      YPC(1) = XPC(2) = oz_newVariableOPT();
      DISPATCH(3);
    }


  Case(UNIFYXY) SETAUX(YPC(2)); goto Unify;
  Case(UNIFYXG) SETAUX(GPC(2)); goto Unify;
  Case(UNIFYXX) SETAUX(XPC(2)); /* fall through */
  {
  Unify:
    const TaggedRef A   = XPC(1);
    const TaggedRef B   = auxTaggedA;
    const OZ_Return ret = fastUnify(A,B);
    if (ret == PROCEED) {
      DISPATCH(3);
    }
    if (ret == FAILED) {
      HF_EQ(A,B);
    }

    tmpRet = ret;
    goto LBLunifySpecial;
  }


  Case(PUTRECORDX)
    {
      TaggedRef label = getLiteralArg(PC+1);
      SRecordArity ff = (SRecordArity) getAdressArg(PC+2);
      SRecord *srecord = SRecord::newSRecord(label,ff,getWidth(ff));

      XPC(3) = makeTaggedSRecord(srecord);
      sPointer = srecord->getRef();

      DISPATCH(4);
    }
  Case(PUTRECORDY)
    {
      TaggedRef label = getLiteralArg(PC+1);
      SRecordArity ff = (SRecordArity) getAdressArg(PC+2);
      SRecord *srecord = SRecord::newSRecord(label,ff,getWidth(ff));

      YPC(3) = makeTaggedSRecord(srecord);
      sPointer = srecord->getRef();

      DISPATCH(4);
    }


  Case(PUTCONSTANTX)
    {
      XPC(2) = getTaggedArg(PC+1);
      DISPATCH(3);
    }
  Case(PUTCONSTANTY)
    {
      YPC(2) = getTaggedArg(PC+1);
      DISPATCH(3);
    }


  Case(PUTLISTX)
    {
      LTuple *term = new LTuple();
      XPC(1)   = makeTaggedLTuple(term);
      sPointer = term->getRef();
      DISPATCH(2);
    }
  Case(PUTLISTY)
    {
      LTuple *term = new LTuple();
      YPC(1)   = makeTaggedLTuple(term);
      sPointer = term->getRef();
      DISPATCH(2);
    }


  Case(SETVARIABLEX)
    {
      *sPointer = e->currentUVarPrototype();
      XPC(1)    = makeTaggedRef(sPointer++);
      DISPATCH(2);
    }
  Case(SETVARIABLEY)
    {
      *sPointer = e->currentUVarPrototype();
      YPC(1)    = makeTaggedRef(sPointer++);
      DISPATCH(2);
    }


  Case(SETVALUEX)
    {
      *sPointer++ = XPC(1);
      DISPATCH(2);
    }
  Case(SETVALUEY)
    {
      *sPointer++ = YPC(1);
      DISPATCH(2);
    }
  Case(SETVALUEG)
    {
      *sPointer++ = GPC(1);
      DISPATCH(2);
    }


  Case(SETCONSTANT)
    {
      *sPointer++ = getTaggedArg(PC+1);
      DISPATCH(2);
    }

  Case(SETPROCEDUREREF)
    {
      *sPointer++ = OZ_makeForeignPointer(getAdressArg(PC+1));
      DISPATCH(2);
    }

  Case(SETVOID)
    {
      int n = getPosIntArg(PC+1);
      while (n > 0) {
        *sPointer++ = e->currentUVarPrototype();
        n--;
      }
      DISPATCH(2);
    }


  Case(GETRECORDY) SETTMPA(YPC(3)); goto GetRecord;
  Case(GETRECORDG) SETTMPA(GPC(3)); goto GetRecord;
  Case(GETRECORDX) SETTMPA(XPC(3)); /* fall through */
  {
  GetRecord:
    TaggedRef label = getLiteralArg(PC+1);
    SRecordArity ff = (SRecordArity) getAdressArg(PC+2);

    TaggedRef term = GETTMPA();
    DEREF(term,termPtr,tag);

    if (isUVar(term)) {
      SRecord *srecord = SRecord::newSRecord(label,ff,getWidth(ff));
      bindOPT(termPtr,makeTaggedSRecord(srecord));
      sPointer = srecord->getRef();
      SetWriteMode;
      DISPATCH(4);
    } else if (oz_isVariable(term)) {
      Assert(isCVar(term));
      TaggedRef record;
      if (oz_onToplevel()) {
        GETTMPA() = record = OZ_newVariable();
        buildRecord(PC,Y,CAP);
        record = oz_deref(record);
        GETTMPA() = makeTaggedRef(termPtr);
      } else {
        SRecord *srecord = SRecord::newSRecord(label,ff,getWidth(ff));
        // mm2: optimize simple variables: use write mode
        // fill w/unb. var.
        srecord->initArgs();
        record=makeTaggedSRecord(srecord);
      }
      tmpRet = oz_var_bind(tagged2CVar(term),termPtr,record);

      if (tmpRet==PROCEED) {
        sPointer = tagged2SRecord(oz_deref(record))->getRef();
        SetReadMode;
        DISPATCH(4);
      }
      if (tmpRet!=FAILED) goto LBLunifySpecial;
      // fall through to failed
    } else if (isSRecordTag(tag) &&
               tagged2SRecord(term)->compareSortAndArity(label,ff)) {
      sPointer = tagged2SRecord(term)->getRef();
      SetReadMode;
      DISPATCH(4);
    }

    HF_TELL(GETTMPA(),mkRecord(label,ff));
  }


  Case(TESTLITERALG) SETAUX(GPC(1)); goto testLiteral;
  Case(TESTLITERALY) SETAUX(YPC(1)); goto testLiteral;
  Case(TESTLITERALX) SETAUX(XPC(1)); /* fall through */
  {
  testLiteral:
    TaggedRef term = auxTaggedA;
    TaggedRef atm  = getLiteralArg(PC+2);

    DEREF(term,termPtr,tag);
    if (oz_isVariable(term)) {
      if (isCVar(term) &&
          !oz_var_valid(tagged2CVar(term),termPtr,atm)) {
        // fail
        JUMPRELATIVE( getLabelArg(PC+3) );
      }
      SUSP_PC(termPtr,PC);
    }
    if (oz_eq(term,atm)) {
      DISPATCH(4);
    }
    // fail
    JUMPRELATIVE( getLabelArg(PC+3) );
  }

  Case(TESTBOOLG) SETAUX(GPC(1)); goto testBool;
  Case(TESTBOOLY) SETAUX(YPC(1)); goto testBool;
  Case(TESTBOOLX) SETAUX(XPC(1)); /* fall through */
  {
  testBool:
    TaggedRef term = auxTaggedA;
    DEREF(term,termPtr,tag);

    if (oz_eq(term,oz_true())) {
      DISPATCH(4);
    }

    if (oz_eq(term,oz_false())) {
      JUMPRELATIVE(getLabelArg(PC+2));
    }

    // mm2: kinded and ofs handling missing
    if (oz_isVariable(term)) {
      SUSP_PC(termPtr, PC);
    }

    JUMPRELATIVE( getLabelArg(PC+3) );
  }

  Case(TESTNUMBERG) SETAUX(GPC(1)); goto testNumber;
  Case(TESTNUMBERY) SETAUX(YPC(1)); goto testNumber;
  Case(TESTNUMBERX) SETAUX(XPC(1)); /* fall through */
  {
  testNumber:
    TaggedRef term = auxTaggedA;
    TaggedRef i = getNumberArg(PC+2);

    DEREF(term,termPtr,tag);

    /* optimized for integer case */
    if (isSmallIntTag(tag)) {
      if (smallIntEq(term,i)) {
        DISPATCH(4);
      }
      JUMPRELATIVE(getLabelArg(PC+3));
    }

    if (oz_numberEq(i,term)) {
      DISPATCH(4);
    }

    if (oz_isVariable(term)) {
      if (oz_isKinded(term) &&
          !oz_var_valid(tagged2CVar(term),termPtr,i)) {
        // fail
        JUMPRELATIVE( getLabelArg(PC+3) );
      }
      SUSP_PC(termPtr,PC);
    }
    // fail
    JUMPRELATIVE( getLabelArg(PC+3) );
  }


  Case(TESTRECORDG) SETAUX(GPC(1)); goto testRecord;
  Case(TESTRECORDY) SETAUX(YPC(1)); goto testRecord;
  Case(TESTRECORDX) SETAUX(XPC(1));  /* fall through */
  {
  testRecord:
    TaggedRef term = auxTaggedA;
    TaggedRef label = getLiteralArg(PC+2);
    SRecordArity sra = (SRecordArity) getAdressArg(PC+3);

    DEREF(term,termPtr,tag);
    if (isSRecordTag(tag)) {
      if (tagged2SRecord(term)->compareSortAndArity(label,sra)) {
        sPointer = tagged2SRecord(term)->getRef();
        DISPATCH(5);
      }
    } else if (oz_isVariable(term)) {
      if (!oz_isKinded(term)) {
        SUSP_PC(termPtr,PC);
      } else if (isCVar(term)) {
        OzVariable *cvar = tagged2CVar(term);
        if (cvar->getType() == OZ_VAR_OF) {
          OzOFVariable *ofsvar = (OzOFVariable *) cvar;
          Literal *lit = tagged2Literal(label);
          if (sraIsTuple(sra) && ofsvar->disentailed(lit,getTupleWidth(sra)) ||
              ofsvar->disentailed(lit,getRecordArity(sra))) {
            JUMPRELATIVE(getLabelArg(PC+4));
          }
          SUSP_PC(termPtr,PC);
        }
      }
      // fall through
    }
    // fail
    JUMPRELATIVE(getLabelArg(PC+4));
  }


  Case(TESTLISTG) SETAUX(GPC(1)); goto testList;
  Case(TESTLISTY) SETAUX(YPC(1)); goto testList;
  Case(TESTLISTX) SETAUX(XPC(1)); /* fall through */
  {
  testList:
    TaggedRef term = auxTaggedA;

    DEREF(term,termPtr,tag);
    if (isLTupleTag(tag)) {
      sPointer = tagged2LTuple(term)->getRef();
      DISPATCH(3);
    } else if (oz_isVariable(term)) {
      if (!oz_isKinded(term)) {
        SUSP_PC(termPtr,PC);
      } else if (isCVar(term)) {
        OzVariable *cvar = tagged2CVar(term);
        if (cvar->getType() == OZ_VAR_OF) {
          OzOFVariable *ofsvar = (OzOFVariable *) cvar;
          if (ofsvar->disentailed(tagged2Literal(AtomCons),2)) {
            JUMPRELATIVE(getLabelArg(PC+2));
          }
          SUSP_PC(termPtr,PC);
        }
      }
      // fall through
    }
    // fail
    JUMPRELATIVE(getLabelArg(PC+2));
  }


  Case(GETLITERALX)
    {
      TaggedRef atm = getLiteralArg(PC+1);

      TaggedRef term = XPC(2);
      DEREF(term,termPtr,tag);

      if (isUVar(tag)) {
        bindOPT(termPtr, atm);
        DISPATCH(3);
      }

      if (oz_eq(term,atm)) {
        DISPATCH(3);
      }

      auxTaggedA = XPC(2);
      goto getLiteralComplicated;
    }
  Case(GETLITERALY)
    {
      TaggedRef atm = getLiteralArg(PC+1);

      TaggedRef term = YPC(2);
      DEREF(term,termPtr,tag);

      if (isUVar(tag)) {
        bindOPT(termPtr, atm);
        DISPATCH(3);
      }

      if (oz_eq(term,atm)) {
        DISPATCH(3);
      }

      auxTaggedA = YPC(2);
      goto getLiteralComplicated;
    }
  Case(GETLITERALG)
    {
      TaggedRef atm = getLiteralArg(PC+1);

      TaggedRef term = GPC(2);
      DEREF(term,termPtr,tag);

      if (isUVar(tag)) {
        bindOPT(termPtr, atm);
        DISPATCH(3);
      }

      if (oz_eq(term,atm)) {
        DISPATCH(3);
      }

      auxTaggedA = GPC(2);
      goto getLiteralComplicated;
    }


  {
  getLiteralComplicated:
    TaggedRef term = auxTaggedA;
    TaggedRef atm = getLiteralArg(PC+1);
    DEREF(term,termPtr,tag);
    if (isCVar(term)) {
      tmpRet = oz_var_bind(tagged2CVar(term),termPtr,atm);
      if (tmpRet==PROCEED) { DISPATCH(3); }
      if (tmpRet!=FAILED)  { goto LBLunifySpecial; }
      // fall through to fail
    }

    HF_TELL(auxTaggedA, atm);
  }


  Case(GETNUMBERX)
    {
      TaggedRef i = getNumberArg(PC+1);
      TaggedRef term = XPC(2);
      DEREF(term,termPtr,tag);

      if (isUVar(tag)) {
        bindOPT(termPtr, i);
        DISPATCH(3);
      }

      if (oz_numberEq(term,i)) {
        DISPATCH(3);
      }

      if (isCVar(term)) {
        tmpRet=oz_var_bind(tagged2CVar(term),termPtr,i);
        if (tmpRet==PROCEED) { DISPATCH(3); }
        if (tmpRet!=FAILED)  { goto LBLunifySpecial; }
        // fall through to fail
      }

      HF_TELL(XPC(2), getNumberArg(PC+1));
    }
  Case(GETNUMBERY)
    {
      TaggedRef i = getNumberArg(PC+1);
      TaggedRef term = YPC(2);
      DEREF(term,termPtr,tag);

      if (isUVar(tag)) {
        bindOPT(termPtr, i);
        DISPATCH(3);
      }

      if (oz_numberEq(term,i)) {
        DISPATCH(3);
      }

      if (isCVar(term)) {
        tmpRet=oz_var_bind(tagged2CVar(term),termPtr,i);
        if (tmpRet==PROCEED) { DISPATCH(3); }
        if (tmpRet!=FAILED)  { goto LBLunifySpecial; }
        // fall through to fail
      }

      HF_TELL(YPC(2), getNumberArg(PC+1));
    }
  Case(GETNUMBERG)
    {
      TaggedRef i = getNumberArg(PC+1);
      TaggedRef term = GPC(2);
      DEREF(term,termPtr,tag);

      if (isUVar(tag)) {
        bindOPT(termPtr, i);
        DISPATCH(3);
      }

      if (oz_numberEq(term,i)) {
        DISPATCH(3);
      }

      if (isCVar(term)) {
        tmpRet=oz_var_bind(tagged2CVar(term),termPtr,i);
        if (tmpRet==PROCEED) { DISPATCH(3); }
        if (tmpRet!=FAILED)  { goto LBLunifySpecial; }
        // fall through to fail
      }

      HF_TELL(GPC(2), getNumberArg(PC+1));
    }

/* getListValVar(N,R,M) == getList(X[N]) unifyVal(R) unifyVar(X[M]) */
  Case(GETLISTVALVARX)
    {
      TaggedRef term = XPC(1);
      DEREF(term,termPtr,tag);

      if (isUVar(term)) {
        register LTuple *ltuple = new LTuple();
        ltuple->setHead(XPC(2));
        ltuple->setTail(e->currentUVarPrototype());
        bindOPT(termPtr,makeTaggedLTuple(ltuple));
        XPC(3) = makeTaggedRef(ltuple->getRef()+1);
        DISPATCH(4);
      }
      if (oz_isLTuple(term)) {
        TaggedRef *argg = tagged2LTuple(term)->getRef();
        OZ_Return aux = fastUnify(XPC(2),
                                  makeTaggedRef(argg));
        if (aux==PROCEED) {
          XPC(3) = tagged2NonVariable(argg+1);
          DISPATCH(4);
        }
        if (aux!=FAILED) {
          tmpRet = aux;
          goto LBLunifySpecial;
        }
        HF_TELL(XPC(2), makeTaggedRef(argg));
      }

      if (oz_isVariable(term)) {
        Assert(isCVar(term));
        // mm2: why not new LTuple(h,t)? OPT?
        register LTuple *ltuple = new LTuple();
        ltuple->setHead(XPC(2));
        ltuple->setTail(e->currentUVarPrototype());
        tmpRet=oz_var_bind(tagged2CVar(term),termPtr,makeTaggedLTuple(ltuple));
        if (tmpRet==PROCEED) {
          XPC(3) = makeTaggedRef(ltuple->getRef()+1);
          DISPATCH(4);
        }
        if (tmpRet!=FAILED)  { goto LBLunifySpecial; }

        HF_TELL(XPC(1), makeTaggedLTuple(ltuple));
      }

      HF_TELL(XPC(1), makeTaggedLTuple(new LTuple(XPC(2),e->currentUVarPrototype())));
    }


  Case(GETLISTG) SETTMPA(GPC(1)); goto getList;
  Case(GETLISTY) SETTMPA(YPC(1)); goto getList;
  Case(GETLISTX) SETTMPA(XPC(1)); /* fall through */
    {
    getList:
      TaggedRef term = GETTMPA();
      DEREF(term,termPtr,tag);

      if (isUVar(term)) {
        LTuple *ltuple = new LTuple();
        sPointer = ltuple->getRef();
        SetWriteMode;
        bindOPT(termPtr,makeTaggedLTuple(ltuple));
        DISPATCH(2);
      } else if (oz_isVariable(term)) {
        Assert(isCVar(term));
        TaggedRef record;
        if (oz_onToplevel()) {
          TaggedRef *regPtr = &(GETTMPA());
          TaggedRef savedTerm = *regPtr;
          *regPtr = record = OZ_newVariable();
          buildRecord(PC,Y,CAP);
          record = oz_deref(record);
          sPointer = tagged2LTuple(record)->getRef();
          *regPtr = savedTerm;
        } else {
          LTuple *ltuple = new LTuple();
          sPointer = ltuple->getRef();
          ltuple->setHead(e->currentUVarPrototype());
          ltuple->setTail(e->currentUVarPrototype());
          record = makeTaggedLTuple(ltuple);
        }
        tmpRet=oz_var_bind(tagged2CVar(term),termPtr,record);
        if (tmpRet==PROCEED) {
          SetReadMode;
          DISPATCH(2);
        }
        if (tmpRet!=FAILED) {
          goto LBLunifySpecial;
        }
        // fall through to fail
      } else if (oz_isLTuple(term)) {
        sPointer = tagged2LTuple(term)->getRef();
        SetReadMode;
        DISPATCH(2);
      }

      HF_TELL(GETTMPA(),
              makeTaggedLTuple(new LTuple(e->currentUVarPrototype(),
                                          e->currentUVarPrototype())));
    }


  /* a unifyVariable in read mode */
  Case(GETVARIABLEX)
    {
      XPC(1) = tagged2NonVariable(sPointer);
      sPointer++;
      DISPATCH(2);
    }
  Case(GETVARIABLEY)
    {
      YPC(1) = tagged2NonVariable(sPointer);
      sPointer++;
      DISPATCH(2);
    }

  Case(GETVARVARXX)
    {
      XPC(1) = tagged2NonVariable(sPointer);
      XPC(2) = tagged2NonVariable(sPointer+1);
      sPointer += 2;
      DISPATCH(3);
    }
  Case(GETVARVARXY)
    {
      XPC(1) = tagged2NonVariable(sPointer);
      YPC(2) = tagged2NonVariable(sPointer+1);
      sPointer += 2;
      DISPATCH(3);
    }
  Case(GETVARVARYX)
    {
      YPC(1) = tagged2NonVariable(sPointer);
      XPC(2) = tagged2NonVariable(sPointer+1);
      sPointer += 2;
      DISPATCH(3);
    }
  Case(GETVARVARYY)
    {
      YPC(1) = tagged2NonVariable(sPointer);
      YPC(2) = tagged2NonVariable(sPointer+1);
      sPointer += 2;
      DISPATCH(3);
    }



  /* a unify void in read case mode */
Case(GETVOID)
    {
      sPointer += getPosIntArg(PC+1);
      DISPATCH(2);
    }


  Case(UNIFYVARIABLEX)
    {
      if (InWriteMode) {
        TaggedRef *sp = GetSPointerWrite(sPointer);
        *sp = e->currentUVarPrototype();
        XPC(1) = makeTaggedRef(sp);
      } else {
        XPC(1) = tagged2NonVariable(sPointer);
      }
      sPointer++;
      DISPATCH(2);
    }
  Case(UNIFYVARIABLEY)
    {
      if (InWriteMode) {
        TaggedRef *sp = GetSPointerWrite(sPointer);
        *sp = e->currentUVarPrototype();
        YPC(1) = makeTaggedRef(sp);
      } else {
        YPC(1) = tagged2NonVariable(sPointer);
      }
      sPointer++;
      DISPATCH(2);
    }


  Case(UNIFYVALUEX)
    {
      TaggedRef term = XPC(1);

      if(InWriteMode) {
        TaggedRef *sp = GetSPointerWrite(sPointer);
        *sp = term;
        sPointer++;
        DISPATCH(2);
      } else {
        tmpRet=fastUnify(tagged2NonVariable(sPointer),term);
        sPointer++;
        if (tmpRet==PROCEED) { DISPATCH(2); }
        if (tmpRet!=FAILED)  {
          PC = lastGetRecord; goto LBLunifySpecial;
        }

        HF_TELL(*(sPointer-1), term);
      }
    }
  Case(UNIFYVALUEY)
    {
      TaggedRef term = YPC(1);

      if(InWriteMode) {
        TaggedRef *sp = GetSPointerWrite(sPointer);
        *sp = term;
        sPointer++;
        DISPATCH(2);
      } else {
        tmpRet=fastUnify(tagged2NonVariable(sPointer),term);
        sPointer++;
        if (tmpRet==PROCEED) { DISPATCH(2); }
        if (tmpRet!=FAILED)  {
          PC = lastGetRecord; goto LBLunifySpecial;
        }

        HF_TELL(*(sPointer-1), term);
      }
    }
  Case(UNIFYVALUEG)
    {
      TaggedRef term = GPC(1);

      if(InWriteMode) {
        TaggedRef *sp = GetSPointerWrite(sPointer);
        *sp = term;
        sPointer++;
        DISPATCH(2);
      } else {
        tmpRet=fastUnify(tagged2NonVariable(sPointer),term);
        sPointer++;
        if (tmpRet==PROCEED) { DISPATCH(2); }
        if (tmpRet!=FAILED)  {
          PC = lastGetRecord; goto LBLunifySpecial;
        }

        HF_TELL(*(sPointer-1), term);
      }
    }

  Case(UNIFYVALVARXX) SETTMPA(XPC(1)); SETTMPB(XPC(2)); goto UnifyValVar;
  Case(UNIFYVALVARXY) SETTMPA(XPC(1)); SETTMPB(YPC(2)); goto UnifyValVar;
  Case(UNIFYVALVARYX) SETTMPA(YPC(1)); SETTMPB(XPC(2)); goto UnifyValVar;
  Case(UNIFYVALVARYY) SETTMPA(YPC(1)); SETTMPB(YPC(2)); goto UnifyValVar;
  Case(UNIFYVALVARGX) SETTMPA(GPC(1)); SETTMPB(XPC(2)); goto UnifyValVar;
  Case(UNIFYVALVARGY) SETTMPA(GPC(1)); SETTMPB(YPC(2)); goto UnifyValVar;
  {
  UnifyValVar:

    if (InWriteMode) {
      TaggedRef *sp = GetSPointerWrite(sPointer);
      *sp = GETTMPA();

      *(sp+1) = e->currentUVarPrototype();
      GETTMPB() = makeTaggedRef(sp+1);

      sPointer += 2;
      DISPATCH(3);
    }

    tmpRet = fastUnify(GETTMPA(),makeTaggedRef(sPointer));

    if (tmpRet==PROCEED) {
      GETTMPB() = tagged2NonVariable(sPointer+1);
      sPointer += 2;
      DISPATCH(3);
    }
    if (tmpRet == FAILED) {
      HF_TELL(*sPointer, GETTMPA());
    }

    PC = lastGetRecord;
    goto LBLunifySpecial;
  }


  Case(UNIFYLITERAL)
     {
       if (InWriteMode) {
         TaggedRef *sp = GetSPointerWrite(sPointer);
         *sp = getTaggedArg(PC+1);
         sPointer++;
         DISPATCH(2);
       }

       TaggedRef atm = getLiteralArg(PC+1);

       /* code adapted from GETLITERAL */
       TaggedRef *termPtr = sPointer;
       sPointer++;
       DEREFPTR(term,termPtr,tag);

       if (isUVar(tag)) {
         bindOPT(termPtr, atm);
         DISPATCH(2);
       }

       if ( oz_eq(term,atm) ) {
         DISPATCH(2);
       }

       if (isCVar(term)) {
         tmpRet=oz_var_bind(tagged2CVar(term),termPtr,atm);
         if (tmpRet==PROCEED) { DISPATCH(2); }
         if (tmpRet!=FAILED)  { PC = lastGetRecord; goto LBLunifySpecial; }
         term = *termPtr;  // Note: 'term' may be disposed
       }

       HF_TELL(*(sPointer-1), getTaggedArg(PC+1));
     }

  Case(UNIFYNUMBER)
    {
      if (InWriteMode) {
        TaggedRef *sp = GetSPointerWrite(sPointer);
        *sp = getTaggedArg(PC+1);
        sPointer++;
        DISPATCH(2);
      }

      TaggedRef i = getNumberArg(PC+1);
      /* code adapted from GETLITERAL */
      TaggedRef *termPtr = sPointer;
      sPointer++;
      DEREFPTR(term,termPtr,tag);

      if (isUVar(tag)) {
        bindOPT(termPtr, i);
        DISPATCH(2);
      }

      if (oz_numberEq(term,i)) {
        DISPATCH(2);
      }

      if (isCVar(term)) {
        tmpRet=oz_var_bind(tagged2CVar(term),termPtr,i);
        if (tmpRet==PROCEED) { DISPATCH(2); }
        if (tmpRet!=FAILED)  { PC = lastGetRecord; goto LBLunifySpecial; }
        term = *termPtr;  // Note: 'term' may be disposed
      }

      HF_TELL(*(sPointer-1), getTaggedArg(PC+1) );
    }


  Case(UNIFYVOID)
    {
      int n = getPosIntArg(PC+1);
      if (InWriteMode) {
        TaggedRef *sp = GetSPointerWrite(sPointer);
        for (int i = n-1; i >=0; i-- ) {
          *sp++ = e->currentUVarPrototype();
        }
      }
      sPointer += n;
      DISPATCH(2);
    }

  Case(MATCHX)
    {
      TaggedRef val     = XPC(1);
      IHashTable *table = (IHashTable *) getAdressArg(PC+2);
      DoSwitchOnTerm(val,table);
    }
  Case(MATCHY)
    {
      TaggedRef val     = YPC(1);
      IHashTable *table = (IHashTable *) getAdressArg(PC+2);
      DoSwitchOnTerm(val,table);
    }
  Case(MATCHG)
    {
      TaggedRef val     = GPC(1);
      IHashTable *table = (IHashTable *) getAdressArg(PC+2);
      DoSwitchOnTerm(val,table);
    }


// ------------------------------------------------------------------------
// INSTRUCTIONS: (Fast-) Call/Execute Inline Funs/Rels
// ------------------------------------------------------------------------

  Case(FASTCALL)
    {
      PushCont(PC+3);   // PC+3 goes into a temp var (mm2)
      // goto LBLFastTailCall; // is not optimized away (mm2)
    }


  Case(FASTTAILCALL)
    //  LBLFastTailCall:
    {
      AbstractionEntry *entry = (AbstractionEntry *)getAdressArg(PC+1);

      CallDoChecks(entry->getAbstr());

      IHashTable *table = entry->indexTable;
      if (table) {
        PC = entry->getPC();
        DoSwitchOnTerm(X[0],table);
      } else {
        JUMPABSOLUTE(entry->getPC());
      }
    }

  Case(CALLBI)
    {
      Builtin* bi = GetBI(PC+1);
      OZ_Location* loc = GetLoc(PC+2);

      Assert(loc->getOutArity()==bi->getOutArity());
      Assert(loc->getInArity()==bi->getInArity());

#ifdef PROFILE_BI
      bi->incCounter();
#endif

      int res = bi->getFun()(X,loc->mapping());
      if (res == PROCEED) { DISPATCH(3); }
      switch (res) {
      case FAILED:        HF_BI_NEW(bi,loc);
      case RAISE:
        if (e->exception.debug) set_exception_info_call(bi,X,loc->mapping());
        RAISE_THREAD;

      case BI_TYPE_ERROR: RAISE_TYPE_NEW(bi,loc);

      case SUSPEND:
        PushContX(PC);
        SUSPENDONVARLIST;

      case BI_PREEMPT:
        PushContX(PC+3);
        return T_PREEMPT;

      case BI_REPLACEBICALL:
        PC += 3;
        Assert(!e->isEmptyPreparedCalls());
        goto LBLreplaceBICall;

      case SLEEP:
      default:
        Assert(0);
      }
    }

  Case(TESTBI)
    {
      Builtin* bi = GetBI(PC+1);
      OZ_Location* loc = GetLoc(PC+2);

      Assert(loc->getInArity()==bi->getInArity());
      Assert(bi->getOutArity()>=1);
      Assert(loc->getOutArity()==bi->getOutArity());

#ifdef PROFILE_BI
      bi->incCounter();
#endif
      int ret = bi->getFun()(X,loc->mapping());
      if (ret==PROCEED) {
        if (oz_isTrue(X[loc->out(0)])) {
          DISPATCH(4);
        } else {
          Assert(oz_isFalse(X[loc->out(0)]));
          JUMPRELATIVE(getLabelArg(PC+3));
        }
      }

      switch (ret) {
      case RAISE:
        if (e->exception.debug) set_exception_info_call(bi,X,loc->mapping());
        RAISE_THREAD;
      case BI_TYPE_ERROR:
        RAISE_TYPE_NEW(bi,loc);

      case SUSPEND:
        PushContX(PC);
        SUSPENDONVARLIST;

        // kost@, PER - added handling 18.01.99
      case BI_REPLACEBICALL:
        PC += 4;
        Assert(!e->isEmptyPreparedCalls());
        goto LBLreplaceBICall;

      case BI_PREEMPT:
      case SLEEP:
      case FAILED:
      default:
        Assert(0);
      }
    }


  Case(INLINEMINUS)
    {
      TaggedRef A = XPC(1);

    retryINLINEMINUSA:

      if (oz_isSmallInt(A)) {
        TaggedRef B = XPC(2);

      retryINLINEMINUSB1:

        if (oz_isSmallInt(B)) {

#if defined(__GNUC__) && defined(__i386__) && defined(REGOPT) && defined(FASTREGACCESS)

          Assert(SMALLINT == 6);
          asm volatile("   xorl $6,%1
                           subl %1,%0
                           jo   0f
                           movl 12(%3),%1
                           movl %0,(%2,%1)
                           addl $16,%3
                           jmp *(%3)
                        0:
                       "
                       :  /* OUTPUT */
                       :  /* INPUT  */
                          "r" (A),
                          "r" (B),
                          "r" (&(am.xRegs[0])),
                          "r" (PC)
                       );
          XPC(3) = oz_int(smallIntValue(oz_deref(XPC(1))) -
                          smallIntValue(oz_deref(XPC(2))));
          DISPATCH(4);

#else

          XPC(3)=oz_int(smallIntValue(A) - smallIntValue(B));
          DISPATCH(4);

#endif


        }

        if (oz_isRef(B)) {
          B = oz_derefOne(B);
          goto retryINLINEMINUSB1;
        }

      }

      if (oz_isFloat(A)) {
        TaggedRef B = XPC(2);

      retryINLINEMINUSB2:

        if (oz_isFloat(B)) {
          XPC(3) = oz_float(floatValue(A) - floatValue(B));
          DISPATCH(4);
        }

        if (oz_isRef(B)) {
          B = oz_derefOne(B);
          goto retryINLINEMINUSB2;
        }

      }

      if (oz_isRef(A)) {
        A = oz_derefOne(A);
        goto retryINLINEMINUSA;
      }

      auxTaggedA = XPC(1);
      auxTaggedB = XPC(2);
      auxInt     = 4;
      auxString = "-";

      tmpRet = BIminusInline(auxTaggedA,auxTaggedB,XPC(3));
      goto LBLhandlePlusMinus;
    }

  Case(INLINEPLUS)
    {
      TaggedRef A = XPC(1);

    retryINLINEPLUSA:

      if (oz_isSmallInt(A)) {
        TaggedRef B = XPC(2);

      retryINLINEPLUSB1:
        if (oz_isSmallInt(B)) {

#if defined(__GNUC__) && defined(__i386__) && defined(REGOPT) && defined(FASTREGACCESS)

          Assert(SMALLINT == 6);
          asm volatile("   xorl $6,%0
                           addl %1,%0
                           jo   0f
                           movl 12(%3),%1
                           movl %0,(%2,%1)
                           addl $16,%3
                           jmp *(%3)
                        0:
                       "
                       :  /* OUTPUT */
                       :  /* INPUT  */
                          "r" (A),
                          "r" (B),
                          "r" (&(am.xRegs[0])),
                          "r" (PC)
                       );
          XPC(3) = oz_int(smallIntValue(oz_deref(XPC(1))) +
                          smallIntValue(oz_deref(XPC(2))));
          DISPATCH(4);

#else

          XPC(3)=oz_int(smallIntValue(A) + smallIntValue(B));
          DISPATCH(4);

#endif

        }

        if (oz_isRef(B)) {
          B = oz_derefOne(B);
          goto retryINLINEPLUSB1;
        }

      }

      if (oz_isFloat(A)) {
        TaggedRef B = XPC(2);

      retryINLINEPLUSB2:

        if (oz_isFloat(B)) {
          XPC(3) = oz_float(floatValue(A) + floatValue(B));
          DISPATCH(4);
        }

        if (oz_isRef(B)) {
          B = oz_derefOne(B);
          goto retryINLINEPLUSB2;
        }

      }

      if (oz_isRef(A)) {
        A = oz_derefOne(A);
        goto retryINLINEPLUSA;
      }

      auxTaggedA = XPC(1);
      auxTaggedB = XPC(2);
      auxInt     = 4;
      auxString = "+";

      tmpRet = BIplusInline(auxTaggedA,auxTaggedB,XPC(3));
      goto LBLhandlePlusMinus;
    }

  Case(INLINEMINUS1)
    {
      TaggedRef A = XPC(1);

    retryINLINEMINUS1:

      if (oz_isSmallInt(A)) {
        /* INTDEP */
        if (A != TaggedOzMinInt) {
          XPC(2) = (int) A - (1<<tagSize);
          DISPATCH(3);
        } else {
          XPC(2) = TaggedOzOverMinInt;
          DISPATCH(3);
        }
      }

      if (oz_isRef(A)) {
        A = oz_derefOne(A);
        goto retryINLINEMINUS1;
      }

      auxTaggedA = XPC(1);
      auxTaggedB = makeTaggedSmallInt(1);
      auxInt     = 3;
      auxString  = "-1";

      tmpRet = BIminusInline(auxTaggedA,auxTaggedB,XPC(2));
      goto LBLhandlePlusMinus;
    }

  Case(INLINEPLUS1)
    {
      TaggedRef A = XPC(1);

    retryINLINEPLUS1:

      if (oz_isSmallInt(A)) {
        /* INTDEP */
        if (A != TaggedOzMaxInt) {
          XPC(2) = (int) A + (1<<tagSize);
          DISPATCH(3);
        } else {
          XPC(2) = TaggedOzOverMaxInt;
          DISPATCH(3);
        }
      }

      if (oz_isRef(A)) {
        A = oz_derefOne(A);
        goto retryINLINEPLUS1;
      }

      auxTaggedA = XPC(1);
      auxTaggedB = makeTaggedSmallInt(1);
      auxInt     = 3;
      auxString = "+1";

      tmpRet = BIplusInline(auxTaggedA,auxTaggedB,XPC(2));
      goto LBLhandlePlusMinus;
    }


  LBLhandlePlusMinus:
  {
      switch(tmpRet) {
      case PROCEED:       DISPATCH(auxInt);
      case BI_TYPE_ERROR: RAISE_TYPE1_FUN(auxString,
                                          oz_mklist(auxTaggedA,auxTaggedB));

      case SUSPEND:
        {
          CheckLiveness(PC);
          PushContX(PC);
          tmpRet = suspendInline(CTT,auxTaggedA,auxTaggedB);
          MAGIC_RET;
        }
      default:    Assert(0);
      }
    }

  Case(INLINEDOT)
    {
      TaggedRef rec = XPC(1);
      DEREF(rec,_1,_2);
      if (oz_isSRecord(rec)) {
        SRecord *srec = tagged2SRecord(rec);
        TaggedRef feature = getLiteralArg(PC+2);
        int index = ((InlineCache*)(PC+4))->lookup(srec,feature);
        if (index<0) {
          (void) oz_raise(E_ERROR,E_KERNEL,".", 2, XPC(1), feature);
          RAISE_THREAD;
        }
        XPC(3) = srec->getArg(index);
        DISPATCH(6);
      }
      {
        TaggedRef feature = getLiteralArg(PC+2);
        OZ_Return res = dotInline(XPC(1),feature,XPC(3));
        if (res==PROCEED) { DISPATCH(6); }

        switch(res) {
        case SUSPEND:
          {
            TaggedRef A=XPC(1);
            CheckLiveness(PC);
            PushContX(PC);
            tmpRet = suspendInline(CTT,A);
            MAGIC_RET;
          }

        case RAISE:
          RAISE_THREAD;

        case BI_TYPE_ERROR:
          RAISE_TYPE1_FUN(".", oz_mklist(XPC(1),feature));

        case SLEEP:
        default:
          Assert(0);
        }
      }
    }

  Case(INLINEAT)
    {
      TaggedRef fea = getLiteralArg(PC+1);
      Object *self = e->getSelf();

      Assert(e->getSelf()!=NULL);
      RecOrCell state = self->getState();
      SRecord *rec;
      if (stateIsCell(state)) {
        rec = getState(state,NO,fea,XPC(2));
        if (rec==NULL) {
          PC += 5;
          Assert(!e->isEmptyPreparedCalls());
          goto LBLreplaceBICall;
        }
      } else {
        rec = getRecord(state);
      }
      Assert(rec!=NULL);
      int index = ((InlineCache*)(PC+3))->lookup(rec,fea);
      if (index>=0) {
        XPC(2) = rec->getArg(index);
        DISPATCH(5);
      }
      (void) oz_raise(E_ERROR,E_OBJECT,"@",2,makeTaggedConst(self),fea);
      RAISE_THREAD;
    }

  Case(INLINEASSIGN)
    {
      TaggedRef fea = getLiteralArg(PC+1);

      Object *self = e->getSelf();

      if (!oz_onToplevel() && !oz_isCurrentBoard(GETBOARD(self))) {
        (void) oz_raise(E_ERROR,E_KERNEL,"globalState",1,OZ_atom("object"));
        RAISE_THREAD;
     }

      RecOrCell state = self->getState();
      SRecord *rec;

      if (stateIsCell(state)) {
        rec = getState(state,OK,fea,XPC(2));
        if (rec==NULL) {
          PC += 5;
          Assert(!e->isEmptyPreparedCalls());
          goto LBLreplaceBICall;
        }
      } else {
        rec = getRecord(state);
      }
      Assert(rec!=NULL);
      int index = ((InlineCache*)(PC+3))->lookup(rec,fea);
      if (index>=0) {
        Assert(oz_isRef(*rec->getRef(index)) || !oz_isVariable(*rec->getRef(index)));
        rec->setArg(index,XPC(2));
        DISPATCH(5);
      }

      (void) oz_raise(E_ERROR,E_OBJECT,"<-",3,
                      makeTaggedConst(self), fea, XPC(2));
      RAISE_THREAD;
    }



// ------------------------------------------------------------------------
// INSTRUCTIONS: Testing
// ------------------------------------------------------------------------

#define LT_IF(T) if (T) THEN_CASE else ELSE_CASE
#define THEN_CASE { XPC(3)=oz_true(); DISPATCH(5); }
#define ELSE_CASE { XPC(3)=oz_false(); JUMPRELATIVE(getLabelArg(PC+4)); }

  Case(TESTLT)
    {
      TaggedRef A = XPC(1); DEREF0(A,_1,tagA);

      if (isSmallIntTag(tagA)) {
        TaggedRef B = XPC(2); DEREF0(B,_2,tagB);
        if (isSmallIntTag(tagB)) {
          LT_IF(smallIntLess(A,B));
        }
      }

      if (isFloatTag(tagA)) {
        TaggedRef B = XPC(2); DEREF0(B,_2,tagB);
        if (isFloatTag(tagB)) {
          LT_IF(floatValue(A) < floatValue(B));
        }
      }

      {
        TaggedRef B = XPC(2); DEREF0(B,_2,tagB);
        if (oz_isAtom(A) && oz_isAtom(B)) {
          LT_IF(strcmp(tagged2Literal(A)->getPrintName(),
                       tagged2Literal(B)->getPrintName()) < 0);
          }
      }
      tmpRet = BILessOrLessEq(OK,XPC(1),XPC(2));
      auxString = "<";
      goto LBLhandleLT;
    }

  LBLhandleLT:
    {
      switch(tmpRet) {

      case PROCEED: THEN_CASE;
      case FAILED:  ELSE_CASE;

      case SUSPEND:
        {
          CheckLiveness(PC);
          PushContX(PC);
          tmpRet = suspendInline(CTT,XPC(1),XPC(2));
          MAGIC_RET;
        }

      case BI_TYPE_ERROR:
        RAISE_TYPE1(auxString,oz_mklist(XPC(1),XPC(2)));

      default:
        Assert(0);
      }
    }

  Case(TESTLE)
    {
      TaggedRef A = XPC(1); DEREF0(A,_1,tagA);
      TaggedRef B = XPC(2); DEREF0(B,_2,tagB);

      if (tagA == tagB) {
        if (tagA == SMALLINT) {
          LT_IF(smallIntLE(A,B));
        }

        if (isFloatTag(tagA)) {
          LT_IF(floatValue(A) <= floatValue(B));
        }

        if (tagA == LITERAL) {
          if (oz_isAtom(A) && oz_isAtom(B)) {
            LT_IF(strcmp(tagged2Literal(A)->getPrintName(),
                         tagged2Literal(B)->getPrintName()) <= 0);
          }
        }
      }
      tmpRet = BILessOrLessEq(NO,XPC(1),XPC(2));
      auxString = "=<";
      goto LBLhandleLT;
    }

#undef THEN_CASE
#undef ELSE_CASE

// -------------------------------------------------------------------------
// INSTRUCTIONS: Environment
// -------------------------------------------------------------------------

  Case(ALLOCATEL)
    {
      int posInt = getPosIntArg(PC+1);
      Assert(posInt > 0);
      Y = allocateY(posInt);
      DISPATCH(2);
    }

  Case(ALLOCATEL1)  { Y =  allocateY(1); DISPATCH(1); }
  Case(ALLOCATEL2)  { Y =  allocateY(2); DISPATCH(1); }
  Case(ALLOCATEL3)  { Y =  allocateY(3); DISPATCH(1); }
  Case(ALLOCATEL4)  { Y =  allocateY(4); DISPATCH(1); }
  Case(ALLOCATEL5)  { Y =  allocateY(5); DISPATCH(1); }
  Case(ALLOCATEL6)  { Y =  allocateY(6); DISPATCH(1); }
  Case(ALLOCATEL7)  { Y =  allocateY(7); DISPATCH(1); }
  Case(ALLOCATEL8)  { Y =  allocateY(8); DISPATCH(1); }
  Case(ALLOCATEL9)  { Y =  allocateY(9); DISPATCH(1); }
  Case(ALLOCATEL10) { Y = allocateY(10); DISPATCH(1); }

  Case(DEALLOCATEL1)  { deallocateY(Y,1);  Y=0; DISPATCH(1); }
  Case(DEALLOCATEL2)  { deallocateY(Y,2);  Y=0; DISPATCH(1); }
  Case(DEALLOCATEL3)  { deallocateY(Y,3);  Y=0; DISPATCH(1); }
  Case(DEALLOCATEL4)  { deallocateY(Y,4);  Y=0; DISPATCH(1); }
  Case(DEALLOCATEL5)  { deallocateY(Y,5);  Y=0; DISPATCH(1); }
  Case(DEALLOCATEL6)  { deallocateY(Y,6);  Y=0; DISPATCH(1); }
  Case(DEALLOCATEL7)  { deallocateY(Y,7);  Y=0; DISPATCH(1); }
  Case(DEALLOCATEL8)  { deallocateY(Y,8);  Y=0; DISPATCH(1); }
  Case(DEALLOCATEL9)  { deallocateY(Y,9);  Y=0; DISPATCH(1); }
  Case(DEALLOCATEL10) { deallocateY(Y,10); Y=0; DISPATCH(1); }

  Case(DEALLOCATEL)
    {
      if (Y) {
        deallocateY(Y);
        Y = NULL;
      }
      DISPATCH(1);
    }
// -------------------------------------------------------------------------
// INSTRUCTIONS: CONTROL: FAIL/SUCCESS/RETURN
// -------------------------------------------------------------------------

  Case(SKIP)
    DISPATCH(1);

  Case(EXHANDLER)
    PushCont(PC+2);
    oz_currentThread()->pushCatch();
    JUMPRELATIVE(getLabelArg(PC+1));

  Case(POPEX)
    {
      TaskStack *taskstack = CTS;
      taskstack->discardCatch();
      /* remove unused continuation for handler */
      taskstack->discardFrame(NOCODE);
      DISPATCH(1);
    }

  Case(LOCKTHREAD)
{
  int lbl = getLabelArg(PC+1);
  TaggedRef aux      = XPC(2);

  DEREF(aux,auxPtr,_1);
  if (oz_isVariable(aux)) {
    SUSP_PC(auxPtr,PC);}

  if (!oz_isLock(aux)) {
    /* arghhhhhhhhhh! fucking exceptions (RS) */
    (void) oz_raise(E_ERROR,E_KERNEL,"type",5,
                    NameUnit,
                    NameUnit,
                    OZ_atom("Lock"),
                    OZ_int(1),
                    OZ_string(""));
    RAISE_TYPE1("lock",oz_mklist(aux));
  }

  OzLock *t = (OzLock*)tagged2Tert(aux);
  Thread *th=oz_currentThread();

  if(t->isLocal()){
    if(!oz_onToplevel()){
      if (!oz_isCurrentBoard(GETBOARD((LockLocal*)t))) {
        (void) oz_raise(E_ERROR,E_KERNEL,"globalState",1,OZ_atom("lock"));
        RAISE_THREAD;}}
    if(((LockLocal*)t)->hasLock(th)) {goto has_lock;}
    if(((LockLocal*)t)->lockB(th)) {goto got_lock;}
    goto no_lock;}

  if(!oz_onToplevel()){
    (void) oz_raise(E_ERROR,E_KERNEL,"globalState",1,OZ_atom("lock"));
    RAISE_THREAD;
  }

  LockRet ret;

  switch(t->getTertType()){
  case Te_Frame:{
    if(((LockFrameEmul *)t)->hasLock(th)) {goto has_lock;}
    ret = ((LockFrameEmul *)t)->lockB(th);
    break;}
  case Te_Proxy:{
    (*lockLockProxy)(t, th);
    goto no_lock;}
  case Te_Manager:{
    if(((LockManagerEmul *)t)->hasLock(th)) {goto has_lock;}
    ret=((LockManagerEmul *)t)->lockB(th);
    break;}
  default:
    Assert(0);}

  if(ret==LOCK_GOT) goto got_lock;
  if(ret==LOCK_WAIT) goto no_lock;

  PushCont(PC+lbl); // failure preepmtion
  PC += 3;
  Assert(!e->isEmptyPreparedCalls());
  goto LBLreplaceBICall;

  got_lock:
    PushCont(PC+lbl);
    CTS->pushLock(t);
    DISPATCH(3);

  has_lock:
    PushCont(PC+lbl);
    DISPATCH(3);

  no_lock:
    PushCont(PC+lbl);
    CTS->pushLock(t);
    PC += 3;
    Assert(!e->isEmptyPreparedCalls());
    goto LBLreplaceBICall;

  }

  Case(RETURN)

    LBLpopTask:
      {
        emulateHookPopTask(e);

        Assert(!CTT->isSuspended());

      LBLpopTaskNoPreempt:
        Assert(CTS==CTT->getTaskStackRef());
        PopFrameNoDecl(CTS,PC,Y,CAP);
        JUMPABSOLUTE(PC);
      }

// ------------------------------------------------------------------------
// INSTRUCTIONS: Definition
// ------------------------------------------------------------------------

  Case(DEFINITIONCOPY)
    isTailCall = OK; // abuse for indicating that we have to copy
    goto LBLDefinition;

  Case(DEFINITION)
      isTailCall = NO;

    LBLDefinition:
    {
      int nxt                     = getLabelArg(PC+2);
      PrTabEntry *predd           = getPredArg(PC+3);
      AbstractionEntry *predEntry = (AbstractionEntry*) getAdressArg(PC+4);
      AssRegArray *list           = (AssRegArray*) getAdressArg(PC+5);
      int size = list->getSize();

      if (predd->getPC()==NOCODE) {
        predd->PC = PC+sizeOf(DEFINITION);
        predd->setGSize(size);
      }

      predd->numClosures++;

      if (isTailCall) { // was DEFINITIONCOPY?
        TaggedRef list = oz_deref(XPC(1));
        ProgramCounter preddPC = predd->PC;
        predd = new PrTabEntry(predd->getName(), predd->getMethodArity(),
                               predd->getFile(), predd->getLine(), predd->getColumn(),
                               oz_nil(), // mm2: inherit sited?
                               predd->getMaxX());
        predd->PC = copyCode(preddPC,list);
        predd->setGSize(size);
      }

      Abstraction *p = Abstraction::newAbstraction(predd, CBB);

      if (predEntry) {
        predEntry->setPred(p);
      }

      for (int i = 0; i < size; i++) {
        switch ((*list)[i].kind) {
        case XReg: p->initG(i, X[(*list)[i].number]); break;
        case YReg: p->initG(i, Y[(*list)[i].number]); break;
        case GReg: p->initG(i, CAP->getG((*list)[i].number)); break;
        }
      }
      XPC(1) = makeTaggedConst(p);
      JUMPRELATIVE(nxt);
    }

// -------------------------------------------------------------------------
// INSTRUCTIONS: CONTROL: FENCE/CALL/EXECUTE/SWITCH/BRANCH
// -------------------------------------------------------------------------

  Case(BRANCH)
    JUMPRELATIVE( getLabelArg(PC+1) );


  Case(TAILSENDMSGX)
    isTailCall = OK; SETAUX(XPC(2)); goto SendMethod;
  Case(TAILSENDMSGY)
    isTailCall = OK; SETAUX(YPC(2)); goto SendMethod;
  Case(TAILSENDMSGG)
    isTailCall = OK; SETAUX(GPC(2)); goto SendMethod;

  Case(SENDMSGX)
    isTailCall = NO; SETAUX(XPC(2)); goto SendMethod;
  Case(SENDMSGY)
    isTailCall = NO; SETAUX(YPC(2)); goto SendMethod;
  Case(SENDMSGG)
    isTailCall = NO; SETAUX(GPC(2)); goto SendMethod;

 SendMethod:
  {
    TaggedRef label    = getLiteralArg(PC+1);
    TaggedRef origObj  = auxTaggedA;
    TaggedRef object   = origObj;
    SRecordArity arity = (SRecordArity) getAdressArg(PC+3);

    DEREF(object,objectPtr,_2);
    if (oz_isObject(object)) {
      Object *obj      = tagged2Object(object);
      Abstraction *def = getSendMethod(obj,label,arity,(InlineCache*)(PC+4),X);
      if (def == NULL) {
        goto bombSend;
      }

      if (!isTailCall) PushCont(PC+6);
      ChangeSelf(obj);
      CallDoChecks(def);
      JUMPABSOLUTE(def->getPC());
    }

    if (oz_isVariable(object)) {
      SUSP_PC(objectPtr,PC);
    }

    if (oz_isProcedure(object))
      goto bombSend;

    RAISE_APPLY(object, oz_mklist(makeMessage(arity,label,X)));

  bombSend:
    if (!isTailCall) PC = PC+6;
    X[0] = makeMessage(arity,label,X);
    predArity = 1;
    predicate = tagged2Const(object);
    goto LBLcall;
  }





  Case(CALLX)
    isTailCall = NO; SETAUX(XPC(1)); goto Call;
  Case(CALLY)
    isTailCall = NO; SETAUX(YPC(1)); goto Call;
  Case(CALLG)
    isTailCall = NO; SETAUX(GPC(1)); goto Call;

  Case(TAILCALLX)
    isTailCall = OK; SETAUX(XPC(1)); goto Call;
  Case(TAILCALLG)
    isTailCall = OK; SETAUX(GPC(1)); goto Call;

 Call:
  asmLbl(TAILCALL);
  {
     {
       TaggedRef taggedPredicate = auxTaggedA;
       predArity = getPosIntArg(PC+2);

       DEREF(taggedPredicate,predPtr,_1);

       if (oz_isAbstraction(taggedPredicate)) {
         Abstraction *def = tagged2Abstraction(taggedPredicate);
         PrTabEntry *pte = def->getPred();
         CheckArity(pte->getArity(), taggedPredicate);
         if (!isTailCall) { PushCont(PC+3); }
         CallDoChecks(def);
         JUMPABSOLUTE(pte->getPC());
       }

       if (!oz_isProcedure(taggedPredicate) && !oz_isObject(taggedPredicate)) {
         if (oz_isVariable(taggedPredicate)) {
           SUSP_PC(predPtr,PC);
         }
         RAISE_APPLY(taggedPredicate,OZ_toList(predArity,X));
       }

       if (!isTailCall) PC = PC+3;
       predicate = tagged2Const(taggedPredicate);
     }

// -----------------------------------------------------------------------
// --- Call: entry point
// -----------------------------------------------------------------------

  LBLcall:
     Builtin *bi;

// -----------------------------------------------------------------------
// --- Call: Abstraction
// -----------------------------------------------------------------------

     {
       TypeOfConst typ = predicate->getType();

       if (typ==Co_Abstraction) {
         Abstraction *def = (Abstraction *) predicate;
         CheckArity(def->getArity(), makeTaggedConst(def));
         if (!isTailCall) { PushCont(PC); }
         CallDoChecks(def);
         JUMPABSOLUTE(def->getPC());
       }

// -----------------------------------------------------------------------
// --- Call: Object
// -----------------------------------------------------------------------
       if (typ==Co_Object) {
         CheckArity(1, makeTaggedConst(predicate));
         Object *o = (Object*) predicate;
         Assert(o->getClass()->getFallbackApply());
         Abstraction *def =
           tagged2Abstraction(o->getClass()->getFallbackApply());
         /* {Obj Msg} --> {SetSelf Obj} {FallbackApply Class Msg} */
         X[1] = X[0];
         X[0] = makeTaggedConst(o->getClass());
         predArity = 2;
         if (!isTailCall) { PushCont(PC); }
         ChangeSelf(o);
         CallDoChecks(def);
         JUMPABSOLUTE(def->getPC());
       }

// -----------------------------------------------------------------------
// --- Call: Builtin
// -----------------------------------------------------------------------
       Assert(typ==Co_Builtin);

       bi = (Builtin *) predicate;

       CheckArity(bi->getArity(),makeTaggedConst(bi));

#ifdef PROFILE_BI
       bi->incCounter();
#endif
       OZ_Return res = oz_bi_wrapper(bi,X);

       switch (res) {

       case SUSPEND:
         {
           if (!isTailCall) PushCont(PC);

           CTT->pushCall(makeTaggedConst(bi),X,predArity);
           SUSPENDONVARLIST;
         }

       case PROCEED:
         if (isTailCall) {
           goto LBLpopTask;
         }
         JUMPABSOLUTE(PC);

       case SLEEP:         Assert(0);
       case RAISE:
         if (e->exception.debug) set_exception_info_call(bi,X);
         RAISE_THREAD;
       case BI_TYPE_ERROR: RAISE_TYPE(bi);
       case FAILED:        HF_BI(bi);

       case BI_PREEMPT:
         if (!isTailCall) {
           PushCont(PC);
         }
         return T_PREEMPT;

      case BI_REPLACEBICALL:
        if (isTailCall) {
          PC=NOCODE;
        }
        Assert(!e->isEmptyPreparedCalls());
        goto LBLreplaceBICall;

       default: Assert(0);
       }
     }
     Assert(0);

   LBLhandleRet:
     switch (tmpRet) {
     case RAISE: RAISE_THREAD;
     case BI_REPLACEBICALL:
       PC=NOCODE;
       Assert(!e->isEmptyPreparedCalls());
       goto LBLreplaceBICall;
     default: break;
     }
     Assert(0);

   LBLMagicRet:
     {
       if (tmpRet == SUSPEND) return T_SUSPEND;
       if (tmpRet == PROCEED) goto LBLpopTask;
       Assert(tmpRet == RAISE || tmpRet == BI_REPLACEBICALL);
       goto LBLhandleRet;
     }


// ------------------------------------------------------------------------
// --- Call: Builtin: replaceBICall
// ------------------------------------------------------------------------

   LBLreplaceBICall:
     {
       if (PC != NOCODE) {
         PushContX(PC);
       }

       e->pushPreparedCalls();

       if (!e->isEmptySuspendVarList()) {
         SUSPENDONVARLIST;
       }
       goto LBLpopTask;
     }
   }
// --------------------------------------------------------------------------
// --- end call/execute -----------------------------------------------------
// --------------------------------------------------------------------------


// -------------------------------------------------------------------------
// INSTRUCTIONS: MISC: ERROR/NOOP/default
// -------------------------------------------------------------------------

  Case(TASKEMPTYSTACK)
    {
      Assert(Y==0 && CAP==0);
      CTS->pushEmpty();   // mm2: is this really needed?
      DebugCode(e->cachedSelf=0);
      return T_TERMINATE;
    }

  Case(TASKPROFILECALL)
    {
      ozstat.leaveCall((PrTabEntry*) Y);
      goto LBLpopTaskNoPreempt;
    }

  Case(TASKCALLCONT)
    {
      TaggedRef taggedPredicate = (TaggedRef)ToInt32(Y);
      RefsArray args = (RefsArray) CAP;
      Y = 0;
      CAP = 0;

      DebugTrace(ozd_trace(toC(taggedPredicate)));

      predArity = args ? getRefsArraySize(args) : 0;

      DEREF(taggedPredicate,predPtr,predTag);
      if (!oz_isProcedure(taggedPredicate) && !oz_isObject(taggedPredicate)) {
        if (isVariableTag(predTag)) {
          CTS->pushCallNoCopy(makeTaggedRef(predPtr),args);
          tmpRet = oz_var_addSusp(predPtr,CTT);
          MAGIC_RET;
        }
        RAISE_APPLY(taggedPredicate,OZ_toList(predArity,args));
      }

      int i = predArity;
      while (--i >= 0) {
        X[i] = args[i];
      }
      disposeRefsArray(args);
      isTailCall = OK;

      predicate=tagged2Const(taggedPredicate);
      goto LBLcall;
    }

  Case(TASKLOCK)
    {
      OzLock *lck = (OzLock *) CAP;
      CAP = (Abstraction *) NULL;
      switch(lck->getTertType()){
      case Te_Local:
        ((LockLocal*)lck)->unlock();
        break;
      case Te_Frame:
        ((LockFrameEmul *)lck)->unlock(oz_currentThread());
        break;
      case Te_Proxy:
        oz_raise(E_ERROR,E_KERNEL,"globalState",1,OZ_atom("lock"));
        RAISE_THREAD_NO_PC;
      case Te_Manager:
        ((LockManagerEmul *)lck)->unlock(oz_currentThread());
        break;}
      goto LBLpopTaskNoPreempt;
    }

  Case(TASKXCONT)
    {
      RefsArray tmpX = Y;
      Y = NULL;
      int i = getRefsArraySize(tmpX);
      while (--i >= 0) {
        X[i] = tmpX[i];
      }
      disposeRefsArray(tmpX);
      goto LBLpopTaskNoPreempt;
    }

  Case(TASKSETSELF)
    {
      e->setSelf((Object *) CAP);
      CAP = NULL;
      goto LBLpopTaskNoPreempt;
    }

  Case(TASKDEBUGCONT)
    {
      Assert(0);
      goto LBLpopTaskNoPreempt;
    }

  Case(TASKCFUNCONT)
     {
       OZ_CFun biFun = (OZ_CFun) (void*) CAP;
       RefsArray tmpX = (RefsArray) Y;
       CAP = 0;
       Y = 0;
       if (tmpX != NULL) {
         predArity = getRefsArraySize(tmpX);
         int i = predArity;
         while (--i >= 0) {
           X[i] = tmpX[i];
         }
       } else {
         predArity = 0;
       }
       disposeRefsArray(tmpX);

       DebugTrace(ozd_trace(cfunc2Builtin((void *) biFun)->getPrintName()));

       switch (biFun(X,OZ_ID_MAP)) {
       case PROCEED:       goto LBLpopTask;
       case FAILED:        HF_BI(cfunc2Builtin((void *) biFun));
       case RAISE:
         if (e->exception.debug)
           set_exception_info_call(cfunc2Builtin((void *) biFun),X);
         RAISE_THREAD_NO_PC;
       case BI_TYPE_ERROR: RAISE_TYPE(cfunc2Builtin((void *) biFun));

       case BI_REPLACEBICALL:
         PC = NOCODE;
         Assert(!e->isEmptyPreparedCalls());
         goto LBLreplaceBICall;

       case SUSPEND:
         CTT->pushCFun(biFun,X,predArity);
         SUSPENDONVARLIST;

      case BI_PREEMPT:
        return T_PREEMPT;

       case SLEEP:
       default:
         Assert(0);
       }
     }

  Case(OZERROR)
    {
      return T_ERROR;
    }

  Case(DEBUGENTRY)
    {
      if ((e->debugmode() || CTT->isTrace()) && oz_onToplevel()) {
        int line = smallIntValue(getNumberArg(PC+2));
        if (line < 0) {
          execBreakpoint(oz_currentThread());
        }

        OzDebug *dbg = new OzDebug(PC,Y,CAP);

        TaggedRef kind = getTaggedArg(PC+4);
        if (oz_eq(kind,AtomDebugCallC) ||
            oz_eq(kind,AtomDebugCallF)) {
          // save abstraction and arguments:
          Bool copyArgs = NO;
          switch (CodeArea::getOpcode(PC+5)) {
          case CALLBI:
            {
              Builtin *bi = GetBI(PC+6);
              dbg->data = makeTaggedConst(bi);
              int iarity = bi->getInArity(), oarity = bi->getOutArity();
              int arity = iarity + oarity;
              int *map = GetLoc(PC+7)->mapping();
              dbg->arity = arity;
              if (arity > 0) {
                dbg->arguments =
                  (TaggedRef *) freeListMalloc(sizeof(TaggedRef) * arity);
                if (map == OZ_ID_MAP)
                  for (int i = iarity; i--; )
                    dbg->arguments[i] = X[i];
                else
                  for (int i = iarity; i--; )
                    dbg->arguments[i] = X[map[i]];
                if (CTT->isStep())
                  for (int i = oarity; i--; )
                    dbg->arguments[iarity + i] = OZ_newVariable();
                else
                  for (int i = oarity; i--; )
                    dbg->arguments[iarity + i] = NameVoidRegister;
              }
            }
            break;
          case CALLX:
            dbg->data  = XPC(6);
            dbg->arity = getPosIntArg(PC+7);
            copyArgs = OK;
            break;
          case CALLY:
            dbg->data  = YPC(6);
            dbg->arity = getPosIntArg(PC+7);
            copyArgs = OK;
            break;
          case CALLG:
            dbg->data  = GPC(6);
            dbg->arity = getPosIntArg(PC+7);
            copyArgs = OK;
            break;
          case CALLPROCEDUREREF:
          case FASTCALL:
            {
              Abstraction *abstr =
                ((AbstractionEntry *) getAdressArg(PC+6))->getAbstr();
              dbg->data = makeTaggedConst(abstr);
              dbg->arity = abstr->getArity();
              copyArgs = OK;
            }
            break;
          case CALLCONSTANT:
            dbg->data = getTaggedArg(PC+6);
            dbg->arity = getPosIntArg(PC+7) >> 1;
            copyArgs = OK;
            break;
          default:
            break;
          }
          if (copyArgs && dbg->arity > 0) {
            dbg->arguments =
              (TaggedRef *) freeListMalloc(sizeof(TaggedRef) * dbg->arity);
            for (int i = dbg->arity; i--; )
              dbg->arguments[i] = e->getX(i);
          }
        } else if (oz_eq(kind,AtomDebugLockC) ||
                   oz_eq(kind,AtomDebugLockF)) {
          // save the lock:
          switch (CodeArea::getOpcode(PC+5)) {
          case LOCKTHREAD:
            dbg->setSingleArgument(XPC(7));
            break;
          default:
            break;
          }
        } else if (oz_eq(kind,AtomDebugCondC) ||
                   oz_eq(kind,AtomDebugCondF)) {
          // look whether we can determine the arbiter:
          switch (CodeArea::getOpcode(PC+5)) {
          case TESTLITERALX:
          case TESTNUMBERX:
          case TESTRECORDX:
          case TESTLISTX:
          case TESTBOOLX:
          case MATCHX:
            dbg->setSingleArgument(XPC(6));
            break;
          case TESTLITERALY:
          case TESTNUMBERY:
          case TESTRECORDY:
          case TESTLISTY:
          case TESTBOOLY:
          case MATCHY:
            dbg->setSingleArgument(YPC(6));
            break;
          case TESTLITERALG:
          case TESTNUMBERG:
          case TESTRECORDG:
          case TESTLISTG:
          case TESTBOOLG:
          case MATCHG:
            dbg->setSingleArgument(GPC(6));
            break;
          default:
            break;
          }
        } else if (oz_eq(kind,AtomDebugNameC) ||
                   oz_eq(kind,AtomDebugNameF)) {
          switch (CodeArea::getOpcode(PC+5)) {
          case PUTCONSTANTX:
          case PUTCONSTANTY:
          case GETLITERALX:
          case GETLITERALY:
          case GETLITERALG:
            dbg->setSingleArgument(getTaggedArg(PC+6));
            break;
          default:
            break;
          }
        }

        if (CTT->isStep()) {
          CTT->pushDebug(dbg,DBG_STEP);
          debugStreamEntry(dbg,CTT->getTaskStackRef()->getFrameId());
          INCFPC(5);
          PushContX(PC);
          return T_PREEMPT;
        } else {
          CTT->pushDebug(dbg,DBG_NOSTEP);
        }
      } else if (e->isPropagatorLocation()) {
        OzDebug *dbg = new OzDebug(PC,NULL,CAP);
        CTT->pushDebug(dbg,DBG_EXIT);
      }

      DISPATCH(5);
    }

  Case(DEBUGEXIT)
    {
      OzDebug *dbg;
      OzDebugDoit dothis;
      CTT->popDebug(dbg, dothis);

      if (dbg != (OzDebug *) NULL) {
        Assert(oz_eq(getLiteralArg(dbg->PC+4),getLiteralArg(PC+4)));
        Assert(e->isPropagatorLocation() ||
               (dbg->Y == Y &&
                ((Abstraction *) tagged2Const(dbg->CAP)) == CAP));

        if (dothis != DBG_EXIT
            && (oz_eq(getLiteralArg(PC+4),AtomDebugCallC) ||
                oz_eq(getLiteralArg(PC+4),AtomDebugCallF))
            && CodeArea::getOpcode(dbg->PC+5) == CALLBI) {
          Builtin *bi = GetBI(dbg->PC+6);
          int iarity = bi->getInArity(), oarity = bi->getOutArity();
          int *map = GetLoc(dbg->PC+7)->mapping();
          if (oarity > 0)
            if (dbg->arguments[iarity] != NameVoidRegister)
              for (int i = oarity; i--; ) {
                TaggedRef x =
                  X[map == OZ_ID_MAP ? iarity + i: map[iarity + i]];
                if (OZ_unify(dbg->arguments[iarity + i], x) == FAILED)
                  return T_FAILURE;
              }
            else
              for (int i = oarity; i--; )
                dbg->arguments[iarity + i] =
                  X[map == OZ_ID_MAP? iarity + i: map[iarity + i]];
        }

        if (dothis == DBG_STEP && CTT->isTrace()) {
          dbg->PC = PC;
          CTT->pushDebug(dbg,DBG_EXIT);
          debugStreamExit(dbg,CTT->getTaskStackRef()->getFrameId());
          PushContX(PC);
          return T_PREEMPT;
        }

        dbg->dispose();
      }

      DISPATCH(5);
    }

  Case(CALLPROCEDUREREF)
    {
      AbstractionEntry *entry = (AbstractionEntry *) getAdressArg(PC+1);
      Bool tailcall           =  getPosIntArg(PC+2) & 1;

      if (entry->getAbstr() == 0) {
        (void) oz_raise(E_ERROR,E_SYSTEM,"inconsistentFastcall",0);
        RAISE_THREAD;
      }
      CodeArea::writeOpcode(tailcall ? FASTTAILCALL : FASTCALL, PC);
      DISPATCH(0);
    }

  Case(CALLCONSTANT)
    {
      TaggedRef pred = getTaggedArg(PC+1);
      int tailcallAndArity  = getPosIntArg(PC+2);

      DEREF(pred,predPtr,_1);
      if (oz_isVariable(pred)) {
        SUSP_PC(predPtr,PC);
      }

      if (oz_isAbstraction(pred)) {
        CodeArea *code = CodeArea::findBlock(PC);
        code->unprotect((TaggedRef*)(PC+1));
        AbstractionEntry *entry = new AbstractionEntry(NO);
        entry->setPred(tagged2Abstraction(pred));
        CodeArea::writeOpcode((tailcallAndArity&1)? FASTTAILCALL: FASTCALL,PC);
        code->writeAbstractionEntry(entry, PC+1);
        DISPATCH(0);
      }
      if (oz_isBuiltin(pred) || oz_isObject(pred)) {
        isTailCall = tailcallAndArity & 1;
        if (!isTailCall) PC += 3;
        predArity = tailcallAndArity >> 1;
        predicate = tagged2Const(pred);
        goto LBLcall;
      }
      RAISE_APPLY(pred,oz_mklist(OZ_atom("proc or builtin expected.")));
    }

  Case(CALLGLOBAL)
    {
      TaggedRef pred = GPC(1);
      int tailcallAndArity  = getPosIntArg(PC+2);
      Bool tailCall = tailcallAndArity & 1;
      int arity     = tailcallAndArity >> 1;

      DEREF(pred,predPtr,_1);
      if (oz_isVariable(pred)) {
        SUSP_PC(predPtr,PC);
      }

      if(oz_isAbstraction(pred)) {
        Abstraction *abstr = tagged2Abstraction(pred);
        if (abstr->getArity() == arity) {
          patchToFastCall(abstr,PC,tailCall);
          DISPATCH(0);
        }
      }

      CodeArea::writeArity(arity, PC+2);
      CodeArea::writeOpcode(tailCall ? TAILCALLG : CALLG,PC);
      DISPATCH(0);
    }

  Case(CALLMETHOD)
    {
      CallMethodInfo *cmi = (CallMethodInfo*)getAdressArg(PC+1);
      TaggedRef pred = CAP->getG(cmi->regIndex);
      DEREF(pred,predPtr,_1);
      if (oz_isVariable(pred)) {
        SUSP_PC(predPtr,PC);
      }

      Bool defaultsUsed;
      Abstraction *abstr = tagged2ObjectClass(pred)->getMethod(cmi->mn,cmi->arity,
                                                               0,defaultsUsed);
      /* fill cache and try again later */
      if (abstr==NULL || defaultsUsed) {
        isTailCall = cmi->isTailCall;
        if (!isTailCall) PC = PC+3;

        Assert(tagged2ObjectClass(pred)->getFallbackApply());

        X[1] = makeMessage(cmi->arity,cmi->mn,X);
        X[0] = pred;

        predArity = 2;
        predicate = tagged2Const(tagged2ObjectClass(pred)->getFallbackApply());
        goto LBLcall;
      }
      patchToFastCall(abstr,PC,cmi->isTailCall);
      cmi->dispose();
      DISPATCH(0);
    }



  /* The following must be different from the following,
   * otherwise definitionEnd breaks under threaded code
   */

  Case(GLOBALVARNAME)
    {
      // Trick: just do something weird
      DISPATCH(4711);
    }

  Case(LOCALVARNAME)
    {
      // Trick: just do something weird, but something different
      goto LBLcall;
    }

  Case(PROFILEPROC)
    {
      static int sizeOfDef = -1;
      if (sizeOfDef==-1) sizeOfDef = sizeOf(DEFINITION);

      Assert(CodeArea::getOpcode(PC-sizeOfDef) == DEFINITION);
      PrTabEntry *pred = getPredArg(PC-sizeOfDef+3); /* this is faster */

      pred->numCalled++;
      if (pred!=ozstat.currAbstr) {
        CTS->pushAbstr(ozstat.currAbstr);
        ozstat.leaveCall(pred);
      }

      DISPATCH(1);
    }

  Case(TASKCATCH)
    {
      Assert(0);
      DISPATCH(1);
    }

  Case(ENDOFFILE)
    {
      return T_ERROR;
    }

  Case(ENDDEFINITION)
    {
      return T_ERROR;
    }

#ifndef THREADED
  default:
    Assert(0);
     break;
   } /* switch*/
#endif


// ----------------- end instructions -------------------------------------


// ------------------------------------------------------------------------
// *** Special return values from unify: SUSPEND, EXCEPTION, etc.
// ------------------------------------------------------------------------

  LBLunifySpecial:
  {
    switch (tmpRet) {
    case BI_REPLACEBICALL:
      Assert(!e->isEmptyPreparedCalls());
      goto LBLreplaceBICall;
    case SUSPEND:
      PushContX(PC);
      SUSPENDONVARLIST;
    case RAISE:
      RAISE_THREAD;
    default:
      Assert(0);
    }
  }


  /*
   * Raise exception
   *
   */

 LBLraise:
  {
    Bool foundHdl;
    Thread *ct = CTT;

    if (e->exception.debug) {
      OZ_Term traceBack;
      foundHdl =
        ct->getTaskStackRef()->findCatch(ct,
                                         e->exception.pc,
                                         e->exception.y,
                                         e->exception.cap,
                                         &traceBack,
                                         e->debugmode());

      e->exception.value = formatError(e->exception.info,
                                       e->exception.value,
                                       traceBack);
    } else {
      foundHdl = ct->getTaskStackRef()->findCatch(ct);
    }

    if (foundHdl) {
      if (e->debugmode() && ct->isTrace())
        debugStreamUpdate(ct);
      e->xRegs[0] = e->exception.value;
      goto LBLpopTaskNoPreempt;
    }

    if (!CBB->isRoot() &&
        OZ_eq(OZ_label(e->exception.value),AtomFailure)) {
      return T_FAILURE;
    }

    if (e->debugmode()) {
      ct->setTrace();
      ct->setStep();
      debugStreamException(ct,e->exception.value);
      return T_PREEMPT;
    }

    if (e->defaultExceptionHdl) {
      ct->pushCall(e->defaultExceptionHdl,e->exception.value);
    } else {
      prefixError();
      fprintf(stderr,
              "Exception raised:\n   %s\n",
              OZ_toC(e->exception.value,100,100));
      fflush(stderr);
    }

    goto LBLpopTaskNoPreempt;
  }


  Assert(0);
  return T_ERROR;

} // end engine



#undef DISPATCH

#define DISPATCH(incPC,incArgs)                 \
   PC += incPC;                                 \
   argsToHandle += incArgs;                     \
   break;

void buildRecord(ProgramCounter PC, RefsArray Y, Abstraction *CAP) {
  Assert(oz_onToplevel());

  TaggedRef *sPointer, *TMPA, *TMPB;

  int argsToHandle = 0;

  TaggedRef * X = am.getXRef();

  int maxX = (CAP->getPred()->getMaxX()) * sizeof(TaggedRef);
  int maxY = (Y ? getRefsArraySize(Y) : 0) * sizeof(TaggedRef);

  void * savedX, * savedY;

  if (maxX > 0)
    savedX = memcpy(freeListMalloc(maxX), X, maxX);
  if (maxY > 0)
    savedY = memcpy(freeListMalloc(maxY), Y, maxY);

  Bool firstCall = OK;

  while (OK) {
    Opcode op = CodeArea::getOpcode(PC);

    if (!firstCall && argsToHandle==0)
      goto exit;

    firstCall = NO;
    switch(op) {

    case GETRECORDY: SETTMPA(YPC(3)); goto getRecord;
    case GETRECORDG: SETTMPA(GPC(3)); goto getRecord;
    case GETRECORDX: SETTMPA(XPC(3)); /* fall through */
      {
      getRecord:
        TaggedRef label = getLiteralArg(PC+1);
        SRecordArity ff = (SRecordArity) getAdressArg(PC+2);
        TaggedRef term  = GETTMPA();
        DEREF(term,termPtr,tag);

        Assert(isUVar(term));
        int numArgs = getWidth(ff);
        SRecord *srecord = SRecord::newSRecord(label,ff, numArgs);
        bindOPT(termPtr,makeTaggedSRecord(srecord));
        sPointer = srecord->getRef();
        DISPATCH(4,numArgs);
      }

    case GETLITERALY: /* fall through */
    case GETNUMBERY:  SETTMPA(YPC(2)); goto getNumber;
    case GETLITERALG: /* fall through */
    case GETNUMBERG:  SETTMPA(GPC(2)); goto getNumber;
    case GETLITERALX: /* fall through */
    case GETNUMBERX:  SETTMPA(XPC(2)); /* fall through */
    getNumber:
    {
      TaggedRef i = getNumberArg(PC+1);
      TaggedRef term = GETTMPA();
      DEREF(term,termPtr,tag);
      Assert(isUVar(tag));
      bindOPT(termPtr, i);
      DISPATCH(3,-1);
    }


    case GETLISTVALVARX:
      {
        TaggedRef term = XPC(1);
        DEREF(term,termPtr,tag);

        Assert(isUVar(term));
        LTuple *ltuple = new LTuple();
        ltuple->setHead(XPC(2));
        ltuple->setTail(am.currentUVarPrototype());
        bindOPT(termPtr,makeTaggedLTuple(ltuple));
        XPC(3) = makeTaggedRef(ltuple->getRef()+1);
        DISPATCH(4,0);
      }

    case GETLISTY: SETTMPA(YPC(1)); goto getList;
    case GETLISTG: SETTMPA(GPC(1)); goto getList;
    case GETLISTX: SETTMPA(XPC(1));
    getList:
      {
        TaggedRef aux = GETTMPA();
        DEREF(aux,auxPtr,tag);

        Assert(isUVar(aux));
        LTuple *ltuple = new LTuple();
        sPointer = ltuple->getRef();
        bindOPT(auxPtr,makeTaggedLTuple(ltuple));
        DISPATCH(2,2);
      }

    case UNIFYVARIABLEY: SETTMPA(YPC(1)); goto unifyVariable;
    case UNIFYVARIABLEX: SETTMPA(XPC(1));
    unifyVariable:
    {
        *sPointer = am.currentUVarPrototype();
        GETTMPA() = makeTaggedRef(sPointer++);
        DISPATCH(2,-1);
      }

    case UNIFYVALUEY: SETTMPA(YPC(1)); goto unifyValue;
    case UNIFYVALUEG: SETTMPA(GPC(1)); goto unifyValue;
    case UNIFYVALUEX: SETTMPA(XPC(1)); goto unifyValue;
    unifyValue:
    {
        *sPointer++ = GETTMPA();
        DISPATCH(2,-1);
      }

    case UNIFYVALVARXY: SETTMPA(XPC(1)); SETTMPB(YPC(2)); goto unifyValVar;
    case UNIFYVALVARYX: SETTMPA(YPC(1)); SETTMPB(XPC(2)); goto unifyValVar;
    case UNIFYVALVARYY: SETTMPA(YPC(1)); SETTMPB(YPC(2)); goto unifyValVar;
    case UNIFYVALVARGX: SETTMPA(GPC(1)); SETTMPB(XPC(2)); goto unifyValVar;
    case UNIFYVALVARGY: SETTMPA(GPC(1)); SETTMPB(YPC(2)); goto unifyValVar;
    case UNIFYVALVARXX: SETTMPA(XPC(1)); SETTMPB(XPC(2)); goto unifyValVar;
      {
      unifyValVar:
        *sPointer++ = GETTMPA();
        *sPointer++ = am.currentUVarPrototype();
        GETTMPB() = makeTaggedRef(sPointer);
        DISPATCH(3,-2);
      }

    case UNIFYVOID:
      {
        int n = getPosIntArg(PC+1);
        for (int i = n-1; i >=0; i-- ) {
          *sPointer++ = am.currentUVarPrototype();
        }
        DISPATCH(2,-n);
      }

    case UNIFYNUMBER:
    case UNIFYLITERAL:
      {
        *sPointer++ = getTaggedArg(PC+1);
        DISPATCH(2,-1);
      }

    default:
#ifdef DEBUG_CHECK
      displayCode(PC,1);
      displayDef(PC,1);
      Assert(0);
#endif
      goto exit;
    }
  }

 exit:
  if (maxX > 0) {
    memcpy(X, savedX, maxX); freeListDispose(savedX, maxX);
  }
  if (maxY > 0) {
    memcpy(Y, savedY, maxY); freeListDispose(savedY, maxY);
  }
}


// outlined:
void pushContX(TaskStack *stk,
               ProgramCounter pc,RefsArray y,Abstraction *cap,
               RefsArray x)
{
  stk->pushCont(pc,y,cap);
  stk->pushX(x,cap->getPred()->getMaxX());
}


#ifdef OUTLINE
#undef inline
#endif
