/*
  Hydra Project, DFKI Saarbruecken,
  Stuhlsatzenhausweg 3, D-66123 Saarbruecken, Phone (+49) 681 302-5312
  Author: scheidhr
  Last modified: $Date$ from $Author$
  Version: $Revision$
  State: $State$

  ------------------------------------------------------------------------
*/

/* classes:
     TaggedRef
     RefsArray
     */


#ifndef __TAGGEDH
#define __TAGGEDH

#ifdef __GNUC__
#pragma interface
#endif

#include <stdio.h>


#include "machine.hh"
#include "error.hh"
#include "types.hh"
#include "mem.hh"
#include "gc.hh"

// ---------------------------------------------------------------------------
// ---------- TAGGED REF -----------------------------------------------------
// ---------------------------------------------------------------------------

// --- TaggedRef: Type Declaration

enum TypeOfTerm {
//  REF              =  0, // XX00

  UVAR             =  1,   // 0001
  SVAR             =  9,   // 1001
  CVAR             =  5,   // 0101

  GCTAG            =  13,  // 1101

  LTUPLE           =  2,   // 0010
  STUPLE           =  3,   // 0011
  SRECORD          = 14,   // 1110

  LITERAL          = 15,   // 1111

  CONST            = 10,   // 1010

  SMALLINT         =  6,   // 0110
  BIGINT           =  7,   // 0111
  FLOAT            = 11    // 1011
// 12 = 1100 unusable \
//  8 = 1000 unusable  >  recognized as reference
//  4 = 0100 unusable /
};

extern char *TypeOfTermString[];

/*
 * We use 4 bits for tags --> adress space is 2^28 == 256MB
 *
 * We can do better, if you define
 *     #define LARGEADRESSES
 *
 * if the value part of a TaggRef contains a pointer (this holds for all
 * except small ints , than it will be word aligned,
 * i.e. its two lower bits are 00
 * --> makeTaggedRefs shifts only up by 2 bits
 * --> tagValueOf shifts down by 2 bits AND zeros the two lowest bits
 *
 */

#define LARGEADRESSES

// ---------------------------------------------------------------------------
// --- TaggedRef: CLASS / BASIC ACCESS FUNCTIONS


typedef TaggedRef *TaggedRefPtr;

const int tagSize = 4;
const int tagMask = 0xF;


// ------------------------------------------------------
// Debug macros for debugging outside of gc

extern int gcing;

#define _tagTypeOf(ref) ((TypeOfTerm)(ref&tagMask))

#ifdef DEBUG_GC
#define GCDEBUG(X)                            \
  if ( gcing && (_tagTypeOf(X)==GCTAG ) )     \
    error("GcTag unexpectedly found.");
#else
#define GCDEBUG(X)
#endif


inline void* TaggedToPointer(TaggedRef t)
{
  return (void*) (mallocBase|t);
}

inline
void *tagValueOf(TaggedRef ref)
{
  GCDEBUG(ref);
#ifdef LARGEADRESSES
  return TaggedToPointer((ref >> (tagSize-2))&~3);
#else
  return TaggedToPointer(ref >> tagSize);
#endif
}

inline
TaggedRef makeTaggedRef(TypeOfTerm tag, int32 i)
{
#ifdef LARGEADRESSES
  Assert((i&3) == 0);
  return (i << (tagSize-2)) | tag;
#else
  return (i << tagSize) | tag;
#endif
}


inline
TaggedRef makeTaggedRef(TypeOfTerm tag, void *ptr)
{
  return makeTaggedRef(tag,ToInt32(ptr));
}

inline
TypeOfTerm tagTypeOf(TaggedRef ref)
{
  GCDEBUG(ref);
  return _tagTypeOf(ref);
}



// ---------------------------------------------------------------------------
// --- TaggedRef: useful functions --> print.C

char *tagged2String(TaggedRef ref, int depth, int offset = 0);
void taggedPrint(TaggedRef ref,int depth = 10, int offset = 0);
void taggedPrintLong(TaggedRef ref, int depth = 10, int offset = 0);


// ---------------------------------------------------------------------------
// --- TaggedRef: CHECK_xx

// Philosophy:
//   Arguments which are passed around are never variables, but only
//     REF or bound data
#define CHECK_NONVAR(term) Assert(isRef(term) || !isAnyVar(term))
#define CHECK_ISVAR(term)  Assert(isAnyVar(term))
#define CHECK_DEREF(term)  Assert(!isRef(term) && !isAnyVar(term))
#define CHECK_POINTER(s)   Assert(s != NULL && !(ToInt32(s) & 3) )
#define CHECK_POINTERLSB(s)   Assert(!(ToInt32(s) & 3) )
#define CHECK_STRPTR(s)    Assert(s != NULL)
#define CHECKTAG(Tag)      Assert(tagTypeOf(ref) == Tag)


// ---------------------------------------------------------------------------
// --- TaggedRef: BASIC TYPE TESTS

// if you want to test a term:
//   1. if you have the tag: is<Type>(tag)
//   2. if you have the term: is<Type>(term)
//   3. if you need many tests:
//        switch(typeOf(term)) { ... }
//    or  switch(tag) { ... }


#define IsRef(term) ((term & 3) == 0)
inline
Bool isRef(TaggedRef term) {
  GCDEBUG(term);
  return IsRef(term);
}


// ---------------------------------------------------------------------------
// Tests for variables and their semantics:
//                           tag
// unconstrained var         0001 (UVAR:1)  isUVar   isNotCVar  isAnyVar
// unconstr. suspending var  1001 (SVAR:9)  isSVar   isNotCVar  isAnyVar
// constrained-susp. var     0101 (CVAR:5)           isCVar    isAnyVar
// ---------------------------------------------------------------------------


inline
Bool isSVar(TypeOfTerm tag) {
  return (tag == SVAR);
}

inline
Bool isSVar(TaggedRef term) {
  GCDEBUG(term);
  Assert(!isRef(term));
  return isSVar(tagTypeOf(term));
}


inline
Bool isCVar(TypeOfTerm tag) {
  return (tag == CVAR);
}

inline
Bool isCVar(TaggedRef term) {
  GCDEBUG(term);
  Assert(!isRef(term));
  return isCVar(tagTypeOf(term));
}


/*
 * Optimized tests for some most often used types: no untagging needed
 * for type tests!
 * Use macros if not DEBUG_CHECK, since gcc creates awful code
 * for inline function version
 */

#define _isAnyVar(val)  (((TaggedRef) val&2)==0)       /* mask = 0010 */
#define _isNotCVar(val) (((TaggedRef) val&6)==0)       /* mask = 0110 */
#define _isUVar(val)    (((TaggedRef) val&14)==0)      /* mask = 1110 */
#define _isLTuple(val)  (((TaggedRef) val&13)==0)      /* mask = 1101 */

#ifdef DEBUG_CHECK

inline Bool isLTuple(TypeOfTerm tag) { return _isLTuple(tag);}

inline
Bool isLTuple(TaggedRef term) {
  GCDEBUG(term);
  Assert(!isRef(term));
  return _isLTuple(term);
}


inline Bool isUVar(TypeOfTerm tag) { return _isUVar(tag);}

inline
Bool isUVar(TaggedRef term) {
  GCDEBUG(term);
  Assert(!isRef(term));
  return _isUVar(term);
}

inline Bool isAnyVar(TypeOfTerm tag) { return _isAnyVar(tag); }

inline
Bool isAnyVar(TaggedRef term) {
  GCDEBUG(term);
  Assert(!isRef(term));
  return _isAnyVar(term);
}

inline Bool isNotCVar(TypeOfTerm tag) { return _isNotCVar(tag);}

inline
Bool isNotCVar(TaggedRef term) {
  GCDEBUG(term);
  Assert(!isRef(term));
  return _isNotCVar(term);
}


#else

#define isAnyVar(term)  _isAnyVar(term)
#define isNotCVar(term) _isNotCVar(term)
#define isUVar(term)    _isUVar(term)
#define isLTuple(term)  _isLTuple(term)

#endif



inline
Bool isLiteral(TypeOfTerm tag) {
  return tag == LITERAL;
}

inline
Bool isLiteral(TaggedRef term) {
  GCDEBUG(term);
  return isLiteral(tagTypeOf(term));
}

inline
Bool isSRecord(TypeOfTerm tag) {
  return tag == SRECORD;
}

inline
Bool isSRecord(TaggedRef term) {
  GCDEBUG(term);
  return isSRecord(tagTypeOf(term));
}

inline
Bool isSTuple(TypeOfTerm tag) {
  return (tag == STUPLE);
}

inline
Bool isSTuple(TaggedRef term) {
  GCDEBUG(term);
  return isSTuple(tagTypeOf(term));
}

inline
Bool isTuple(TypeOfTerm tag) {
  return isSTuple(tag) || isLTuple(tag) || isLiteral(tag);
}

inline
Bool isTuple(TaggedRef term) {
  GCDEBUG(term);
  return isTuple(tagTypeOf(term));
}

inline
Bool isFloat(TypeOfTerm tag) {
  return (tag == FLOAT);
}

inline
Bool isFloat(TaggedRef term) {
  GCDEBUG(term);
  return isFloat(tagTypeOf(term));
}

inline
Bool isSmallInt(TypeOfTerm tag) {
  return (tag == SMALLINT);
}

inline
Bool isSmallInt(TaggedRef term) {
  return isSmallInt(tagTypeOf(term));
}

inline
Bool isBigInt(TypeOfTerm tag) {
  return (tag == BIGINT);
}

inline
Bool isBigInt(TaggedRef term) {
  GCDEBUG(term);
  return isBigInt(tagTypeOf(term));
}

inline
Bool isInt(TypeOfTerm tag) {
  return (isSmallInt(tag) || isBigInt(tag));
}

inline
Bool isInt(TaggedRef term) {
  GCDEBUG(term);
  return isInt(tagTypeOf(term));
}

inline
Bool isNumber(TypeOfTerm tag) {
  return (isInt(tag) || isFloat(tag));
}

inline
Bool isNumber(TaggedRef term) {
  GCDEBUG(term);
  return isNumber(tagTypeOf(term));
}

inline
Bool isConst(TypeOfTerm tag) {
  return (tag == CONST);
}

inline
Bool isConst(TaggedRef term) {
  GCDEBUG(term);
  return isConst(tagTypeOf(term));
}

// ---------------------------------------------------------------------------
// --- TaggedRef: create: makeTagged<Type>

// this function should be used, if tagged references are to be initialized
#ifdef DEBUG_CHECK
inline
TaggedRef makeTaggedNULL()
{
  return makeTaggedRef((TypeOfTerm)0,(void*)NULL);
}
#else
#define makeTaggedNULL() ((TaggedRef) 0)
#endif

inline
TaggedRef makeTaggedMisc(void *s)
{
  return makeTaggedRef((TypeOfTerm)0,s);
}

inline
TaggedRef makeTaggedMisc(int32 s)
{
  return makeTaggedRef((TypeOfTerm)0,s);
}

inline
TaggedRef makeTaggedRef(TaggedRef *s)
{
  CHECK_POINTER(s);
  DebugGC(gcing == 0 && !MemChunks::list->inChunkChain ((void *)s),
          error ("making TaggedRef pointing to 'from' space"));
  return (TaggedRef) ToInt32(s);
}

inline
TaggedRef makeTaggedRefToFromSpace(TaggedRef *s)
{
  CHECK_POINTER(s);
/*  DebugGCT(extern MemChunks * from);
  DebugGC(gcing == 0 && !from->inChunkChain ((void *)s),
          error ("making TaggedRef pointing to 'to' space"));
          */
  return (TaggedRef) ToInt32(s);
}

inline
TaggedRef makeTaggedUVar(Board *s)
{
  CHECK_POINTER(s);
  return makeTaggedRef(UVAR,s);
}

inline
TaggedRef makeTaggedSVar(SVariable *s)
{
  CHECK_POINTER(s);
  return makeTaggedRef(SVAR,s);
}

inline
TaggedRef makeTaggedCVar(GenCVariable *s) {
  CHECK_POINTER(s);
  return makeTaggedRef(CVAR, s);
}

inline
TaggedRef makeTaggedSTuple(STuple *s)
{
  CHECK_POINTER(s);
  return makeTaggedRef(STUPLE,s);
}

inline
TaggedRef makeTaggedLTuple(LTuple *s)
{
  CHECK_POINTER(s);
  return makeTaggedRef(LTUPLE,s);
}

inline
TaggedRef makeTaggedSRecord(SRecord *s)
{
  CHECK_POINTER(s);
  return makeTaggedRef(SRECORD,s);
}


inline
TaggedRef makeTaggedAtom(char *s)
{
  CHECK_STRPTR(s);
  return makeTaggedRef(LITERAL,addToAtomTab(s));
}

inline
TaggedRef makeTaggedName(char *s)
{
  CHECK_STRPTR(s);
  return makeTaggedRef(LITERAL,addToNameTab(s));
}

inline
TaggedRef makeTaggedLiteral(Literal *s)
{
  CHECK_POINTER(s);
  return makeTaggedRef(LITERAL,s);
}

inline
TaggedRef makeTaggedSmallInt(int32 s)
{
#ifdef LARGEADRESSES
  /* small ints are the only TaggedRefs that do not
   * contain a pointer in the value part */
  return (s << tagSize) | SMALLINT;
#else
  return makeTaggedRef(SMALLINT,(void*)s);
#endif
}

inline
TaggedRef makeTaggedBigInt(BigInt *s)
{
  CHECK_POINTER(s);
  return makeTaggedRef(BIGINT,s);
}

inline
TaggedRef makeTaggedFloat(Float *s)
{
  CHECK_POINTER(s);
  return makeTaggedRef(FLOAT,s);
}


inline
TaggedRef makeTaggedConst(ConstTerm *s)
{
  CHECK_POINTER(s);
  return makeTaggedRef(CONST,s);
}

// getArg() and the like may never return variables
inline
TaggedRef tagged2NonVariable(TaggedRef *term)
{
  GCDEBUG(*term);
  TaggedRef ret = *term;
  if (!IsRef(ret) && isAnyVar(ret)) {
    ret = makeTaggedRef(term);
  }
  return ret;
}


// ---------------------------------------------------------------------------
// --- TaggedRef: allocate on heap, an return a ref to it

inline
TaggedRef *newTaggedSVar(SVariable *c)
{
  TaggedRef *ref = (TaggedRef *) int32Malloc(sizeof(TaggedRef));
  *ref = makeTaggedSVar(c);
  return ref;
}

inline
TaggedRef *newTaggedUVar(TaggedRef proto)
{
  TaggedRef *ref = (TaggedRef *) int32Malloc(sizeof(TaggedRef));
  *ref = proto;
  return ref;
}

inline
TaggedRef *newTaggedUVar(Board *c)
{
  return newTaggedUVar(makeTaggedUVar(c));
}

inline
TaggedRef *newTaggedCVar(GenCVariable *c) {
  TaggedRef *ref = (TaggedRef *) int32Malloc(sizeof(TaggedRef));
  *ref = makeTaggedCVar(c);
  return ref;
}



// ---------------------------------------------------------------------------
// --- TaggedRef: conversion: tagged2<Type>


inline
void *tagValueOf(TypeOfTerm tag, TaggedRef ref)
{
  GCDEBUG(ref);
#ifdef LARGEADRESSES
  return TaggedToPointer((ref>>(tagSize-2)) - (tag>>2));
#else
  return tagValueOf(ref);
#endif
}

#define _tagged2Ref(ref) ((TaggedRef *) ToPointer(ref))

#ifdef DEBUG_CHECK
inline
TaggedRef *tagged2Ref(TaggedRef ref)
{
  GCDEBUG(ref);
// cannot use CHECKTAG(REF); because only last two bits must be zero
  Assert((ref & 3) == 0);
  return _tagged2Ref(ref);
}
#else
/* macros are faster */
#define tagged2Ref(ref) _tagged2Ref(ref)
#endif

/* does not deref home pointer! */
inline
Board *tagged2VarHome(TaggedRef ref)
{
  GCDEBUG(ref);
  CHECKTAG(UVAR);
  return (Board *) tagValueOf(UVAR,ref);
}

inline
STuple *tagged2STuple(TaggedRef ref)
{
  GCDEBUG(ref);
  CHECKTAG(STUPLE);
  return (STuple *) tagValueOf(STUPLE,ref);
}

inline
SRecord *tagged2SRecord(TaggedRef ref)
{
  GCDEBUG(ref);
  CHECKTAG(SRECORD);
  return (SRecord *) tagValueOf(SRECORD,ref);
}

inline
LTuple *tagged2LTuple(TaggedRef ref)
{
  GCDEBUG(ref);
  CHECKTAG(LTUPLE);
  return (LTuple *) tagValueOf(LTUPLE,ref);
}

inline
Literal *tagged2Literal(TaggedRef ref)
{
  GCDEBUG(ref);
  CHECKTAG(LITERAL);
  return (Literal *) tagValueOf(LITERAL,ref);
}

inline
Float *tagged2Float(TaggedRef ref)
{
  GCDEBUG(ref);
  CHECKTAG(FLOAT);
  return (Float *) tagValueOf(FLOAT,ref);
}

inline
BigInt *tagged2BigInt(TaggedRef ref)
{
  GCDEBUG(ref);
  CHECKTAG(BIGINT);
  return (BigInt *) tagValueOf(BIGINT,ref);
}


inline
ConstTerm *tagged2Const(TaggedRef ref)
{
  GCDEBUG(ref);
  CHECKTAG(CONST);
  return (ConstTerm *) tagValueOf(CONST,ref);
}

inline
SVariable *tagged2SVar(TaggedRef ref)
{
  GCDEBUG(ref);
  CHECKTAG(SVAR);
  return (SVariable *) tagValueOf(SVAR,ref);
}

inline
GenCVariable *tagged2CVar(TaggedRef ref) {
  GCDEBUG(ref);
  CHECKTAG(CVAR);
  return (GenCVariable *) tagValueOf(CVAR,ref);
}


// ---------------------------------------------------------------------------
// --- TaggedRef: DEREF

// DEREF - Philosophy:
//   Arguments are never deref'ed
// especially: builtins cannot immediately access the tag of their arguments,
//   but must first call DEREF


// DEREF MACRO:
//  - declares 'termPtr','tag' as local variables
//  - destructively changes 'term'
//  - 'tag' is the tag of the deref'ed 'term'
//  - 'termPtr' is the pointer to the last REF-Cell in a chain of REF's
//    and is NULL, if no deref step was necessary (needed for destructive
//    changes of variables)

// Usage:
// void test(TaggedRef a) {
//   DEREF(a,ptr,tag);
//   if (isLiteral(tag)) { ... }
//   if (isAnyVar(tag) { *ptr = ... }
//   ....
// }


#define _DEREF(term, termPtr, tag)                                            \
  while(IsRef(term)) {                                                        \
    termPtr = tagged2Ref(term);                                               \
    term = *termPtr;                                                          \
  }                                                                           \
  TypeOfTerm tag = tagTypeOf(term);

#define DEREF(term, termPtr, tag)                                             \
  register TaggedRef *termPtr = NULL;                                         \
  _DEREF(term,termPtr,tag);

#define DEREFPTR(term, termPtr, tag)                                          \
  register TaggedRef term = *termPtr;                                         \
  _DEREF(term,termPtr,tag);


inline
TaggedRef deref(TaggedRef t) {
  DEREF(t,_1,_2);
  return t;
}

// ---------------------------------------------------------------------------
// ------- RefsArray ----------------------------------------------------------
// ---------------------------------------------------------------------------


// RefsArray is an array of TaggedRef
// a[-1] = LL...LLLTTTT,
//     L = length
//     T = tag (RADirty/RAFreed) or GCTAG during GC


typedef TaggedRef *RefsArray;

/* any combination of the following two must not be equal to the GCTAG */
const int RADirty = 1; // means something has suspendeed on it
#ifdef DEBUG_CHECK
const int RAFreed = 2; // means has been already deallocated
#endif

inline
Bool isDirtyRefsArray(RefsArray a)
{
  return (a[-1]&RADirty);
}

inline
void markDirtyRefsArray(RefsArray a)
{
  if (a) a[-1] |= RADirty;
}

#ifdef DEBUG_CHECK
inline
Bool isFreedRefsArray(RefsArray a)
{
  return (a && a[-1]&RAFreed);
}

inline
void markFreedRefsArray(RefsArray a)
{
  if (a) a[-1] |= RAFreed;
}

#endif

inline
void setRefsArraySize(RefsArray a, int32 n)
{
  a[-1] = (n<<tagSize);
}

inline
int getRefsArraySize(RefsArray a)
{
  return (a[-1]>>tagSize);
}


inline
Bool initRefsArray(RefsArray a, int size, Bool init)
{
  setRefsArraySize(a,size);
  if (init) {
    switch (size) {
    case 10: a[9] = makeTaggedNULL();
    case  9: a[8] = makeTaggedNULL();
    case  8: a[7] = makeTaggedNULL();
    case  7: a[6] = makeTaggedNULL();
    case  6: a[5] = makeTaggedNULL();
    case  5: a[4] = makeTaggedNULL();
    case  4: a[3] = makeTaggedNULL();
    case  3: a[2] = makeTaggedNULL();
    case  2: a[1] = makeTaggedNULL();
    case  1: a[0] = makeTaggedNULL();
      break;
    default:
      {
        for(int i = size-1; i >= 0; i--)
          a[i] = makeTaggedNULL();
      }
      break;
    }
  }

  return OK;  /* due to stupid CC */
}

inline
RefsArray allocateRefsArray(int n, Bool init=OK)
{
  Assert(n > 0);
  RefsArray a = ((RefsArray) freeListMalloc((n+1) * sizeof(TaggedRef)));
  a += 1;
  initRefsArray(a,n,init);
  return a;
}

inline
RefsArray allocateRefsArray(int n, TaggedRef initRef)
{
  Assert(n > 0);
  RefsArray a = ((RefsArray) freeListMalloc((n+1) * sizeof(TaggedRef)));
  a += 1;
  // Initialize with given TaggedRef:
  setRefsArraySize(a,n);
  for(int i = n-1; i >= 0; i--)
    a[i] = initRef;
  return a;
}

inline
void disposeRefsArray(RefsArray a)
{
  if (a) {
    int sz = getRefsArraySize(a);
    a -= 1;
    freeListDispose(a, (sz+1) * sizeof(TaggedRef));
  }
}

inline
RefsArray allocateY(int n)
{
  RefsArray a = ((RefsArray) freeListMalloc((n+1) * sizeof(TaggedRef)));
  a += 1;
  initRefsArray(a,n,OK);
  return a;
}

inline
void deallocateY(RefsArray a)
{
#ifdef DEBUG_CHECK
  markFreedRefsArray(a);
#else
  freeListDispose(a-1,(getRefsArraySize(a)+1) * sizeof(TaggedRef));
#endif
}

inline
RefsArray allocateStaticRefsArray(int n) {
// RefsArray a = (RefsArray) new char[(n+1) * sizeof(TaggedRef)];
  RefsArray a = new TaggedRef[n + 1];
  a += 1;
  initRefsArray(a,n,OK);
  return a;
}


inline
RefsArray copyRefsArray(RefsArray a) {
  int n = getRefsArraySize(a);
  RefsArray r = allocateRefsArray(n,NO);
  for (int i = n-1; i >= 0; i--) {
    r[i] = tagged2NonVariable(&a[i]);
  }
  return r;
}

inline
RefsArray copyRefsArray(RefsArray a,int n,Bool init=NO) {
  RefsArray r = allocateRefsArray(n,init);
  for (int i = n-1; i >= 0; i--) {
    CHECK_NONVAR(a[i]);
    r[i] = a[i];
  }
  return r;
}


inline
RefsArray resize(RefsArray r, int s){
  int size = getRefsArraySize(r);
  if (s < size){
    setRefsArraySize(r,s);
    return r;
  }

  if (s > size){
    RefsArray aux = allocateRefsArray(s);
    for(int j = size-1; j >= 0; j--)
      aux[j] = r[j];
    return aux;
  }
  return r;
} // resize


//
// identity test
//
inline Bool sameTerm(TaggedRef t1, TaggedRef t2)
{
  DEREF(t1,t1Ptr,_1);
  DEREF(t2,t2Ptr,_2);
  if (isAnyVar(t1) || isAnyVar(t2)) {
    return t1Ptr==t2Ptr;
  }
  return t1==t2;
}


extern void initTagged();

#endif
