/*
 *  Authors:
 *    Per Brand (perbrand@sics.se)
 *    Michael Mehl (mehl@dfki.de)
 *    Ralf Scheidhauer (Ralf.Scheidhauer@ps.uni-sb.de)
 *
 *  Contributors:
 *    Denys Duchier (duchier@ps.uni-sb.de)
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
 *     http://mozart.ps.uni-sb.de
 *
 *  See the file "LICENSE" or
 *     http://mozart.ps.uni-sb.de/LICENSE.html
 *  for information on usage and redistribution
 *  of this file, and for a DISCLAIMER OF ALL
 *  WARRANTIES.
 *
 */

#ifdef INTERFACE
#pragma implementation "marshaler.hh"
#endif

#ifdef HAVE_CONFIG_H
#include "conf.h"
#endif

#include "wsock.hh"

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <strings.h>
#include <ctype.h>
#include <stdarg.h>

#include "codearea.hh"
#include "indexing.hh"

#include "perdio.hh"
#include "perdio_debug.hh"
#include "genvar.hh"
#include "perdiovar.hh"
#include "gc.hh"
#include "dictionary.hh"
#include "urlc.hh"
#include "marshaler.hh"
#include "comm.hh"
#include "msgbuffer.hh"

/* *****************************************************************************
                       ORGANIZATION


            1  forward declarations
            2  global variables
            3  utility routines
            4  class RefTable RefTrail
            5  unmarshalHeader
            6  simple ground marshaling/unmarshaling
            7  gname marshaling/unmarshaling
            8  url marshaling/unmarshaling
            9  marshaling routines
            10 ConstTerm and term marshaling
            11 unmarshaling routines
            12 term unmarshaling
            13 perdiovar special
            14 full object/class marshaling/unmarshaling
            15 statistics
            16 initialization
            17 code marshaling
            18 Exported to marshalMsg.m4cc
            19 message marshaling

***************************************************************************** */

/* *********************************************************************/
/*   SECTION 1: forward declarations                                  */
/* *********************************************************************/

OZ_Term unmarshalTerm(MsgBuffer *);
void unmarshalUnsentTerm(MsgBuffer *);
void marshalTerm(OZ_Term, MsgBuffer *);
ProgramCounter unmarshalCode(MsgBuffer*,Bool);
void marshalVariable(PerdioVar *, MsgBuffer *);
SRecord *unmarshalSRecord(MsgBuffer *);
void unmarshalUnsentSRecord(MsgBuffer *);
void unmarshalTerm(MsgBuffer *, OZ_Term *);
int unmarshalUnsentNumber(MsgBuffer *bs);

#define CheckD0Compatibility \
   if (ozconf.perdiod0Compatiblity) goto bomb;


/* *********************************************************************/
/*   SECTION 2: global variables                                       */
/* *********************************************************************/

SendRecvCounter dif_counter[DIF_LAST];
SendRecvCounter misc_counter[MISC_LAST];

char *misc_names[MISC_LAST] = {
  "string",
  "gname",
  "site"
};

/* *********************************************************************/
/*   SECTION 3: utility routines                                       */
/* *********************************************************************/


/* *********************************************************************/
/*   SECTION 4: classes RefTable RefTrail                              */
/* *********************************************************************/

class RefTable {
  OZ_Term *array;
  int size;
  int pos;
public:
  RefTable()
  {
    pos = 0;
    size = 100;
    array = new OZ_Term[size];
  }
  void reset() { pos=0; }
  OZ_Term get(int i)
  {
    Assert(i<size);
    return array[i];
  }
  int set(OZ_Term val)
  {
    if (pos>=size)
      resize(pos);
    array[pos] = val;
    return pos++;
  }
  void resize(int newsize)
  {
    int oldsize = size;
    OZ_Term  *oldarray = array;
    while(size <= newsize) {
      size = (size*3)/2;
    }
    array = new OZ_Term[size];
    for (int i=0; i<oldsize; i++) {
      array[i] = oldarray[i];
    }
    delete oldarray;
  }
  DebugCode(int getPos() { return pos; })
};

RefTable *refTable;

#ifdef DEBUG_REFCOUNTERS
#define MagicConst 23
#endif


inline void gotRef(MsgBuffer *bs, TaggedRef val)
{
  int counter = refTable->set(val);
  PD((REF_COUNTER,"got: %d",counter));
#ifdef DEBUG_REFCOUNTERS
  int n1 = unmarshalNumber(bs);
  Assert(n1==MagicConst);
  int n2 = unmarshalNumber(bs);
  Assert(n2==counter);
#endif
}



/*

  RefTrail
  Problem: there is no room in lists to remember, that they have
  been visited already: first element might be a variable which was bound
  --> we might have REF cells pointing to the beginning of the list, so we
  run into problems if the list is _first_ marshalled.
  Solution: for lists we do not mark the datastructure but remember a
  pointer to it on the refTrail together with its counter value.

 */

#define RT_LISTTAG 0x1

class RefTrail: public Stack {
  int counter;

public:
  int getCounter() { return counter; }

  RefTrail() : Stack(200,Stack_WithMalloc) { counter=0; }
  void pushInt(int i) { push(ToPointer(i)); }
  int trail(OZ_Term *t)
  {
    pushInt(*t);
    push(t);
    return counter++;
  }
  int trail(LTuple *l)
  {
    Assert(find(l)==-1);
    pushInt(counter++);
    pushInt(ToInt32(l)|RT_LISTTAG);
    return counter-1;
  }

  int find(LTuple *l)
  {
    int ret = -1;
    StackEntry *savedTop = tos;

    while(!isEmpty()) {
      unsigned int l1 = ToInt32(pop());
      int n = ToInt32(pop());
      if ((l1&RT_LISTTAG) && l1==(ToInt32(l)|RT_LISTTAG)) {
        ret = n;
        break;
      }
    }
    tos = savedTop;
    return ret;
  }

  void unwind()
  {
    while(!isEmpty()) {
      OZ_Term *loc = (OZ_Term*) pop();
      OZ_Term oldval = ToInt32(pop());
      if ((ToInt32(loc)&RT_LISTTAG)==0) {
        *loc = oldval;
      }
      counter--;
    }
    Assert(counter==0);
  }
};

RefTrail *refTrail;

/* *********************************************************************/
/*   SECTION 5: unmarshalHeader                                        */
/* *********************************************************************/

MessageType unmarshalHeader(MsgBuffer *bs){
  bs->unmarshalBegin();
  refTable->reset();
  MessageType mt= (MessageType) bs->get();
  mess_counter[mt].recv();
  return mt;}

/* *********************************************************************/
/*   SECTION 6:  simple ground marshaling/unmarshaling                 */
/* *********************************************************************/

unsigned short unmarshalShort(MsgBuffer *bs){
  unsigned short sh;
  unsigned int i1 = bs->get();
  unsigned int i2 = bs->get();
  sh= (i1 + (i2<<8));
  PD((UNMARSHAL_CT,"Short %d BYTES:2",sh));
  return sh;}

class DoubleConv {
public:
  union {
    unsigned char c[sizeof(double)];
    int i[sizeof(double)/sizeof(int)];
    double d;
  } u;
};

Bool isLowEndian()
{
  DoubleConv dc;
  dc.u.i[0] = 1;
  return dc.u.c[0] == 1;
}

const Bool lowendian = isLowEndian();

void marshalFloat(double d, MsgBuffer *bs)
{
  static DoubleConv dc;
  dc.u.d = d;
  if (lowendian) {
    marshalNumber(dc.u.i[0],bs);
    marshalNumber(dc.u.i[1],bs);
  } else {
    marshalNumber(dc.u.i[1],bs);
    marshalNumber(dc.u.i[0],bs);
  }
}

double unmarshalFloat(MsgBuffer *bs)
{
  static DoubleConv dc;
  if (lowendian) {
    dc.u.i[0] = unmarshalNumber(bs);
    dc.u.i[1] = unmarshalNumber(bs);
  } else {
    dc.u.i[1] = unmarshalNumber(bs);
    dc.u.i[0] = unmarshalNumber(bs);
  }
  return dc.u.d;
}

char *unmarshalString(MsgBuffer *bs)
{
  misc_counter[MISC_STRING].recv();
  int i = unmarshalNumber(bs);

  char *ret = new char[i+1];
  for (int k=0; k<i; k++) {
    ret[k] = bs->get();
  }
  PD((UNMARSHAL_CT,"String BYTES:%d",i));
  ret[i] = '\0';
  return ret;
}

void unmarshalUnsentString(MsgBuffer *bs)
{
  char *aux = unmarshalString(bs);
  delete [] aux;
}



/* *********************************************************************/
/*   SECTION 7: gname marshaling/unmarshaling                          */
/* *********************************************************************/

void marshalGName(GName *gname, MsgBuffer *bs)
{
  misc_counter[MISC_GNAME].send();
  PD((MARSHAL,"gname: s:%s", gname->site->stringrep()));
  gname->site->marshalPSite(bs);
  for (int i=0; i<fatIntDigits; i++) {
    PD((MARSHAL,"gname: id%d:%u", i,gname->id.number[i]));
    marshalNumber(gname->id.number[i],bs);
  }
  marshalNumber((int)gname->gnameType,bs);
}

void unmarshalGName1(GName *gname, MsgBuffer *bs)
{
  gname->site=unmarshalPSite(bs);
  PD((UNMARSHAL,"gname: s:%s", gname->site->stringrep()));
  for (int i=0; i<fatIntDigits; i++) {
    gname->id.number[i] = unmarshalNumber(bs);
    PD((MARSHAL,"gname: id%d:%u", i, gname->id.number[i]));
  }
  gname->gnameType = (GNameType) unmarshalNumber(bs);
  PD((UNMARSHAL,"gname finish type:%d", gname->gnameType));
}

GName *unmarshalGName(TaggedRef *ret, MsgBuffer *bs)
{
  PD((UNMARSHAL,"gname"));
  misc_counter[MISC_GNAME].recv();
  GName gname;
  unmarshalGName1(&gname,bs);

  TaggedRef aux = findGNameOutline(&gname);
  if (aux) {
    if (ret) *ret = aux; // ATTENTION
    return 0;
  }
  return new GName(gname);
}

/* *********************************************************************/
/*   SECTION 9: term marshaling routines                               */
/* *********************************************************************/

int debugRefs = 0;

void marshalRef(int n, MsgBuffer *bs, TypeOfTerm tag)
{
  PD((MARSHAL,"circular: %d",n));

  if (debugRefs && tag!=REFTAG1) {
    marshalDIF(bs,DIF_REF_DEBUG);
    marshalNumber(n,bs);
    marshalNumber(tag,bs);
  } else {
    marshalDIF(bs,DIF_REF);
    marshalNumber(n,bs);
  }
}

inline Bool checkCycle(OZ_Term t, MsgBuffer *bs, TypeOfTerm tag)
{
  if ((t&tagMask)==GCTAG) {
    marshalRef(t>>tagSize,bs,tag);
    return OK;
  }
  return NO;
}


#define Comment(Args) if (bs->textmode()) {comment Args;}

void comment(MsgBuffer *bs, const char *format, ...)
{
  char buf[10000];
  va_list ap;
  va_start(ap,format);
  vsprintf(buf,format,ap);
  va_end(ap);
  putComment(buf,bs);
}




inline void trailCycle(OZ_Term *t, MsgBuffer *bs)
{
  int counter = refTrail->trail(t);
  Comment((bs,"DEFREF %d",counter));
  PD((REF_COUNTER,"trail: %d",counter));
  *t = ((counter)<<tagSize)|GCTAG;
#ifdef DEBUG_REFCOUNTERS
  marshalNumber(MagicConst,bs);
  marshalNumber(counter,bs);
#endif
}

inline Bool checkCycle(LTuple *l, MsgBuffer *bs)
{
  int n = refTrail->find(l);
  if (n>=0) {
    marshalRef(n,bs,LTUPLE);
    return OK;
  }
  return NO;
}

inline void trailCycle(LTuple *l, MsgBuffer *bs)
{
  int counter = refTrail->trail(l);
  Comment((bs,"DEFREF %d",counter));
#ifdef DEBUG_REFCOUNTERS
  marshalNumber(MagicConst,bs);
  marshalNumber(counter,bs);
#endif
}

void marshalSRecord(SRecord *sr, MsgBuffer *bs)
{
  TaggedRef t = oz_nil();
  if (sr) {
    t = makeTaggedSRecord(sr);
  }
  marshalTerm(t,bs);
}

void marshalClass(ObjectClass *cl, MsgBuffer *bs)
{
  marshalDIF(bs,DIF_CLASS);
  marshalGName(cl->getGName(),bs);
  trailCycle(cl->getCycleRef(),bs);
  marshalSRecord(cl->getFeatures(),bs);
}



void marshalNoGood(TaggedRef term, MsgBuffer *bs)
{
  bs->addNogood(term);
  marshalTerm(NameNonExportable,bs); // to make bs consistent
}



void marshalObject(Object *o, MsgBuffer *bs, GName *gnclass)
{
  if (marshalTertiary(o,DIF_OBJECT,bs)) return;   /* ATTENTION */
  trailCycle(o->getCycleRef(),bs);
  Assert(o->hasGName());
  marshalGName(o->hasGName(),bs);
  marshalGName(gnclass,bs);
}

/* *********************************************************************/
/*   SECTION 10: ConstTerm and Term marshaling                          */
/* *********************************************************************/

void marshalConst(ConstTerm *t, MsgBuffer *bs)
{
  switch (t->getType()) {
  case Co_BigInt:
    PD((MARSHAL,"bigint"));
    marshalDIF(bs,DIF_BIGINT);
    marshalString(toC(makeTaggedConst(t)),bs);
    return;

  case Co_Dictionary:
    {
      PD((MARSHAL,"dictionary"));
      OzDictionary *d = (OzDictionary *) t;

      if (!d->isSafeDict()) {
        goto bomb;
      }

      marshalDIF(bs,DIF_DICT);
      int size = d->getSize();
      marshalNumber(size,bs);
      trailCycle(d->getCycleRef(),bs);

      int i = d->getFirst();
      i = d->getNext(i);
      while(i>=0) {
        marshalTerm(d->getKey(i),bs);
        marshalTerm(d->getValue(i),bs);
        i = d->getNext(i);
        size--;
      }
      return;
    }

  case Co_Builtin:
    {
      PD((MARSHAL,"builtin"));
      marshalDIF(bs,DIF_BUILTIN);
      PD((MARSHAL_CT,"tag DIF_BUILTIN BYTES:1"));
      Builtin *bi= (Builtin *)t;

      if (bi->isNative())
        goto bomb;

      marshalString(bi->getPrintName(),bs);
      break;
    }
  case Co_Chunk:
    {
      PD((MARSHAL,"chunk"));
      SChunk *ch=(SChunk *) t;
      GName *gname=ch->getGName();
      marshalDIF(bs,DIF_CHUNK);
      marshalGName(gname,bs);
      trailCycle(t->getCycleRef(),bs);
      marshalTerm(ch->getValue(),bs);
      return;
    }
  case Co_Class:
    {
      PD((MARSHAL,"class"));
      ObjectClass *cl = (ObjectClass*) t;
      if (cl->isNative())
        goto bomb;

      cl->globalize();
      marshalClass(cl,bs);
      return;
    }
  case Co_Abstraction:
    {
      PD((MARSHAL,"abstraction"));
      Abstraction *pp=(Abstraction *) t;
      if (pp->getPred()->isNative())
        goto bomb;

      GName *gname=pp->getGName();

      marshalDIF(bs,DIF_PROC);
      marshalGName(gname,bs);

      marshalTerm(pp->getName(),bs);
      marshalNumber(pp->getArity(),bs);
      ProgramCounter pc = pp->getPC();
      int gs = pp->getPred()->getGSize();
      marshalNumber(gs,bs);
      marshalNumber(pp->getPred()->getMaxX(),bs);
      trailCycle(t->getCycleRef(),bs);
      for (int i=0; i<gs; i++) {
        marshalTerm(pp->getG(i),bs);
      }

      PD((MARSHAL,"code begin"));
      marshalCode(pc,bs);
      PD((MARSHAL,"code end"));
      return;
    }

  case Co_Object:
    {
      CheckD0Compatibility;

      PD((MARSHAL,"object"));
      Object *o = (Object*) t;
      ObjectClass *oc = o->getClass();
      oc->globalize();
      o->globalize();
      bs->addRes(makeTaggedConst(t));
      marshalObject(o,bs,oc->getGName());
      return;
    }
  case Co_Lock:
    CheckD0Compatibility;

    PD((MARSHAL,"lock"));
    bs->addRes(makeTaggedConst(t));
    if (marshalTertiary((Tertiary *) t,DIF_LOCK,bs)) return;
    break;

  case Co_Cell:
    CheckD0Compatibility;

    PD((MARSHAL,"cell"));
    bs->addRes(makeTaggedConst(t));
    if (marshalTertiary((Tertiary *) t,DIF_CELL,bs)) return;
    break;

  case Co_Port:
    PD((MARSHAL,"port"));
    bs->addRes(makeTaggedConst(t));
    if (marshalTertiary((Tertiary *) t,DIF_PORT,bs)) return;
    break;

  default:
    goto bomb;
  }

  trailCycle(t->getCycleRef(),bs);
  return;

bomb:
  marshalNoGood(makeTaggedConst(t),bs);
}

void marshalTerm(OZ_Term t, MsgBuffer *bs)
{
  int depth = 0;
loop:
  DEREF(t,tPtr,tTag);
  PD((MARSHAL,"tag:%d",tTag));
  switch(tTag) {

  case SMALLINT:
    PD((MARSHAL,"small int: %d",smallIntValue(t)));
    marshalDIF(bs,DIF_SMALLINT);
    marshalNumber(smallIntValue(t),bs);
    break;

  case OZFLOAT:
    PD((MARSHAL,"float"));
    marshalDIF(bs,DIF_FLOAT);
    marshalFloat(tagged2Float(t)->getValue(),bs);
    break;

  case LITERAL:
    {
      PD((MARSHAL,"literal"));
      Literal *lit = tagged2Literal(t);
      /* make things more readbale (less refs) in textmode */
      if (0 && bs->textmode() && lit->isAtom()) {
        marshalDIF(bs,DIF_ATOMNOREF);
        marshalString(lit->getPrintName(),bs);
        break;
      }
      if (checkCycle(*lit->getCycleRef(),bs,tTag)) goto exit;

      if (lit->isAtom()) {
        marshalDIF(bs,DIF_ATOM);
        PD((MARSHAL_CT,"tag DIF_ATOM  BYTES:1"));
        marshalString(lit->getPrintName(),bs);
        PD((MARSHAL,"atom: %s",lit->getPrintName()));
      } else if (lit->isUniqueName()) {
        marshalDIF(bs,DIF_UNIQUENAME);
        marshalString(lit->getPrintName(),bs);
        PD((MARSHAL,"unique name: %s",lit->getPrintName()));
      } else if (lit->isCopyableName()) {
        marshalDIF(bs,DIF_COPYABLENAME);
        marshalString(lit->getPrintName(),bs);
        PD((MARSHAL,"copyable name: %s",lit->getPrintName()));
      } else {
        marshalDIF(bs,DIF_NAME);
        GName *gname = ((Name*)lit)->globalize();
        marshalGName(gname,bs);
        marshalString(lit->getPrintName(),bs);
        PD((MARSHAL,"name: %s",lit->getPrintName()));
      }
      trailCycle(lit->getCycleRef(),bs);
      break;
    }

  case LTUPLE:
    {
      depth++; Comment((bs,"("));
      PD((MARSHAL,"ltuple"));
      LTuple *l = tagged2LTuple(t);
      if (checkCycle(l,bs)) goto exit;
      marshalDIF(bs,DIF_LIST);
      PD((MARSHAL_CT,"tag DIF_LIST BYTES:1"));
      PD((MARSHAL,"list"));

      trailCycle(l,bs);
      marshalTerm(l->getHead(),bs);

      // tail recursion optimization
      t = l->getTail();
      goto loop;
    }

  case SRECORD:
    {
      depth++; Comment((bs,"("));
      PD((MARSHAL,"srecord"));
      SRecord *rec = tagged2SRecord(t);
      if (checkCycle(*rec->getCycleAddr(),bs,tTag)) goto exit;
      TaggedRef label = rec->getLabel();

      if (rec->isTuple()) {
        marshalDIF(bs,DIF_TUPLE);
        PD((MARSHAL_CT,"tag DIF_TUPLE BYTES:1"));
        marshalNumber(rec->getTupleWidth(),bs);
      } else {
        marshalDIF(bs,DIF_RECORD);
        PD((MARSHAL_CT,"tag DIF_RECORD BYTES:1"));
        marshalTerm(rec->getArityList(),bs);
      }
      marshalTerm(label,bs);
      trailCycle(rec->getCycleAddr(),bs);
      int argno = rec->getWidth();
      PD((MARSHAL,"record-tuple no:%d",argno));

      for(int i=0; i<argno-1; i++) {
        marshalTerm(rec->getArg(i),bs);
      }
      // tail recursion optimization
      t = rec->getArg(argno-1);
      goto loop;
    }

  case OZCONST:
    {
      PD((MARSHAL,"constterm"));
      if (!checkCycle(*(tagged2Const(t)->getCycleRef()),bs,tTag)) {
        Comment((bs,"("));
        marshalConst(tagged2Const(t),bs);
        Comment((bs,")"));
      }
      break;
    }

  case FSETVALUE:
    {
      CheckD0Compatibility;

      PD((MARSHAL,"finite set value"));
      OZ_FSetValue * fsetval = tagged2FSetValue(t);
      marshalDIF(bs,DIF_FSETVALUE);
      // tail recursion optimization
      t = fsetval->getKnownInList();
      goto loop;
    }

  case UVAR:
  case SVAR:
  case CVAR:
    {
      PerdioVar *pvar = var2PerdioVar(tPtr);
      if (pvar==NULL) {
        t = makeTaggedRef(tPtr);
        goto bomb;
      }
      pvar->markExported();
      bs->addRes(makeTaggedRef(tPtr));
      marshalVariable(pvar,bs);
      break;
    }

  default:
  bomb:
    marshalNoGood(t,bs);
    break;
  }

 exit:
  while(depth--) {
    Comment((bs,")"));
  }
  return;
}

/* *********************************************************************/
/*   SECTION 11: term unmarshaling routines                            */
/* *********************************************************************/

void unmarshalDict(MsgBuffer *bs, TaggedRef *ret)
{
  int size = unmarshalNumber(bs);
  PD((UNMARSHAL,"dict size:%d",size));
  Assert(oz_onToplevel());
  OzDictionary *aux = new OzDictionary(am.currentBoard(),size);
  aux->markSafe();
  *ret = makeTaggedConst(aux);
  gotRef(bs,*ret);

  while(size-- > 0) {
    TaggedRef key = unmarshalTerm(bs);
    TaggedRef val = unmarshalTerm(bs);
    aux->setArg(key,val);
  }
  return;
}

void unmarshalObject(ObjectFields *o, MsgBuffer *bs){
  o->feat = unmarshalSRecord(bs);
  o->state=unmarshalTerm(bs);
  o->lock=unmarshalTerm(bs);}

void fillInObject(ObjectFields *of, Object *o){
  o->setFreeRecord(of->feat);
  o->setState(tagged2Tert(of->state));
  o->setLock(oz_isNil(of->lock) ? (LockProxy*)NULL : (LockProxy*)tagged2Tert(of->lock));}

void unmarshalUnsentObject(MsgBuffer *bs){
  unmarshalUnsentSRecord(bs);
  unmarshalUnsentTerm(bs);
  unmarshalUnsentTerm(bs);}

void unmarshalObjectAndClass(ObjectFields *o, MsgBuffer *bs){
  unmarshalObject(o,bs);
  o->clas = unmarshalTerm(bs);}

void unmarshalUnsentObjectAndClass(MsgBuffer *bs){
  unmarshalUnsentObject(bs);
  unmarshalUnsentTerm(bs);}

void fillInObjectAndClass(ObjectFields *of, Object *o){
  fillInObject(of,o);
  o->setClass(tagged2ObjectClass(of->clas));}

void unmarshalClass(ObjectClass *cl, MsgBuffer *bs)
{
  SRecord *feat = unmarshalSRecord(bs);

  if (cl==NULL)  return;

  TaggedRef ff = feat->getFeature(NameOoUnFreeFeat);
  Bool locking = literalEq(NameTrue,oz_deref(feat->getFeature(NameOoLocking)));

  cl->import(feat,
             tagged2Dictionary(feat->getFeature(NameOoFastMeth)),
             oz_isSRecord(ff) ? tagged2SRecord(ff) : (SRecord*)NULL,
             tagged2Dictionary(feat->getFeature(NameOoDefaults)),
             locking);
}

OZ_Term unmarshalTerm(MsgBuffer *bs)
{
  OZ_Term ret;
  unmarshalTerm(bs,&ret);
  return ret;
}

inline
ObjectClass *newClass(GName *gname) {
  Assert(oz_onToplevel());
  ObjectClass *ret = new ObjectClass(NULL,NULL,NULL,NULL,NO,NO,am.currentBoard());
  ret->setGName(gname);
  return ret;
}

SRecord *unmarshalSRecord(MsgBuffer *bs){
  TaggedRef t = unmarshalTerm(bs);
  return oz_isNil(t) ? (SRecord*)NULL : tagged2SRecord(t);
}

void unmarshalUnsentSRecord(MsgBuffer *bs){
  unmarshalUnsentTerm(bs);}

/* *********************************************************************/
/*   SECTION 12: term unmarshaling                                     */
/* *********************************************************************/

void unmarshalTerm(MsgBuffer *bs, OZ_Term *ret)
{
loop:
  MarshalTag tag = (MarshalTag) bs->get();
  PD((UNMARSHAL,"term tag:%s %d",dif_names[(int) tag].name,tag));

  dif_counter[tag].recv();
  switch(tag) {

  case DIF_SMALLINT:
    *ret = OZ_int(unmarshalNumber(bs));
    PD((UNMARSHAL,"small int %d",smallIntValue(*ret)));
    return;

  case DIF_FLOAT:
    *ret = OZ_float(unmarshalFloat(bs));
    PD((UNMARSHAL,"float"));
    return;

  case DIF_NAME:
    {
      GName *gname    = unmarshalGName(ret,bs);
      char *printname = unmarshalString(bs);

      PD((UNMARSHAL,"name %s",printname));

      if (gname) {
        Name *aux;
        if (strcmp("",printname)==0) {
          aux = Name::newName(am.currentBoard());
        } else {
          aux = NamedName::newNamedName(ozstrdup(printname));
        }
        aux->import(gname);
        *ret = makeTaggedLiteral(aux);
        addGName(gname,*ret);
      }
      delete printname;
      gotRef(bs,*ret);
      return;
    }

  case DIF_COPYABLENAME:
    {
      char *printname = unmarshalString(bs);

      NamedName *aux = NamedName::newNamedName(ozstrdup(printname));
      aux->setFlag(Lit_isCopyableName);
      *ret = makeTaggedLiteral(aux);
      delete printname;
      gotRef(bs,*ret);
      return;
    }

  case DIF_UNIQUENAME:
    {
      char *printname = unmarshalString(bs);

      PD((UNMARSHAL,"unique name %s",printname));

      *ret = getUniqueName(printname);
      delete printname;
      gotRef(bs,*ret);
      return;
    }

  case DIF_ATOM:
  case DIF_ATOMNOREF:
    {
      char *aux = unmarshalString(bs);
      PD((UNMARSHAL,"atom %s",aux));
      *ret = OZ_atom(aux);
      delete aux;
      if (tag==DIF_ATOM)
        gotRef(bs,*ret);
      return;
    }

  case DIF_BIGINT:
    {
      char *aux = unmarshalString(bs);
      PD((UNMARSHAL,"big int %s",aux));
      *ret = OZ_CStringToNumber(aux);
      delete aux;
      return;
    }

  case DIF_LIST:
    {
      PD((UNMARSHAL,"list"));
      LTuple *l = new LTuple();
      *ret = makeTaggedLTuple(l);
      gotRef(bs,*ret);
      unmarshalTerm(bs,l->getRefHead());
      // tail recursion optimization
      ret = l->getRefTail();
      goto loop;
    }
  case DIF_TUPLE:
    {
      int argno = unmarshalNumber(bs);
      PD((UNMARSHAL,"tuple no_args:%d",argno));
      TaggedRef label = unmarshalTerm(bs);
      SRecord *rec = SRecord::newSRecord(label,argno);
      *ret = makeTaggedSRecord(rec);
      gotRef(bs,*ret);

      for(int i=0; i<argno-1; i++) {
        unmarshalTerm(bs,rec->getRef(i));
      }
      // tail recursion optimization
      ret = rec->getRef(argno-1);
      goto loop;
    }

  case DIF_RECORD:
    {
      TaggedRef arity = unmarshalTerm(bs);
      TaggedRef sortedarity = arity;
      if (!isSorted(arity)) {
        int len;
        TaggedRef aux = duplist(arity,len);
        sortedarity = sortlist(aux,len);
      }
      PD((UNMARSHAL,"record no:%d",oz_fastlength(arity)));
      TaggedRef label = unmarshalTerm(bs);
      SRecord *rec    = SRecord::newSRecord(label,aritytable.find(sortedarity));
      *ret = makeTaggedSRecord(rec);
      gotRef(bs,*ret);

      while(oz_isCons(arity)) {
        TaggedRef val = unmarshalTerm(bs);
        rec->setFeature(oz_head(arity),val);
        arity = oz_tail(arity);
      }
      return;
    }

  case DIF_REF:
    {
      int i = unmarshalNumber(bs);
      PD((UNMARSHAL,"ref: %d",i));
      *ret = refTable->get(i);
      Assert(*ret);
      return;
    }

  case DIF_REF_DEBUG:
    {
      int i          = unmarshalNumber(bs);
      TypeOfTerm tag = (TypeOfTerm) unmarshalNumber(bs);
      PD((UNMARSHAL,"ref: %d",i));
      *ret = refTable->get(i);
      Assert(*ret);
      Assert(tag==tagTypeOf(*ret));
      return;
    }

  case DIF_OWNER:
  case DIF_OWNER_SEC:
    {
      *ret=unmarshalOwner(bs,tag);
      return;
    }
  case DIF_PORT:
  case DIF_THREAD:
  case DIF_SPACE:
  case DIF_CELL:
  case DIF_LOCK:
  case DIF_OBJECT:
    {
      *ret=unmarshalTertiary(bs,tag);
      gotRef(bs,*ret);
      return;}
  case DIF_CHUNK:
    {
      PD((UNMARSHAL,"chunk"));

      GName *gname=unmarshalGName(ret,bs);

      SChunk *sc;
      if (gname) {
        Assert(oz_onToplevel());
        sc=new SChunk(am.currentBoard(),0);
        sc->setGName(gname);
        *ret = makeTaggedConst(sc);
        addGName(gname,*ret);
      } else {
        // mm2: share the follwing code DIF_CHUNK, DIF_CLASS, DIF_PROC!
        Assert(oz_isSChunk(oz_deref(*ret)));
        sc = 0;
      }
      gotRef(bs,*ret);
      TaggedRef value = unmarshalTerm(bs);
      if (sc) sc->import(value);
      return;
    }

  case DIF_CLASS:
    {
      PD((UNMARSHAL,"class"));

      GName *gname=unmarshalGName(ret,bs);

      ObjectClass *cl;
      if (gname) {
        cl = newClass(gname);
        *ret = makeTaggedConst(cl);
        addGName(gname,*ret);
      } else {
        Assert(oz_isClass(oz_deref(*ret)));
        cl = 0;
      }
      gotRef(bs,*ret);
      unmarshalClass(cl,bs);
      return;
    }



  case DIF_VAR:
    {
      *ret=unmarshalVar(bs);
      return;
    }

  case DIF_PROC:
    {
      PD((UNMARSHAL,"proc"));

      GName *gname  = unmarshalGName(ret,bs);
      OZ_Term name  = unmarshalTerm(bs);
      int arity     = unmarshalNumber(bs);
      int gsize     = unmarshalNumber(bs);
      int maxX      = unmarshalNumber(bs);

      if (gname) {
        PrTabEntry *pr = new PrTabEntry(name,mkTupleWidth(arity),0,0,0,
                                        oz_nil(), maxX);
        Assert(oz_onToplevel());
        pr->setGSize(gsize);
        Abstraction *pp = Abstraction::newAbstraction(pr,am.currentBoard());
        *ret = makeTaggedConst(pp);
        pp->setGName(gname);
        addGName(gname,*ret);
        gotRef(bs,*ret);
        for (int i=0; i<gsize; i++) {
          pp->initG(i, unmarshalTerm(bs));
        }
        pr->PC=unmarshalCode(bs,NO);
        pr->patchFileAndLine();
      } else {
        Assert(oz_isAbstraction(oz_deref(*ret)));
        gotRef(bs,*ret);
        for (int i=0; i<gsize; i++) {
          (void) unmarshalTerm(bs);
        }
        (void) unmarshalCode(bs,OK);
      }
      return;
    }

  case DIF_DICT:
    {
      PD((UNMARSHAL,"dict"));
      unmarshalDict(bs,ret);
      return;
    }
  case DIF_ARRAY:
    {
      PD((UNMARSHAL,"array"));
      warning("unmarshal array not impl");  // mm2
      return;
    }
  case DIF_BUILTIN:
    {
      char *name = unmarshalString(bs); // ATTENTION deletion
      PD((UNMARSHAL,"builtin: %s",name));
      Builtin * found = string2Builtin(name);

      if (!found) {
        warning("Builtin '%s' not in table.", name);
        *ret = oz_nil();
        return;
      }

      if (found->isNative()) {
        warning("Unpickling sited builtin: '%s'", name);
      }

      *ret = makeTaggedConst(found);
      gotRef(bs,*ret);
      return;
    }

  case DIF_FSETVALUE:
    {
      PD((UNMARSHAL,"finite set value"));
      OZ_Term glb=unmarshalTerm(bs);
      extern void makeFSetValue(OZ_Term,OZ_Term*);
      makeFSetValue(glb,ret);
      return;
    }

  default:
    error("unmarshal: unexpected tag: %d\n",tag);
    Assert(0);
    *ret = oz_nil();
    return;
  }

  Assert(0);
}

void unmarshalUnsentTerm(MsgBuffer *bs) {
  OZ_Term t=unmarshalTerm(bs);}

/* *********************************************************************/
/*   SECTION 13: perdiovar - special                                  */
/* *********************************************************************/

void marshalVariable(PerdioVar *pvar, MsgBuffer *bs)
{
  if((pvar->isProxy()) || pvar->isManager()) {
    marshalVar(pvar,bs);
    return;}

  Assert(pvar->isObject());

  PD((MARSHAL,"var objectproxy"));

  if (checkCycle(*(pvar->getObject()->getCycleRef()),bs,OZCONST))
    return;

  GName *classgn =  pvar->isObjectClassAvail() ?
                      pvar->getClass()->getGName() : pvar->getGNameClass();

  marshalObject(pvar->getObject(),bs,classgn);
  return;
}

/* *********************************************************************/
/*   SECTION 14: full object/class unmarshaling/marshaling             */
/* *********************************************************************/

void marshalFullObject(Object *o,MsgBuffer* bs){
  PD((MARSHAL,"full object"));
  marshalSRecord(o->getFreeRecord(),bs);
  marshalTerm(makeTaggedConst(getCell(o->getState())),bs);
  if (o->getLock()) {marshalTerm(makeTaggedConst(o->getLock()),bs);}
  else {marshalTerm(oz_nil(),bs);}}

void marshalFullObjectAndClass(Object *o,MsgBuffer* bs){
  PD((MARSHAL,"full object and class"));
  ObjectClass *oc=o->getClass();
  marshalFullObject(o,bs);
  marshalClass(oc,bs);}

/* *********************************************************************/
/*   SECTION 15: statistics                                            */
/* *********************************************************************/

#ifdef MISC_BUILTINS

OZ_BI_define(BIperdioStatistics,0,1)
{
  OZ_Term dif_send_ar=oz_nil();
  OZ_Term dif_recv_ar=oz_nil();
  int i;
  for (i=0; i<DIF_LAST; i++) {
    dif_send_ar=oz_cons(oz_pairAI(dif_names[i].name,dif_counter[i].getSend()),
                        dif_send_ar);
    dif_recv_ar=oz_cons(oz_pairAI(dif_names[i].name,dif_counter[i].getRecv()),
                        dif_recv_ar);
  }
  OZ_Term dif_send=OZ_recordInit(oz_atom("dif"),dif_send_ar);
  OZ_Term dif_recv=OZ_recordInit(oz_atom("dif"),dif_recv_ar);

  OZ_Term misc_send_ar=oz_nil();
  OZ_Term misc_recv_ar=oz_nil();
  for (i=0; i<MISC_LAST; i++) {
    misc_send_ar=oz_cons(oz_pairAI(misc_names[i],misc_counter[i].getSend()),
                         misc_send_ar);
    misc_recv_ar=oz_cons(oz_pairAI(misc_names[i],misc_counter[i].getRecv()),
                         misc_recv_ar);
  }
  OZ_Term misc_send=OZ_recordInit(oz_atom("misc"),misc_send_ar);
  OZ_Term misc_recv=OZ_recordInit(oz_atom("misc"),misc_recv_ar);

  OZ_Term mess_send_ar=oz_nil();
  OZ_Term mess_recv_ar=oz_nil();
  for (i=0; i<M_LAST; i++) {
    mess_send_ar=oz_cons(oz_pairAI(mess_names[i],mess_counter[i].getSend()),
                         mess_send_ar);
    mess_recv_ar=oz_cons(oz_pairAI(mess_names[i],mess_counter[i].getRecv()),
                         mess_recv_ar);
  }
  OZ_Term mess_send=OZ_recordInit(oz_atom("messages"),mess_send_ar);
  OZ_Term mess_recv=OZ_recordInit(oz_atom("messages"),mess_recv_ar);


  OZ_Term send_ar=oz_nil();
  send_ar = oz_cons(oz_pairA("dif",dif_send),send_ar);
  send_ar = oz_cons(oz_pairA("misc",misc_send),send_ar);
  send_ar = oz_cons(oz_pairA("messages",mess_send),send_ar);
  OZ_Term send=OZ_recordInit(oz_atom("send"),send_ar);

  OZ_Term recv_ar=oz_nil();
  recv_ar = oz_cons(oz_pairA("dif",dif_recv),recv_ar);
  recv_ar = oz_cons(oz_pairA("misc",misc_recv),recv_ar);
  recv_ar = oz_cons(oz_pairA("messages",mess_recv),recv_ar);
  OZ_Term recv=OZ_recordInit(oz_atom("recv"),recv_ar);


  OZ_Term ar=oz_nil();
  ar=oz_cons(oz_pairA("send",send),ar);
  ar=oz_cons(oz_pairA("recv",recv),ar);
  OZ_RETURN(OZ_recordInit(oz_atom("perdioStatistics"),ar));
} OZ_BI_end

#endif

/* *********************************************************************/
/*   SECTION 16: initialization                                       */
/* *********************************************************************/

void initMarshaler(){
  refTable = new RefTable();
  refTrail = new RefTrail();
}


static
void splitversion(char *vers, char *&major, char*&minor)
{
  major = vers;
  minor = strrchr(vers,'#');
  if (minor) {
    minor++;
  } else {
    minor = "0";
  }
}


Bool unmarshal_SPEC(MsgBuffer* buf,char* &vers,OZ_Term &t){
  PD((MARSHAL_BE,"unmarshal begin: %s s:%s","$1",buf->siteStringrep()));
  refTable->reset();
  Assert(creditSite==NULL);
  Assert(refTrail->isEmpty());
  if(buf->get()==DIF_SECONDARY) {Assert(0);return NO;}
  vers=unmarshalString(buf);
  char *major, *minor;
  splitversion(vers,major,minor);
  if (strncmp(PERDIOMAJOR,major,strlen(PERDIOMAJOR))!=0) {
    return NO;}
  int minordiff = atoi(PERDIOMINOR) - atoi(minor);
  //  if (minordiff > 1 || /* we only support the last minor */
  //      minordiff < 0) { /* emulator older than component */
  //    return NO;
  //  }
  if (minordiff)
    return NO;
  t=unmarshalTerm(buf);
  buf->unmarshalEnd();
  refTrail->unwind();
  PD((MARSHAL_BE,"unmarshal end: %s s:%s","$1",buf->siteStringrep()));
  return OK;}


/* *********************************************************************/
/*   SECTION 17: code unmarshaling/marshaling                          */
/* *********************************************************************/

#include "marshalcode.cc"

/* *********************************************************************/
/*   SECTION 18: Exported to marshalMsg.m4cc                         */
/* *********************************************************************/

void marshalFullObjectRT(Object *o,MsgBuffer* bs){
  Assert(refTrail->isEmpty());
  marshalFullObject(o,bs);
  refTrail->unwind();}

void marshalFullObjectAndClassRT(Object *o,MsgBuffer* bs){
  Assert(refTrail->isEmpty());
  marshalFullObjectAndClass(o, bs);
  refTrail->unwind();}

void marshalTermRT(OZ_Term t, MsgBuffer *bs){
  Assert(refTrail->isEmpty());
  marshalTerm(t, bs);
  refTrail->unwind();}

OZ_Term unmarshalTermRT(MsgBuffer *bs){
  OZ_Term ret;
  refTable->reset();
  Assert(refTrail->isEmpty());
  ret =  unmarshalTerm(bs);
  refTrail->unwind();
  return ret;
}

void unmarshalObjectRT(ObjectFields *o, MsgBuffer *bs){
  refTable->reset();
  Assert(refTrail->isEmpty());
  unmarshalObject(o,bs);
  refTrail->unwind();
}


void unmarshalObjectAndClassRT(ObjectFields *o, MsgBuffer *bs){
  refTable->reset();
  Assert(refTrail->isEmpty());
  unmarshalObjectAndClass(o, bs);
  refTrail->unwind();
}


/* *********************************************************************/
/*   SECTION 19: message marshaling                                    */
/* *********************************************************************/

#include "marshalMsg.cc"


/* *********************************************************************/
/*   SECTION 20: The following go at the very end,                     */
/*               so they will not be inlined                           */
/* *********************************************************************/

int unmarshalUnsentNumber(MsgBuffer *bs) // ATTENTION
{
  return unmarshalNumber(bs);
}

#include "pickle.cc"
