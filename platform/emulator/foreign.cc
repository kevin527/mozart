/*
  Hydra Project, DFKI Saarbruecken,
  Stuhlsatzenhausweg 3, D-W-6600 Saarbruecken 11, Phone (+49) 681 302-5312
  Author: scheidhr
  Last modified: $Date$ from $Author$
  Version: $Revision$
  State: $State$
  */


#include <ctype.h>

#include "oz.h"


#include "am.hh"
#include "bignum.hh"
#include "builtins.hh"
#include "cell.hh"
#include "io.hh"
#include "records.hh"
#include "thread.hh"
#include "suspension.hh"


/* TmpBuffer with at LEAST 512 characters,
   must be sufficiently large to convert
     smallInts and floats to strings/BigInts */
char TmpBuffer[512];

/* ------------------------------------------------------------------------ *
 * tests
 * ------------------------------------------------------------------------ */

int OZ_isAtom(OZ_Term term)
{
  DEREF(term,_1,_2);
  return (isXAtom(term) == OK) ? 1 : 0;
}

int OZ_isCell(OZ_Term term)
{
  DEREF(term,_1,_2);
  return (isCell(term) == OK) ? 1 : 0;
}

int OZ_isCons(OZ_Term term)
{
  return (isCons(term) == OK) ? 1 : 0;
}

int OZ_isFloat(OZ_Term term)
{
  DEREF(term,_1,_2);
  return (isFloat(term) == OK) ? 1 : 0;
}

int OZ_isInt(OZ_Term term)
{
  DEREF(term,_1,_2);
  return (isInt(term) == OK) ? 1 : 0;
}

int OZ_isLiteral(OZ_Term term)
{
  DEREF(term,_1,_2);
  return (isLiteral(term) == OK) ? 1 : 0;
}

int OZ_isName(OZ_Term term)
{
  DEREF(term,_1,_2);
  return (isXName(term) == OK) ? 1 : 0;
}

int OZ_isNil(OZ_Term term)
{
  return (isNil(term) == OK) ? 1 : 0;
}

int OZ_isNoNumber(OZ_Term term)
{
  DEREF(term,_1,tag);
  return isRecord(tag) || isTuple(tag);
}

int OZ_isRecord(OZ_Term term)
{
  DEREF(term,_1,_2);
  return (isRecord(term) == OK) ? 1 : 0;
}

int OZ_isTuple(OZ_Term term)
{
  DEREF(term,_1,_2);
  return (isTuple(term) == OK) ? 1 : 0;
}

int OZ_isValue(OZ_Term term)
{
  DEREF(term,_1,_2);
  return !isAnyVar(term);
}

int OZ_isVariable(OZ_Term term)
{
  DEREF(term,_1,_2);
  return isAnyVar(term);
}

OZ_Term OZ_termType(OZ_Term term)
{
  DEREF(term,_,tag);

  if (isAnyVar(tag)) {
    return AtomVariable;
  }

  if (isInt(tag)) {
    return AtomInt;
  }

  if (isFloat(tag)) {
    return AtomFloat;
  }

  if (isLiteral(tag)) {
    return (tagged2Atom(term)->isXName() ? AtomName : AtomAtom);
  }

  if (isSTuple(tag) || isLTuple(tag)) {
    return AtomTuple;
  }

  if (isProcedure(term)) { // before isSRecord test !!
    return AtomProcedure;
  }

  if (isCell(term)) { // before isSRecord test !!
    return AtomCell;
  }

  if (isSRecord(tag)) {
    return AtomRecord;
  }

  return AtomUnknown;
}

/* -----------------------------------------------------------------
 * convert: C from/to Oz datastructure
 * -----------------------------------------------------------------*/

/*
 * Ints
 */

OZ_Term OZ_CToInt(int i)
{
  return makeInt(i);
}

int OZ_intToC(OZ_Term term)
{
  DEREF(term,_1,tag);

  if (isSmallInt(tag)) {
    return smallIntValue(term);
  }

  if (isBigInt(tag)) {
    return tagged2BigInt(term)->BigInt2Int();
  }

  OZ_warning("intToC(%s): int arg expected", OZ_toC(term));
  return 0;
}

OZ_Term OZ_CStringToInt(char *str)
{
  char *help = ozstrdup(str);
  replChar(help,'~','-');

  OZ_Term ret = (new BigInt(help))->shrink();
  delete [] help;
  return ret;
}

char *OZ_normInt(char *s)
{
  if (s[0] == '-') {
    s[0] = '~';
  }
  return s;
}

char *OZ_normFloat(char *s)
{
  replChar(s,'-','~');
  delChar(s,'+');
  return s;
}

/*
 * parse: [~]<int>.<digit>*[(e|E)<int>]
 */
char *OZ_parseFloat(char *s) {
  char *p = OZ_parseInt(s);
  if (!p || *p++ != '.') {
    return NULL;
  }
  while (isdigit(*p)) {
    p++;
  }
  switch (*p) {
  case 'e':
  case 'E':
    p++;
    break;
  default:
    return p;
  }
  return OZ_parseInt(p);
}

/*
 * parse: [~]<digit>+
 */
char *OZ_parseInt(char *s)
{
  char *p = s;
  if (*p == '~') {
    p++;
  }
  if (!isdigit(*p++)) {
    return NULL;
  }
  while (isdigit(*p)) {
    p++;
  }
  return p;
}

char *OZ_intToCString(OZ_Term term)
{
  DEREF(term,_,tag);
  switch (tag) {
  case SMALLINT:
    {
      sprintf(TmpBuffer,"%d",smallIntValue(term));
      return ozstrdup(TmpBuffer);
    }
  case BIGINT:
    {
      BigInt *bb = tagged2BigInt(term);
      char *str = new char[bb->stringLength()+1];
      bb->getString(str);
      return str;
    }
  default:
    OZ_warning("intToCString(%s): expecting int arg",OZ_toC(term));
    return NULL;
  }
}

/*
 * Floats
 */

OZ_Term OZ_CToFloat(OZ_Float i)
{
  return makeTaggedFloat(new Float(i));
}

OZ_Float OZ_floatToC(OZ_Term term)
{
  DEREF(term,_1,tag);

  if (isFloat(tag)) {
    return floatValue(term);
  }
  OZ_warning("floatToC(%s): float arg expected",OZ_toC(term));
  return 0.0;
}

OZ_Term OZ_CStringToFloat(char *s)
{
  char *help = ozstrdup(s);
  replChar(help,'~','-');
  char *end;
  OZ_Float res = strtod(help,&end);
  if (*end != '\0') {
    OZ_warning("CStringToCFloat(%s): couldn't parse the end of %s",s,end);
  }
  return OZ_CToFloat(res);
}

char *OZ_floatToCString(OZ_Term term)
{
  OZ_Float f = OZ_floatToC(term);
  sprintf(TmpBuffer,"%e",f);
  return ozstrdup(TmpBuffer);
}

char *OZ_floatToCStringInt(OZ_Term term)
{
  OZ_Float f = OZ_floatToC(term);
  sprintf(TmpBuffer,"%.0f",f);
  return ozstrdup(TmpBuffer);
}

char *OZ_floatToCStringPretty(OZ_Term term)
{
  OZ_Float f = OZ_floatToC(term);
  sprintf(TmpBuffer,"%g",f);
  for (char *s = TmpBuffer; *s!='.'; s++) {
    if (*s == 'e') {
      for (char *p = s+strlen(s); p >= s; p--) {
        *(p+2) = *p;
      }
      *s++='.';
      *s = '0';
      break;
    }
    if (*s == 0) {
      *s++ = '.';
      *s++ = '0';
      *s = 0;
      break;
    }
  }
  return ozstrdup(TmpBuffer);
}

/*
 * Numbers
 */

OZ_Term OZ_CStringToNumber(char *s)
{
  if (strchr(s, '.') != NULL) {
    return OZ_CStringToFloat(s);
  }
  return OZ_CStringToInt(s);
}


/*
 * Atoms
 */

char *OZ_atomToC(OZ_Term term)
{
  DEREF(term,_1,tag);
  if (isXAtom(term)) {
//    return ozstrdup(tagged2Atom(term)->getPrintName());
    return tagged2Atom(term)->getPrintName();
  }
  OZ_warning("atomToC(%s): atom arg expected",OZ_toC(term));
  return NULL;
}

OZ_Term OZ_CToAtom(char *s)
{
  return makeTaggedAtom(s);
}

/*
 * Any
 */

char *OZ_toC(OZ_Term term)
{
  if (term == makeTaggedNULL()) {
    return "*** NULL TERM ***";
  }

  DEREF(term,termPtr,tag)
  switch(tag) {
  case UVAR:
    return tagged2String(term,am.conf.printDepth);
//    stream << "UV@" << termPtr;
    break;
  case SVAR:
    return tagged2String(term,am.conf.printDepth);
//    tagged2SVar(term)->print(stream,depth,offset);
    break;
  case CVAR:
    return tagged2String(term,am.conf.printDepth);
//    tagged2CVar(term)->print(stream, depth, offset);
    break;
  case STUPLE:
    return tagged2String(term,am.conf.printDepth);
//    tagged2STuple(term)->print(stream,depth,offset);
    break;
  case SRECORD:
    return tagged2String(term,am.conf.printDepth);
//    tagged2SRecord(term)->print(stream,depth,offset);
    break;
  case LTUPLE:
    return tagged2String(term,am.conf.printDepth);
//    tagged2LTuple(term)->print(stream,depth,offset);
    break;
  case ATOM:
    {
      Atom *a = tagged2Atom(term);
      char *s = a->getPrintName();
      if (a->isXName()) {
        char *tmp = new char[strlen(s)+20];
        sprintf(tmp,"*%s-0x%x*",s,a);
        return tmp;
      } else {
//      return ozstrdup(s);
        return s;
      }
    }
  case FLOAT:
    return OZ_floatToCString(term);
  case BIGINT:
  case SMALLINT:
    return OZ_intToCString(term);
  case CONST:
    return tagged2String(term,am.conf.printDepth);
//    tagged2Const(term)->print(stream,depth,offset);
    break;

  default:
    Assert(0);
    break;
  }

  warning("OZ_toC: failed");
  return ozstrdup("unknown term");
}

/* -----------------------------------------------------------------
 * virtual strings
 * -----------------------------------------------------------------*/

/* convert a C string (char*) to an Oz string */
OZ_Term OZ_CToString(char *s)
{
  char *p=s;
  while (*p!='\0') {
    p++;
  }
  OZ_Term ret = OZ_nil();
  while (p!=s) {
    ret = OZ_cons(OZ_CToInt((unsigned char)*(--p)), ret);
  }
  return ret;
}

/* convert an Oz string to a C string */
char *OZ_stringToC(OZ_Term list)
{
  int len = OZ_length(list);
  if (len < 0) {
    return (char *) len;
  }

  char *s = new char[len+1];
  char *p = s;

  for (OZ_Term tmp = list; OZ_isCons(tmp); tmp=OZ_tail(tmp)) {
    OZ_Term hh = OZ_head(tmp);
    int i;
    if (!OZ_isInt(hh)) {
      delete [] s;
      if (OZ_isVariable(hh)){
        return (char *) -1; // SUSPENDED
      }
      return (char *) -2; // FAILED
    }
    i = OZ_intToC(hh);
    if (i < 0 || i > 255) {
      delete [] s;
      return (char *) -2; // FAILED
    }
    *p++ = i;
  }
  *p = 0;
  return s;
}

/* -----------------------------------------------------------------
 * tuple
 * -----------------------------------------------------------------*/

OZ_Term OZ_label(OZ_Term term)
{
  DEREF(term,_1,tag);

  switch (tag) {
  case LTUPLE:
    return tagged2LTuple(term)->getLabel();
  case STUPLE:
    return tagged2STuple(term)->getLabel();
  case ATOM:
    return term;
  case SRECORD:
    return tagged2SRecord(term)->getLabel();
  default:
    break;
  }
  OZ_warning("OZ_label(%s): no number expected",OZ_toC(term));
  return nil();
}

int OZ_width(OZ_Term term)
{
  DEREF(term,_1,tag);

  switch (tag) {
  case LTUPLE:
    return 2;
  case STUPLE:
    return tagged2STuple(term)->getSize ();
  case SRECORD:
    return tagged2SRecord(term)->getWidth();
  case ATOM:
    return 0;
  default:
    break;
  }
  OZ_warning("OZ_width(%s): no number expected",OZ_toC(term));
  return 0;
}

OZ_Term OZ_tuple(OZ_Term label, int width)
{
  DEREF(label,_1,_2);

  if (!isLiteral(label)) {
    OZ_warning("OZ_tuple(%s,%d): literal expected",
               OZ_toC(label),width);
    return nil();
  }

  if (width == 2 && sameAtom(label,AtomCons)) {
    // have to make a list
    return makeTaggedLTuple(new LTuple());
  }

  if (width <= 0) {
    return label;
  }

  return makeTaggedSTuple(STuple::newSTuple(label,width));
}

int OZ_putArg(OZ_Term term, int pos, OZ_Term newTerm)
{
  DEREF(term,_1,tag);

  if (isLTuple(tag)) {
    switch (pos) {
    case 1:
      tagged2LTuple(term)->setHead(newTerm);
      return 1;
    case 2:
      tagged2LTuple(term)->setTail(newTerm);
      return 1;
    default:
      OZ_warning("OZ_putArg(%s,%d,%s): bad arg",
                 OZ_toC(term),pos,OZ_toC(term));
      return 0;
    }
  }
  if (isSTuple(term) && (pos >= 1) && pos <= tagged2STuple(term)->getSize ()) {
    tagged2STuple(term)->setArg(pos-1,newTerm);
    return 1;
  }

  OZ_warning("OZ_putArg(%s,%d,%s): bad arg",
             OZ_toC(term),pos,OZ_toC(term));

  return 0;
}

OZ_Term OZ_getArg(OZ_Term term, int pos)
{
  DEREF(term,_1,tag);
  if (isLTuple(tag)) {
    switch (pos) {
    case 1:
      return tagged2LTuple(term)->getHead();
    case 2:
      return tagged2LTuple(term)->getTail();
    default:
      OZ_warning("OZ_getArg(%s,%d): bad arg", OZ_toC(term),pos);
      return nil();
    }
  }
  if (isSTuple(term) && (pos >= 1) && pos <= tagged2STuple(term)->getSize ())
    return tagged2STuple(term)->getArg(pos-1);

  OZ_warning("OZ_getArg(%s,%d): bad arg",OZ_toC(term),pos);
  return nil();
}

OZ_Term OZ_nil()
{
  return nil();
}

OZ_Term OZ_cons(OZ_Term hd,OZ_Term tl)
{
  return cons(hd,tl);
}

OZ_Term OZ_head(OZ_Term list)
{
  return head(list);
}

OZ_Term OZ_tail(OZ_Term list)
{
  return tail(list);
}

/*
 * Compute the length of a list and check for determination.
 * Returns:
 *  -1, if the list end is not determined (SUSPENDED)
 *  -2, if it is not a proper list (FAILED)
 *  else the length of the list
 */
int OZ_length(OZ_Term list)
{
  int len = 0;

  for (OZ_Term tmp= list; OZ_isCons(tmp); tmp=OZ_tail(tmp)) {
    len++;
  }

  if (OZ_isNil(tmp)) {
    return len;
  }

  if (OZ_isVariable(tmp)) {
    return -1; // SUSPENDED
  }

  return -2; // FAILED
}

/* -----------------------------------------------------------------
 * record
 * -----------------------------------------------------------------*/

/* take a label and an arity (as list) and construct a record
   the fields are not initialized */
OZ_Term OZ_record(OZ_Term label, OZ_Term arity)
{
  OZ_warning("OZ_record not impl");
  return OZ_nil();
}

/* take a label and a property list and construct a record */
OZ_Term OZ_recordProp(OZ_Term label, OZ_Term propList)
{
  OZ_warning("not impl");
#ifdef MM2
  OZ_Term out;
  OZ_Bool ret = adjoinPropList(label,propList,out,NO);

  if (ret != PROCEED) {
    OZ_warning("OZ_record(%s,%s): failed",
               OZ_toC(label),OZ_toC(propList));
    return nil();
  }

  return out;
#endif
}

void OZ_putRecordArg(OZ_Term record, OZ_Term feature, OZ_Term value)
{
  DEREF(record,_1,recTag);
  DEREF(feature,_2,feaTag);

  if (isLiteral(feaTag) ) {
    if ( isSRecord(recTag) ) {
      SRecord *recOut = tagged2SRecord(record)->replaceFeature(feature,value);
      if (recOut) {
        return;
      }
    }
  }
  OZ_warning("OZ_putRecordArg(%s,%s,%s): failed",
             OZ_toC(record),OZ_toC(feature),OZ_toC(value));
}

OZ_Term OZ_getRecordArg(OZ_Term term, OZ_Term fea)
{
  DEREF(term,_1,_2);
  if (isSRecord(term)) {
    OZ_Term ret = tagged2SRecord(term)->getFeature(fea);
    if (ret) {
      return ret;
    }
  }
  OZ_warning("OZ_getArg(%s,%%): bad arg",OZ_toC(term),OZ_toC(fea));
  return 0;
}

/* -----------------------------------------------------------------
 * unification
 * -----------------------------------------------------------------*/

OZ_Bool OZ_unify(OZ_Term t1, OZ_Term t2)
{
  return am.unify(t1, t2) ? PROCEED : FAILED;
}

OZ_Term OZ_newVariable()
{
  return makeTaggedRef(newTaggedUVar(am.currentBoard));
}

/* -----------------------------------------------------------------
 * IO
 * -----------------------------------------------------------------*/

int OZ_select(int fd)
{
  return IO::setIORequest(fd) ? 1 : 0;
}

int OZ_openIO(int fd)
{
  IO::openIO(fd);
  return 1;
}

int OZ_closeIO(int fd)
{
  IO::closeIO(fd);
  return 1;
}

/* -----------------------------------------------------------------
 * garbage collection
 * -----------------------------------------------------------------*/

int OZ_protect(OZ_Term *t)
{
  if (!gcProtect(t)) {
    OZ_warning("protect: failed");
    return 0;
  }
  return 1;
}

int OZ_unprotect(OZ_Term *t)
{
  if (!gcUnprotect(t)) {
    OZ_warning("unprotect: failed");
    return 0;
  }
  return 1;
}

/* -----------------------------------------------------------------
 * free strings
 * -----------------------------------------------------------------*/

void OZ_free(char *s)
{
  delete [] s;
}

/* -----------------------------------------------------------------
 *
 * -----------------------------------------------------------------*/

int OZ_addBuiltin(char *name, int arity, OZ_CFun fun)
{
  return BIadd(name,arity,fun,OK) == NULL ? 0 : 1;
}

/* -----------------------------------------------------------------
 * Suspending builtins
 * -----------------------------------------------------------------*/

OZ_Suspension OZ_makeSuspension(OZ_Bool (*fun)(int,OZ_Term[]),
                                 OZ_Term *args,int arity)
{
  am.currentBoard->incSuspCount();
  return (OZ_Suspension)
    new Suspension(new CFuncContinuation(am.currentBoard,
                                         am.currentThread->getPriority(),
                                         fun, args, arity));
}

void OZ_addSuspension(OZ_Term var, OZ_Suspension susp)
{
  DEREF(var, varPtr, varTag);
  if (!isAnyVar(varTag)) {
    OZ_warning("OZ_addSuspension(%s): var arg expected",
               OZ_toC(var));
    return;
  }

  SVariable *svar = taggedBecomesSuspVar(varPtr);
  Suspension *s = (Suspension *) susp;

  svar->addSuspension(s);
}

/* -----------------------------------------------------------------
 *
 * -----------------------------------------------------------------*/

int OZ_onToplevel()
{
  return am.isToplevel() ? 1 : 0;
}
