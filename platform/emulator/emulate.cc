/* This may look like C code, but it is really -*- C++ -*-

  Hydra Project, DFKI Saarbruecken,
  Stuhlsatzenhausweg 3, D-66123 Saarbruecken, Phone (+49) 681 302-5312
  Author: popow,mehl,scheidhr
  Last modified: $Date$ from $Author$
  Version: $Revision$
  State: $State$

  The main engine
  ------------------------------------------------------------------------
*/

#ifdef __GNUC__
#pragma implementation "emulate.hh"
#endif

#include "am.hh"

#include "indexing.hh"

#include "genvar.hh"
#include "fdhook.hh"


// -----------------------------------------------------------------------
// TOPLEVEL FAILURE (HF = Handle Failure)



#define HF_BODY(MSG_SHORT,MSG_LONG)                                           \
  if (allowTopLevelFailureMsg) {                                              \
    if (ozconf.errorVerbosity > 0) {                                         \
      toplevelErrorHeader();                                                  \
      {MSG_SHORT;}                                                            \
      if (ozconf.errorVerbosity > 1) {                                       \
        message("\n");                                                        \
        {MSG_LONG;}                                                           \
        e->currentThread->printDebug(PC,NO,10000);                    \
      }                                                                       \
      errorTrailer();                                                         \
    } else {                                                                  \
      message("Toplevel Failure\n");                                          \
    }                                                                         \
  } else {                                                                    \
    allowTopLevelFailureMsg = TRUE;                                           \
  }                                                                           \
  DebugCheck(ozconf.stopOnToplevelFailure, tracerOn();trace("toplevel failed"));



/* called if t1=t2 fails */
void failureUnify(AM *e, char *msgShort, TaggedRef arg1, TaggedRef arg2,
                  char *msgLong, ProgramCounter PC)
{
  HF_BODY(message(msgShort, OZ_toC(arg1),
                  (arg2 == makeTaggedNULL()) ? "" : OZ_toC(arg2)),
          message(msgLong));
}


#define HF_UNIFY(MSG_SHORT,T1,T2,MSG_LONG)                                    \
   if (!e->isToplevel()) { goto LBLfailure; }                                 \
   failureUnify(e,MSG_SHORT,T1,T2,MSG_LONG,PC);                               \
   goto LBLkillThread;



#define HF_FAIL(MSG_SHORT,MSG_LONG)                                           \
   if (!e->isToplevel()) { goto LBLfailure; }                                 \
   HF_BODY(MSG_SHORT,MSG_LONG);                                               \
   goto LBLkillThread;


void failureNomsg(AM *e, ProgramCounter PC) { HF_BODY(,); }

#define HF_NOMSG                                                              \
   if (!e->isToplevel()) { goto LBLfailure; }                                 \
   failureNomsg(e,PC);                                                        \
   goto LBLkillThread;



// always issue the message
#define HF_WARN(MSG_SHORT,MSG_LONG)                                           \
  if (ozconf.errorVerbosity > 0) {                                            \
    warningHeader();                                                          \
    { MSG_SHORT; }                                                            \
    if (ozconf.errorVerbosity > 1) {                                          \
       message("\n");                                                         \
       { MSG_LONG; }                                                          \
    }                                                                         \
    errorTrailer();                                                           \
  }                                                                           \
  HF_NOMSG;



#define NOFLATGUARD()   (!inShallowGuard)

#define SHALLOWFAIL if (!NOFLATGUARD()) { goto LBLshallowFail; }


#define CheckArity(arity,arityExp,pred,cont)                                  \
if (arity != arityExp && VarArity != arityExp) {                              \
  HF_WARN(applFailure(pred);                                                  \
          message("Wrong number of arguments: expected %d got %d\n",arityExp,arity),); \
}

// TOPLEVEL END
// -----------------------------------------------------------------------


#define DoSwitchOnTerm(indexTerm,table)                                       \
      TaggedRef term = indexTerm;                                             \
      DEREF(term,_1,_2);                                                      \
                                                                              \
      if (!isLTuple(term)) {                                                  \
        TaggedRef *sp = sPointer;                                             \
        ProgramCounter offset = switchOnTermOutline(term,table,sp);           \
        sPointer = sp;                                                        \
        JUMP(offset);                                                         \
      }                                                                       \
                                                                              \
      ProgramCounter offset = table->listLabel;                               \
      sPointer = tagged2LTuple(term)->getRef();                               \
      JUMP(offset);                                                           \



static
ProgramCounter switchOnTermOutline(TaggedRef term, IHashTable *table,
                                   TaggedRef *&sP)
{
  ProgramCounter offset = table->getElse();
  if (isSTuple(term)) {
    if (table->functorTable) {
      Literal *lname = tagged2STuple(term)->getLabelLiteral();
      int hsh = lname ? table->hash(lname->hash()) : 0;
      offset = table->functorTable[hsh]
            ->lookup(lname,tagged2STuple(term)->getSize(),offset);
      sP = tagged2STuple(term)->getRef();
    }
    return offset;
  }

  if (isLiteral(term)) {
    if (table->literalTable) {
      int hsh = table->hash(tagged2Literal(term)->hash());
      offset = table->literalTable[hsh]->lookup(tagged2Literal(term),offset);
    }
    return offset;
  }

  if (isNotCVar(term)) {
    return table->varLabel;
  }

  if (isSmallInt(term)) {
    if (table->numberTable) {
      int hsh = table->hash(smallIntHash(term));
      offset = table->numberTable[hsh]->lookup(term,offset);
    }
    return offset;
  }

  if (isFloat(term)) {
    if (table->numberTable) {
      int hsh = table->hash(tagged2Float(term)->hash());
      offset = table->numberTable[hsh]->lookup(term,offset);
    }
    return offset;
  }

  if (isBigInt(term)) {
    if (table->numberTable) {
      int hsh = table->hash(tagged2BigInt(term)->hash());
      offset =table->numberTable[hsh]->lookup(term,offset);
    }
    return offset;
  }

  if (isCVar(term)) {
    return (table->index(tagged2CVar(term),offset));
  }

  return offset;
}

Bool Board::isFailureInBody ()
{
  Assert(isWaiting () == OK);
  if (isWaitTop () == OK) {
    return (NO);
  } else {
#ifdef THREADED
    Opcode op = CodeArea::adressToOpcode (CodeArea::getOP (body.getPC ()));
#else
    Opcode op = CodeArea::getOP (body.getPC ());
#endif
    return (op == FAILURE);
  }
}

// -----------------------------------------------------------------------
// CALL HOOK


#ifdef OUTLINE
#define inline
#endif

/* the hook functions return:
     TRUE: must reschedule
     FALSE: can continue
   */

Bool AM::emulateHookOutline(Abstraction *def,
                            int arity,
                            TaggedRef *arguments)
{
  // without signal blocking;
  if (isSetSFlag(ThreadSwitch)) {
    if (threadQueueIsEmpty()
        || threadsHead->getPriority() < currentThread->getPriority()){
      restartThread();
    } else {
      return TRUE;
    }
  }
  if (isSetSFlag(StartGC)) {
    return TRUE;
  }

  osBlockSignals();
  // & with blocking of signals;
  if (isSetSFlag(UserAlarm)) {
    handleUser();
  }
  if (isSetSFlag(IOReady)) {
    handleIO();
  }

  osUnblockSignals();

  if (def && isSetSFlag(DebugMode)) {
    enterCall(currentBoard,def,arity,arguments);
  }

  return FALSE;
}

inline
Bool AM::hookCheckNeeded()
{
#ifdef DEBUG_DET
  static int counter = 100;
  if (--counter == 0) {
    handleAlarm();   // simulate an alarm
    counter = 100;
  }
#endif

  return (isSetSFlag());
}

/* macros are faster ! */
#define emulateHook(e,def,arity,arguments) \
 (e->hookCheckNeeded() && e->emulateHookOutline(def, arity, arguments))

#define emulateHook0(e) emulateHook(e,NULL,0,NULL)


/* NOTE:
 * in case we have call(x-N) and we have to switch process or do GC
 * we have to save as cont address Pred->getPC() and NOT PC
 */
#define CallDoChecks(Pred,gRegs,IsEx,ContAdr,Arity,CheckMode)                 \
     if (! IsEx) {                              \
       e->pushTask(CBB,ContAdr,Y,G);            \
     }                                          \
     G = gRegs;                                                               \
     if (CheckMode) e->currentThread->checkCompMode(Pred->getCompMode()); \
     if (emulateHook(e,Pred,Arity,X)) {                                       \
        e->pushTask(CBB,Pred->getPC(),NULL,G,X,Arity);                \
        goto LBLschedule;                                                     \
     }

// load a continuation into the machine registers PC,Y,G,X
#define LOADCONT(cont) \
  { \
      Continuation *tmpCont = cont; \
      PC = tmpCont->getPC(); \
      Y = tmpCont->getY(); \
      G = tmpCont->getG(); \
      XSize = tmpCont->getXSize(); \
      tmpCont->getX(X); \
  }

// -----------------------------------------------------------------------
// THREADED CODE

#if defined(RECINSTRFETCH) && defined(THREADED)
 Error: RECINSTRFETCH requires THREADED == 0;
#endif

#define INCFPC(N) PC += N

// #define WANT_INSTRPROFILE
#if defined(WANT_INSTRPROFILE)
#define asmLbl(INSTR) asm(" " #INSTR ":");
#else
#define asmLbl(INSTR)
#endif


/* threaded code broken on linux, leads to memory leek,
 * this is a workaround
 */

#ifdef THREADED
#define Case(INSTR)   asmLbl(INSTR); INSTR##LBL

#ifdef LINUX
#define DISPATCH(INC) INCFPC(INC); goto LBLdispatcher
#else

// let gcc fill in the delay slot of the "jmp" instruction:
#define DISPATCH(INC) {                                                       \
  intlong aux = *(PC+INC);                                                    \
  INCFPC(INC);                                                                \
  goto* (void*) (aux|textBase);                                       \
}
#endif /* LINUX */

#else /* THREADED */

#define Case(INSTR)   case INSTR
#define DISPATCH(INC) INCFPC(INC); goto LBLdispatcher

#endif

#define JUMP(absAdr) PC = absAdr; DISPATCH(0)

#define ONREG(Label,R)      HelpReg = (R); goto Label
#define ONREG2(Label,R1,R2) HelpReg = (R1); HelpReg2 = (R2); goto Label


#ifdef FASTREGACCESS
#define RegAccess(Reg,Index) (*(RefsArray)((intlong) Reg + Index))
#define LessIndex(Index,CodedIndex) (Index <= CodedIndex/sizeof(TaggedRef))
#else
#define RegAccess(Reg,Index) (Reg[Index])
#define LessIndex(I1,I2) (I1<=I2)
#endif

#define Xreg(N) RegAccess(X,N)
#define Yreg(N) RegAccess(Y,N)
#define Greg(N) RegAccess(G,N)

#define XPC(N) Xreg(getRegArg(PC+N))


/* install new board, continue only if successful
   opt:
     if already installed do nothing
 */

#define INSTALLPATH(bb)                                                       \
  if (CBB != bb) {                                                            \
    LOCAL_PROPAGATION(Suspension * tmp = currentTaskSusp;)                    \
    switch (e->installPath(bb)) {                                             \
    case INST_REJECTED:                                                       \
      currentTaskSusp = NULL;                                                 \
      goto LBLpopTask;                                                        \
    case INST_FAILED:                                                         \
      currentTaskSusp = NULL;                                                 \
      goto LBLfailure;                                                        \
    case INST_OK:                                                             \
      LOCAL_PROPAGATION(currentTaskSusp = tmp;)                               \
      break;                                                                  \
    }                                                                         \
  }                                                                           \
  CBB->unsetNervous();



/* define REGOPT if you want the into register optimization for GCC */
#if defined(REGOPT) &&__GNUC__ >= 2 && (defined(MIPS) || defined(OSF1_ALPHA) || defined(SPARC)) && !defined(DEBUG_CHECK)
#define Into(Reg) asm(#Reg)

#ifdef SPARC
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

#ifdef MIPS
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

#define SetReadMode
#define SetWriteMode (sPointer = (TaggedRef *)((long)sPointer+1));

#define InWriteMode (((long)sPointer)&1)

#define GetSPointerWrite(ptr) (TaggedRef*)(((long)ptr)-1)

// ------------------------------------------------------------------------
// outlined auxiliary functions
// ------------------------------------------------------------------------

#define CHECKSEQ                                                              \
if (e->currentThread->compMode == ALLSEQMODE) {                               \
  e->currentThread=0;                                                         \
  goto LBLstart;                                                              \
}                                                                             \
goto LBLcheckEntailment;

inline
void addSusp(TaggedRef var,Suspension *susp)
{
  DEREF(var,varPtr,_1);
  taggedBecomesSuspVar(varPtr)->addSuspension(susp);
}

inline
void addSusp(TaggedRef *varPtr,Suspension *susp)
{
  taggedBecomesSuspVar(varPtr)->addSuspension(susp);
}

/*
 * create the suspension for builtins returning SUSPEND
 *
 * PRE: no reference chains !!
 */
void AM::suspendOnVarList(Suspension *susp)
{
  Assert(suspendVarList!=makeTaggedNULL());
  TaggedRef varList=suspendVarList;
  while (!isRef(varList)) {
    Assert(isCons(varList));
    addSusp(head(varList),susp);
    varList=tail(varList);
  }
  addSusp(varList,susp);
  suspendVarList=makeTaggedNULL();
}

//inline
Suspension *AM::mkSuspension(Board *b, int prio, ProgramCounter PC,
                             RefsArray Y, RefsArray G,
                             RefsArray X, int argsToSave)
{
#ifndef NEWCOUNTER
  b->incSuspCount();
#endif
  switch (currentThread->getCompMode()) {
  case ALLSEQMODE:
    pushTask(b,PC,Y,G,X,argsToSave);
    return new Suspension(currentThread);
  case SEQMODE:
    {
      pushTask(b,PC,Y,G,X,argsToSave);
      Thread *th=newThread(currentThread->getPriority(),currentBoard);
      th->getSeqFrom(currentThread);
      return new Suspension(th);
    }
  case PARMODE:
    return new Suspension(b,prio,PC,Y,G,X,argsToSave);
  default:
    Assert(0);
    return 0;
  }
}

//inline
Suspension *AM::mkSuspension(Board *b, int prio, OZ_CFun bi,
                             RefsArray X, int argsToSave)
{
#ifndef NEWCOUNTER
  b->incSuspCount();
#endif
  switch (currentThread->getCompMode()) {
  case ALLSEQMODE:
    pushCFun(b,bi,X,argsToSave);
    return new Suspension(currentThread);
  case SEQMODE:
    {
      pushCFun(b,bi,X,argsToSave);
      Thread *th=newThread(currentThread->getPriority(),currentBoard);
      th->getSeqFrom(currentThread);
      return new Suspension(th);
    }
  case PARMODE:
    return new Suspension(b,prio,bi,X,argsToSave);
  default:
    Assert(0);
    return 0;
  }
}

void AM::suspendInline(Board *bb,int prio,OZ_CFun fun,int n,
                       OZ_Term A,OZ_Term B,OZ_Term C,OZ_Term D)
{
  static RefsArray X = allocateStaticRefsArray(4);
  X[0]=A;
  X[1]=B;
  X[2]=C;
  X[3]=D;

  Suspension *susp=mkSuspension(bb,prio,fun,X,n);
  while (--n>=0) {
    DEREF(X[n],ptr,_1);
    if (isAnyVar(X[n])) addSusp(ptr,susp);
  }
}


static
TaggedRef makeMethod(int arity, TaggedRef label, TaggedRef *X)
{
  if (arity == 0) {
    return label;
  } else {
    if (arity == 2 && sameLiteral(label,AtomCons)) {
      return makeTaggedLTuple(new LTuple(X[3],X[4]));
    } else {
      STuple *tuple = STuple::newSTuple(label,arity);
      for (int i = arity-1;i >= 0; i--) {
        tuple->setArg(i,X[i+3]);
      }
      return makeTaggedSTuple(tuple);
    }
  }
}

TaggedRef AM::createNamedVariable(int regIndex, TaggedRef name)
{
  int size = getRefsArraySize(toplevelVars);
  if (LessIndex(size,regIndex)) {
    int newSize = int(size*1.5);
    message("resizing store for toplevel vars from %d to %d\n",size,newSize);
    toplevelVars = resize(toplevelVars,newSize);
    // no deletion of old array --> GC does it
  }
  SVariable *svar = new SVariable(currentBoard);
  TaggedRef ret = makeTaggedRef(newTaggedSVar(svar));
  VariableNamer::addName(ret,name);
  return ret;
}

// aux debugging;
#define VERBMSG(S,A1,A2)                                                   \
    fprintf(verbOut,"(em) %s (arg#1 0x%x, arg#2 0x%x) :%d\n",              \
            S,A1,A2,__LINE__);                                             \
    fflush(verbOut);


/* &Var prevent Var to be allocated to a register,
 * increases chances that variables declared as "register"
 * will be really allocated to registers
 */

#define NoReg(Var) { void *p = &Var; }


void engine() {

// ------------------------------------------------------------------------
// *** Global Variables
// ------------------------------------------------------------------------
  /* ordered by iomportance: first variables will go into machine registers
   * if -DREGOPT is set
   */
  register ProgramCounter PC   Reg1 = 0;
  register RefsArray X         Reg2 = am.xRegs;
  register RefsArray Y         Reg3 = NULL;
  register TaggedRef *sPointer Reg4 = NULL;
  register AM *e               Reg5 = &am;
  register RefsArray G         Reg6 = NULL;

  int XSize = 0; NoReg(XSize);

  Bool isTailCall = NO; NoReg(isTailCall);
  Suspension* &currentTaskSusp =
    FDcurrentTaskSusp; NoReg(currentTaskSusp);
  AWActor *CAA = NULL;
  Board *tmpBB = NULL; NoReg(tmpBB);

# define CBB (e->currentBoard)
# define CPP (e->currentThread->getPriority())

  RefsArray HelpReg = NULL, HelpReg2 = NULL;
  OZ_CFun biFun = NULL;     NoReg(biFun);

  /* shallow choice pointer */
  ByteCode *shallowCP = NULL;
  Bool inShallowGuard = NO;

  /* which kind of solve combinator to choose */
  Bool isEatWaits    = NO;
  Bool isSolveGuided = NO;

  Chunk *predicate; NoReg(predicate);
  int predArity;    NoReg(predArity);

#ifdef CATCH_SEGV
  switch (e->catchError()) {

  case NOEXCEPTION:
    break;

  case SEGVIO:
    HF_FAIL(message("segmentation violation"),);
    break;
   case BUSERROR:
    HF_FAIL(message("bus error"),);
    break;
  }
#endif


#ifdef THREADED
# include "instrtab.hh"
  CodeArea::globalInstrTable = instrTable;
# define op (Opcode) -1
#else
  Opcode op = (Opcode) -1;
#endif

  goto LBLstart;

// ------------------------------------------------------------------------
// *** RUN: Main Loop
// ------------------------------------------------------------------------
 LBLschedule:

  e->scheduleThread(e->currentThread);
  e->currentThread=(Thread *) NULL;

 LBLerror:
 LBLstart:

// ------------------------------------------------------------------------
// *** gc
// ------------------------------------------------------------------------
  if (e->isSetSFlag(StartGC)) {
    e->doGC();
  }

// ------------------------------------------------------------------------
// *** process switch
// ------------------------------------------------------------------------
  if (e->threadQueueIsEmpty()) {
    e->suspendEngine();
  }

  e->currentThread = e->getFirstThread();

  DebugTrace(trace("thread switched"));

  e->restartThread();


// ------------------------------------------------------------------------
// *** pop a task
// ------------------------------------------------------------------------
 LBLpopTask:
  {
    asmLbl(popTask);
    DebugCheckT(Board *fsb);
    if (emulateHook0(e)) {
      goto LBLschedule;
    }

    DebugCheckT(CAA = NULL);

    TaskStack *taskstack = &e->currentThread->taskStack;
    TaskStackEntry *topCache = taskstack->getTop();
    TaskStackEntry topElem=TaskStackPop(topCache-1);
    TaggedBoard tb = (TaggedBoard) ToInt32(topElem);

    ContFlag cFlag = getContFlag(tb);


    /* RS: Optimize most probable case:
     *  - do not handle C_CONT in switch --> faster
     *  - assume cFlag == C_CONT implies stack does not contain empty mark
     *  - if tb==rootBoard then no need to call getBoard
     *  - topCache maintained more efficiently
     */
    if (cFlag == C_CONT) {
      Assert(!taskstack->isEmpty((TaskStackEntry) tb));
      PC = (ProgramCounter) TaskStackPop(topCache-2);
      Y = (RefsArray) TaskStackPop(topCache-3);
      G = (RefsArray) TaskStackPop(topCache-4);
      taskstack->setTop(topCache-4);
      Board *auxBoard = getBoard(tb,C_CONT);
      if (!auxBoard->isRoot()) {
        auxBoard = auxBoard->getBoardFast();
      } else {
        /* optimization: no need to deref
           [ and maintain counter (mm2)] (RS) */
      }
      Assert((fsb = auxBoard->getSolveBoard())==0 ||
             !fsb->isReflected ());
      INSTALLPATH(auxBoard);
#ifndef NEWCOUNTER
      auxBoard->decSuspCount();
#endif
      goto LBLemulate;
    }

    if (taskstack->isEmpty((TaskStackEntry) tb)) { //
      if (e->currentThread->hasNotificationBoard () == OK) {
        Board *nb = e->currentThread->notificationBoard->getBoardFast();
        e->decSolveThreads(nb);
      }
      goto LBLkillThread;
    }

    topCache--;
    if (cFlag == C_COMP_MODE) {
      taskstack->setTop(topCache);
      e->currentThread->compMode=TaskStack::getCompMode(topElem);
      goto LBLpopTask;;
    }
    tmpBB = getBoard(tb,cFlag)->getBoardFast();
    switch (cFlag){
    case C_XCONT:
      PC = (ProgramCounter) TaskStackPop(--topCache);
      Y = (RefsArray) TaskStackPop(--topCache);
      G = (RefsArray) TaskStackPop(--topCache);
      {
        RefsArray tmpX = (RefsArray) TaskStackPop(--topCache);
        XSize = getRefsArraySize(tmpX);
        int i = XSize;
        while (--i >= 0) {
          X[i] = tmpX[i];
        }
        disposeRefsArray(tmpX);
      }
      taskstack->setTop(topCache);

      Assert((fsb = tmpBB->getSolveBoard())==0 ||
             !fsb->isReflected ());
      INSTALLPATH(tmpBB);
#ifndef NEWCOUNTER
      tmpBB->decSuspCount();
#endif

      goto LBLemulate;

    case C_DEBUG_CONT:
      {
        OzDebug *ozdeb = (OzDebug *) TaskStackPop(--topCache);
        taskstack->setTop(topCache);
        if (CBB != tmpBB) {
          switch (e->installPath(tmpBB)) {
          case INST_REJECTED: exitCall(FAILED,ozdeb); goto LBLpopTask;
          case INST_FAILED:   exitCall(FAILED,ozdeb); goto LBLfailure;
          case INST_OK:       break;
          }
        }
#ifndef NEWCOUNTER
        tmpBB->decSuspCount();
#endif

        exitCall(PROCEED,ozdeb);
        goto LBLcheckEntailment;
      }

    case C_CALL_CONT:
      {
        predicate = (Chunk *) TaskStackPop(--topCache);
        RefsArray tmpX = (RefsArray) TaskStackPop(--topCache);
        predArity = tmpX ? getRefsArraySize(tmpX) : 0;
        int i = predArity;
        while (--i >= 0) {
          X[i] = tmpX[i];
        }
        disposeRefsArray(tmpX);
        taskstack->setTop(topCache);
        INSTALLPATH(tmpBB);
#ifndef NEWCOUNTER
        tmpBB->decSuspCount();
#endif

        isTailCall = OK;
        goto LBLcall;
      }
    case C_NERVOUS:
      {
        // by kost@ : 'SolveActor::Waker' can produce such task
        // (if the search problem is stable by its execution);
        taskstack->setTop(topCache);
        // nervous already done ?
        if (!tmpBB->isNervous()) {
          goto LBLpopTask;
        }

        DebugTrace(trace("nervous",tmpBB));
        INSTALLPATH(tmpBB);
        goto LBLcheckEntailment;
      }

    case C_CFUNC_CONT:
      {
        // by kost@ : 'solve actors' are represented via the c-function;
        biFun = (OZ_CFun) TaskStackPop(--topCache);
        currentTaskSusp = (Suspension*) TaskStackPop(--topCache);
        RefsArray tmpX = (RefsArray) TaskStackPop(--topCache);
        if (tmpX != NULL) {
          XSize = getRefsArraySize(tmpX);
          int i = XSize;
          while (--i >= 0) {
            X[i] = tmpX[i];
          }
        } else {
          XSize = 0;
        }
        /* RS: dont't know, if we can dispose for FDs */
        if (currentTaskSusp == NULL) {
          disposeRefsArray(tmpX);
        }
        taskstack->setTop(topCache);

        if (currentTaskSusp != NULL && currentTaskSusp->isDead()) {
          currentTaskSusp = NULL;
          goto LBLpopTask;
        }

        Assert((fsb = tmpBB->getSolveBoard())==0 || !fsb->isReflected ());
        INSTALLPATH(tmpBB);
#ifndef NEWCOUNTER
        tmpBB->decSuspCount();
#endif

        LOCAL_PROPAGATION(Assert(localPropStore.isEmpty()));

        switch (biFun(XSize, X)) {
        case FAILED:
          killPropagatedCurrentTaskSusp();
          LOCAL_PROPAGATION(localPropStore.reset());
        localhack0:
          HF_FAIL(applFailure(biFun), printArgs(X,XSize));
        case PROCEED:
          killPropagatedCurrentTaskSusp();
          LOCAL_PROPAGATION(if (! localPropStore.do_propagation())
                            goto localhack0;);
          goto LBLcheckEntailment;
        case SUSPEND:
          {
            killPropagatedCurrentTaskSusp();
            LOCAL_PROPAGATION(if (! localPropStore.do_propagation())
                              goto localhack0;);
            Suspension *susp = e->mkSuspension(CBB,CPP,biFun,X,XSize);
            e->suspendOnVarList(susp);
            CHECKSEQ;
          }
        default:
          Assert(0);
          goto LBLerror;
        } // switch
      }

    default:
      Assert(0);
      goto LBLerror;
    }  // switch


  LBLkillThread:
    {
      Thread *tmpThread = e->currentThread;
      e->currentThread=(Thread *) NULL;
      if (tmpThread) {  /* may happen if catching SIGSEGV and SIGBUS */
        e->disposeThread(tmpThread);
      }
    }
    goto LBLstart;
  }

  error("never here");
  goto LBLerror;

// ----------------- end popTask -----------------------------------------

// ------------------------------------------------------------------------
// *** Emulate if no task
// ------------------------------------------------------------------------

 LBLemulateHook:
  if (emulateHook0(e)) {
    e->pushTaskOutline(CBB,PC,Y,G,X,XSize);
    goto LBLschedule;
  }
  goto LBLemulate;

// ------------------------------------------------------------------------
// *** Emulate: execute continuation
// ------------------------------------------------------------------------
 LBLemulate:

  JUMP( PC );

 LBLdispatcher:

#if defined(THREADED) && defined(LINUX)
  /* threaded code broken under linux */
  goto* (void*) (*PC);
#endif

#ifdef SLOW_DEBUG_CHECK
  /* These tests make the emulator really sloooooww */
  DebugCheck(osBlockSignals() == NO,
             error("signalmask not zero"));
  DebugCheckT(osUnblockSignals());
  DebugCheck ((e->currentSolveBoard != CBB->getSolveBoard ()),
              error ("am.currentSolveBoard and real solve board mismatch"));

  Assert(!isFreedRefsArray(Y));
#endif

#ifndef THREADED
  op = CodeArea::getOP(PC);
#endif

#ifdef RECINSTRFETCH
  CodeArea::recordInstr(PC);
#endif

  DebugTrace( if (!trace("emulate",CBB,CAA,PC,Y,G)) {
                goto LBLfailure;
              });

#ifndef THREADED
  switch (op) {
#endif

// the instructions are classified into groups
// to find a certain class of instructions search for the String "CLASS:"
// -------------------------------------------------------------------------
// CLASS: TERM: MOVE/UNIFY/CREATEVAR/...
// -------------------------------------------------------------------------

#include "register.hh"

// ------------------------------------------------------------------------
// CLASS: (Fast-) Call/Execute Inline Funs/Rels
// ------------------------------------------------------------------------

  Case(FASTCALL):
    {

      AbstractionEntry *entry = (AbstractionEntry *) getAdressArg(PC+1);
      INCFPC(2);

      Assert((e->currentThread->compMode&1) == entry->getAbstr()->getCompMode());
      CallDoChecks(entry->getAbstr(),entry->getGRegs(),NO,PC,
                   entry->getAbstr()->getArity(),NO);

      Y = NULL; // allocateL(0);
      // set pc
      IHashTable *table = entry->indexTable;
      if (table) {
        DoSwitchOnTerm(X[0],table);
      } else {
        JUMP(entry->getPC());
      }
    }


  Case(FASTTAILCALL):
    {
      AbstractionEntry *entry = (AbstractionEntry *) getAdressArg(PC+1);

      Assert((e->currentThread->compMode&1) == entry->getAbstr()->getCompMode());
      CallDoChecks(entry->getAbstr(),entry->getGRegs(),OK,PC,
                   entry->getAbstr()->getArity(),NO);

      Y = NULL; // allocateL(0);
      // set pc
      IHashTable *table = entry->indexTable;
      if (table) {
        DoSwitchOnTerm(X[0],table);
      } else {
        JUMP(entry->getPC());
      }
    }

  Case(CALLBUILTIN):
    {
      BuiltinTabEntry* entry = (BuiltinTabEntry*) getAdressArg(PC+1);
      OZ_CFun fun = entry->getFun();
      int arityGot = getPosIntArg(PC+2);
      int arity = entry->getArity();

      CheckArity(arityGot,arity,entry,PC+3);

      LOCAL_PROPAGATION(Assert(localPropStore.isEmpty()));

      switch (fun(arity, X)){
      case SUSPEND:
        {
          e->pushTaskOutline(CBB,PC+3,Y,G,0,0);
          Suspension *susp = e->mkSuspension(CBB,CPP,fun,X,arity);
          e->suspendOnVarList(susp);
          CHECKSEQ;
        }
      case FAILED:
        killPropagatedCurrentTaskSusp();
        LOCAL_PROPAGATION(localPropStore.reset());
      localhack1:
        HF_FAIL(applFailure(fun), printArgs(X,arity));
      case PROCEED:
        killPropagatedCurrentTaskSusp();
        LOCAL_PROPAGATION(if (! localPropStore.do_propagation())
                          goto localhack1;);
        DISPATCH(3);
      default:
        Assert(0);
      }
    }


  Case(INLINEREL1):
    {
      BuiltinTabEntry* entry = (BuiltinTabEntry*) getAdressArg(PC+1);
      InlineRel1 rel         = (InlineRel1)entry->getInlineFun();

      switch(rel(XPC(2))) {
      case PROCEED:
        DISPATCH(4);

      case SUSPEND:
        if (shallowCP) {
          e->trail.pushIfVar(XPC(2));
          DISPATCH(4);
        }
        e->pushTaskOutline(CBB,PC+4,Y,G,X,getPosIntArg(PC+3));
        e->suspendInline(CBB,CPP,entry->getFun(),1,XPC(2));
        CHECKSEQ;
      case FAILED:
        SHALLOWFAIL;
        HF_FAIL(applFailure(entry), printArgs(1,XPC(2)));
      }
    }

  Case(INLINEREL2):
    {
      BuiltinTabEntry* entry = (BuiltinTabEntry*) getAdressArg(PC+1);
      InlineRel2 rel         = (InlineRel2)entry->getInlineFun();

      switch(rel(XPC(2),XPC(3))) {
      case PROCEED:
        DISPATCH(5);

      case SUSPEND:
        {
          if (shallowCP) {
            e->trail.pushIfVar(XPC(2));
            e->trail.pushIfVar(XPC(3));
            DISPATCH(5);
          }

          e->pushTaskOutline(CBB,PC+5,Y,G,X,getPosIntArg(PC+4));
          e->suspendInline(CBB,CPP,entry->getFun(),2,XPC(2),XPC(3));
          CHECKSEQ;
        }
      case FAILED:
        SHALLOWFAIL;
        HF_FAIL(applFailure(entry), printArgs(2,XPC(2),XPC(3)));
      }
    }

  Case(INLINEFUN1):
    {
      BuiltinTabEntry* entry = (BuiltinTabEntry*) getAdressArg(PC+1);
      InlineFun1 fun         = (InlineFun1)entry->getInlineFun();

      // XPC(3) maybe the same register as XPC(2)
      switch(fun(XPC(2),XPC(3))) {
      case PROCEED:
        DISPATCH(5);

      case SUSPEND:
        {
          TaggedRef A=XPC(2);
          TaggedRef B=XPC(3) = makeTaggedRef(newTaggedUVar(CBB));
          if (shallowCP) {
            e->trail.pushIfVar(A);
            DISPATCH(5);
          }
          e->pushTaskOutline(CBB,PC+5,Y,G,X,getPosIntArg(PC+4));
          e->suspendInline(CBB,CPP,entry->getFun(),2,A,B);
          CHECKSEQ;
        }

      case FAILED:
        SHALLOWFAIL;
        HF_FAIL(applFailure(entry), printArgs(1,XPC(2)));
      }
    }

  Case(INLINEFUN2):
    {
      BuiltinTabEntry* entry = (BuiltinTabEntry*) getAdressArg(PC+1);
      InlineFun2 fun = (InlineFun2)entry->getInlineFun();

      // note XPC(4) is maybe the same as XPC(2) or XPC(3) !!
      switch(fun(XPC(2),XPC(3),XPC(4))) {
      case PROCEED:
        DISPATCH(6);

      case SUSPEND:
        {
          TaggedRef A=XPC(2);
          TaggedRef B=XPC(3);
          TaggedRef C=XPC(4) = makeTaggedRef(newTaggedUVar(CBB));
          if (shallowCP) {
            e->trail.pushIfVar(A);
            e->trail.pushIfVar(B);
            DISPATCH(6);
          }
          e->pushTaskOutline(CBB,PC+6,Y,G,X,getPosIntArg(PC+5));
          e->suspendInline(CBB,CPP,entry->getFun(),3,A,B,C);
          CHECKSEQ;
        }

      case FAILED:
        SHALLOWFAIL;
        HF_FAIL(applFailure(entry), printArgs(2,XPC(2),XPC(3)));
      }
    }


  Case(INLINEFUN3):
    {
      BuiltinTabEntry* entry = (BuiltinTabEntry*) getAdressArg(PC+1);
      InlineFun3 fun = (InlineFun3)entry->getInlineFun();

      // note XPC(5) is maybe the same as XPC(2) or XPC(3) or XPC(4) !!
      switch(fun(XPC(2),XPC(3),XPC(4),XPC(5))) {
      case PROCEED:
        DISPATCH(7);

      case SUSPEND:
        {
          TaggedRef A=XPC(2);
          TaggedRef B=XPC(3);
          TaggedRef C=XPC(4);
          TaggedRef D=XPC(5) = makeTaggedRef(newTaggedUVar(CBB));
          if (shallowCP) {
            e->trail.pushIfVar(A);
            e->trail.pushIfVar(B);
            e->trail.pushIfVar(C);
            DISPATCH(7);
          }
          e->pushTaskOutline(CBB,PC+7,Y,G,X,getPosIntArg(PC+6));
          e->suspendInline(CBB,CPP,entry->getFun(),4,A,B,C,D);
          CHECKSEQ;
        }

      case FAILED:
        SHALLOWFAIL;
        HF_FAIL(applFailure(entry), printArgs(3,XPC(2),XPC(3),XPC(4)));
      }
    }

  Case(INLINEEQEQ):
    {
      BuiltinTabEntry* entry = (BuiltinTabEntry*) getAdressArg(PC+1);
      InlineFun2 fun = (InlineFun2)entry->getInlineFun();

      // note XPC(4) is maybe the same as XPC(2) or XPC(3) !!
      switch (fun(XPC(2),XPC(3),XPC(4))) {
      case FAILED:  Assert(0);
      case PROCEED:
        DISPATCH(6);
      case SUSPEND:
        {
          TaggedRef A=XPC(2);
          TaggedRef B=XPC(3);
          TaggedRef C=XPC(4) = makeTaggedRef(newTaggedUVar(CBB));
          e->pushTaskOutline(CBB,PC+6,Y,G,X,getPosIntArg(PC+5));
          e->suspendInline(CBB,CPP,entry->getFun(),3,A,B,C);
          CHECKSEQ;
        }
      }
    }

#undef SHALLOWFAIL

// ------------------------------------------------------------------------
// CLASS: Shallow guards stuff
// ------------------------------------------------------------------------

  Case(SHALLOWGUARD):
    {
      shallowCP = PC;
      inShallowGuard = OK;
      e->trail.pushMark();
      DISPATCH(3);
    }

  Case(SHALLOWTEST1):
    {
      BuiltinTabEntry* entry = (BuiltinTabEntry*) getAdressArg(PC+1);
      InlineRel1 rel         = (InlineRel1)entry->getInlineFun();

      switch(rel(XPC(2))) {

      case PROCEED: DISPATCH(5);

      case FAILED:  JUMP( getLabelArg(PC+3) );

      case SUSPEND:
        {
          Suspension *susp = e->mkSuspension(CBB,CPP,
                                             PC,Y,G,X,getPosIntArg(PC+4));
          addSusp(XPC(2),susp);
        }
        CHECKSEQ;
      }
    }

  Case(SHALLOWTEST2):
    {
      BuiltinTabEntry* entry = (BuiltinTabEntry*) getAdressArg(PC+1);
      InlineRel2 rel         = (InlineRel2)entry->getInlineFun();

      switch(rel(XPC(2),XPC(3))) {

      case PROCEED: DISPATCH(6);

      case FAILED:  JUMP( getLabelArg(PC+4) );

      case SUSPEND:
        {
          Suspension *susp=e->mkSuspension(CBB,CPP,
                                           PC,Y,G,X,getPosIntArg(PC+5));
          OZ_Term A=XPC(2);
          OZ_Term B=XPC(3);
          DEREF(A,APtr,ATag); DEREF(B,BPtr,BTag);
          Assert(isAnyVar(ATag) || isAnyVar(BTag));
          if (isAnyVar(A)) addSusp(APtr,susp);
          if (isAnyVar(B)) addSusp(BPtr,susp);
          CHECKSEQ;
        }
      }
    }

  Case(SHALLOWTHEN):
    {
      if (e->trail.isEmptyChunk()) {
        inShallowGuard = NO;
        shallowCP = NULL;
        e->trail.popMark();
        DISPATCH(1);
      }

/* RS:  OUTLINE */
      int numbOfCons = e->trail.chunkSize();
      Assert(numbOfCons>0);

      if (ozconf.showSuspension) {
        printSuspension(PC);
      }

      int argsToSave = getPosIntArg(shallowCP+2);
      Suspension *susp = e->mkSuspension(CBB,CPP,
                                         shallowCP,Y,G,X,argsToSave);
      inShallowGuard = NO;
      shallowCP = NULL;
      e->reduceTrailOnShallow(numbOfCons);
      e->suspendOnVarList(susp);
      CHECKSEQ;
    }


// -------------------------------------------------------------------------
// CLASS: Environment
// -------------------------------------------------------------------------

  Case(ALLOCATEL):
    {
      int posInt = getPosIntArg(PC+1);
      Assert(posInt > 0);
      Y = allocateY(posInt);
      DISPATCH(2);
    }

  Case(ALLOCATEL1):  { Y =  allocateY(1); DISPATCH(1); }
  Case(ALLOCATEL2):  { Y =  allocateY(2); DISPATCH(1); }
  Case(ALLOCATEL3):  { Y =  allocateY(3); DISPATCH(1); }
  Case(ALLOCATEL4):  { Y =  allocateY(4); DISPATCH(1); }
  Case(ALLOCATEL5):  { Y =  allocateY(5); DISPATCH(1); }
  Case(ALLOCATEL6):  { Y =  allocateY(6); DISPATCH(1); }
  Case(ALLOCATEL7):  { Y =  allocateY(7); DISPATCH(1); }
  Case(ALLOCATEL8):  { Y =  allocateY(8); DISPATCH(1); }
  Case(ALLOCATEL9):  { Y =  allocateY(9); DISPATCH(1); }
  Case(ALLOCATEL10): { Y = allocateY(10); DISPATCH(1); }

  Case(DEALLOCATEL):
    {
      Assert(!isFreedRefsArray(Y));
      if (!isDirtyRefsArray(Y)) {
        deallocateY(Y);
      }
      Y=NULL;
      DISPATCH(1);
    }
// -------------------------------------------------------------------------
// CLASS: CONTROL: FAIL/SUCCESS/RETURN/SAVECONT
// -------------------------------------------------------------------------

  Case(FAILURE):
    {
      HF_FAIL(, message("Executing 'false'\n"));
    }


  Case(SUCCEED):
    DISPATCH(1);

  Case(SAVECONT):
    {
      e->pushTask(CBB,getLabelArg(PC+1),Y,G);
      DISPATCH(2);
    }

  Case(RETURN):
  {
    goto LBLcheckEntailment;
  }


// ------------------------------------------------------------------------
// CLASS: Definition
// ------------------------------------------------------------------------

  Case(DEFINITION):
    {
      Reg reg                     = getRegArg(PC+1);
      ProgramCounter nxt          = getLabelArg(PC+2);
      PrTabEntry *predd           = getPredArg(PC+5);
      AbstractionEntry *predEntry = (AbstractionEntry*) getAdressArg(PC+6);

      AssRegArray &list = predd->gRegs;
      int size = list.getSize();
      RefsArray gRegs = (size == 0) ? (RefsArray) NULL : allocateRefsArray(size);

      Abstraction *p = new Abstraction (predd, gRegs, CBB);

      if (predEntry) {
        predEntry->setPred(p);
      }

      for (int i = 0; i < size; i++) {
        switch (list[i].kind) {
        case XReg: gRegs[i] = X[list[i].number]; break;
        case YReg: gRegs[i] = Y[list[i].number]; break;
        case GReg: gRegs[i] = G[list[i].number]; break;
        }
      }
      Xreg(reg) = makeTaggedConst(p);
      JUMP(nxt);
    }

// -------------------------------------------------------------------------
// CLASS: CONTROL: FENCE/CALL/EXECUTE/SWITCH/BRANCH
// -------------------------------------------------------------------------

  Case(BRANCH):
    JUMP( getLabelArg(PC+1) );


  /* weakDet(X) woken up, WHENEVER something happens to X
   *            (important if X is a FD var) */
  Case(WEAKDETX): ONREG(WeakDet,X);
  Case(WEAKDETY): ONREG(WeakDet,Y);
  Case(WEAKDETG): ONREG(WeakDet,G);
  WeakDet:
  {
    TaggedRef term = RegAccess(HelpReg,getRegArg(PC+1));
    DEREF(term,termPtr,tag);

    if (!isAnyVar(tag)) {
      DISPATCH(3);
    }
    /* INCFPC(3); do NOT suspend on next instructions: DET suspensions are
                  woken up always, even if variable is bound to another var */

    int argsToSave = getPosIntArg(PC+2);
    Suspension *susp = e->mkSuspension(CBB,CPP,PC,Y,G,X,argsToSave);
    addSusp(makeTaggedRef(termPtr),susp);
    CHECKSEQ;
  };


  /* det(X) wait until X will be ground */
#define DETBUGGY
#ifdef DETBUGGY
  Case(DETX): ONREG(WeakDet,X);
  Case(DETY): ONREG(WeakDet,Y);
  Case(DETG): ONREG(WeakDet,G);
#else
  Case(DETX): ONREG(Det,X);
  Case(DETY): ONREG(Det,Y);
  Case(DETG): ONREG(Det,G);
  Det:
  {
    TaggedRef term = RegAccess(HelpReg,getRegArg(PC+1));
    DEREF(term,termPtr,tag);

    if (!isAnyVar(tag)) {
      DISPATCH(3);
    }
    int argsToSave = getPosIntArg(PC+2);
    INCFPC(3); /* suspend on next instruction! */
    Suspension *susp = e->mkSuspension(CBB,CPP,PC,Y,G,X,argsToSave);
    if (isCVar(tag)) {
      tagged2CVar(term)->addDetSusp(susp);
    } else {
      addSusp(makeTaggedRef(termPtr),susp);
    }
    CHECKSEQ;
  }
#endif

  Case(TAILSENDMSGX): isTailCall = OK; ONREG(SendMethod,X);
  Case(TAILSENDMSGY): isTailCall = OK; ONREG(SendMethod,Y);
  Case(TAILSENDMSGG): isTailCall = OK; ONREG(SendMethod,G);

  Case(SENDMSGX): isTailCall = NO; ONREG(SendMethod,X);
  Case(SENDMSGY): isTailCall = NO; ONREG(SendMethod,Y);
  Case(SENDMSGG): isTailCall = NO; ONREG(SendMethod,G);

 SendMethod:
  {
    TaggedRef label   = getLiteralArg(PC+1);
    TaggedRef origObj = RegAccess(HelpReg,getRegArg(PC+2));
    TaggedRef object  = origObj;
    int arity         = getPosIntArg(PC+3);

    PC = isTailCall ? 0 : PC+4;

    DEREF(object,_1,objectTag);
    if (!isConstChunk(object)) {
      if (isAnyVar(objectTag)) {
        X[0] = makeMethod(arity,label,X);
        X[1] = origObj;
        predArity = 2;
        predicate = chunkCast(e->suspCallHandler);
        goto LBLcall;
      }

      HF_WARN(applFailure(object),
              message("send method application\n");
              printArgs(X+3,arity));
    }

    Abstraction *def = getSendMethod(object,label,arity,X);
    if (def == NULL) {
      goto bombSend;
    }

    CallDoChecks(def,def->getGRegs(),isTailCall,PC,arity+3,OK);
    Y = NULL; // allocateL(0);
    JUMP(def->getPC());


  bombSend:
    X[0] = makeMethod(arity,label,X);
    predArity = 1;
    predicate = chunkCast(object);
    goto LBLcall;
  }


  Case(TAILAPPLMETHX): isTailCall = OK; ONREG(ApplyMethod,X);
  Case(TAILAPPLMETHY): isTailCall = OK; ONREG(ApplyMethod,Y);
  Case(TAILAPPLMETHG): isTailCall = OK; ONREG(ApplyMethod,G);

  Case(APPLMETHX): isTailCall = NO; ONREG(ApplyMethod,X);
  Case(APPLMETHY): isTailCall = NO; ONREG(ApplyMethod,Y);
  Case(APPLMETHG): isTailCall = NO; ONREG(ApplyMethod,G);

 ApplyMethod:
  {
    TaggedRef label        = getLiteralArg(PC+1);
    TaggedRef origObject   = RegAccess(HelpReg,getRegArg(PC+2));
    TaggedRef object       = origObject;
    int arity              = getPosIntArg(PC+3);
    Abstraction *def;

    PC = isTailCall ? 0 : PC+4;

    DEREF(object,objectPtr,objectTag);
    if (!isSRecord(objectTag) ||
        NULL == (def = getApplyMethod(object,label,arity-3+3,X[0]))) {
      goto bombApply;
    }

    CallDoChecks(def,def->getGRegs(),isTailCall,PC,arity,OK);
    Y = NULL; // allocateL(0);
    JUMP(def->getPC());


  bombApply:
    if (methApplHdl == makeTaggedNULL()) {
      HF_WARN(,
              message("Application handler not set (apply method)\n"));
    }

    TaggedRef method = makeMethod(arity-3,label,X);
    X[4] = X[2];   // outState
    X[3] = X[1];   // ooSelf
    X[2] = method;
    X[1] = X[0];   // inState
    X[0] = origObject;

    predArity = 5;
    predicate = chunkCast(methApplHdl);
    goto LBLcall;
  }


  Case(CALLX): isTailCall = NO; ONREG(Call,X);
  Case(CALLY): isTailCall = NO; ONREG(Call,Y);
  Case(CALLG): isTailCall = NO; ONREG(Call,G);

  Case(TAILCALLX): isTailCall = OK; ONREG(Call,X);
  Case(TAILCALLY): isTailCall = OK; ONREG(Call,Y);
  Case(TAILCALLG): isTailCall = OK; ONREG(Call,G);

 Call:
   {
     {
       TaggedRef taggedPredicate = RegAccess(HelpReg,getRegArg(PC+1));
       predArity = getPosIntArg(PC+2);

       PC = isTailCall ? 0 : PC+3;

       DEREF(taggedPredicate,predPtr,predTag);
       if (!isConstChunk(taggedPredicate)) {
         if (isAnyVar(predTag)) {
           X[predArity++] = makeTaggedRef(predPtr);
           predicate = chunkCast(e->suspCallHandler);
           goto LBLcall;
         }
         HF_WARN(applFailure(taggedPredicate),
                 printArgs(X,predArity));
       }

       predicate = chunkCast(taggedPredicate);
     }

// -----------------------------------------------------------------------
// --- Call: entry point
// -----------------------------------------------------------------------

  LBLcall:
     Builtin *bi;

// -----------------------------------------------------------------------
// --- Call: Abstraction
// -----------------------------------------------------------------------

    TypeOfChunk typ = predicate->getCType();

    switch (typ) {
    case C_ABSTRACTION:
    case C_OBJECT:
      {
        Abstraction *def = (Abstraction *) predicate;

        CheckArity(predArity, def->getArity(), def, PC);
        CallDoChecks(def,def->getGRegs(),isTailCall,PC,def->getArity(),OK);
        Y = NULL; // allocateL(0);

        JUMP(def->getPC());
      }


// -----------------------------------------------------------------------
// --- Call: Builtin
// -----------------------------------------------------------------------
    case C_BUILTIN:
      {
        bi = (Builtin *) predicate;

        CheckArity(predArity, bi->getArity(),bi,PC);

        switch (bi->getType()) {

        case BIsolve:
          {
            isSolveGuided = NO;
            isEatWaits    = NO;
            goto LBLBIsolve;
          }
        case BIsolveEatWait:
          {
            isSolveGuided = NO;
            isEatWaits    = OK;
            goto LBLBIsolve;
          }
        case BIsolveGuided:
          {
            isSolveGuided = OK;
            isEatWaits    = OK;
            goto LBLBIsolve;
          }
        case BIsolveCont:    goto LBLBIsolveCont;
        case BIsolved:       goto LBLBIsolved;

        case BIDefault:
          {
            LOCAL_PROPAGATION(Assert(localPropStore.isEmpty()));

            if (e->isSetSFlag(DebugMode)) {
              enterCall(CBB,bi,predArity,X);
            }

            OZ_Bool res = bi->getFun()(predArity, X);
            if (e->isSetSFlag(DebugMode)) {
              exitBuiltin(res,bi,predArity,X);
            }

            killPropagatedCurrentTaskSusp();

            switch (res) {

            case SUSPEND:
              LOCAL_PROPAGATION(Assert(localPropStore.isEmpty()));

              predicate = bi->getSuspHandler();
              if (!predicate) {
                if (!isTailCall) e->pushTask(CBB,PC,Y,G);
                Suspension *susp=e->mkSuspension(CBB,CPP,
                                                 bi->getFun(),X,predArity);
                e->suspendOnVarList(susp);
                CHECKSEQ;
              }
              goto LBLcall;
            case FAILED:
              LOCAL_PROPAGATION(Assert(localPropStore.isEmpty()));
              LOCAL_PROPAGATION(localPropStore.reset());
            localHack0:
              HF_FAIL(applFailure(bi), printArgs(X,predArity));
            case PROCEED:
              LOCAL_PROPAGATION(if (! localPropStore.do_propagation())
                                   goto localHack0;);
              if (emulateHook0(e)) {
                if (!isTailCall) {
                  e->pushTask(CBB,PC,Y,G);
                }
                goto LBLschedule;
              }
              if (isTailCall) {
                goto LBLcheckEntailment;
              }
              JUMP(PC);
            default:
              error("builtin: bad return value");
              goto LBLerror;
            }
          }
        default:
          break;
        }
        error("emulate: call: builtin %s not correctly specified",
              bi->getPrintName());
        goto LBLerror;
      } // end builtin
    default:
      HF_WARN(applFailure(makeTaggedConst(predicate)),
                      );
    } // end switch on type of predicate

// ------------------------------------------------------------------------
// --- Call: Builtin: solve
// ------------------------------------------------------------------------

   LBLBIsolve:
     {
       TaggedRef x0 = X[0];
       DEREF (x0, _0, x0Tag);

       if (isAnyVar (x0Tag) == OK) {
         predicate = bi->getSuspHandler();
         if (!predicate) {
           HF_WARN(applFailure(bi),
                   message("No suspension handler\n"));
         }
         goto LBLcall;
       }

       if (!isConstChunk(x0) ||
           !(chunkCast(x0)->getCType () == C_ABSTRACTION ||
             chunkCast(x0)->getCType () == C_BUILTIN)) {
         HF_FAIL (,
                  message("Application failed: no abstraction or builtin in solve combinator\n"));

       }

       TaggedRef output, guidance;

       if (isSolveGuided) {
         // This is a guided solver, check whether the input list is ground
         TaggedRef guide = deref(X[1]);

         while (isCons(guide) && isInt(headDeref(guide))) {
           guide = tailDeref(guide);
         }

         if (!isLiteral(guide)) {
           predicate = bi->getSuspHandler();
           if (!predicate) {
             HF_WARN(applFailure(bi),
                     message("No suspension handler\n"));
           }
           goto LBLcall;
         }

         output   = X[2];
         guidance = X[1];
       } else {
         output   = X[1];
         guidance = 0;
       }


       // put continuation if any;
       if (isTailCall == NO)
         e->pushTaskOutline(CBB, PC, Y, G);

       // create solve actor(x1);
       // Note: don't perform any derefencing on X[1];
       SolveActor *sa = new SolveActor (CBB,CPP,
                                        output, guidance);

       if (isSolveGuided)
         sa->setGuided();

       if (isEatWaits)
         sa->setEatWaits();

       e->setCurrent(new Board(sa, Bo_Solve), OK);
       CBB->setInstalled();
       e->trail.pushMark();
       sa->setSolveBoard(CBB);

       // put ~'solve actor';
       // Note that CBB is already the 'solve' board;
       e->pushCFun(CBB, &AM::SolveActorWaker);    // no args;

       // apply the predicate;
       predArity = 1;
       predicate = (Abstraction *) tagValueOf (x0);
       X[0] = makeTaggedRef (sa->getSolveVarRef ());
       isTailCall = OK;
       goto LBLcall;   // spare a task - call the predicate directly;
     }

// ------------------------------------------------------------------------
// --- Call: Builtin: solveCont
// ------------------------------------------------------------------------

   LBLBIsolveCont:
     {
       if (((OneCallBuiltin *)bi)->isSeen () == OK) {
         HF_FAIL(,
                 message("once-only abstraction applied more than once\n"));
       }

       Board *solveBB =
         (Board *) tagValueOf ((((OneCallBuiltin *) bi)->getGRegs ())[0]);
       // VERBMSG("solve continuation",((void *) bi),((void *) solveBB));
       // kost@ 22.12.94: 'hasSeen' has new implementation now;
       ((OneCallBuiltin *) bi)->hasSeen ();
       DebugCheck ((solveBB->isSolve () == NO),
                   error ("no 'solve' blackboard  in solve continuation builtin"));
       DebugCheck((solveBB->isCommitted () == OK ||
                   solveBB->isFailed () == OK),
                  error ("Solve board in solve continuation builtin is gone"));
       SolveActor *solveAA = SolveActor::Cast (solveBB->getActor ());

       // link and commit the board (actor will be committed too);
       solveBB->setCommitted (CBB);
       CBB->incSuspCount (solveBB->getSuspCount ()); // similar to the 'unit commit'
       DebugCheck (((solveBB->getScriptRef ()).getSize () != 0),
                   error ("non-empty script in solve blackboard"));

       // adjoin the list of or-actors to the list in actual solve actor!!!
       Board *currentSolveBB = e->currentSolveBoard;
       if (currentSolveBB == (Board *) NULL) {
         DebugCheckT (message ("solveCont is applied not inside of a search problem?\n"));
       } else {
         SolveActor::Cast (currentSolveBB->getActor ())->pushWaitActorsStackOf (solveAA);
       }

       // install (i.e. perform 'unit commit') 'board-to-install' if any;
       Board *boardToInstall = solveAA->getBoardToInstall ();
       if (boardToInstall != (Board *) NULL) {
         DebugCheck ((boardToInstall->isCommitted () == OK ||
                      boardToInstall->isFailed () == OK),
                     error ("boardToInstall is already committed"));
         boardToInstall->setCommitted (CBB);
#ifdef DEBUG_CHECK
         if ( !e->installScript (boardToInstall->getScriptRef ()) ) {
           LOCAL_PROPAGATION(HF_FAIL(,));
           // error ("installScript has failed in solveCont");
           message("installScript has failed in solveCont (0x%x to 0x%x)\n",
                    (void *) boardToInstall, (void *) solveBB);
         }
#else

         LOCAL_PROPAGATION(
           if (!e->installScript (boardToInstall->getScriptRef ())) {
             HF_NOMSG;
           }
         )

         NO_LOCAL_PROPAGATION(
           (void) e->installScript (boardToInstall->getScriptRef ());
         )
#endif
         // add the suspensions of the committed board and remove
         // its suspension itself;
         CBB->incSuspCount (boardToInstall->getSuspCount () - 1);
         // get continuation of 'board-to-install' if any;
         if (boardToInstall->isWaitTop () == NO) {
           if (isTailCall == NO)
             e->pushTaskOutline(CBB,PC,Y,G);
           else
             isTailCall = NO;
           LOADCONT(boardToInstall->getBodyPtr ());
         }
       }
       // NB:
       //    Every 'copyTree' call should compress a path in a computation space,
       //    and not only for copy of, but for an original subtree too.
       //    Otherwise we would get a long chain of already-commited blackboards
       //    of former solve-actors;
       // NB2:
       //    CBB can not become reducible after the applying of solveCont,
       //    since its childCount can not become smaller.
       if ( !e->fastUnifyOutline(solveAA->getSolveVar(), X[0], OK) ) {
         HF_NOMSG;
       }

       if (isTailCall) {
         goto LBLcheckEntailment;
       }
       goto LBLemulate;
     }

// ------------------------------------------------------------------------
// --- Call: Builtin: solved
// ------------------------------------------------------------------------

   LBLBIsolved:
     {
       TaggedRef valueIn = (((SolvedBuiltin *) bi)->getGRegs ())[0];
       Assert(!isRef(valueIn));

       if (isConst(valueIn) && tagged2Const(valueIn)->getType() == Co_Board) {
         Board *solveBB =
           (Board *) tagValueOf ((((SolvedBuiltin *) bi)->getGRegs ())[0]);
         // VERBMSG("solved",((void *) bi),((void *) solveBB));
         Assert(solveBB->isSolve());
         DebugCheck((solveBB->isCommitted () == OK ||
                     solveBB->isFailed () == OK),
                    error ("Solve board in solve continuation builtin is gone"));

         SolveActor::Cast (solveBB->getActor ())->setBoard (CBB);

         Bool isGround;
         Board *newSolveBB = (Board *) e->copyTree (solveBB, &isGround);
         SolveActor *solveAA = SolveActor::Cast (newSolveBB->getActor ());

         if (isGround == OK) {
           TaggedRef solveTRef = solveAA->getSolveVar ();  // copy of;
           DEREF(solveTRef, _0, _1);
           (((SolvedBuiltin *) bi)->getGRegs ())[0] = solveTRef;
           goto LBLBIsolved;
         }

         // link and commit the board;
         newSolveBB->setCommitted (CBB);
         CBB->incSuspCount (newSolveBB->getSuspCount ());
         // similar to the 'unit commit'
         DebugCheck (((newSolveBB->getScriptRef ()).getSize () != 0),
                     error ("non-empty script in solve blackboard"));

         // adjoin the list of or-actors to the list in actual solve actor!!!
         Board *currentSolveBB = e->currentSolveBoard;
         if (currentSolveBB == (Board *) NULL) {
           DebugCheckT(message("solved is applied not inside of a search problem?\n"));
         } else {
           SolveActor::Cast(currentSolveBB->getActor())->pushWaitActorsStackOf(solveAA);
         }

         if ( !e->fastUnifyOutline(solveAA->getSolveVar(), X[0], OK) ) {
           HF_NOMSG;
         }
       } else {
         if ( !e->fastUnifyOutline(valueIn, X[0], OK) ) {
           HF_NOMSG;
         }
       }

       if (isTailCall) {
         goto LBLcheckEntailment;
       }
       JUMP(PC);
     }

   }

// --------------------------------------------------------------------------
// --- end call/execute -----------------------------------------------------
// --------------------------------------------------------------------------


  Case(WAIT):
    {
      Assert(CBB->isWait() && !CBB->isCommitted());

      /* unit commit */
      WaitActor *aa = WaitActor::Cast(CBB->getActor());
      if (aa->hasOneChild()) {
        Board *waitBoard = CBB;
        e->reduceTrailOnUnitCommit();
        waitBoard->unsetInstalled();
        e->setCurrent(aa->getBoardFast());

        waitBoard->setCommitted(CBB);   // by kost@ 4.10.94
        Bool ret = e->installScript(waitBoard->getScriptRef());
        if (!ret) {
          HF_NOMSG;
        }
        Assert(ret!=NO);
        CBB->incSuspCount(waitBoard->getSuspCount()-1);
        DISPATCH(1);
      }

      // not commitable, suspend
      CBB->setWaiting();
      goto LBLsuspendBoard;
    }


  Case(WAITTOP):
    {
      /* top commit */
      if ( e->entailment() )

      LBLtopCommit:
       {
         e->trail.popMark();

         tmpBB = CBB;

         e->setCurrent(CBB->getParentFast());
         tmpBB->unsetInstalled();
         tmpBB->setCommitted(CBB);
#ifndef NEWCOUNTER
         CBB->decSuspCount();
#endif

         goto LBLcheckEntailment;
       }

      /* unit commit for WAITTOP */
      WaitActor *aa = WaitActor::Cast(CBB->getActor());
      Board *bb = CBB;
      if (aa->hasOneChild()) {
        e->reduceTrailOnUnitCommit();
        bb->unsetInstalled();
        e->setCurrent(aa->getBoardFast());

        bb->setCommitted(CBB);    // by kost@ 4.10.94
        Bool ret = e->installScript(bb->getScriptRef());
        if (!ret) {
          HF_NOMSG;
        }
        Assert(ret != NO);
        CBB->incSuspCount(bb->getSuspCount());
        CBB->decSuspCount();
        goto LBLcheckEntailment;
      }

      /* suspend WAITTOP */
      CBB->setWaitTop();
      CBB->setWaiting();
      goto LBLsuspendBoardWaitTop;
    }

  Case(ASK):
    {
      // entailment ?
      if (e->entailment()) {
        e->trail.popMark();
        tmpBB = CBB;
        e->setCurrent(CBB->getParentFast());
        tmpBB->unsetInstalled();
        tmpBB->setCommitted(CBB);
        CBB->decSuspCount();
        DISPATCH(1);
      }

      if (ozconf.showSuspension) {
        printSuspension(PC);
      }

    LBLsuspendBoard:

      CBB->setBody(PC+1, Y, G,NULL,0);

    LBLsuspendBoardWaitTop:
      markDirtyRefsArray(Y);
      DebugTrace(trace("suspend clause",CBB,CAA));

      CAA = AWActor::Cast (CBB->getActor());

      if (CAA->hasNext()) {

        e->deinstallCurrent();

      LBLexecuteNext:
        DebugTrace(trace("next clause",CBB,CAA));

        LOADCONT(CAA->getNext());

        goto LBLemulate; // no thread switch allowed here (CAA)
      }

      // suspend a actor
      DebugTrace(trace("suspend actor",CBB,CAA));

      goto LBLpopTask;
    }


// -------------------------------------------------------------------------
// CLASS: NODES: CREATE/END
// -------------------------------------------------------------------------

  Case(CREATECOND):
    {
      ProgramCounter elsePC = getLabelArg(PC+1);
      int argsToSave = getPosIntArg(PC+2);

      CAA = new AskActor(CBB,CPP,
                         elsePC ? elsePC : NOCODE,
                         NOCODE, Y, G, X, argsToSave);
      DISPATCH(3);
    }

  Case(CREATEOR):
    {
      ProgramCounter elsePC = getLabelArg (PC+1);
      int argsToSave = getPosIntArg (PC+2);
      CAA = new WaitActor(CBB,CPP,NOCODE,Y,G,X,argsToSave);
      DISPATCH(3);
    }

  Case(CREATEENUMOR):
    {
      ProgramCounter elsePC = getLabelArg (PC+1);
      int argsToSave = getPosIntArg (PC+2);

      CAA = new WaitActor(CBB,CPP,
                          NOCODE, Y, G, X, argsToSave);
      if (e->currentSolveBoard != (Board *) NULL) {
        SolveActor *sa= SolveActor::Cast (e->currentSolveBoard->getActor ());
        sa->pushWaitActor (WaitActor::Cast (CAA));
      }
      DISPATCH(3);
    }

  Case(WAITCLAUSE):
    {
      // create a node
      e->setCurrent(new Board(CAA,Bo_Wait),OK);
      CBB->setInstalled();
      e->trail.pushMark();
      DebugCheckT(CAA=NULL);
      IncfProfCounter(waitCounter,sizeof(Board));
      DISPATCH(1);
    }

  Case(ASKCLAUSE):
    {
      e->setCurrent(new Board(CAA,Bo_Ask),OK);
      CBB->setInstalled();
      e->trail.pushMark();
      DebugCheckT(CAA=NULL);
      IncfProfCounter(askCounter,sizeof(Board));
      DISPATCH(1);
    }


  Case(ELSECLAUSE):
    DISPATCH(1);

  Case(THREAD):
    {
      markDirtyRefsArray(Y);
      ProgramCounter newPC = PC+2;
      ProgramCounter contPC = getLabelArg(PC+1);

      int prio = CPP;
      int defPrio = ozconf.defaultPriority;
      if (prio > defPrio) {
        prio = defPrio;
      }

      Thread *tt = e->newThread(prio,CBB);
      if (e->currentSolveBoard != (Board *) NULL) {
        e->incSolveThreads (e->currentSolveBoard);
        tt->setNotificationBoard (e->currentSolveBoard);
      }
      IncfProfCounter(procCounter,sizeof(Thread));
      tt->pushCont(CBB,newPC,Y,G,NULL,0,OK);
      e->scheduleThread(tt);
      JUMP(contPC);
    }


  Case(NEXTCLAUSE):
      CAA->nextClause(getLabelArg(PC+1));
      DISPATCH(2);

  Case(LASTCLAUSE):
      CAA->lastClause();
      DISPATCH(1);

// -------------------------------------------------------------------------
// CLASS: MISC: ERROR/NOOP/default
// -------------------------------------------------------------------------

  Case(ERROR):
      error("Emulate: ERROR command executed");
      goto LBLerror;


  Case(DEBUGINFO):
    {
      TaggedRef filename = getLiteralArg(PC+1);
      int line           = smallIntValue(getNumberArg(PC+2));
      int absPos         = smallIntValue(getNumberArg(PC+3));
      int comment        = getLiteralArg(PC+4);
      int noArgs         = smallIntValue(getNumberArg(PC+5));
      printf("%s in line %d in file: %s\n",
             OZ_toC(comment),line,OZ_toC(filename));
      if (noArgs != -1) {
        printf("\t");
        for (int i=0; i<noArgs; i++) {
          printf("%s ",OZ_toC(X[i]));
        }
      }
      printf("\n");
      DISPATCH(6);
    }

  Case(TESTLABEL1):
  Case(TESTLABEL2):
  Case(TESTLABEL3):
  Case(TESTLABEL4):

  Case(TEST1):
  Case(TEST2):
  Case(TEST3):
  Case(TEST4):

  Case(INLINEDOT):

  Case(ENDOFFILE):
  Case(ENDDEFINITION):

  Case(SWITCHCOMPMODE):
    e->currentThread->switchCompMode();
    DISPATCH(1);

#ifndef THREADED
  default:
    warning("emulate instruction: default should never happen");
    break;
  } /* switch*/
#endif


// ----------------- end emulate ------------------------------------------


// ------------------------------------------------------------------------
// *** REDUCE Board
// ------------------------------------------------------------------------

 LBLcheckEntailment:
  DebugTrace(trace("reduce board",CBB));

  /* optimize: builtin called on toplevel mm (29.8.94) */
  if (CBB == e->rootBoard) {
    goto LBLpopTask;
  }

  CBB->unsetNervous();

  if (CBB->isAsk()) {
    if ( e->entailment() ) {
      LOADCONT(CBB->getBodyPtr());

      e->trail.popMark();

      tmpBB = CBB;

      e->setCurrent(CBB->getParentFast());
      tmpBB->unsetInstalled();
      tmpBB->setCommitted(CBB);

      CBB->decSuspCount();

      goto LBLemulateHook;
    }
  } else if (CBB->isWait ()) {
// WAITTTOP
    if (CBB->isWaitTop()) {

// WAITTOP: top commit
      if ( e->entailment() ) {
        goto LBLtopCommit;
      }

      DebugCheck(WaitActor::Cast(CBB->getActor())->hasOneChild(),
                 error("reduce: waittop: can not happen"));

// WAITTOP: no rule
      goto LBLpopTask;
    }

    DebugCheck((CBB->isWaiting () == OK &&
                WaitActor::Cast(CBB->getActor())->hasOneChild()),
               error("reduce: wait: unit commit can not happen"));
    goto LBLpopTask;

// WAIT: no rule
  }  else if (CBB->isSolve ()) {
    // try to reduce a solve board;
    DebugCheck ((CBB->isReflected () == OK),
                error ("trying to reduce an already reflected solve actor"));
    if (e->isStableSolve(SolveActor::Cast(CBB->getActor()))) {
      DebugCheck ((e->trail.isEmptyChunk () == NO),
                  error ("non-empty trail chunk for solve board"));
      // all possible reduction steps require this;

      SolveActor *solveAA = SolveActor::Cast (CBB->getActor ());
      Board *solveBB = CBB;

      if (solveBB->hasSuspension () == NO) {
        // 'solved';
        // don't unlink the subtree from the computation tree;
        e->trail.popMark ();
        CBB->unsetInstalled ();
        e->setCurrent (CBB->getParentFast());
        CBB->decSuspCount ();

        DebugCheckT (solveBB->setReflected ());
        // statistic
        ozstat.incSolveSolved();
        if ( !e->fastUnifyOutline(solveAA->getResult(), solveAA->genSolved(), OK) ) {
          HF_NOMSG;
        }
      } else {
        // 'stabe' (stuck) or enumeration;
        WaitActor *wa = solveAA->getDisWaitActor ();

        if (wa == (WaitActor *) NULL) {
          // "stuck" (stable without distributing waitActors);
          // don't unlink the subtree from the computation tree;
          e->trail.popMark ();
          CBB->unsetInstalled ();
          e->setCurrent (CBB->getParentFast());
          CBB->decSuspCount ();

          DebugCheckT (solveBB->setReflected ());
          if ( !e->fastUnifyOutline(solveAA->getResult(), solveAA->genStuck(), OK) ) {
            HF_NOMSG;
          }
        } else {
          // to enumerate;
          DebugCheck ((wa->hasOneChild () == OK),
                      error ("wait actor for enumeration with single clause?"));
          DebugCheck (((WaitActor::Cast (wa))->hasNext () == OK),
                      error ("wait actor for distribution has a continuation"));
          DebugCheck ((solveBB->hasSuspension () == NO),
                      error ("solve board by the enumertaion without suspensions?"));

          Bool stableWait = (wa->getChildCount() == 2 &&
                             (wa->getChildRefAt(1))->isFailureInBody() == OK);

          if (stableWait == OK && solveAA->isEatWaits()) {
            Board *waitBoard = wa->getChildRef();

            waitBoard->setCommitted(solveBB);
            if (!e->installScript(waitBoard->getScriptRef())) {
              HF_FAIL(,
                      message("commit of wait disjunction failed\n"));
            }

            solveBB->incSuspCount(waitBoard->getSuspCount()-1);

            // Make the actor unstable by incremneting the thread counter
            solveAA->incThreads();

            // put ~'solve actor';
            e->pushCFun(solveBB, &AM::SolveActorWaker);    // no args;

            if (waitBoard->isWaitTop()) {
              goto LBLcheckEntailment;
            }

            LOADCONT(waitBoard->getBodyPtr());
            Assert(PC != NOCODE);

            goto LBLemulate;

          }

          if (solveAA->isGuided()) {

            TaggedRef guide = deref(solveAA->getGuidance());

            if (isCons(guide)) {
              // it is asserted that each element is an int (see above)
              int clauseNo = smallIntValue(headDeref(guide)) - 1;

              solveAA->setGuidance(tail(guide));

              if ((clauseNo >= 0) && (clauseNo < wa->getChildCount())) {

                Board *waitBoard = wa->getChildRefAt(clauseNo);

                waitBoard->setCommitted(solveBB);

                if (!e->installScript(waitBoard->getScriptRef())) {
                  HF_FAIL(,
                          message("commit of wait disjunction failed\n"));
                }

                solveBB->incSuspCount(waitBoard->getSuspCount()-1);

                // Make the actor unstable by incremneting the thread counter
                solveAA->incThreads();

                // put ~'solve actor';
                e->pushCFun(solveBB, &AM::SolveActorWaker);    // no args;

                if (waitBoard->isWaitTop()) {
                  goto LBLcheckEntailment;
                }

                LOADCONT(waitBoard->getBodyPtr());
                Assert(PC != NOCODE);

                goto LBLemulate;

              } else {
                e->trail.popMark ();
                CBB->unsetInstalled ();
                e->setCurrent (CBB->getParentFast());
                CBB->decSuspCount ();

                if (!e->fastUnifyOutline(solveAA->getResult(),
                                         solveAA->genFailed(),
                                         OK)) {
                  HF_NOMSG;
                }
              }

            } else {
              // put back wait actor
              solveAA->pushWaitActor(wa);
              // give back number of clauses
              e->trail.popMark ();
              CBB->unsetInstalled ();
              e->setCurrent (CBB->getParentFast());
              CBB->decSuspCount ();

              DebugCheckT (solveBB->setReflected ());
              if (!e->fastUnifyOutline(solveAA->getResult(),
                                       solveAA->genChoice(wa->getChildCount()),
                                       OK)) {
                HF_NOMSG;
              }

            }
          } else {

            Board *waitBoard = wa->getChild();
            wa->decChilds();

            if (stableWait) {
              (void) wa->getChild ();  // remove the last child;
              wa->decChilds ();
              DebugCheck((wa->hasNoChilds () == NO),
                         error ("error in the '... [] true then false ro' case"));
              e->trail.popMark ();
              CBB->unsetInstalled ();
              e->setCurrent (CBB->getParentFast());
              CBB->decSuspCount ();

              waitBoard->setActor (wa);
              ((AWActor *) wa)->addChild (waitBoard);
              solveAA->setBoardToInstall (waitBoard);
              DebugCheckT (solveBB->setReflected ());
              // statistics
              ozstat.incSolveDistributed();
              if ( !e->fastUnifyOutline(solveAA->getResult(), solveAA->genEnumedFail() ,OK)) {
                HF_NOMSG;
              }

            } else {
              // 'proper' enumeration;
              e->trail.popMark ();
              CBB->unsetInstalled ();
              e->setCurrent (CBB->getParentFast());
              CBB->decSuspCount ();

              WaitActor *nwa = new WaitActor (wa);
              solveBB->decSuspCount ();   // since WaitActor::WaitActor adds one;
              waitBoard->setActor (nwa);
              ((AWActor *) nwa)->addChild (waitBoard);
              wa->unsetBoard ();  // may not copy the actor and rest of boards too;
              solveAA->setBoardToInstall (waitBoard);

              //  Now, the following state has been reached:
              // The waitActor with the 'rest' of actors is unlinked from the
              // computation space; instead, a new waitActor (*nwa) is linked to it,
              // and all the tree from solve blackboard (*solveBB) will be now copied.
              // Moreover, the copy has already the 'boardToInstall' setted properly;
              Board *newSolveBB = e->copyTree (solveBB, (Bool *) NULL);

              // ... and now set the original waitActor backward;
              waitBoard->flags |= Bo_Failed;   // this subtree is discarded;
              wa->setBoard (solveBB);          // original waitActor;
              // the subtrees (new and old ones) are still linked to the
              // computation tree;
              if (wa->hasOneChild () == OK) {
                solveAA->setBoardToInstall (wa->getChild ());
              } else {
                solveAA->setBoardToInstall ((Board *) NULL);
                // ... since we have set previously board-to-install for copy;
                solveAA->pushWaitActor (wa);
                //  If this waitActor has yet more than one clause, it can be
                // distributed again ... Moreover, it must be considered first.
              }
              DebugCheckT (solveBB->setReflected ());
              DebugCheckT (newSolveBB->setReflected ());
              // ... and now there are two proper branches of search problem;

              // statistics
              ozstat.incSolveDistributed();
              if ( !e->fastUnifyOutline(solveAA->getResult(),
                                        solveAA->genEnumed(newSolveBB),
                                        OK)) {
                HF_NOMSG;
              }
            }
          }
        }
      }
    }
  } else {
    Assert(CBB->isRoot());
  }
  goto LBLpopTask;

// ----------------- end reduce -------------------------------------------

// ------------------------------------------------------------------------
// *** FAILURE
// ------------------------------------------------------------------------
 LBLshallowFail:
  {
    e->reduceTrailOnFail();
    ProgramCounter nxt = getLabelArg(shallowCP+1);
    inShallowGuard = NO;
    shallowCP = NULL;
    JUMP(nxt);
  }

 LBLfailure:
  {
    DebugTrace(trace("fail",CBB));
    Assert(CBB->isInstalled());
    Actor *aa=CBB->getActor();
    if (aa->isAskWait()) {
      (AWActor::Cast(aa))->failChild(CBB);
    }
    CBB->setFailed();
    e->reduceTrailOnFail();
    CBB->unsetInstalled();
    e->setCurrent(aa->getBoardFast());

// ------------------------------------------------------------------------
// *** REDUCE Actor
// ------------------------------------------------------------------------

    DebugTrace(trace("reduce actor",CBB,aa));

    if (aa->isAsk()) {
      if ((AskActor::Cast (aa))->hasNext () == OK) {
        CAA = AskActor::Cast (aa);
        goto LBLexecuteNext;
      }
/* check if else clause must be activated */
      if ( (AskActor::Cast (aa))->isLeaf() ) {

/* rule: if else ... fi
   push the else cont on parent && remove actor */
        aa->setCommitted();
        LOADCONT((AskActor::Cast (aa))->getNext());
        PC = AskActor::Cast(aa)->getElsePC();
        if (PC != NOCODE) {
          CBB->decSuspCount();
          goto LBLemulateHook;
        }

/* rule: if fi --> false */
        HF_FAIL(,message("reducing 'if fi' to 'false'\n"));
      }
    } else if (aa->isWait ()) {
      if ((WaitActor::Cast (aa))->hasNext () == OK) {
        CAA = WaitActor::Cast (aa);
        goto LBLexecuteNext;
      }
/* rule: or ro (bottom commit) */
      if ((WaitActor::Cast (aa))->hasNoChilds()) {
        aa->setCommitted();
        HF_FAIL(,
                message("bottom commit\n"));
      }
/* rule: or <sigma> ro (unit commit rule) */
      if ((WaitActor::Cast (aa))->hasOneChild()) {
        Board *waitBoard = (WaitActor::Cast (aa))->getChild();
        DebugTrace(trace("reduce actor unit commit",waitBoard,aa));
        if (waitBoard->isWaiting()) {
          waitBoard->setCommitted(CBB); // do this first !!!
          if (!e->installScript(waitBoard->getScriptRef())) {
            HF_FAIL(,
                    message("unit commit failed\n"));
          }

          /* add the suspension from the committed board
             remove the suspension for the board itself */
          CBB->incSuspCount(waitBoard->getSuspCount()-1);

          /* unit commit & WAITTOP */
          if (waitBoard->isWaitTop()) {
            if (!waitBoard->hasSuspension()) {
              goto LBLcheckEntailment;
            }

            /* or guard not completed:  e.g. or task X = 1 end [] false ro */
            goto LBLpopTask;
          }

          /* unit commit & WAIT, e.g. or X = 1 ... then ... [] false ro */
          LOADCONT(waitBoard->getBodyPtr());
          Assert(PC != NOCODE);

          goto LBLemulateHook;
        }
      }
    } else {
      //  Reduce (i.e. with failure in this case) the solve actor;
      //  The solve actor goes simply away, and the 'failed' atom is bound to
      // the result variable;
      aa->setCommitted();
      CBB->decSuspCount();
      // for statistic purposes
      ozstat.incSolveFailed();
      if ( !e->fastUnifyOutline(SolveActor::Cast(aa)->getResult(),
                                SolveActor::Cast(aa)->genFailed(),
                                OK) ) {
        HF_NOMSG;
      }
    }

/* no rule: suspend longer */
    goto LBLpopTask;
  }

// ----------------- end reduce actor --------------------------------------


// ----------------- end failure ------------------------------------------
}

#ifdef OUTLINE
#undef inline
#endif
