/* -*- C++ -*-
 *  Authors:
 *    Per Brand (perbrand@sics.se)
 *    Michael Mehl (mehl@dfki.de)
 *    Ralf Scheidhauer (Ralf.Scheidhauer@ps.uni-sb.de)
 *
 *  Contributors:
 *    optional, Contributor's name (Contributor's email address)
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

#ifndef __MARSHALER_HH
#define __MARSHALER_HH

#include "base.hh"
#include "msgbuffer.hh"

//  provided by marshaler to protocol-layer.

void marshalTerm(OZ_Term,MsgBuffer*);
void marshalTermRT(OZ_Term t, MsgBuffer *bs);
void marshalSRecord(SRecord *sr, MsgBuffer *bs);
void marshalClass(ObjectClass *cl, MsgBuffer *bs);
void marshalNumber(unsigned int,MsgBuffer*);
void marshalShort(unsigned short,MsgBuffer*);
void marshalString(const char *s, MsgBuffer *bs);
void marshalDIF(MsgBuffer *bs, MarshalTag tag) ;
void marshalGName(GName*, MsgBuffer*);

OZ_Term unmarshalTerm(MsgBuffer*);
OZ_Term unmarshalTermRT(MsgBuffer *bs);
int unmarshalNumber(MsgBuffer*);
char *unmarshalString(MsgBuffer *);
unsigned short unmarshalShort(MsgBuffer*);
GName* unmarshalGName(TaggedRef*,MsgBuffer*);
SRecord* unmarshalSRecord(MsgBuffer*);

extern RefTable *refTable;
extern RefTrail *refTrail;

void initMarshaler();

// message routines provided by marshaler

Bool unmarshal_SPEC(MsgBuffer*, char* &,OZ_Term&);

// the names of the difs for statistics

enum {
  MISC_STRING,
  MISC_GNAME,
  MISC_SITE,

  MISC_LAST
};

class SendRecvCounter {
private:
  long c[2];
public:
  SendRecvCounter() { c[0]=0; c[1]=0; }
  void send() { c[0]++; }
  long getSend() { return c[0]; }
  void recv() { c[1]++; }
  long getRecv() { return c[1]; }
};

extern SendRecvCounter dif_counter[];
extern SendRecvCounter misc_counter[];
extern char *misc_names[];

/* *********************************************************************/
/*   classes RefTable RefTrail                              */
/* *********************************************************************/

class RefTable {
  OZ_Term *array;
  int size;
  int nextFree; // only for backwards compatibility
public:
  void reset() { nextFree=0; }
  RefTable()
  {
    reset();
    size     = 100;
    array    = new OZ_Term[size];
  }
  OZ_Term get(int i)
  {
    return (i>=size) ? makeTaggedNULL() : array[i];
  }
  void set(OZ_Term val, int pos)
  {
    if (pos == -1) {
      pos = nextFree++;
    }
    if (pos>=size)
      resize(pos);
    array[pos] = val;
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
};

inline int unmarshalRefTag(MsgBuffer *bs)
{
  return bs->oldFormat() ? -1 : unmarshalNumber(bs);
}

inline void gotRef(MsgBuffer *bs, TaggedRef val, int index)
{
  refTable->set(val,index);
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


void trailCycleOutLine(LTuple *l, MsgBuffer *bs);
void trailCycleOutLine(OZ_Term *l, MsgBuffer *bs);
Bool checkCycleOutLine(LTuple *l, MsgBuffer *bs);
Bool checkCycleOutLine(OZ_Term t, MsgBuffer *bs, TypeOfTerm tag);


#endif // __MARSHALER_HH
