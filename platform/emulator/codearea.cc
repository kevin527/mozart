/*
  Hydra Project, DFKI Saarbruecken,
  Stuhlsatzenhausweg 3, D-W-6600 Saarbruecken 11, Phone (+49) 681 302-5312
  Author: popow
  Last modified: $Date$ from $Author$
  Version: $Revision$
  State: $State$

  ------------------------------------------------------------------------
*/


#include "am.hh"
#include "indexing.hh"
#include "optostr.hh"

//
// Globals
//


AbstractionEntry* AbstractionEntry::allEntries = NULL;

#ifdef RECINSTRFETCH

int CodeArea::fetchedInstr = 0;
ProgramCounter CodeArea::ops[RECINSTRFETCH];



void CodeArea::recordInstr(ProgramCounter PC){
  ops[fetchedInstr] = PC;
  fetchedInstr++;
  if(fetchedInstr >= RECINSTRFETCH)
    fetchedInstr = 0;
}

#endif

HashTable CodeArea::atomTab(HT_CHARKEY,10000);
HashTable CodeArea::nameTab(HT_CHARKEY,1000);
int CodeArea::totalSize = 0; /* in bytes */
char **CodeArea::opToString = initOpToString();
CodeArea *CodeArea::allBlocks = NULL;

#ifdef THREADED
void **CodeArea::globalInstrTable = NULL;
#endif



Opcode CodeArea::stringToOp(char *s)
{
  for (int i=0; i < (Opcode) OZERROR; i++) {
    if (strcmp(s,opToString[i]) == 0 ) {
      return (Opcode) i;
    }
  }

  return OZERROR;
}

inline Literal *addToLiteralTab(char *str, HashTable *table, Bool isName)
{
  Literal *found = (Literal *) table->htFind(str);

  if (found != (Literal *) htEmpty) {
    return found;
  }

  str = ozstrdup(str);

  if (isName) {
    found = NamedName::newNamedName(str);
  } else {
    found = Atom::newAtom(str);
  }

  if (table->htAdd(str,found,NO)) {
    return found;
  }
  error("addToLiteralTab: failed");
  return NULL;
}


Literal *addToAtomTab(char *str)
{
  return addToLiteralTab(str,&CodeArea::atomTab,NO);
}

Literal *addToNameTab(char *str)
{
  return addToLiteralTab(str,&CodeArea::nameTab,OK);
}


AbstractionTable CodeArea::abstractionTab(4000);

/*
  AbstractionEntry::defaultEntry: for bug fix. If you feed

  declare P1 P2 Px in

  proc {P1 X} true end
  proc {Px X} false end
  Px=P1
  proc {P2 X} true end

  this gives toplevel failure. Afterwards feed
   {P2 1}
  this SEGV, since FASTCALL points to an unfilled AbstractionEntry.
  Use defaultEntry for all newly created AbstractionEntry. Set by a builtin.

*/


AbstractionEntry *AbstractionTable::add(Abstraction *abstr)
{
  AbstractionEntry *ret = new AbstractionEntry(NO);
  ret->setPred(abstr);
  return ret;
}

AbstractionEntry *AbstractionTable::add(int id)
{
  if (id == 0)
    return NULL;

  AbstractionEntry *found = (AbstractionEntry *) CodeArea::abstractionTab.htFind(id);

  if (found != (AbstractionEntry *) htEmpty) {
    return found;
  }

  found = new AbstractionEntry(NO);
  if (CodeArea::abstractionTab.htAdd(id,found)) {
    return found;
  }

  Assert(0);
  return NULL;
}


/*
  we store the absolute adress of the indices in the
  instruction tables
  */

#ifdef THREADED
AdressOpcode CodeArea::opcodeToAdress(Opcode oc)
{
  return ToInt32(globalInstrTable[oc]);
}


Opcode CodeArea::adressToOpcode(AdressOpcode adr)
{
  for(int i = 0; i < (int) OZERROR; i++)
    if (ToInt32(globalInstrTable[i]) == adr)
      return (Opcode)i;
  return OZERROR;
}

#else /* THREADED */
AdressOpcode CodeArea::opcodeToAdress(Opcode oc)  { return  oc; }
Opcode CodeArea::adressToOpcode(AdressOpcode adr) { return adr; }
#endif /* THREADED */


void AbstractionEntry::setPred(Abstraction *ab)
{
  abstr = ab;
  pc    = abstr->getPC();
  g     = abstr->getGRegs();
  arity = abstr->getArity();

  // indexing on X[0] optimized !!!
  if (pc != NOCODE &&
      CodeArea::getOpcode(pc) == SWITCHONTERMX &&
      getRegArg(pc+1) == 0) {
    indexTable = (IHashTable *) getAdressArg(pc+2);
  } else {
    indexTable = NULL;
  }
}


#define DISPATCH() PC += sizeOf(op); break



char *getBIName(ProgramCounter PC)
{
  BuiltinTabEntry* entry = (BuiltinTabEntry*) getAdressArg(PC);
  return entry->getPrintName();

}


void CodeArea::printDef(ProgramCounter PC)
{
  TaggedRef file, comment;
  int line, abspos;
  ProgramCounter pc;

#ifdef DEBUG
  pc = nextDebugInfo(PC);
  if (pc != NOCODE) {
    getDebugInfoArgs(pc,file,line,abspos,comment);
    message("\tnext application: in file \"%s\", line %d, column %d, comment: %s, PC=%ld)\n",
            OZ_atomToC(file),line,abspos,toC(comment),PC);
    return;
  }
#endif

  pc = definitionStart(PC);
  if (pc == NOCODE || pc == NOCODE_GLOBALVARNAME) {
    message("\tSpecial task or on toplevel (PC=%s)\n",opToString[(int)getOpcode(PC)]);
    return;
  }

  Reg reg;
  ProgramCounter next;
  PrTabEntry *pred;

  getDefinitionArgs(pc,reg,next,file,line,pred);

  char *predName;
  if (!pred)
    predName = "???";
  else if (*pred->getPrintName())
    predName = pred->getPrintName();
  else
    predName = 0;

  if (predName)
    message("\tprocedure '%s' in file \"%s\", line %d, PC=%ld\n",
            predName,OZ_atomToC(file),line,PC);
  else
    message("\tprocedure in file \"%s\", line %d, PC=%ld\n",
            predName,OZ_atomToC(file),line,PC);
}

TaggedRef CodeArea::dbgGetDef(ProgramCounter PC)
{
  ProgramCounter pc = definitionStart(PC);

  if (pc == NOCODE)
    return OZ_atom("toplevel");
  if (pc == NOCODE_GLOBALVARNAME)
    return nil();

  Reg reg;
  ProgramCounter next;
  PrTabEntry *pred;

  TaggedRef file;
  int line;
  // file & line might be overwritten some lines later...
  getDefinitionArgs(pc,reg,next,file,line,pred);

  // if we are lucky there's some debuginfo and we can determine
  // the exact position inside the procedure application
  TaggedRef _dbgComment; int _dbgAbspos;
  ProgramCounter dbgPC = CodeArea::nextDebugInfo(PC);
  if (dbgPC != NOCODE)
    CodeArea::getDebugInfoArgs(dbgPC,file,line,_dbgAbspos,_dbgComment);

  TaggedRef pairlist =
    OZ_cons(OZ_pairA("PC", OZ_int((int)PC)),
            OZ_cons(OZ_pairA("name", OZ_atom(pred ?
                                             pred->getPrintName() : "???")),
                    OZ_cons(OZ_pairA("file", file),
                            OZ_cons(OZ_pairA("line", OZ_int(line)),
                                    OZ_nil()))));

  return OZ_recordInit(OZ_atom("proc"), pairlist);
}

Bool CodeArea::existVarNames(ProgramCounter PC) {
  ProgramCounter aux = definitionEnd(PC);
  if (aux != NOCODE && aux != NOCODE_GLOBALVARNAME) {
    aux += sizeOf(getOpcode(aux));
    Opcode op = getOpcode(aux);
    if (op == LOCALVARNAME || op == GLOBALVARNAME)
      return OK;
  }
  return NO;
}

TaggedRef CodeArea::varNames(ProgramCounter PC, RefsArray G, RefsArray Y)
{
  TaggedRef locals = nil();
  TaggedRef globals = nil();

  ProgramCounter aux = definitionEnd(PC);

  if (aux != NOCODE && aux != NOCODE_GLOBALVARNAME) {
    aux += sizeOf(getOpcode(aux));

    for (int i=0; getOpcode(aux) == LOCALVARNAME; i++) {
      if (Y) {
        TaggedRef aux1 = getLiteralArg(aux+1);
        locals = cons(OZ_mkTupleC("#", 2,       aux1,
                                  Y[i] ? Y[i] : OZ_atom("unallocated")),
                      locals);
      }
      aux += sizeOf(getOpcode(aux));
    }

    if (G) {
      for (int i=0; getOpcode(aux) == GLOBALVARNAME; i++) {
        TaggedRef aux1 = getLiteralArg(aux+1);
        globals = cons(OZ_mkTupleC("#", 2, aux1, G[i]), globals);
        aux += sizeOf(getOpcode(aux));
      }
    }
  }

  TaggedRef pairlist =
    cons(OZ_pairA("Y", locals),
         cons(OZ_pairA("G", globals),
              nil()));

  TaggedRef ret = OZ_recordInit(OZ_atom("v"), pairlist);
  return ret;
}

TaggedRef CodeArea::argumentList(RefsArray X, int arity)
{
  TaggedRef ret;
  ret = nil();
  for(--arity; arity>=0; arity--)
    ret = cons(X[arity], ret);
  return ret;
}

ProgramCounter CodeArea::definitionStart(ProgramCounter from)
{
  ProgramCounter ret = definitionEnd(from);
  return (ret == NOCODE ||
          ret == NOCODE_GLOBALVARNAME) ? ret : getLabelArg(ret+1);
}


ProgramCounter CodeArea::nextDebugInfo(ProgramCounter from)
{
  ProgramCounter end = definitionEnd(from);
  if (end==NOCODE || end==NOCODE_GLOBALVARNAME)
    return NOCODE;

  ProgramCounter PC = from;

  while (1) {
    if (PC>=end)
      return NOCODE;
    Opcode op = getOpcode(PC);
    switch (op) {
    case OZERROR:   return NOCODE;
    case DEFINITION:
      PC = definitionEnd(PC+sizeOf(op));
      continue;
    case DEBUGINFO: return PC;
    default: DISPATCH();
    }
  }
  return NOCODE;
}


/* find the end of the definition where from points into */
ProgramCounter CodeArea::definitionEnd(ProgramCounter from)
{
  if (from==NULL) {
    return NOCODE;
  }
  ProgramCounter PC = from;

  while (1) {
    Opcode op = getOpcode(PC);
    switch (op) {
    case CREATENAMEDVARIABLEX:
    case CREATENAMEDVARIABLEY:
    case CREATENAMEDVARIABLEG:
    case OZERROR:
      return NOCODE;

    case GLOBALVARNAME:    // last instr in CodeArea::init
      return NOCODE_GLOBALVARNAME;

    case DEFINITION:
      {
        Reg reg;
        ProgramCounter next;
        TaggedRef file;
        int line;
        PrTabEntry *pred;
        getDefinitionArgs(PC,reg,next,file,line,pred);
        PC=next;
        continue;
      }

    case ENDDEFINITION:
      return PC;

    default:
      DISPATCH();
    }
  }
  return NOCODE;
}

void displayCode(ProgramCounter from, int ssize)
{
 CodeArea::display(from,ssize,stderr);
 fflush(stderr);
}

void displayDef(ProgramCounter from, int ssize)
{
  displayCode(CodeArea::definitionStart(from),ssize);
}


void CodeArea::getDefinitionArgs(ProgramCounter PC,
                                 Reg &reg, ProgramCounter &next, TaggedRef &file,
                                 int &line, PrTabEntry *& pred)
{
  Assert(getOpcode(PC) == DEFINITION);
  reg  = regToInt(getRegArg(PC+1));
  next = getLabelArg(PC+2);
  pred = getPredArg(PC+3);
  file = pred ? pred->getFileName() : nil();
  line = pred ? pred->getLine() : 0;
}

void CodeArea::getDebugInfoArgs(ProgramCounter PC,
                                TaggedRef &file, int &line, int &abspos,
                                TaggedRef &comment)
{
  Assert(getOpcode(PC) == DEBUGINFO);
  file    = getLiteralArg(PC+1);
  line    = smallIntValue(getNumberArg(PC+2));
  abspos  = smallIntValue(getNumberArg(PC+3));
  comment = getLiteralArg(PC+4);
}


void CodeArea::display (ProgramCounter from, int sz, FILE* ofile)
{
  if (sz <=0) return;

  ProgramCounter PC = from;

  for (int i = 1; i<=sz; i++) {
    fprintf(ofile, "0x%p:  ", PC);
    Opcode op = getOpcode(PC);
    if (op == OZERROR) {
      message("End of code block reached\n");
      return;
    }

    fprintf(ofile, "%03d %s", op,opToString[(int)op]);

    switch (op) {
    case FAILURE:
    case SUCCEED:
    case WAIT:
    case EMPTYCLAUSE:
    case WAITTOP:
    case ASK:
    case RETURN:
    case CLAUSE:
    case LASTCLAUSE:
    case DEALLOCATEL:
    case DEALLOCATEL1:
    case DEALLOCATEL2:
    case DEALLOCATEL3:
    case DEALLOCATEL4:
    case DEALLOCATEL5:
    case DEALLOCATEL6:
    case DEALLOCATEL7:
    case DEALLOCATEL8:
    case DEALLOCATEL9:
    case DEALLOCATEL10:
    case ALLOCATEL1:
    case ALLOCATEL2:
    case ALLOCATEL3:
    case ALLOCATEL4:
    case ALLOCATEL5:
    case ALLOCATEL6:
    case ALLOCATEL7:
    case ALLOCATEL8:
    case ALLOCATEL9:
    case ALLOCATEL10:
    case SHALLOWTHEN:
    case CREATEOR:
    case CREATEENUMOR:
    case CREATECHOICE:
    case PROFILEPROC:
    case POPEX:
    case TASKXCONT:
    case TASKCFUNCONT:
    case TASKDEBUGCONT:
    case TASKCALLCONT:
    case TASKLOCK:
    case TASKSETSELF:
    case TASKLTQ:
    case TASKCATCH:
    case TASKEMPTYSTACK:
    case TASKPROFILECALL:
    case TASKACTOR:
          /* Commands with no args.   */
      fprintf(ofile, "\n");
      DISPATCH();
    case DEBUGINFO:
      {
        TaggedRef filename,comment;
        int line, abspos;
        char cs[50];

        getDebugInfoArgs(PC,filename,line,abspos,comment);

        strcpy(cs, toC(comment));   // we must save this, blame toC()!
        fprintf(ofile,"(%s, line: %d, file: %s)\n",cs,line,toC(filename));
        DISPATCH();
      }
    case PUTLISTX:
    case PUTLISTY:
    case PUTLISTG:
    case SETVALUEX:
    case SETVALUEY:
    case SETVALUEG:
    case GETLISTX:
    case GETLISTY:
    case GETLISTG:
    case UNIFYVALUEX:
    case UNIFYVALUEY:
    case UNIFYVALUEG:
    case GETVARIABLEX:
    case GETVARIABLEY:
    case GETVARIABLEG:
    case SETVARIABLEX:
    case SETVARIABLEY:
    case SETVARIABLEG:
    case UNIFYVARIABLEX:
    case UNIFYVARIABLEY:
    case UNIFYVARIABLEG:
    case CREATEVARIABLEX:
    case CREATEVARIABLEY:
    case CREATEVARIABLEG:
    case GETSELF:
    case CLEARY:
      /* OP Reg       */
      fprintf(ofile, "(%d)\n", regToInt(getRegArg(PC+1)));
      DISPATCH();

    case CREATEVARIABLEMOVEX:
    case CREATEVARIABLEMOVEY:
    case CREATEVARIABLEMOVEG:
          /* OP Reg RegIndex       */
      fprintf (ofile,"(%d,X[%d])\n",
               regToInt(getRegArg(PC+1)),
               regToInt(getRegArg(PC+2)));
      DISPATCH();

    case GETLISTVALVARX:
    case GETLISTVALVARY:
    case GETLISTVALVARG:
      /* OP RegIndex Reg RegIndex       */
      fprintf (ofile,"(X[%d],%d,X[%d])\n",
               regToInt(getRegArg(PC+1)),
               regToInt(getRegArg(PC+2)),
               regToInt(getRegArg(PC+3)));
      DISPATCH();

    case MOVEMOVEXYXY:
    case MOVEMOVEYXXY:
    case MOVEMOVEYXYX:
    case MOVEMOVEXYYX:
          /* ***type 1:    OP Reg       */
      fprintf (ofile,"(%d,%d,%d,%d)\n",
              regToInt(getRegArg(PC+1)),regToInt(getRegArg(PC+2)),
              regToInt(getRegArg(PC+3)),regToInt(getRegArg(PC+4)));
      DISPATCH();

    case SHALLOWTEST1:
      fprintf (ofile,
               "(%s,X[%d],0x%p,%d)\n",
               getBIName(PC+1),
               regToInt(getRegArg(PC+2)),
               getLabelArg(PC+3),
               getPosIntArg(PC+4));
      DISPATCH();

    case TESTCONSTX:
    case TESTCONSTY:
    case TESTCONSTG:
    case TESTNUMBERX:
    case TESTNUMBERY:
    case TESTNUMBERG:
      {
        TaggedRef literal = getLiteralArg(PC+2);
        fprintf (ofile,
                 "(%d,%s,0x%p,0x%p,%d)\n",
                 regToInt(getRegArg(PC+1)),
                 toC(literal),
                 getLabelArg(PC+3),
                 getLabelArg(PC+4),
                 getPosIntArg(PC+5));
        DISPATCH();
      }

    case TESTBOOLX:
    case TESTBOOLY:
    case TESTBOOLG:
      {
        fprintf (ofile,
                 "(%d,0x%p,0x%p,0x%p,%d)\n",
                 regToInt(getRegArg(PC+1)),
                 getLabelArg(PC+2),
                 getLabelArg(PC+3),
                 getLabelArg(PC+4),
                 getPosIntArg(PC+5));
        DISPATCH();
      }

    case SHALLOWTEST2:
      fprintf (ofile,
               "(%s,X[%d],X[%d],0x%p,%d)\n",
               getBIName(PC+1),
               regToInt(getRegArg(PC+2)),
               regToInt(getRegArg(PC+3)),
               getLabelArg(PC+4),
               getPosIntArg(PC+5));
      DISPATCH();

    case INLINEREL1:
      fprintf (ofile,
               "(%s,X[%d],%d)\n",
               getBIName(PC+1),
               regToInt(getRegArg(PC+2)),
               getPosIntArg(PC+3));
      DISPATCH();

    case INLINEFUN1:
    case INLINEREL2:
      fprintf (ofile,
               "(%s,X[%d],X[%d],%d)\n",
               getBIName(PC+1),
               regToInt(getRegArg(PC+2)),
               regToInt(getRegArg(PC+3)),
               getPosIntArg(PC+4));
      DISPATCH();

    case INLINEFUN2:
    case INLINEREL3:
    case INLINEEQEQ:
      fprintf (ofile,
               "(%s,X[%d],X[%d],X[%d],%d)\n",
               getBIName(PC+1),
               regToInt(getRegArg(PC+2)),
               regToInt(getRegArg(PC+3)),
               regToInt(getRegArg(PC+4)),
               getPosIntArg(PC+5));
      DISPATCH();

    case INLINEUPARROW:
      {
        TaggedRef literal = getLiteralArg(PC+2);
        fprintf (ofile,
                 "(X[%d],X[%d],X[%d],%d)\n",
                 regToInt(getRegArg(PC+1)),
                 regToInt(getRegArg(PC+2)),
                 regToInt(getRegArg(PC+3)),
                 getPosIntArg(PC+4));
      }
      DISPATCH();

    case INLINEDOT:
      {
        TaggedRef literal = getLiteralArg(PC+2);
        fprintf (ofile,
                 "(X[%d],%s,X[%d])\n",
                 regToInt(getRegArg(PC+1)),
                 toC(literal),
                 regToInt(getRegArg(PC+3)));
      }
      DISPATCH();

    case INLINEAT:
    case INLINEASSIGN:
      {
        TaggedRef literal = getLiteralArg(PC+1);
        fprintf (ofile,
                 "(%s,X[%d])\n",
                 toC(literal),
                 regToInt(getRegArg(PC+2)));
      }
      DISPATCH();

    case INLINEFUN3:
      fprintf (ofile,
               "(%s,X[%d],X[%d],X[%d],X[%d],%d)\n",
               getBIName(PC+1),
               regToInt(getRegArg(PC+2)),
               regToInt(getRegArg(PC+3)),
               regToInt(getRegArg(PC+4)),
               regToInt(getRegArg(PC+5)),
               getPosIntArg(PC+6));
      DISPATCH();

    case CALLBUILTIN:
      fprintf (ofile,
               "(%s,%d)\n",
               getBIName(PC+1),
               getPosIntArg(PC+2));
      DISPATCH();

    case GENFASTCALL:
    case FASTCALL:
    case FASTTAILCALL:
      {
        /* type: OP PredicateRef */
        AbstractionEntry *entry = (AbstractionEntry *) getAdressArg(PC+1);
        Abstraction *abstr = entry->getAbstr();
        if (abstr) {  /* may be NULL during loading */
          fprintf(ofile, "(%s,%d)\n", abstr->getPrintName(),abstr->getArity());
        } else {
          fprintf(ofile, "(??,??)\n");
        }
        DISPATCH();
      }
    case APPLMETHX:
    case APPLMETHY:
    case APPLMETHG:
    case TAILAPPLMETHX:
    case TAILAPPLMETHY:
    case TAILAPPLMETHG:
      {
        ApplMethInfoClass *ami = (ApplMethInfoClass*) getAdressArg(PC+1);
        int arity               = ami->arity;
        Reg reg                 = regToInt(getRegArg(PC+2));
        fprintf(ofile, "(%s,%d,%d)\n", toC(ami->methName),arity,reg);
        DISPATCH();
      }
    case SENDMSGX:
    case SENDMSGY:
    case SENDMSGG:
    case TAILSENDMSGX:
    case TAILSENDMSGY:
    case TAILSENDMSGG:
      {
        TaggedRef mn   = getLiteralArg(PC+1);
        Reg reg        = regToInt(getRegArg(PC+2));
        int arity      = getPosIntArg(PC+3);
        fprintf(ofile, "(%s,%d,%d)\n",toC(mn),reg,arity);
        DISPATCH();
      }
    case CALLX:
    case CALLY:
    case CALLG:
    case TAILCALLX:
    case TAILCALLY:
    case TAILCALLG:
    case WEAKDETX:
    case WEAKDETY:
    case WEAKDETG:
    case DETX:
    case DETY:
    case DETG:
          /* ***type 3a:    OP Reg Int      */
      {
        Reg reg = regToInt(getRegArg(PC+1));
        fprintf(ofile, "(%d,%d)\n", reg,getPosIntArg(PC+2));
      }
      DISPATCH();
    case SETVOID:
    case GETVOID:
    case UNIFYVOID:
    case ALLOCATEL:
          /* ***type 2:    OP PosInt    */
      fprintf(ofile, "(%d)\n",getPosIntArg(PC+1));
      DISPATCH();
    case SETINT:
    case UNIFYINT:
          /* ***type 10:   OP Number       */
      {
        fprintf(ofile, "(");
        TaggedRef b = getNumberArg(PC+1);
        if (b == makeTaggedNULL())
          fprintf(ofile, "(NULL)");
        else
          fprintf(ofile, "%s", toC(b));
        fprintf(ofile, ")\n");
      }
      DISPATCH();
    case PUTINTX:
    case PUTINTY:
    case PUTINTG:
    case GETINTX:
    case GETINTY:
    case GETINTG:
          /* ***type 3:    OP PosInt Reg   */
      {
        fprintf(ofile, "(");
        TaggedRef b = getNumberArg(PC+1);
        if (b == makeTaggedNULL())
          fprintf(ofile, "(NULL)");
        else
          fprintf(ofile, "%s", toC(b));
        fprintf(ofile, ",%d)\n", regToInt(getRegArg(PC+2)));
      }
      DISPATCH();
    case MOVEXX:
    case MOVEXY:
    case MOVEXG:
    case MOVEYX:
    case MOVEYY:
    case MOVEYG:
    case MOVEGX:
    case MOVEGY:
    case MOVEGG:

    case UNIFYXX:
    case UNIFYXY:
    case UNIFYXG:
    case UNIFYYX:
    case UNIFYYY:
    case UNIFYYG:
    case UNIFYGX:
    case UNIFYGY:
    case UNIFYGG:

    case GETVARVARXX:
    case GETVARVARXY:
    case GETVARVARXG:
    case GETVARVARYX:
    case GETVARVARYY:
    case GETVARVARYG:
    case GETVARVARGX:
    case GETVARVARGY:
    case GETVARVARGG:

    case UNIFYVALVARXX:
    case UNIFYVALVARXY:
    case UNIFYVALVARXG:
    case UNIFYVALVARYX:
    case UNIFYVALVARYY:
    case UNIFYVALVARYG:
    case UNIFYVALVARGX:
    case UNIFYVALVARGY:
    case UNIFYVALVARGG:
          /* ***type 4:    OP Reg Reg   */
      {
        Reg reg = regToInt(getRegArg(PC+1));
        fprintf(ofile, "(%d,%d)\n", reg, regToInt(getRegArg(PC+2)));
      }
      DISPATCH();
    case SETCONSTANT:
    case UNIFYCONSTANT:
    case GLOBALVARNAME:
    case LOCALVARNAME:
          /* ***type 5:    OP ConstantName  */
      {
        TaggedRef literal = getLiteralArg(PC+1);

        fprintf(ofile, "(%s)\n", toC(literal));
      }
      DISPATCH();
    case DEFINITION:
          /* ***type 11:    OP predicate     */
      {
        Reg reg;
        ProgramCounter next;
        TaggedRef file;
        int line;
        PrTabEntry *pred;
        getDefinitionArgs(PC,reg,next,file,line,pred);
        AssRegArray *list = (AssRegArray*) getAdressArg(PC+5);

        fprintf(ofile, "(X%d,0x%p,%s,%s,%d,[",reg,next,
                pred ? pred->getPrintName() : "(NULL)",
                toC(file), line);

        for (int k = 0; k < list->getSize(); k++) {
          switch ((*list)[k].kind) {
          case XReg: fprintf(ofile,"X%d ",(*list)[k].number); break;
          case YReg: fprintf(ofile,"Y%d ",(*list)[k].number); break;
          case GReg: fprintf(ofile,"G%d ",(*list)[k].number); break;
          }
        }

        fprintf(ofile, "])\n");
      }
      DISPATCH();

    case PUTRECORDX:
    case PUTRECORDY:
    case PUTRECORDG:
    case GETRECORDX:
    case GETRECORDY:
    case GETRECORDG:
          /* ***type 6:    OP LiteralName Reg */
      {
        TaggedRef literal = getLiteralArg(PC+1);
        int n = getPosIntArg(PC+2);

        fprintf(ofile, "(%s,%i,%d)\n", toC(literal),
                 n, regToInt(getRegArg(PC+3)));
      }
      DISPATCH();
    case PUTCONSTANTX:
    case PUTCONSTANTY:
    case PUTCONSTANTG:
    case GETCONSTANTX:
    case GETCONSTANTY:
    case GETCONSTANTG:
          /* ***type 7:    OP ConstantName Reg */
      {
        TaggedRef literal = getLiteralArg(PC+1);

        fprintf(ofile, "(%s,%d)\n", toC(literal),
                 regToInt(getRegArg(PC+2)));
      }
      DISPATCH();

    case CREATENAMEDVARIABLEX:
    case CREATENAMEDVARIABLEY:
    case CREATENAMEDVARIABLEG:
          /* ***type 7:    OP ConstantName Reg */
      {
        TaggedRef literal = getLiteralArg(PC+2);

        fprintf(ofile, "(%s,%d)\n", toC(literal),
                 regToInt(getRegArg(PC+1)));
      }
      DISPATCH();
    case ENDDEFINITION:
    case BRANCH:
    case NEXTCLAUSE:
    case THREAD:
    case SAVECONT:
    case EXHANDLER:
          /* ***type 8:    OP Label */
      fprintf(ofile, "(@ 0x%p)\n", getLabelArg (PC+1));
      DISPATCH();

    case THREADX:
          /* ***type 8:    OP PosInt Label */
      fprintf(ofile, "%d (@ 0x%p)\n", getPosIntArg(PC+1), getLabelArg(PC+2));
      DISPATCH();

    case BRANCHONVARX:
    case BRANCHONVARY:
    case BRANCHONVARG:
    case BRANCHONNONVARX:
    case BRANCHONNONVARY:
    case BRANCHONNONVARG:

          /* ***type 9:    OP Reg Label */
      {
        Reg reg = regToInt(getRegArg(PC+1));

        fprintf(ofile, "(%d,@ 0x%p)\n", reg, getLabelArg (PC+2));
      }
      DISPATCH();

    case SWITCHONTERMX:
    case SWITCHONTERMY:
    case SWITCHONTERMG:
      {
        Reg reg = regToInt(getRegArg(PC+1));

        fprintf(ofile, "(%d", reg);
        fprintf(ofile, "...)\n");
      }

      DISPATCH();

    case SHALLOWGUARD:
    case CREATECOND:
          /* ***type 8:    OP Label Int*/
      {
        ProgramCounter lbl = getLabelArg(PC+1);
        int n = getPosIntArg(PC+2);
        fprintf(ofile, "(@ 0x%p, %d)\n", lbl, n);
      }
      DISPATCH();

    case LOCKTHREAD:
      {
        ProgramCounter lbl = getLabelArg(PC+1);
        int n      = regToInt(getRegArg(PC+2));
        int toSave = getPosIntArg(PC+3);
        fprintf(ofile, "(@ 0x%p, %d, %d)\n", lbl, n, toSave);
      }
      DISPATCH();

    case GENCALL:
      {
        fprintf(ofile, "(0x%p,%d)\n", getAdressArg(PC+1),getPosIntArg(PC+2));
        DISPATCH();
      }

    case MARSHALLEDFASTCALL:
      {
        fprintf(ofile, "(%s,%d)\n", toC(getTaggedArg(PC+1)),getPosIntArg(PC+2));
        DISPATCH();
      }

    default:
      fflush(ofile);
      warning("Undefined command: %d (function void CodeArea::display)", op);
      return;
    }
  }
}

#undef DISPATCH

ProgramCounter
  C_XCONT_Ptr,
  C_CFUNC_CONT_Ptr,
  C_DEBUG_CONT_Ptr,
  C_CALL_CONT_Ptr,
  C_LOCK_Ptr,
  C_COMMIT_Ptr,
  C_SET_SELF_Ptr,
  C_SET_ABSTR_Ptr,
  C_LTQ_Ptr,
  C_CATCH_Ptr,
  C_ACTOR_Ptr,
  C_EMPTY_STACK;



CodeArea::CodeArea(int sz)
{
  allocateBlock(sz);
}

void CodeArea::init(void **instrTable)
{
#ifdef THREADED
  globalInstrTable = instrTable;
#endif
  CodeArea *code = new CodeArea(20);
  C_XCONT_Ptr = code->getStart();
  C_CFUNC_CONT_Ptr   = writeOpcode(TASKXCONT,C_XCONT_Ptr);
  C_DEBUG_CONT_Ptr   = writeOpcode(TASKCFUNCONT,C_CFUNC_CONT_Ptr);
  C_CALL_CONT_Ptr    = writeOpcode(TASKDEBUGCONT,C_DEBUG_CONT_Ptr);
  C_LOCK_Ptr         = writeOpcode(TASKCALLCONT,C_CALL_CONT_Ptr);
  C_SET_SELF_Ptr     = writeOpcode(TASKLOCK,C_LOCK_Ptr);
  C_SET_ABSTR_Ptr    = writeOpcode(TASKSETSELF,C_SET_SELF_Ptr);
  C_LTQ_Ptr          = writeOpcode(TASKPROFILECALL,C_SET_ABSTR_Ptr);
  C_ACTOR_Ptr        = writeOpcode(TASKLTQ,C_LTQ_Ptr);
  C_CATCH_Ptr        = writeOpcode(TASKACTOR,C_ACTOR_Ptr);
  C_EMPTY_STACK      = writeOpcode(TASKCATCH,C_CATCH_Ptr);
  ProgramCounter aux =  writeOpcode(TASKEMPTYSTACK,C_EMPTY_STACK);
  /* mark end with GLOBALVARNAME, so definitionEnd works properly */
  (void) writeOpcode(GLOBALVARNAME,aux);

}
