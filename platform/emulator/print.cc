/*
  Hydra Project, DFKI Saarbruecken,
  Stuhlsatzenhausweg 3, D-66123 Saarbruecken, Phone (+49) 681 302-5312
  Author: mehl
  Last modified: $Date$ from $Author$
  Version: $Revision$
  State: $State$

  ------------------------------------------------------------------------
*/

#ifdef __GNUC__
#pragma implementation "print.hh"
#endif

#include <ctype.h>

#include "actor.hh"
#include "am.hh"
#include "builtins.hh"
#include "bignum.hh"
#include "board.hh"
#include "cell.hh"
#include "debug.hh"
#include "genvar.hh"
#include "fdomn.hh"
#include "misc.hh"
#include "records.hh"
#include "taskstk.hh"
#include "term.hh"
#include "thread.hh"
#include "objects.hh"


void printWhere(ostream &cout,ProgramCounter PC);


#define PRINT(C) \
     void C::print(ostream &stream, int depth, int offset)

#define PRINTLONG(C) \
     void C::printLong(ostream &stream, int depth, int offset)


/* print tuple & records in one or more lines ??? */
// #define NEWLINE(off) stream << endl << indent((off));
#define NEWLINE(off) stream << " ";

//-----------------------------------------------------------------------------
//                         Miscellaneous stuff

// mm2
// returns OK if associated suspension is alive
inline Bool isEffectiveSusp(SuspList* sl)
{
  Suspension* s = sl->getSusp();
  if (s->isDead())
    return NO;
  if (!s->getBoard()->getBoardDeref())
    return NO;
  return OK;
}

// returns OK if sl contains at least one alive suspension element
inline Bool isEffectiveList(SuspList* sl) {
  for (; sl != NULL; sl = sl->getNext())
    if (isEffectiveSusp(sl) == OK)
      return OK;
  return NO;
}


// ----------------------------------------------------------------
// PRINT
// ----------------------------------------------------------------

static void tagged2Stream(TaggedRef ref,ostream &stream=cout,
                          int depth = 10,int offset = 0) {

  if (ref == makeTaggedNULL()) {
    stream << "*** NULL TERM ***";
    return;
  }

  TaggedRef origRef = ref;
  DEREF(ref,refPtr,tag)
  switch(tag) {
  case UVAR:
    stream << "_"
           << hex << (int) refPtr << dec;
    break;
  case SVAR:
    tagged2SVar(ref)->print(stream,depth,offset,origRef);
    break;
  case CVAR:
    tagged2CVar(ref)->print(stream, depth, offset,origRef);
    break;
  case STUPLE:
    tagged2STuple(ref)->print(stream,depth,offset);
    break;
  case SRECORD:
    tagged2SRecord(ref)->print(stream,depth,offset);
    break;
  case LTUPLE:
    tagged2LTuple(ref)->print(stream,depth,offset);
    break;
  case LITERAL:
    tagged2Literal(ref)->print(stream,depth,offset);
    break;
  case FLOAT:
    tagged2Float(ref)->print(stream,depth,offset);
    break;
  case BIGINT:
  case SMALLINT:
    {
      char *s = OZ_intToCString(ref);
      OZ_normInt(s);
      stream << s;
      OZ_free(s);
    }
    break;
  case CONST:
    tagged2Const(ref)->print(stream,depth,offset);
    break;
  default:
    if (isRef(ref)) {
      stream << "PRINT: REF detected";
    } else {
      stream << "PRINT: unknown tag in term: " << tag;
    }
    break;
  }
}

char *getVarName(TaggedRef v)
{
  TaggedRef n = VariableNamer::getName(v);
  DEREF(n,_1,_2);
  if (!OZ_isAtom(n)) {
    n = AtomVoid;
  }
  return tagged2Literal(n)->getPrintName();
}

void SVariable::print(ostream &stream, int depth, int offset, TaggedRef v)
{
  stream << "SV:"
         << getVarName(v)
         << "@"
         << this
         << (isEffectiveList(suspList) == OK ? "*" : "");
}

void GenCVariable::print(ostream &stream, int depth, int offset, TaggedRef v)
{
  switch(getType()){
  case FDVariable:
    {
      stream << indent(offset)
             << "<CV: "
             << getVarName(v)
             << " @"
             << this;
      if (isEffectiveList(suspList) == OK)
        stream << " a" << suspList->length();

      GenFDVariable * me = (GenFDVariable *) this;
      if (isEffectiveList(me->fdSuspList[fd_det]) == OK)
        stream << " d("
               << me->fdSuspList[fd_det]->length()
               << '/'
               << me->fdSuspList[fd_det]->lengthProp()
               << ')';
      if (isEffectiveList(me->fdSuspList[fd_bounds]) == OK)
        stream << " b(" << me->fdSuspList[fd_bounds]->length()
               << '/'
               << me->fdSuspList[fd_bounds]->lengthProp()
               << ')';
      stream << ' ';
      me->getDom().print(stream, 0);

      stream << ">";
      break;
    }

  case OFSVariable:
    {
      if (depth<=1) {
          stream << "...(...)";
      } else {
          GenOFSVariable* me = (GenOFSVariable *) this;
          tagged2Stream(me->getLabel(),stream,depth-1,offset);
          // me->getLabel()->print(stream, depth-1, offset);
          me->getTable()->print(stream, depth-1, offset+2);
      }
      break;
   }

  default:
    error("Unexpected type generic variable at %s:%d.",
          __FILE__, __LINE__);
    break;
  }
} // PRINT(GenCVariable)


PRINT(DynamicTable)
{
    stream << '(';
    int nonempty=FALSE;
    for (dt_index i=0; i<size; i++) {
        if (table[i].ident) {
            nonempty=TRUE;
            CHECK_DEREF(table[i].ident);
            stream << ' ';
            tagged2Stream(table[i].ident,stream,depth);
            stream << ':';
            stream << ' ';
            tagged2Stream(table[i].value,stream,depth);
        }
    }
    if (nonempty) stream << ' ';
    stream << "...)" ;
}

PRINTLONG(DynamicTable)
{
    print(stream, depth, offset);
}



PRINT(STuple)
{
  int i;

  tagged2Stream(getLabel(),stream,depth,offset);
  if ( depth <= 1 ) {
    stream << "(...)";
  } else {
    stream << "(";
    NEWLINE(offset+2);
    for (i = 0; i < getSize (); i++) {
      tagged2Stream(getArg(i),stream, depth-1,offset+2);
      NEWLINE(offset+2);
    }
    stream << ")";
  }
}

PRINT(SRecord)
{
  switch (getType()) {
  case R_ABSTRACTION:
  case R_OBJECT:
    ((Abstraction *) this)->print(stream,depth,offset);
    break;
  case R_CELL:
    ((Cell *) this)->print(stream,depth,offset);
    break;
  case R_BUILTIN:
    ((Builtin *) this)->print(stream,depth,offset);
    break;
  case R_CHUNK:
  case R_RECORD:
    tagged2Stream(getLabel(),stream,depth,offset);
    break;
  }

  TaggedRef ar = getArityList();
  CHECK_DEREF(ar);
  if (isCons(ar)) {
    if (depth <= 1) {
      stream << "(...)";
    } else {
      stream << "(";
      NEWLINE(offset+2);
      while (isCons(ar)) {
        TaggedRef feat = head(ar);
        CHECK_DEREF(feat);
        tagged2Stream(feat,stream,depth,offset);
        ar = tail(ar);
        CHECK_DEREF(ar);
        stream << ": ";
        tagged2Stream(getFeature(feat),stream,depth-1,offset+2);
        NEWLINE(offset+2);
      }
      stream << ")";
    }
  }
}

PRINT(LTuple)
{
  if ( depth <= 1 ) {
    stream << " ... ";
  } else {
    TaggedRef headd = getHead();
    DEREF(headd,_1,tag1);
    if (isLTuple(tag1) ) {
      stream << "(";
      tagged2Stream(headd,stream, depth-1,offset);
      stream << ")"
             << NameOfCons;
    } else {
      tagged2Stream(getHead(),stream, depth-1,offset);
      stream << NameOfCons;
    }
    tagged2Stream(getTail(),stream, depth-1, offset);
  }
}


static void ppLiteral(ostream &stream, char *s) {
  stream << "'";
  char c;
  while (c = *s) {
    switch (c) {
    case '\'':
      stream << "\\'";
      break;
    case '\n':
      stream << "\\n";
      break;
    case '\r':
      stream << "\\r";
      break;
    case '\\':
      stream << "\\\\";
      break;
    case '\t':
      stream << "\\t";
      break;
    default:
      stream << c;
      break;
    }
    s++;
  }
  stream << "'";
}


#define WELLFORMED

static Bool isWellFormed(char *s)
{
#ifdef WELLFORMED
  if (!islower(*s)) {
    return NO;
  }
  s++;
  while (*s) {
    if (!isalnum(*s) && *s != '_') {
      return NO;
    }
    s++;
  }
#endif
  return OK;
}

PRINT(Literal)
{
  if (!isAtom()) {
    stream << OZ_literalToC(makeTaggedLiteral(this));
    return;
  }

  char *s = getPrintName();
  if (isWellFormed(s)) {
    stream << OZ_atomToC(makeTaggedLiteral(this));
  } else {
    ppLiteral(stream,s);
  }
}

PRINT(Float)
{
  char *s = OZ_floatToCString(makeTaggedFloat(this));
  OZ_normFloat(s);
  stream << s;
  OZ_free(s);
}

PRINT(Cell)
{
  stream << "C:"
         << getPrintName()
         <<"@" << (void*) getId();
}

PRINT(Abstraction)
{
  if (getType() == R_OBJECT) {
    stream << "O:" << ((Object*)this)->getPrintName();
    return;
  }
  stream << "P:"
         << getPrintName() << "/" << getArity();
//       << "@" << (void*) getId();
}

PRINT(Builtin)
{
  stream << "B:"
         << getPrintName() << "/" << getArity();
//       << "@" << (void*) getId();
}


PRINT(BuiltinTabEntry)
{
  stream << "<builtin "
         << getPrintName()
         << "/"
         << arity
         << " @"
         << this
         << ">";
}


PRINT(Arity)
{
  stream << "Arity: ";
  tagged2Stream(list,stream,depth,offset);
  stream << endl;

  for(int i=0; i< size ; i++) {
    stream << "Slot: "
           << i
           << " Entry: ";
    if (keytable[i] == NULL) {
      stream << "<empty>\n";
    } else {
      keytable[i]->print(stream,depth,offset);
      stream << " Value: " << indextable[i] << endl;
    }
  }
  stream << numberofentries
    << " entries, but only "
    << numberofcollisions
    << " collisions.\n";
}


PRINT(ArityTable)
{
  Arity *c;
  for (int i = 0 ; i < size ; i ++) {
    stream << "Position " << i << endl;
    c = table[i];
    while (c != NULL ) {
      c->print(stream,depth,offset);
      c = c->next;
    }
  }
}


// ---------------------------------------------------



PRINT(SuspList)
{
  if (isEffectiveList(this) == NO) {
    stream << indent(offset) << "- empty -" << endl;
    return;
  }

  for (SuspList* sl = this; sl != NULL; sl = sl->getNext()) {
    if (isEffectiveSusp(sl) == NO)
      continue;
    if (sl->isCondSusp())
      stream << indent(offset)
             << ((CondSuspList*)sl)->getCondNum() << " conds";
    else
      stream << indent(offset) << "true";
    sl->getSusp()->print(stream);
    stream << endl;
  } // for
}

PRINT(Suspension)
{
    stream << indent(offset) << " [";
    if (isDead()) stream << 'D';
    if (isPropagated()) stream << 'P';
    if (isResistant()) stream << 'R';
    if (isExtSusp()) stream << 'E';
    if (isSurvSusp()) stream << 'S';
    if (isUnifySusp()) stream << 'U';
    if (isLocalSusp()) stream << 'L';
    stream << "] -> ";

    if (getCont())
      stream << "cont ";
    else if (getCCont()) {
      stream  << "ccont = "
              << builtinTab.getName((void *)getCCont()->getCFunc())
              << '(' << getCCont()->getXSize() << ", "
              << (void *) getCCont()->getX() << "[]), ";
    } else {
      stream << "board ";
    }
    getBoard()->print(stream, 0);
}


// ----------------------------------------------------------------
// PRINT LONG
// ----------------------------------------------------------------


static void tagged2StreamLong(TaggedRef ref,ostream &stream = cout,
                              int depth = 1,int offset = 0)
{
  if (ref == makeTaggedNULL()) {
    stream << "*** NULL TERM ***" << endl;
    return;
  }

  if (isRef(ref)) {
    stream << indent(offset)
           << '@'
           << (void *) tagged2Ref(ref)
           << ": "
           << (void *) *tagged2Ref(ref)
           << endl;
    tagged2StreamLong(*tagged2Ref(ref),stream,depth-1,offset+2);
    return;
  }

  switch(tagTypeOf(ref)) {
  case UVAR:
    {
      stream << indent(offset)
             << "UV @"
             << tagValueOf(ref)
             << endl
             << indent(offset)
             << "HomeNode: ";
      tagged2VarHome(ref)->getBoardDeref()->print(stream,0);
      stream << endl;
    }
    break;

  case SVAR:
    tagged2SVar(ref)->printLong(stream,depth,offset,AtomVoid);
    break;
  case CVAR:
    tagged2CVar(ref)->printLong(stream, depth, offset,AtomVoid);
    break;
  case STUPLE:
    tagged2STuple(ref)->printLong(stream,depth,offset);
    break;
  case SRECORD:
    tagged2SRecord(ref)->printLong(stream,depth,offset);
    break;
  case LTUPLE:
    tagged2LTuple(ref)->printLong(stream,depth,offset);
    break;
  case LITERAL:
    tagged2Literal(ref)->printLong(stream,depth,offset);
    break;
  case FLOAT:
    tagged2Float(ref)->printLong(stream,depth,offset);
    break;
  case BIGINT:
    tagged2BigInt(ref)->printLong(stream,depth,offset);
    break;
  case SMALLINT:
    {
      stream << indent(offset)
             << "SmallInt @"
             << ref
             << ": ";
      tagged2Stream(ref,stream,depth,offset);
      stream << endl;
    }
    break;
  case CONST:
    tagged2Const(ref)->printLong(stream,depth,offset);
    break;
  default:
    if (isRef(ref)) {
      stream << "PRINT: REF detected" << endl;
    } else {
      stream << "PRINT: unknown tag in term: " << tagTypeOf(ref) << endl;
    }
    break;
  }
}

PRINTLONG(ConstTerm)
{
  switch (typeOf()) {
  case Co_Board:
    ((Board *) this)->printLong(stream, depth, offset);
  case Co_Thread:
    ((Thread *) this)->printLong(stream, depth, offset);
  case Co_Actor:
    ((Actor *) this)->printLong(stream, depth, offset);
  default:
    error("ConstTerm::printLong");
  }
}

PRINT(ConstTerm)
{
  switch (typeOf()) {
  case Co_Board:
    ((Board *) this)->print(stream, depth, offset);
    break;
  case Co_Thread:
    ((Thread *) this)->print(stream, depth, offset);
    break;
  case Co_Actor:
    ((Actor *) this)->print(stream, depth, offset);
    break;
  default:
    error("ConstTerm::print");
  }
}

void AM::print()
{
  cout << "class AM" << endl
       << "  currentBoard: ";
  currentBoard->print(cout,0,0);
  cout << endl
       << "  rootBoard:    ";
  rootBoard->print(cout,0,0);
  cout << endl
       << "  currentThread: ";
  currentThread->print(cout,0,0);
  cout << endl
       << "  rootThread:    ";
  rootThread->print(cout,0,0);
  cout << endl;
}

PRINT(Board)
{
  if (!this) {
    stream << indent(offset) << "(NULL Board)";
    return;
  }

  if (isRoot()) {
    stream << "Root";
  } else if (isWait()) {
    stream << "Wait";
  } else if (isAsk()) {
    stream << "Ask";
  } else if (isSolve ()) {
    stream << "Solve";
  }

  stream << "Board @" << this->getBoardDeref() << " [";

  if (isCommitted()) stream << 'C';
  if (isReflected()) stream << 'R';
  if (isInstalled()) stream << 'I';
  if (isNervous())   stream << 'N';
  if (isWaitTop())   stream << 'T';
  if (isPathMark())  stream << 'P';
  if (isFailed())    stream << 'F';
  if (isDiscarded()) stream << 'D';
  if (isWaiting())   stream << 'W';

  stream << " #" << suspCount;
  stream << ']';
}

PRINTLONG(Board)
{
  print(stream,depth,offset);
  stream << endl;
}

void Board::Print()
{
  cout << "class Board" << endl
       << "  currentBoard: ";
  am.currentBoard->print(cout,0,0);
  cout << endl
       << "  rootBoard:    ";
  am.rootBoard->print(cout,0,0);
  cout << endl;
}

PRINT(Actor)
{
  if (!this) {
    stream << indent(offset) << "(NULL Actor)";
    return;
  }

  stream << indent(offset)
    << "Actor @"
    << this;
}

PRINTLONG(Actor)
{
  print(stream,depth,offset);
  stream << endl;
}

PRINT(SolveActor)
{
}

PRINTLONG(SolveActor)
{
  stream  << indent(offset) << "solveVar=";
  tagged2Stream(solveVar,stream,depth-1,0);
  stream << endl;
  stream << indent(offset) << "result=";
  tagged2Stream(result,stream,depth-1,0);
  stream << endl;
  stream << indent(offset) << "threads=" << threads << endl;
  stream << indent(offset) << "SuspList:" << endl;
  suspList->print(stream,depth-1,offset+2);
}

void Thread::Print()
{
  cout << "class Thread" << endl
       << "  currentThread: ";
  am.currentThread->print(cout,0,0);
  cout << endl
       << "  rootThread:    ";
  am.rootThread->print(cout,0,0);
  cout << endl
       << "  Queue:" << endl;
  for (Thread *th=Head; th; th = th->next) {
    th->print(cout,0,4);
    if (th == Head) {
      cout << " HEAD";
    }
    if (th == Tail) {
      cout << " TAIL";
    }
    if (th == am.rootThread) {
      cout << " ROOT";
    }
    if (th == am.currentThread) {
      cout << " CURRENT";
    }
    cout << endl;
  }
}

PRINT(Thread)
{
  if (!this) {
    stream << indent(offset) << "(NULL Thread)";
    return;
  }

  stream << indent(offset)
    << "Thread @" << this
    << " [ prio: " << priority
    << ", ";
  stream << " Normal (#"
         << taskStack.getUsed()-1
         << ")";
  stream << " ]";
}

PRINTLONG(Thread)
{
  this->print(stream,depth,offset);
  stream << endl;
}

PRINTLONG(Literal)
{
  if (isAtom()) {
    stream << indent(offset) << "Atom @" << this << ": ";
  } else {
    stream << indent(offset) << "Name @" << this << ": ";
  }
  this->print(stream,depth,offset);
  stream << endl;
}


void SVariable::printLong(ostream &stream, int depth, int offset, TaggedRef v)
{
  stream << indent(offset)
         << "SV "
         << getVarName(v)
         << " @"
         << this
         << endl
         << indent(offset)
         << "SuspList:\n";
  suspList->print(stream, 0, offset+3);

  stream << indent(offset)
         << "HomeNode: ";
  home->getBoardDeref()->print(stream,0);
  stream << endl;
}

void GenCVariable::printLong(ostream &stream, int depth, int offset, TaggedRef v)
{
  stream << indent(offset)
         << "CV "
         << getVarName(v)
         << " @"
         << this
         << endl
         << indent(offset)
         << "Any SuspList:\n";
  suspList->print(stream, 0, offset+3);

  if (getType() == FDVariable) {
    stream << indent(offset) << "Det SuspList:\n";
    ((GenFDVariable*)this)->fdSuspList[fd_det]->print(stream, 0, offset+3);

    stream << indent(offset) << "Bounds SuspList:\n";
    ((GenFDVariable*)this)->fdSuspList[fd_bounds]->print(stream, 0, offset+3);
  }

  stream << indent(offset) << "HomeNode: ";
  home->getBoardDeref()->print(stream,0);
  stream << endl;

  switch(getType()){
  case FDVariable:
    ((GenFDVariable*)this)->getDom().printLong(stream, offset);
    break;
  case OFSVariable:
    {
      stream << ' ';
      if (depth<=1) {
          stream << "...(...)";
      } else {
          GenOFSVariable* me = (GenOFSVariable *) this;
          tagged2Stream(me->getLabel(),stream,depth-1,offset);
          // me->getLabel()->print(stream, depth-1, offset);
          me->getTable()->print(stream, depth-1, offset+2);
      }
      break;
    }
  default:
    error("Unexpected type generic variable at %s:%d.",
          __FILE__, __LINE__);
    break;
  }

} // PRINTLONG(GenCVariable)


PRINTLONG(STuple)
{
  int i;

  stream << indent(offset) << "Tuple @" << this << endl
         << indent(offset) << "Label: ";
  tagged2StreamLong(label,stream,depth,offset);
  stream << endl;
  if ( depth <= 1 ) {
    stream << indent(offset) <<  "Args: ...";
  } else {
    for (i = 0; i < getSize (); i++) {
      stream << indent(offset) <<  "Arg "<< i << ":\n";
      tagged2StreamLong (getArg(i),stream,depth-1,offset);
      stream << " ";
    }
  }
  stream << endl;
}


PRINTLONG(LTuple)
{
  stream << indent(offset) << "List @" << this << endl;

  if ( depth <= 1 ) {
    stream << indent(offset) << "Args: ...\n";
  } else {
    stream << indent(offset) << "Head:\n";
    tagged2StreamLong(getHead(),stream,depth-1,offset+2);
    stream << indent(offset) << "Tail:\n";
    tagged2StreamLong(getTail(),stream,depth-1,offset+2);
  }
}

PRINTLONG(Abstraction)
{
  stream << indent(offset)
         << (getType() == R_OBJECT ? "Object " : "")
         << "Abstraction @id"
         << getId() << endl;
  pred->printLong(stream,depth,offset);
  int n = gRegs ? getRefsArraySize(gRegs) : 0;
  if (offset == 0) {
    if (n > 0) {
      stream <<  "G Regs:";
      for (int i = 0; i < n; i++) {
        stream << " G[" << i << "]:\n";
        tagged2StreamLong(gRegs[i],stream,depth,offset+2);
      }
    } else {
      stream << "No G-Regs" << endl;
    }
  } else {
    stream << indent(offset) << "G Regs: #" << n << endl;
  }
}

PRINTLONG(PrTabEntry)
{
  stream << indent(offset)
         <<  "Name: " << getPrintName()
         << "/" << arity
         << "/" << gRegs.getSize() << endl
         << indent(offset)
         <<  "ProgramCounter: " << (void *) PC << endl
         << indent(offset) <<  "Arity: " << arity << endl;
}

PRINTLONG(Builtin)
{
  print(stream,depth,offset);
  stream << endl;
}



PRINTLONG(Cell)
{
  stream << indent(offset)
         << "Cell " << getPrintName()
         << " @id"  << getId() << endl
         << indent(offset)
         << " value:"<<endl;
  tagged2StreamLong(val,stream,depth,offset+2);
}

PRINTLONG(SRecord)
{
  stream << indent(offset);
  switch (getType()) {
  case R_ABSTRACTION:
  case R_OBJECT:
    ((Abstraction *) this)->printLong(stream,depth,offset);
    break;
  case R_CELL:
    ((Cell *) this)->printLong(stream,depth,offset);
    break;
  case R_BUILTIN:
    ((Builtin *) this)->printLong(stream,depth,offset);
    break;
  case R_CHUNK:
    stream << "Chunk @" << this << ":\n";
    break;
  case R_RECORD:
    stream << "Record @"
           << this << ":\n"
           << indent(offset)
           << "Label: ";
    tagged2Stream(u.label,stream,depth-1,offset);
    stream << endl;
    break;
  }

  if (depth <= 1) {
    stream << indent(offset) << "Args: ...\n";
  } else {
    stream << indent(offset) << "Args:\n";
    TaggedRef ar = getArityList();
    CHECK_DEREF(ar);
    while (isCons(ar)) {
      stream << indent(offset+2);
      TaggedRef feat = head(ar);
      CHECK_DEREF(feat);
      tagged2Stream(feat,stream,depth-1,offset+2);
      ar = tail(ar);
      CHECK_DEREF(ar);
      stream << ": ";
      tagged2StreamLong(getFeature(feat),stream,depth-1,offset+2);
    }
    stream << indent(offset) << "End of Args\n";
  }
}


PRINTLONG(Float)
{
  stream << indent(offset) << "Float @" << this << ": ";
  print(stream,depth,offset);
  stream << endl;
}

PRINTLONG(BigInt)
{
  stream << indent(offset)
         << "BigInt @" << this << ": ";
  tagged2Stream(makeTaggedBigInt(this),stream,depth,offset);
  stream << endl;
}



PRINT(TaskStack)
{
  int counter=1;
  stream << "*traceback*" << endl;
  if (this && !isEmpty()) {

    TaskStackEntry *p = getTop();

    while (!isEmpty()) {
      TaggedBoard tb = (TaggedBoard) pop();
      ContFlag flag = getContFlag(tb);
      Board* n = getBoard(tb,flag);
      switch (flag){
      case C_CONT:
        {
          ProgramCounter PC = (ProgramCounter) pop();
          RefsArray Y = (RefsArray) pop();
          RefsArray G = (RefsArray) pop();

          stream << "  #" << counter << "  ";
          printWhere(stream,PC);
          stream << endl;
        }
        break;
      case C_XCONT:
        {
          ProgramCounter PC = (ProgramCounter) pop();
          RefsArray Y = (RefsArray) pop();
          RefsArray G = (RefsArray) pop();
          RefsArray X = (RefsArray) pop();

          stream << "  #" << counter << "  ";
          printWhere(stream,PC);
          stream << endl;
        }
        break;
      case C_NERVOUS:
        {
          stream << "NERVOUS ";
          stream << endl;
        }
        break;
      case C_CFUNC_CONT:
        {
          OZ_CFun biFun = (OZ_CFun) pop();
          Suspension* susp = (Suspension*) pop();
          RefsArray X = (RefsArray) pop();

          stream << "  CFUNC_CONT ";
          stream << endl;
        }
        break;

      case C_DEBUG_CONT:
        {
          OzDebug *deb = (OzDebug*) pop();

          stream << "  DEBUG_CONT ";
          stream << endl;
        }
        break;
      case C_CALL_CONT:
        {
          SRecord *s = (SRecord *) pop();
          RefsArray X = (RefsArray) pop();

          stream << "  CALL_CONT ";
          stream << endl;
        }
        break;

      default:
        error("unexpected task found.");
      }
      counter++;
    }

    setTop(p);
  }
  stream << "*bottom*" << endl;
}

PRINTLONG(TaskStack)
{
  if (isEmpty() == OK) {
    stream << "*** TaskStack is empty ***" << endl;
    return;
  }
  stream << "TaskStack" << endl;

  TaskStackEntry *p = getTop();

  while (!isEmpty() && depth-- > 0) {
    TaggedBoard tb = (TaggedBoard) pop();
    ContFlag flag = getContFlag(tb);
    Board* n = getBoard(tb,flag);
    switch (flag){
    case C_CONT:
      {
        ProgramCounter PC = (ProgramCounter) pop();
        RefsArray Y = (RefsArray) pop();
        RefsArray G = (RefsArray) pop();
        stream << "CONT\n";
        n->print(stream,depth,offset); stream << endl;
        CodeArea::display(CodeArea::definitionStart(PC),1,stdout);
      }
      break;
    case C_XCONT:
      {
        ProgramCounter PC = (ProgramCounter) pop();
        RefsArray Y = (RefsArray) pop();
        RefsArray G = (RefsArray) pop();
        RefsArray X = (RefsArray) pop();
        stream << "XCONT\n";
        n->print(stream,depth,offset); stream << endl;
        CodeArea::display(CodeArea::definitionStart(PC),1,stdout);
      }
      break;
    case C_NERVOUS:
      stream << "WAKEUP\n";
      n->print(stream,depth,offset); stream << endl;
      break;
    case C_CFUNC_CONT:
      {
        OZ_CFun biFun = (OZ_CFun) pop();
        Suspension* susp = (Suspension*) pop();
        RefsArray X = (RefsArray) pop();
        stream << "CFUNC\n";
      }
      break;

    case C_DEBUG_CONT:
      {
        OzDebug *deb = (OzDebug*) pop();
        stream << "DEBUG\n";
        deb->printCall();
      }
      break;
    case C_CALL_CONT:
      {
        SRecord *s = (SRecord *) pop();
        RefsArray X = (RefsArray) pop();
        stream << "CCALL";
      }
      break;

    default:
      error("TaskStack::printLong: unexpected task found.");
    } // switch
  } // while

  setTop(p);
  stream << "End of TaskStack\n";
}


#ifdef RECINSTRFETCH

#define InstrDumpFile "fetchedInstr.dump"

void CodeArea::writeInstr(void){
  FILE* ofile;
  if(ofile = fopen(InstrDumpFile, "w")){
    int i = fetchedInstr;
//    ofile=stdout;
    do {
      if (ops[i]) {
        display(ops[i], 1, ofile);
      }
      i++;
      if(i >= RECINSTRFETCH)
        i = 0;
    } while (i != fetchedInstr);
    fclose(ofile);
    fprintf(stderr,
            "Wrote the %d most recently fetched instructions in file '%s'\n",
            RECINSTRFETCH, InstrDumpFile);
  } else
    error("Cannot open file '%s'.", InstrDumpFile);
} // CodeArea::writeInstr
#endif



// ----------------------------------------------------

char *tagged2String(TaggedRef ref,int depth,int offset)
{
  ostrstream out;

  tagged2Stream(ref,out,depth,offset);
  out << ends;
  char *s = out.str();
  return s;
}

void taggedPrint(TaggedRef ref, int depth, int offset)
{
  tagged2Stream(ref,cout,depth,offset);
  cout << flush;
}

void taggedPrintLong(TaggedRef ref, int depth, int offset)
{
  tagged2StreamLong(ref,cout,depth,offset);
  cout << flush;
}

void Board::printTree()
{
  Board *bb = this;
  Actor *aa;
  int off=0;
  while (bb!=am.rootBoard) {
    bb->print(cout,1,off);
    cout << endl;
    if (bb->isCommitted()) {
      bb=bb->u.board;
    } else {
      off++;
      aa = bb->u.actor;
      aa->print(cout,1,off);
      cout << endl;
      off++;
      bb = aa->getBoard();
    }
  }
}

void printSuspension(ProgramCounter pc)
{
  cout << "Suspension in ";
  printWhere(cout,pc);
  cout << endl;
  if (am.conf.showSuspension > 1) {
    am.currentThread->taskStack.print(cout);
  }
}

void printWhere(ostream &stream,ProgramCounter PC)
{
  PC = CodeArea::definitionStart(PC);

  if (PC == NOCODE) {
    stream << "on toplevel";
  } else {
    TaggedRef file      = getLiteralArg(PC+3);
    TaggedRef line      = getNumberArg(PC+4);
    PrTabEntry *pred    = getPredArg(PC+5);

    stream << "procedure "
           << (pred ? pred->getPrintName() : "(NULL)")
           << " in file "
           << OZ_atomToC(file)
           << " at line "
           << OZ_intToC(line);
  }
}
