/*
  Perdio Project, DFKI & SICS,
  Universit"at des Saarlandes
  Postfach 15 11 59, D-66041 Saarbruecken, Phone (+49) 681 302-5312
  SICS
  Box 1263, S-16428 Sweden, Phone (+46) 8 7521500
  Author: brand,scheidhr, mehl
  Last modified: $Date$ from $Author$
  Version: $Revision$
  State: $State$

  network layer
  ------------------------------------------------------------------------
*/

#ifdef PERDIO

#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include "oz.h"
#include "am.hh"
#include "ip.hh"

#include "perdio_debug.hh"  
#include "perdio_debug.cc"  

#include "genhashtbl.cc"

/**********************************************************************/
/**********************************************************************/
/*                        INITIAL                                     */
/**********************************************************************/
/**********************************************************************/

#define PENDLINK_CUTOFF 100
#define PENDENTRY_CUTOFF 100

typedef Tertiary ProtocolObject;
typedef Tertiary Proxy;
typedef long Credit;  /* TODO: full credit,long credit? */

class ByteStream;

PERDIO_DEBUG_DO(void printTables());

/**********************************************************************/
/*                        NetAddress (not allocate only cast)         */
/**********************************************************************/

class NetAddress {       
public:
  /*  DummyClassConstruction(NetAddress)*/
  int site;
  int index;
  
  NetAddress(int s, int i) : site(s), index(i) {}

  void set(int s,int i) {site=s,index=i;}

  Bool same(NetAddress *na) { return na->site==site && na->index==index; }

  Bool isLocal() { return site==localSite; }
};


inline int reliableSend0(int,ByteStream *);
inline void marshallNumber(unsigned int,ByteStream *);
inline void marshallMySite(ByteStream* );
inline void marshallCredit(Credit,ByteStream *);

/* ********************************************************************** */
/*                  BYTE STREAM
/* ********************************************************************** */

#define BSEOF (unsigned int) -1
#define DEFAULT_BYTE_STREAM_SIZE ozconf.tcpPacketSize

class ByteStream {
  BYTE *array;
  int size;
  BYTE *pos;
  int len;
public:
  ByteStream()
  {
    len = ipHeaderSize;
    size = DEFAULT_BYTE_STREAM_SIZE;
    array = new BYTE[size];
    pos = array+len;
  }
  ByteStream(BYTE *buf,int len) : len(len)
  {
    size=-1;
    array = buf;
    pos = buf;
  }
  ~ByteStream() { if (size>0) delete array; }

  void resize();

  void reset()  { pos = array; }

  unsigned int get() 
  {
    return pos>=array+len ? BSEOF : (unsigned int) (BYTE) *pos++;
  }

  void endCheck(){
    return; /* TODO */
  }
       

  void put(BYTE c)
  {
    Assert(size>0);
    if (pos>=array+size)
      resize();
    *pos++ = c;
    len++;
  }
  BYTE *getPtr() { return array; }
  int getLen() { return len; }
};

/* ********************************************************************** */
/*                  PENDING MESSAGES STUFF
/* ********************************************************************** */


class PendEntry {
  int refCount;
  ByteStream *bs;
  BorrowEntry *back;
  int site;
public:    
  void send();

  void initialize(ByteStream *bs1,int sd){
    bs=bs1;
    refCount=0;
    back=NULL;
    site=sd;}

  void initialize(ByteStream *bs1,int sd,BorrowEntry *b){
    initialize(bs1,sd);
    back=b;}

  void inc() {refCount++;}
  void dec() {refCount--;}
  BorrowEntry *getBack() {return back;}
  int getrefCount() {return refCount;}
  Bool isFIFO() {return back!=NULL;}
};

void PendEntry::send(){
  int ret=reliableSend0(site,bs);  /* TODO: delayed sending?? */
  Assert(ret==PROCEED); // TODO
}

class PendEntryManager: public FreeListManager{
public:
  PendEntryManager():FreeListManager(PENDENTRY_CUTOFF){}

  PendEntry *newPendEntry(){
    FreeListEntry *f=getOne();
    if(f==NULL) {return new PendEntry();}
    PendEntry *pe;
    Cast(f,FreeListEntry*,pe,PendEntry*);
    return pe;}

  void deletePendEntry(PendEntry* p){
    FreeListEntry *f;
    Cast(p,PendEntry*,f,FreeListEntry*);
    if(putOne(f)) {return;}
    delete p;
    return;}
};

PendEntryManager *pendEntryManager;

/* ******************************************************************** */

class PendLink {
  Credit debt;
  PendEntry *pend;
public:
  PendLink *next;

  void initialize(Credit c,PendEntry* p){
    debt=c;
    pend=p;
    next=NULL;  }

  void setTag(){
    pend= (PendEntry *)(((unsigned int) pend) | 1);}

  Bool isTagged(){
    return ((unsigned int) pend & 1);}

  PendEntry *getPend(){
    return (PendEntry *)((unsigned int) pend & ~1);}

  Credit getDebt() {return debt;}
  void setDebt(Credit c) {debt=c;}
};

class PendLinkManager: public FreeListManager{
public:
  PendLinkManager():FreeListManager(PENDLINK_CUTOFF){}

  PendLink *newPendLink(){
    FreeListEntry *f=getOne();
    if(f==NULL) {return new PendLink();}
    PendLink *pl;
    Cast(f,FreeListEntry*,pl,PendLink*);
    return pl;}

  void deletePendLink(PendLink* p){
    FreeListEntry *f;
    Cast(p,PendLink*,f,FreeListEntry*);
    if(putOne(f)) {return;}
    delete p; 
    return;}
};

PendLinkManager *pendLinkManager;

/* ********************************************************************** */
/* ********************************************************************** */
/*                  OWNER TABLE STUFF                                     */
/* ********************************************************************** */
/* ********************************************************************** */

/* credit           infinity=  1?----? 
                    max=       01----1
                               .....
		    min=       0----01 
                    dead=      0-----0   Invariant: credit never 0 for live owner
*/



#define INFINITE_CREDIT          -1
#define FREE_TAG                  0  

/*
#define START_CREDIT_SIZE        ((1<<31) - 1)
#define OWNER_GIVE_CREDIT_SIZE   ((1<<15))
#define BORROW_GIVE_CREDIT_SIZE  ((1<<7))
#define MIN_BORROW_CREDIT_SIZE   2
#define MAX_BORROW_CREDIT_SIZE   8 * OWNER_GIVE_CREDIT_SIZE
#define ASK_CREDIT_SIZE          OWNER_GIVE_CREDIT_SIZE

*/

#define START_CREDIT_SIZE        (256)
#define OWNER_GIVE_CREDIT_SIZE   (16)
#define BORROW_GIVE_CREDIT_SIZE  (4)
#define MIN_BORROW_CREDIT_SIZE   2
#define MAX_BORROW_CREDIT_SIZE   2 * OWNER_GIVE_CREDIT_SIZE
#define ASK_CREDIT_SIZE          OWNER_GIVE_CREDIT_SIZE


#define DEFAULT_OWNER_TABLE_SIZE   100
#define DEFAULT_BORROW_TABLE_SIZE  100
#define NET_HASH_TABLE_DEFAULT_SIZE 100

static double TABLE_LOW_LIMIT=0.20;
static double TABLE_EXPAND_FACTOR=2.00;

#define TABLE_BUFFER 50
#define TABLE_WORTHWHILE_REALLOC 200

/* ********************************************************************** */
/*                  HASH TABLE def
/* ********************************************************************** */

class NetHashTable: public GenHashTable{
  int hashFunc(NetAddress *);
  Bool findPlace(int ,NetAddress *, GenHashNode *&);
public:
  NetHashTable():GenHashTable(NET_HASH_TABLE_DEFAULT_SIZE){}
  BorrowEntry* findNA(NetAddress *);
  void add(NetAddress *,int);
  void sub(NetAddress *);
#ifdef DEBUG_PERDIO
  void print();
#endif
};

/* ********************************************************************** */
/*    OB_Entry - common to borrow and owner tables                        */
/* ********************************************************************** */

class OB_Entry {
protected:
  union
  {
    ProtocolObject *object;
    int nextfree;
  }entry;
  Credit credit;

  Bool isFree() {return(credit==FREE_TAG);}

  void makeFree(int next) {credit=FREE_TAG;entry.nextfree=next;}

  void setObject(ProtocolObject *o)  {
    entry.object=o;}

  void setObjectCredit(ProtocolObject *o,Credit c)  {
    entry.object=o;
    credit=c;}

  int getNextFree(){
    Assert(credit==FREE_TAG);
    return entry.nextfree;  }

  void setCredit(Credit c) {credit=c;}

  void addToCredit(Credit c) {credit +=c;}

  void subFromCredit(Credit c) {credit -=c;}



public:
  Credit getCredit(){Assert(credit!=FREE_TAG);return credit;} 

  ProtocolObject* getObject() {
    Assert(credit!=FREE_TAG);
    return entry.object; }
};
  
/* ********************************************************************** */


class OwnerEntry: public OB_Entry {
friend class OwnerTable;
public:
  void initOwner(ProtocolObject *o){
    Assert(isFree());
    setObjectCredit(o,START_CREDIT_SIZE);}

  void setOwnerObject(ProtocolObject *p){setObject(p);}
  Bool isFullCredit() {return credit==START_CREDIT_SIZE;}

  void returnCredit(Credit c) {
    if (credit == INFINITE_CREDIT) return;
    addToCredit(c);}

  void returnOneCredit() {
    if (credit != INFINITE_CREDIT) 
      addToCredit(1);}

  Credit getSendCredit(){
    Credit c=getCredit();
    if(c < 2*OWNER_GIVE_CREDIT_SIZE) setCredit(INFINITE_CREDIT);
    if(c == INFINITE_CREDIT) return(OWNER_GIVE_CREDIT_SIZE);	
    subFromCredit(OWNER_GIVE_CREDIT_SIZE);
    return(OWNER_GIVE_CREDIT_SIZE);}

  Credit giveMoreCredit(){
    Credit c=getCredit();
    if(c < 2*ASK_CREDIT_SIZE) setCredit(INFINITE_CREDIT);
    if(c == INFINITE_CREDIT) return(ASK_CREDIT_SIZE);	
    subFromCredit(ASK_CREDIT_SIZE);
    return(ASK_CREDIT_SIZE);}

}; 


/* ********************************************************** */
/*                    OWNER TABLE                             */
/* ********************************************************** */

#define END_FREE -1

class OwnerTable {
  int size;
  int no_used;
  int nextfree;
  
  void init(int,int);
  void compactify();

public:
  OwnerEntry* array;  /* TODO move to private */

#ifdef DEBUG_PERDIO
  void print();

#endif

  OwnerEntry *getOwner(int i)  {return &array[i];}
  
  int getSize() {return size;}

  OwnerTable(int sz) {
    size = sz;
    array = (OwnerEntry*) malloc(size*sizeof(OwnerEntry));
    Assert(array!=NULL);
    nextfree = END_FREE;
    no_used=0;
    init(0,sz);
  }

  void gcOwnerTable();

  void resize();

  int newOwner(Tertiary*);

  void freeOwnerEntry(int);

  void receiveReturnCredit(int,Credit);
};

void OwnerTable::receiveReturnCredit(int index,Credit c){

  OwnerEntry *o= getOwner(index);
  o->returnCredit(c);
  if(!(o->isFullCredit())) return;
  Tertiary *te=o->entry.object;
  switch(te->getType()){
  case Co_Port:{
    Assert((te->getTertType())==Te_Manager);
    (tert2PortManager(te))->localize();
    break;}
  default:
    Assert(0);
    break;}
  freeOwnerEntry(index);}

void OwnerTable::init(int beg,int end)
{
  int i=beg;
  while(i<end){
    array[i].makeFree(i+1);
    i++;}
  i--;
  array[i].makeFree(END_FREE);
  nextfree=beg;
}

void OwnerTable::compactify()
{
  Assert(size>=DEFAULT_OWNER_TABLE_SIZE);
  if(size==DEFAULT_OWNER_TABLE_SIZE) return;
  if(no_used/size< TABLE_LOW_LIMIT) return;
  PERDIO_DEBUG2(TABLE,"TABLE:owner compactify enter: size:%d no_used:%d",
	       size,no_used);
  PERDIO_DEBUG_DO1(TABLE2,printTables());
  int i=0;
  int used_slot= -1;
  int* base = &nextfree;
  while(TRUE){
    if(i>=size) break;
    if(array[i].isFree()){
      *base=i;
      base=&array[i].entry.nextfree;}
    else used_slot=i;
    i++;}
  *base=END_FREE;
  int first_free=used_slot+1;
  int newsize= first_free-no_used < TABLE_BUFFER ? 
    first_free-no_used+TABLE_BUFFER : used_slot+1;
  if(first_free < size - TABLE_WORTHWHILE_REALLOC){
    PERDIO_DEBUG1(TABLE,"TABLE:owner compactify free slots: new%d",newsize);
    OwnerEntry *oldarray;
    array = (OwnerEntry*) realloc(array,newsize*sizeof(OwnerEntry));
    Assert(array!=NULL);
    if(array!=NULL){
      size=newsize;
      init(first_free,size);
      return;}
    array=oldarray;}
  init(first_free,size);      
  PERDIO_DEBUG(TABLE,"TABLE:owner compactify no realloc");
  PERDIO_DEBUG_DO1(TABLE2,printTables());
  return;}

void OwnerTable::resize(){
  int newsize = ((int) (TABLE_EXPAND_FACTOR *size));
  PERDIO_DEBUG3(TABLE,"TABLE:resize owner old:%d no_used:%d new:%d",
		size,no_used,newsize);
  PERDIO_DEBUG_DO1(TABLE2,printTables());
  array = (OwnerEntry*) realloc(array,newsize*sizeof(OwnerEntry));
  Assert(array!=NULL);
  if(array==NULL){
    error("Memory allocation: Owner Table growth not possible");}
  init(size, newsize);  
  size=newsize;
  PERDIO_DEBUG(TABLE2,"TABLE:resize owner complete");
  PERDIO_DEBUG_DO1(TABLE2,printTables());
  return;}

int OwnerTable::newOwner(ProtocolObject *obj){
  if(nextfree == END_FREE) resize();
  int index=nextfree;
  nextfree=array[index].getNextFree();
  nextfree= array[index].entry.nextfree;
  OwnerEntry *oe= (OwnerEntry *)&(array[index]);
  oe->initOwner(obj);

  PERDIO_DEBUG1(TABLE,"TABLE:owner insert: %d",index);
  no_used++;
  return index;}


void OwnerTable::freeOwnerEntry(int i){
  array[i].credit=FREE_TAG;
  array[i].entry.nextfree=nextfree;
  nextfree=i;
  no_used--;
  PERDIO_DEBUG1(TABLE,"TABLE:owner delete %d",i);
  return;}

#ifdef DEBUG_PERDIO
void OwnerTable::print(){
  printf("***********************************************\n");
  printf("********* OWNER TABLE *************************\n");
  printf("***********************************************\n");
  printf("Size:%d No_used:%d \n",size,no_used);
  int i;
  OwnerEntry *oe;
  for(i=0;i<size;i++){
    if(!(array[i].isFree())){
      oe=getOwner(i);
      printf("<%d> OWNER: Credit:%d\n",i,oe->getCredit());}
    else{
      printf("<%d> FREE: next:%d\n",i,array[i].entry.nextfree);}}
  printf("-----------------------------------------------\n");  
}
#endif

OwnerTable *ownerTable;


/* ********************************************************************** */
/* ********************************************************************** */
/*                  BORROW TABLE STUFF                                    */
/* ********************************************************************** */
/* ********************************************************************** */

#define BORROW_GC_MARK 1

int borrowEntryToIndex(BorrowEntry *b);

class BorrowEntry: public OB_Entry {
friend class BorrowTable;
private:
  NetAddress netaddr;
  PendLink *pendLink;
  void inDebt(PendLink *);
  Credit pendLinkCredit(Credit c);
  void pendLinkHandle();

public:

  void makeMark(){
    pendLink = (PendLink*)((unsigned int) pendLink | BORROW_GC_MARK);}

  Bool isMarked(){
    return ((unsigned int) pendLink & BORROW_GC_MARK);}   

  void removeMark(){
    pendLink = (PendLink*)((unsigned int) pendLink & (~BORROW_GC_MARK));}

  Bool isPending(){
    Assert(!isFree());
    return(pendLink!=NULL);}

  Bool gcCheck() {
    Assert(!isFree());
    Tertiary *tert=getObject();
    if(isMarked()){
      removeMark();
      PERDIO_DEBUG(GC,"GC:mark found");
      return FALSE;}
    PERDIO_DEBUG(GC,"GC:no mark found");
    return TRUE;}           // maybe garbage (only if pendLink==NULL); 

inline void copyBorrow(BorrowEntry* from,int i){
    setCredit(from->getCredit());
    setObject(from->getObject());
    pendLink=from->pendLink;
    netaddr.set(from->netaddr.site,from->netaddr.index);
    from->getObject()->setIndex(i);}

  void initBorrow(Credit c,int s,int i){
    Assert(isFree());
    setObject(NULL);
    setCredit(c);
    pendLink=NULL;
    netaddr.set(s,i);
    return;}

  void setBorrowObject(ProtocolObject *p){setObject(p);}

  NetAddress* getNetAddress() {
    Assert(!isFree());
    return &netaddr;}
  
  void addPendLink(PendLink*);

  void freeBorrowEntry();

  void addAskCredit(Credit c){
    addToCredit(1);
    addCredit(c-1);
  }

#ifdef DEBUG_PERDIO
  void DEBUG_pendLink(PendLink *pl){
    while(pl!=NULL) {pl=pl->next;}
    return;}
#endif

  void addCredit(Credit cin){ 
    PERDIO_DEBUG4(CREDIT,"CREDIT:borrow add <%d:%d> add:%d to:%d",
		  getNetAddress()->site,
		  getNetAddress()->index,
		  cin,getCredit());
    if(pendLink!=NULL){
      cin=pendLinkCredit(cin);      
      pendLinkHandle();
      PERDIO_DEBUG_DO(DEBUG_pendLink(pendLink));}
    addToCredit(cin);
    Credit cur=getCredit();
    if(cur>MAX_BORROW_CREDIT_SIZE){
      giveBackCredit(cur-MAX_BORROW_CREDIT_SIZE);
      setCredit(MAX_BORROW_CREDIT_SIZE);}
  }
  
  Bool getOneAskCredit() {
    Credit c=getCredit();
    if(c==1) {
      PERDIO_DEBUG(CREDIT,"CREDIT:getOneAskCredit failed");
      return FALSE;
    }
    PERDIO_DEBUG(CREDIT,"CREDIT:getOneAskCredit OK");
    subFromCredit(1);
    return TRUE;}

  Bool getOneCredit() { 
    Credit c=getCredit();
    Assert(c>0);
    if(c <= MIN_BORROW_CREDIT_SIZE) {
      PERDIO_DEBUG(CREDIT,"CREDIT:getOneCredit failed");      
      return FALSE;}
    PERDIO_DEBUG(CREDIT,"CREDIT:getOneCredit OK");
    subFromCredit(1);
    return TRUE; }

  Bool getSmallCredit(Credit &c){
    Credit cur=getCredit();
    if(cur < 2 * MIN_BORROW_CREDIT_SIZE) return FALSE;
    if(cur >  2 * BORROW_GIVE_CREDIT_SIZE) c=BORROW_GIVE_CREDIT_SIZE;
    else{
      if(cur >= 2 * MIN_BORROW_CREDIT_SIZE) c=MIN_BORROW_CREDIT_SIZE;}
    PERDIO_DEBUG1(CREDIT,"CREDIT:give small credit:%d",c);
    subFromCredit(c);
    return TRUE;}
  
  void inDebtMain(PendEntry *);
  void inDebtSec(Credit,PendEntry *);
  void moreCredit();

  void giveBackCredit(Credit c);
  Bool fifoCanSend(PendLink *,PendEntry *pe,Bool flag);
};

void BorrowEntry::inDebtMain(PendEntry *pe){
  PendLink *pl=pendLinkManager->newPendLink();
  pl->initialize(1,pe);
  pl->setTag();
  pe->inc();
  inDebt(pl);}

void BorrowEntry::inDebtSec(Credit c,PendEntry *pe){
  PendLink *pl=pendLinkManager->newPendLink();
  pl->initialize(c,pe);  
  pe->inc();
  inDebt(pl);}

void BorrowEntry::inDebt(PendLink *pl){
  if(pendLink==NULL) {
    PERDIO_DEBUG(PENDLINK,"PENDLINK:new- none so far");
    moreCredit();
    pendLink=pl;
    return;}
  PERDIO_DEBUG(PENDLINK,"PENDLINK:new- others around far");
  PendLink* aux=pendLink;
  while(aux->next!=NULL) aux=aux->next;
  aux->next=pl;
  return;
}

Credit BorrowEntry::pendLinkCredit(Credit c){
  PendLink *pl= pendLink;
  while(c>0){
    Credit d= pl->getDebt(); 
    if(d>c) {
      pl->setDebt(d-c);
      return 0;}
    PERDIO_DEBUG(PENDLINK,"PENDLINK one entry = 0");
    pl->setDebt(0);
    c = c-d;
    (pl->getPend())->dec();
    pl=pl->next;
    if(pl==NULL) return c;
  }
  return c;}

Bool BorrowEntry::fifoCanSend(PendLink *cur,PendEntry *pe,Bool flag){
  if(!pe->isFIFO()) return TRUE;
  BorrowEntry *bb=pe->getBack();
  if(bb==this){
    if((cur->isTagged())&& (!flag)) {
      PERDIO_DEBUG(PENDLINK,"PENDLINK:cannot send due to FIFO");
      return FALSE;}
    return TRUE;}
  PendLink *pl=pendLink;
  while(TRUE){
    Assert(pl!=NULL);
    if(pl->getPend()==pe) return TRUE;
    if(pl->isTagged()) return FALSE;
    pl=pl->next; }}

void BorrowEntry::pendLinkHandle(){
  PendLink *cur=pendLink;
  PendLink **base=&pendLink;
  PendLink *aux;
  PendEntry *pe;
  Bool flag=TRUE; /* can send */
  Bool msgsent;

  PERDIO_DEBUG(PENDLINK,"PENDLINK:entering debt handler");
  while(TRUE){
    if(cur==NULL) {
      *base=cur;
      return;}
    if(cur->getDebt()!=0){
      PERDIO_DEBUG(PENDLINK,"PENDLINK - ran into non-zero debt");
      *base=cur;
      moreCredit();
      return;}
    pe=cur->getPend();
    if((pe->getrefCount()==0)){
      if(fifoCanSend(cur,pe,flag)){
	PERDIO_DEBUG(DELAYED_MSG_SENT,"DELAYED_MSG_SENT:pendLinkHandle");
	pe->send();
	msgsent=TRUE;
	delete pe;}
      else{
	PERDIO_DEBUG(PENDLINK,"PENDLINK - ran into fifo cannot send");
	msgsent=FALSE;}}
    else{
      PERDIO_DEBUG(PENDLINK,"PENDLINK - ran into non-zero ref ct");
      msgsent=FALSE;}
    if(cur->isTagged() && ((!flag) || (!msgsent))){
      PERDIO_DEBUG(PENDLINK,"PENDLINK fifo restriction cannot remove");      
      flag=FALSE;
      base= &(cur->next);
      cur=cur->next;}
    else{
      aux=cur->next;
      PERDIO_DEBUG(PENDLINK,"PENDLINK removal");
      delete cur;
      cur=aux;}}
}


void BorrowEntry::moreCredit(){
  if(!getOneAskCredit()) return;
  ByteStream *bs= new ByteStream();
  bs->put(ASK_FOR_CREDIT);
  NetAddress *na = getNetAddress();
  int site = na->site;
  int index = na->index;
  marshallNumber(index,bs);
  marshallMySite(bs);
  PERDIO_DEBUG2(MSG_SENT,"MSG_SENT:ASK_FOR_CREDIT sd:%d,index:%d",
		site,index);
  int ret= reliableSend0(site,bs);
  Assert(ret==PROCEED); // TODO
  delete bs;}

void BorrowEntry::giveBackCredit(Credit c){
  PERDIO_DEBUG1(CREDIT,"CREDIT:give back - %d",c);
  ByteStream *bs= new ByteStream();
  bs->put(OWNER_CREDIT);
  NetAddress *na = getNetAddress();
  int site = na->site;
  int index = na->index;
  marshallNumber(index,bs);
  marshallCredit(c,bs);
  PERDIO_DEBUG3(MSG_SENT,"MSG_SENT:OWNER_CREDIT sd:%d,index:%d,credit:%d",
		site,index,c);
  int ret= reliableSend0(site,bs);
  Assert(ret==PROCEED); // TODO
  delete bs;}

void BorrowEntry::freeBorrowEntry(){
  giveBackCredit(credit);
  networkSiteDec((getNetAddress())->site);}


/* ********************************************************************** */
/*         BorrowTable                                                    */
/* ********************************************************************** */

class BorrowTable {           
private:
  int no_used;
  BorrowEntry* array;
  int size;
  int nextfree;

  void init(int,int);
  void compactify();

public:
  NetHashTable *hshtbl;

  BorrowEntry *getBorrow(int i)  {return &array[i];}

  int ptr2Index(BorrowEntry *a) { return(a-array);}

  int getSize() {return size;}   

  BorrowTable(int sz)  {
    size= sz;
    array = (BorrowEntry*) malloc(size *sizeof(BorrowEntry));
    Assert(array!=NULL);
    nextfree = END_FREE;
    init(0,sz);
    no_used=0;
    hshtbl = new NetHashTable();  }

  void gcBorrowTable();

  BorrowEntry *find(NetAddress *na)  {
    BorrowEntry *b=hshtbl->findNA(na);
    if(b==NULL) {
      PERDIO_DEBUG(LOOKUP,"LOOKUP:borrow NO");}
    else {
      PERDIO_DEBUG(LOOKUP,"LOOKUP:borrow yes");}
    return b;
  }

  void resize();

  int newBorrow(Credit,int,int);

  void maybeFreeBorrowEntry(int);

  int getOriginSite(int bi){
    return getBorrow(bi)->getNetAddress()->site;}

  int getOriginIndex(int bi){
    return getBorrow(bi)->getNetAddress()->index;}

  void copyBorrowTable(BorrowEntry *,int);

#ifdef DEBUG_PERDIO
  void print();
#endif
};

void BorrowTable::init(int beg,int end)
{
  int i=beg;
  while(i<end){
    array[i].entry.nextfree=i+1;
    array[i].credit=FREE_TAG;
    i++;}
  i--;
  array[i].entry.nextfree=nextfree;
  nextfree=beg;
}

void BorrowTable::compactify(){
  if(no_used / size >= TABLE_LOW_LIMIT) return;
  Assert(size>=DEFAULT_BORROW_TABLE_SIZE);
  if(size==DEFAULT_BORROW_TABLE_SIZE) return;
  int newsize= no_used+TABLE_BUFFER;
  if(newsize<DEFAULT_BORROW_TABLE_SIZE) newsize=DEFAULT_BORROW_TABLE_SIZE;
  PERDIO_DEBUG3(TABLE,"TABLE:compactify borrow old:%d no_used:%d new:%d",
		size,no_used,newsize);
  PERDIO_DEBUG_DO1(TABLE2,printTables());
  BorrowEntry *oldarray=array;
  array = (BorrowEntry*) malloc(newsize*sizeof(BorrowEntry));
  if(array==NULL){
    PERDIO_DEBUG(TABLE,"TABLE: compactify borrow NOT POSSIBLE");
    array=oldarray;
    return;}
  int oldsize=size;
  size=newsize;
  copyBorrowTable(oldarray,oldsize);
  PERDIO_DEBUG(TABLE,"TABLE:compactify borrow complete");
  PERDIO_DEBUG_DO1(TABLE2,printTables());
  return;}

void BorrowTable::resize(){
  Assert(no_used==size);
  int newsize = int (TABLE_EXPAND_FACTOR*size);
  PERDIO_DEBUG3(TABLE,"TABLE:resize borrow old:%d no_used:%d new:%d",
		size,no_used,newsize);
  PERDIO_DEBUG_DO1(TABLE2,printTables());
  BorrowEntry *oldarray=array;
  array = (BorrowEntry*) malloc(newsize*sizeof(BorrowEntry));
  Assert(array!=NULL);
  if(array==NULL){
    error("Memory allocation: Borrow Table growth not possible");}
  int oldsize=size;
  size=newsize;
  copyBorrowTable(oldarray,oldsize);
  PERDIO_DEBUG(TABLE,"TABLE:resize borrow complete");
  PERDIO_DEBUG_DO1(TABLE2,printTables());
  return;}

int BorrowTable::newBorrow(Credit c,int sd,int off){
  if(nextfree == END_FREE) resize();
  int index=nextfree;
  nextfree= array[index].entry.nextfree;
  BorrowEntry* oe = &(array[index]);
  oe->initBorrow(c,sd,off);
  PERDIO_DEBUG4(HASH2,"HASH2:<SPECIAL>:net=%x borrow=%x owner=%x hash=%x",
		oe->getNetAddress(),array,ownerTable->array,
		hshtbl->table);
  hshtbl->add(oe->getNetAddress(),index);
  no_used++;
  PERDIO_DEBUG1(TABLE,"TABLE:borrow insert: %d",index);
  return index;}

void BorrowTable::maybeFreeBorrowEntry(int index){
  BorrowEntry *b = &(array[index]);
  if(b->isPending()) return; /* cannot remove as msgs pending on credit */
  b->freeBorrowEntry();  
  hshtbl->sub(getBorrow(index)->getNetAddress());
  array[index].credit=FREE_TAG;
  array[index].entry.nextfree=nextfree;
  nextfree=index;
  no_used--;
  PERDIO_DEBUG1(TABLE,"TABLE:borrow delete: %d",index);
  return;}

void BorrowTable::copyBorrowTable(BorrowEntry *oarray,int osize){
  hshtbl->clear();
  int oindex=0;
  int nindex= 0;
  BorrowEntry *ob,*nb;
  while(TRUE){
    nb=&(array[nindex]);
    ob= &(oarray[oindex]);
    while(ob->isFree()) {
      oindex++;
      if(oindex>=osize) goto fin;
      ob= &(oarray[oindex]);}
    nb->copyBorrow(ob,nindex);
    hshtbl->add(nb->getNetAddress(),nindex);
    nindex++;
    oindex++;
    if(oindex>=osize) goto fin;}
fin:
  nextfree=END_FREE;
  init(nindex,size);
  free(oarray);
}

#ifdef DEBUG_PERDIO
void BorrowTable::print(){
  printf("***********************************************\n");
  printf("********* BORROW TABLE *************************\n");
  printf("***********************************************\n");
  printf("Size:%d No_used:%d \n",size,no_used);
  int i;
  BorrowEntry *b;
  for(i=0;i<size;i++){
    if(!(array[i].isFree())){
      b=getBorrow(i);
      printf("<%d> BORROW: Credit:%d net=%d:%d pend%d\n",i,b->getCredit(),
	     b->getNetAddress()->site,b->getNetAddress()->index,
	     b->pendLink);}
    else{
      printf("<%d> FREE: next:%d\n",i,array[i].entry.nextfree);}}
  printf("-----------------------------------------------\n");  
}

#endif

BorrowTable *borrowTable;

int borrowEntryToIndex(BorrowEntry *b){return borrowTable->ptr2Index(b);}

#ifdef DEBUG_PERDIO

void printTables(){
  ownerTable->print();
  borrowTable->print();
  borrowTable->hshtbl->print();}

void resize_hash(){
  borrowTable->hshtbl->print();}

#endif

/* ********************************************************************** */
/*                  HASH TABLE methods
/* ********************************************************************** */

inline NetAddress * GenHashNode2NetAddr(GenHashNode *ghn){
  NetAddress *na;
  Cast(ghn->getBaseKey(),GenHashBaseKey*,na,NetAddress*);
  return na;}

inline int GenHashNode2BorrowIndex(GenHashNode *ghn){
  int i;
  Cast(ghn->getEntry(),GenHashEntry*,i,int);
  Assert(i>=0);
  Assert(i<borrowTable->getSize());
  return i;}

inline Bool NetHashTable::findPlace(int hvalue,NetAddress *na,GenHashNode *&ghn){
  PERDIO_DEBUG3(HASH,"HASH:find Place hvalue=%d, net%d:%d",hvalue,
	       na->site,na->index);
  ghn=htFindFirst(hvalue);
  NetAddress *na2;
  while(ghn!=NULL){
    na2=GenHashNode2NetAddr(ghn);
    if(na->same(na2)){
      PERDIO_DEBUG4(HASH,"HASH:compare success hvalue=%d bk=%x net%d:%d",
		    ghn->getKey(),ghn->getBaseKey,na2->site,na2->index);
      return TRUE;}
    PERDIO_DEBUG4(HASH,"HASH:compare fail hvalue=%d bk=%x net%d:%d",
		  ghn->getKey(),ghn->getBaseKey,na2->site,na2->index);
    ghn=htFindNext(ghn,hvalue);}
  return FALSE;}

int NetHashTable::hashFunc(NetAddress *na){
  unsigned char *p = (unsigned char*) na;
  int i;
  unsigned h = 0, g;

  for(i=0;i<8; i++,p++) {
    h = (h << 4) + (*p);
    if ((g = h & 0xf0000000)) {
      h = h ^ (g >> 24);
      h = h ^ g;}}
  return (int) h;}

BorrowEntry *NetHashTable::findNA(NetAddress *na){
  GenHashNode *ghn;
  int bindex;
  int hvalue=hashFunc(na);
  if(findPlace(hvalue,na,ghn)){
    int bindex= GenHashNode2BorrowIndex(ghn);
    PERDIO_DEBUG1(HASH,"HASH:borrow index %d",bindex);
    return borrowTable->getBorrow(bindex);}
  return NULL;}

void NetHashTable::add(NetAddress *na,int bindex){
  int hvalue=hashFunc(na);
  GenHashNode *ghn;
  Assert(!findPlace(hvalue,na,ghn));
  PERDIO_DEBUG4(HASH,"HASH:adding hvalue=%d net=%d:%d bindex=%d",
	       hvalue,na->site,na->index,bindex);
  GenHashBaseKey* ghn_bk;
  GenHashEntry* ghn_e;
  Cast(na,NetAddress*,ghn_bk,GenHashBaseKey*);
  Cast(bindex,int,ghn_e,GenHashEntry*);  
  htAdd(hvalue,ghn_bk,ghn_e);}

void NetHashTable::sub(NetAddress *na){
  int hvalue=hashFunc(na);
  GenHashNode *ghn;
  findPlace(hvalue,na,ghn);
  PERDIO_DEBUG3(HASH,"HASH:deleting hvalue=%d net=%d:%d bindex=%d",
	       hvalue,na->site,na->index);
  htSub(hvalue,ghn);}

#ifdef DEBUG_PERDIO
void NetHashTable::print(){
  int limit=getSize();
  int used=getUsed();
  int i;
  printf("******************************************************\n");
  printf("************* NetAddress Hash Table *****************\n");
  printf("******************************************************\n");
  printf("Size:%d Used:%d\n",limit,used);
  GenHashNode *ghn;
  NetAddress *na;
  for(i=0;i<limit;i++){
    ghn=getElem(i);
    if(ghn!=NULL){
      na=GenHashNode2NetAddr(ghn);
      printf("<%d> - net%d:%d\n",i,na->site,na->index);
      ghn=ghn->getNext();
      while(ghn!=NULL){
	na=GenHashNode2NetAddr(ghn);
	printf("<coll> - net%d:%d\n",na->site,na->index);
	ghn=ghn->getNext();}}}
  printf("-----------------------------------\n");
}
#endif

/**********************************************************************/
/**********************************************************************/
/*                      GARBAGE COLLECTION
/**********************************************************************/
/**********************************************************************/

/* OBS: ---------- interface to gc.cc ----------*/

void gcOwnerTable() { ownerTable->gcOwnerTable();}
void gcBorrowTable(){ borrowTable->gcBorrowTable();}

void gcPortProxy(PortProxy *pp){
  int i=pp->getIndex();
  borrowTable->getBorrow(i)->makeMark();
  PERDIO_DEBUG3(GC,"GC-relocate borrow:%d old%x new %x",
	       i,borrowTable->getBorrow(i),pp);
  (borrowTable->getBorrow(i))->setBorrowObject(pp);}

void gcPortManager(PortManager *pm){
  int i=pm->getIndex();
  PERDIO_DEBUG3(GC,"GC-relocate owner:%d old%x new %x",
	       i,ownerTable->getOwner(i),pm);
  ownerTable->getOwner(i)->setOwnerObject(pm);}

/*--------------------*/

void OwnerTable::gcOwnerTable()
{
  PERDIO_DEBUG(GC,"GC:owner gc");
  int i;
  for(i=0;i<size;i++){
      OwnerEntry* o = ownerTable->getOwner(i);
      if(!(o->isFree())){
	o->entry.object = (ProtocolObject*) o->entry.object->gcConstTerm();}}
  compactify();
  return;
}

/* OBSERVE - this must done at the end of other gc */
void BorrowTable::gcBorrowTable()
{
  PERDIO_DEBUG(GC,"GC:borrow gc");
  BorrowEntry *b;
  int i;
  for(i=0;i<size;i++){
    b=borrowTable->getBorrow(i);
    if(!(b->isFree())){
      if(b->gcCheck()) borrowTable->maybeFreeBorrowEntry(i);}}
  compactify();
  hshtbl->compactify();
}

/**********************************************************************/
/**********************************************************************/
/*                      GLOBALIZING                                   */
/**********************************************************************/
/**********************************************************************/

PortManager *PortLocal::globalize()
{ 
  PERDIO_DEBUG(GLOBALIZING,"GLOBALIZING:port");
  Assert(sizeof(PortManager)==sizeof(PortLocal));
  PortManager *pm= (PortManager*) this;
  pm->setTertType(Te_Manager);
  int i = ownerTable->newOwner(pm);
  pm->setIndex(i);
  return pm;
}

/**********************************************************************/
/**********************************************************************/
/*                      LOCALIZING
/**********************************************************************/
/**********************************************************************/

PortLocal *PortManager::localize()
{ 
  PERDIO_DEBUG(GLOBALIZING,"GLOBALIZING: localizing port");
  Assert(sizeof(PortManager)==sizeof(PortLocal));
  PortLocal *pl= (PortLocal*) this;
  pl->setTertType(Te_Local);
  pl->setBoard(am.rootBoard);
  return pl;
}

/**********************************************************************/
/**********************************************************************/
/*                            MARSHALLING STUFF                       */
/**********************************************************************/
/**********************************************************************/



typedef enum {SMALLINTTAG, BIGINTTAG, FLOATTAG, ATOMTAG, 
	      RECORDTAG, TUPLETAG, LISTTAG, REFTAG, 
	      OWNERTAG, BORROWTAG, 
	      PORTTAG} MarshallTag;

int unmarshallWithDest(BYTE *buf, int len, OZ_Term *t);
void unmarshallNoDest(BYTE *buf, int len, OZ_Term *t);

/**********************************************************************/
/*                Help-Classes for Marshalling                        */
/**********************************************************************/


void ByteStream::resize()
{
  Assert(0);
  PERDIO_DEBUG(AUXILLARY,"AUXILLARY:resizing bytestream");
  Assert(size>0);
  int oldsize = size;
  BYTE *oldarray = array;
  BYTE *oldpos = pos;
  size = (size*3)/2;
  array = new BYTE[size];
  pos = array;
  for (BYTE *s=oldarray; s<oldpos;) {
    *pos++ = *s++;
  }
  delete oldarray;
}

int refCounter = 0;

class RefTable {
  OZ_Term *array;
  int size;
public:
  RefTable()
  {
    size = 100;
    array = new OZ_Term[size];
  }
  OZ_Term get(int i)
  {
    Assert(i<size);
    return array[i];
  }
  void set(int pos, OZ_Term val)
  {
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

RefTable *refTable;

class RefTrail: public Stack {
public:
  RefTrail() : Stack(200,Stack_WithMalloc) { }
  void trail(OZ_Term *t)
  {
    push(t);
    push(ToPointer(*t));
  }
  void unwind()
  {
    while(!isEmpty()) {
      OZ_Term oldval = ToInt32(pop());
      OZ_Term *loc = (OZ_Term*) pop();
      *loc = oldval;
    }
  }
};

RefTrail *refTrail;

class DebtRec:public Stack {
public:
  DebtRec():Stack(200,Stack_WithMalloc) {}

  void handler(PendEntry *pe){
    BorrowEntry *b;
    int debt,bi;
    Assert(!isEmpty());
    while(!isEmpty()) {
      bi = (int) pop();
      Assert(!isEmpty());
      debt = (int) pop();
      b=borrowTable->getBorrow(bi);
      b->inDebtSec(debt,pe);}
    return;}
  
  void debtPush(int debt,int i){ 
    PERDIO_DEBUG1(DEBT,"DEBT:push %d",i);
    push((void *)debt);
    push((void *)i);
  }
};


DebtRec* debtRec;


/**********************************************************************/
/*                 MARSHALLING/UNMARSHALLING  GROUND STRUCTURES       */
/**********************************************************************/

const int intSize = sizeof(int32); 

inline
void marshallNumber(unsigned int i, ByteStream *bs)
{
  for (int k=0; k<intSize; k++) {
    bs->put(i&0xFF);
    i = i>>8;}
}

inline
int unmarshallNumber(ByteStream *bs)
{
  unsigned int i1 = bs->get();
  unsigned int i2 = bs->get();
  unsigned int i3 = bs->get();
  unsigned int i4 = bs->get();
  return (int) (i1 + (i2<<8) + (i3<<16) + (i4<<24));
}

class DoubleConv {
public:
  union {
    int32 i[2];
    double d;
  } u;
};

inline
void marshallFloat(double d, ByteStream *bs)
{
  static DoubleConv dc;
  dc.u.d = d;
  marshallNumber(dc.u.i[0],bs);
  marshallNumber(dc.u.i[1],bs);
}

inline
double unmarshallFloat(ByteStream *bs)
{
  static DoubleConv dc;
  dc.u.i[0] = unmarshallNumber(bs);
  dc.u.i[1] = unmarshallNumber(bs);
  return dc.u.d;
}

inline
char *unmarshallString(ByteStream *bs)
{
  int i = unmarshallNumber(bs);
  char *ret = new char[i+1];  /* TODO: ask Ralph */
  for (int k=0; k<i; k++) {
    ret[k] = bs->get();
  }
  ret[i] = '\0';
  return ret;
}

inline
void marshallString(char *s, ByteStream *bs)
{
  marshallNumber(strlen(s),bs);
  while(*s) {
    bs->put(*s);
    s++;  }
}

/**********************************************************************/
/*            MARSHALLING/UNMARSHALLING NETWORK ADDRESSES             */
/**********************************************************************/

void unmarshallTerm(ByteStream*,OZ_Term*);

inline
void marshallCredit(Credit credit,ByteStream *bs){
  Assert(sizeof(Credit)==sizeof(long));
  PERDIO_DEBUG1(MARSHALL,"MARSHALL-credit c=%d",credit);
  marshallNumber(credit,bs);}

inline
Credit unmarshallCredit(ByteStream *bs){
  Assert(sizeof(Credit)==sizeof(long));
  Credit c=unmarshallNumber(bs);
  PERDIO_DEBUG1(UNMARSHALL,"UNMARSHALL-credit c=%d",c);
  return c;}

inline
void marshallSite(int sd,ByteStream *bs){
  PERDIO_DEBUG1(MARSHALL,"MARSHALL-site id=%d",sd);
  char *host;
  int port, timestamp;
  getSite(sd,host,port,timestamp);
  marshallString(host,bs);
  marshallNumber(port,bs);
  marshallNumber(timestamp,bs);}

inline
void marshallMySite(ByteStream *bs){
  marshallSite(localSite,bs);}

inline
void marshallNetAddress2(int site,int index,ByteStream *bs){
  marshallSite(site,bs);
  marshallNumber(index,bs);}

inline
void marshallNetAddress(NetAddress *a, ByteStream *bs){
  marshallNetAddress2(a->site,a->index,bs);}

inline
int unmarshallSiteId(ByteStream *bs){
  char *host = unmarshallString(bs);
  int port = unmarshallNumber(bs);
  int timestamp = unmarshallNumber(bs);
  int sd=lookupSite(host,port,timestamp);
  PERDIO_DEBUG1(UNMARSHALL,"UNMARSHALL-site id=%d",sd);
  return sd;}

void marshallOwnHead(int i,ByteStream *bs){
  OwnerEntry *o=ownerTable->getOwner(i);
  marshallNetAddress2(localSite,i,bs);
  marshallNumber(o->getSendCredit(),bs);
  PERDIO_DEBUG2(MARSHALL,"MARSHALL:owned=%d remCredit=%d ",i,o->getCredit());
}

void marshallToOwner(int bi,ByteStream *bs,DebtRec *dr){
  bs->put(OWNERTAG);
  marshallNumber(borrowTable->getOriginIndex(bi),bs);
  BorrowEntry *b=borrowTable->getBorrow(bi); /* implicit 1 credit */
  if(b->getOneCredit()) {  
    PERDIO_DEBUG2(MARSHALL,"MARSHALL:toOwner Borrow:%d Owner:%d",
		  bi,borrowTable->getOriginIndex(bi));
    return;}
  dr->debtPush(1,bi);
  PERDIO_DEBUG2(MARSHALL,"MARSHALL:toOwner Borrow:%d Owner:%d debt=1",
		bi,borrowTable->getOriginIndex(bi));
  return;}

void marshallBorrowHead(int bi,ByteStream *bs,DebtRec *dr){
  BorrowEntry *b=borrowTable->getBorrow(bi);
  bs->put(BORROWTAG);
  marshallNetAddress(b->getNetAddress(),bs);
  Credit cred;
  if(b->getSmallCredit(cred)) {
    PERDIO_DEBUG3(MARSHALL,"MARSHALL:borrowed %d remCredit=%d give=%d",
		bi,b->getCredit(),cred);
    marshallCredit(cred,bs);
    return;  }
  PERDIO_DEBUG3(MARSHALL,"MARSHALL:borrowed %d remCredit=%d debt=",
	       bi,b->getCredit(),MIN_BORROW_CREDIT_SIZE);
  marshallCredit(MIN_BORROW_CREDIT_SIZE,bs);
  dr->debtPush(MIN_BORROW_CREDIT_SIZE,bi);
  return;}

Bool unmarshallBorrow(ByteStream *bs,BorrowEntry *&b,int &bi){
  int sd=unmarshallSiteId(bs);
  int si=unmarshallNumber(bs);
  Credit cred = unmarshallCredit(bs);
  PERDIO_DEBUG3(UNMARSHALL,"UNMARSHALL:borrowed sd:%d si=%d cr=%d",sd,si,cred);
  Assert(sd!=localSite);
  NetAddress na = NetAddress(sd,si); 
  int hindex;
  b = borrowTable->find(&na);
  if (b!=NULL) {
    PERDIO_DEBUG(UNMARSHALL,"UNMARSHALL:borrowed hit");
    b->addCredit(cred);
    return TRUE;  }
  bi=borrowTable->newBorrow(cred,sd,si);
  b=borrowTable->getBorrow(bi);
  PERDIO_DEBUG(UNMARSHALL,"UNMARSHALL:borrowed miss");
  return FALSE;  }

int unmarshallOwn(ByteStream *bs,OwnerEntry *&oe){
  int si=unmarshallNumber(bs);
  PERDIO_DEBUG1(UNMARSHALL,"UNMARSHALL:own %d",si);
  oe=ownerTable->getOwner(si);
  oe->returnOneCredit();
  return si;
}

void unmarshallNoDest(BYTE *buf, int len, OZ_Term *t){
  ByteStream *bs = new ByteStream(buf,len);
  OZ_Term ret;
  refCounter = 0;
  unmarshallTerm(bs,t);
  bs->endCheck();
  delete bs;}

int unmarshallWithDest(BYTE *buf, int len, OZ_Term *t){
  ByteStream *bs = new ByteStream(buf,len);
  OZ_Term ret;
  refCounter = 0;
  int dest = unmarshallNumber(bs);
  unmarshallTerm(bs,t);
  bs->endCheck();
  delete bs;
  return dest;}

/**********************************************************************/
/*                 MARSHALLING terms                                  */
/**********************************************************************/


#define CheckCycle(expr)			\
  {						\
    OZ_Term t = expr;				\
    if (!isRef(t) && tagTypeOf(t)==GCTAG) {	\
      PERDIO_DEBUG(MARSHALL,"MARSHALL:circular");	\
      bs->put(REFTAG);				\
      marshallNumber(t>>tagSize,bs);		\
      return;					\
    }						\
  }

inline
void trailCycle(OZ_Term *t)
{					
  refTrail->trail(t);
  *t = (refCounter<<tagSize)|GCTAG;
  refCounter++;
}

void marshallTerm(int sd,OZ_Term t, ByteStream *bs, DebtRec *dr)
{
  OZ_Term *args;
  int argno;

loop:
  t = deref(t);
  switch(tagTypeOf(t)) {

  case SMALLINT:
    bs->put(SMALLINTTAG);
    marshallNumber(smallIntValue(t),bs);
    PERDIO_DEBUG(MARSHALL,"MARSHALL:small int");
    return;

  case OZFLOAT:
    bs->put(FLOATTAG);
    marshallFloat(tagged2Float(t)->getValue(),bs);
    PERDIO_DEBUG(MARSHALL,"MARSHALL:float");
    return;

  case BIGINT:
    bs->put(BIGINTTAG);
    marshallString(toC(t),bs);
    PERDIO_DEBUG(MARSHALL,"MARSHALL:big int");
    return;

  case LITERAL:
    bs->put(ATOMTAG);
    marshallString(tagged2Literal(t)->getPrintName(),bs);
    PERDIO_DEBUG(MARSHALL,"MARSHALL:atom");
    return;

  case LTUPLE:
    {
      LTuple *l = tagged2LTuple(t);
      CheckCycle(*l->getRef());
      bs->put(LISTTAG);
      argno = 2;
      args  = l->getRef();
      PERDIO_DEBUG(MARSHALL,"MARSHALL:list");
      goto processArgs;
    }

  case SRECORD:
    {
      SRecord *rec = tagged2SRecord(t);
      CheckCycle(*rec->getRef()); /* TODO mark instead of getRef ??*/
      if (rec->isTuple()) {
	bs->put(TUPLETAG);
	marshallNumber(rec->getTupleWidth(),bs);
      } else {
	bs->put(RECORDTAG);
	marshallTerm(sd,rec->getArityList(),bs,dr);
      }
      marshallTerm(sd,rec->getLabel(),bs,dr);
      argno = rec->getWidth();
      args  = rec->getRef();
      PERDIO_DEBUG1(MARSHALL,"MARSHALL:record-tuple no:%d",argno);
      goto processArgs;
    }

  case OZCONST:
    {
      PERDIO_DEBUG(MARSHALL,"MARSHALL:constterm");
      Tertiary *ptr = tagged2Tert(t);
      switch(ptr->getType()) {
      case Co_Port:
	{
	  if(isProxy(ptr)) {
	      PortProxy *pp=(PortProxy*) ptr;
	      PERDIO_DEBUG(MARSHALL,"MARSHALL:port proxy");
	      int bi=pp->getIndex();
	      if(borrowTable->getOriginSite(bi)==sd){
		marshallToOwner(bi,bs,dr);
		return;}
	      marshallBorrowHead(bi,bs,dr);
	      bs->put(PORTTAG);
	      return;}
	  bs->put(BORROWTAG);
	  PortManager *pm;
	  if(isLocal(ptr)){
	    PERDIO_DEBUG(MARSHALL,"MARSHALL:port local");
	    pm=((PortLocal *)ptr)->globalize();}
	  else {
	    PERDIO_DEBUG(MARSHALL,"MARSHALL:port manager");
	    pm= (PortManager *)ptr;
	    CheckCycle(*(pm->getRef()));}
	  trailCycle(pm->getRef());
	  marshallOwnHead(pm->getIndex(),bs);
	  bs->put(PORTTAG);
	  return;
	}	    
      default:
	warning("Cannot marshall generic %s",toC(t));
	marshallTerm(sd,nil(),bs,dr);
	return;
      }
    }
    // no break here
  default:
    if (isAnyVar(t)) {
      warning("Cannot marshall variables");
    } else {
      warning("Cannot marshall %s",toC(t));
    }
    marshallTerm(sd,nil(),bs,dr);
    return;
  }

processArgs:
  OZ_Term arg0 = *args;
  trailCycle(args);
  marshallTerm(sd,arg0,bs,dr);
  args++;
  if (argno == 1) return;
  for(int i=1; i<argno-1; i++) {
    marshallTerm(sd,*args,bs,dr);
    args++;
  }
  // tail recursion optimization
  t = *args;
  goto loop;
}



/**********************************************************************/
/*                 UNMARSHALLING terms                                */
/**********************************************************************/

void unmarshallTerm(ByteStream *bs, OZ_Term *ret)
{
  int argno;
loop:
  MarshallTag tag = (MarshallTag) bs->get();

  switch(tag) {

  case SMALLINTTAG: 
    *ret = OZ_int(unmarshallNumber(bs)); 
    PERDIO_DEBUG(UNMARSHALL,"UNMARSHALL:small int");
    return;

  case FLOATTAG:    
    *ret = OZ_float(unmarshallFloat(bs)); 
    PERDIO_DEBUG(UNMARSHALL,"UNMARSHALL:float");
    return;

  case ATOMTAG:     
    {
      char *aux = unmarshallString(bs);
      *ret = OZ_atom(aux);
      delete aux;
      PERDIO_DEBUG1(UNMARSHALL,"UNMARSHALL:atom %s",aux);
      return;
    }

  case BIGINTTAG:
    {
      char *aux = unmarshallString(bs);
      *ret = OZ_CStringToNumber(aux);
      delete aux;
      PERDIO_DEBUG(UNMARSHALL,"UNMARSHALL:big int");
      return;
    }

  case LISTTAG:
    {
      OZ_Term head, tail;
      argno = 2;
      LTuple *l = new LTuple();
      *ret = makeTaggedLTuple(l);
      refTable->set(refCounter++,*ret);
      ret = l->getRef();
      PERDIO_DEBUG(UNMARSHALL,"UNMARSHALL:list");
      goto processArgs;
    }
  case TUPLETAG:
    {
      argno = unmarshallNumber(bs);
      TaggedRef label;
      unmarshallTerm(bs,&label);
      SRecord *rec = SRecord::newSRecord(label,argno);
      *ret = makeTaggedSRecord(rec);
      refTable->set(refCounter++,*ret);
      ret = rec->getRef();
      PERDIO_DEBUG1(UNMARSHALL,"UNMARSHALL:tuple no_args:%d",argno)
      goto processArgs;      
    }

  case RECORDTAG:
    {
      TaggedRef arity;
      unmarshallTerm(bs,&arity);
      argno = length(arity);
      TaggedRef label;
      unmarshallTerm(bs,&label);
      SRecord *rec = SRecord::newSRecord(label,mkArity(arity));
      *ret = makeTaggedSRecord(rec);
      refTable->set(refCounter++,*ret);
      ret = rec->getRef();
      PERDIO_DEBUG1(UNMARSHALL,"UNMARSHALL:record no:%d",argno)
      goto processArgs;      
    }

  case REFTAG:
    {
      PERDIO_DEBUG(UNMARSHALL,"UNMARSHALL:ref");
      int i = unmarshallNumber(bs);
      *ret = refTable->get(i);
      return;
    }

  case OWNERTAG:
    {
      OwnerEntry *oe;
      int si=unmarshallOwn(bs,oe);
      PERDIO_DEBUG1(UNMARSHALL,"UNMARSHALL:owner=%d",si);
      *ret=makeTaggedConst(oe->getObject());
      return;}

  case BORROWTAG:
    {
      BorrowEntry *b;
      int bi;
      if(unmarshallBorrow(bs,b,bi)) {
	PERDIO_DEBUG(UNMARSHALL,"UNMARSHALL:borrow hit");
	*ret=makeTaggedConst(b->getObject());
	tag = (MarshallTag) bs->get();
	switch(tag){
	case PORTTAG:return;
	default:
	  /* TODO proper exception */
	  printf("unmarshall: unexpected tag: %d\n",tag);
	  Assert(0);
	  *ret = nil();
	  return;}
	Assert(0);
	return;}
      PERDIO_DEBUG(UNMARSHALL,"UNMARSHALL:borrow miss");
      tag = (MarshallTag) bs->get();
      switch(tag){
      case PORTTAG:{
	PERDIO_DEBUG(UNMARSHALL,"UNMARSHALL:port proxy");
	PortProxy *pp=new PortProxy(bi);
	*ret= makeTaggedConst(pp);
	b->setBorrowObject(pp);
	return;}
      default:
	/* TODO - proper exception */
	printf("unmarshall: unexpected tag: %d\n",tag);
	Assert(0);
	*ret = nil();
	return;}
    }
  default:
    printf("unmarshall: unexpected tag: %d\n",tag); 
    Assert(0);
    *ret = nil();
    return;
  }

processArgs:
  for(int i=0; i<argno-1; i++) {
    unmarshallTerm(bs,ret++);
  }
  // tail recursion optimization
  goto loop;
}


/**********************************************************************
**********************************************************************
                            MAIN RECEIVE                            
SITESEND       term                                             
PORTSEND       oindex  term             implicit 1 credit        
OWNER_CREDIT   oindex  credit     
BORROW_CREDIT  netaddr credit     
ASK_FOR_CREDIT oindex site              implicit 1 credit        
                                                                
    term :=      ....                                                
                 BORROWTAG netaddr credit d-term                           
	    
    d-term :=    PORTTAG

    netaddr:=    site index
    site    :=   host port timestamp
            
            
**********************************************************************
**********************************************************************/

OZ_Term ozport=0;

void siteReceive(BYTE *msg,int len)
{
  OZ_Term recvPort;

  switch (msg[0]) {
  case SITESEND:
    {
      PERDIO_DEBUG(MSG_RECEIVED,"MSG_RECEIVED:SITESEND");
      OZ_Term t;
      unmarshallNoDest(msg+1,len-1,&t);
      if (!t) {
	if (ozconf.debugPerdio) {
	  printf("siteReceive: message SITESEND:");
	  printBytes(msg,len);
	}
	OZ_fail("siteReceive: SITESEND unmarshall failed\n");
      }
      if (ozconf.debugPerdio) {
	printf("siteReceive: SITESEND '%s'\n",OZ_toC(t,10,10));
      }
      sendPort(ozport,t);
      break;
    }
  case PORTSEND:    /* PORTSEND index term */
    {
      PERDIO_DEBUG(MSG_RECEIVED,"MSG_RECEIVED:PORTSEND");
      OZ_Term t;
      int portIndex;
      portIndex=unmarshallWithDest(msg+1,len-1,&t);
      if (!t) {
	if (ozconf.debugPerdio) {
	  printf("siteReceive: message PORTSEND:");
	  printBytes(msg,len);
	}
	OZ_fail("siteReceive: PORTSEND unmarshall failed\n");
      }
      if (ozconf.debugPerdio) {
	printf("siteReceive: PORTSEND '%s'\n",OZ_toC(t,10,10));
      }
      OwnerEntry *oe=ownerTable->getOwner(portIndex);
      ownerTable->receiveReturnCredit(portIndex,1);
      Tertiary *tert= oe->getObject();
      if(!(tert->checkTertiary(Co_Port,Te_Manager)))
	{OZ_fail("siteReceive: PORTSEND not to port\n");}
      sendPort(makeTaggedConst(tert),t);
      break;
      }
  case ASK_FOR_CREDIT:
    {
      PERDIO_DEBUG(MSG_RECEIVED,"MSG_RECEIVED:ASK_FOR_CREDIT");
      ByteStream *bs=new ByteStream(msg+1,len-1);
      int na_index=unmarshallNumber(bs);
      OwnerEntry *o=ownerTable->getOwner(na_index);
      int rsite=unmarshallSiteId(bs);
      Assert(rsite>=0);
      bs->endCheck();
      o->returnOneCredit(); /* cannot be full credit */
      delete bs;
      Credit c= o->giveMoreCredit();
      ByteStream *bs1=new ByteStream();
      bs1->put(BORROW_CREDIT);
      NetAddress na = NetAddress(localSite,na_index);
      marshallNetAddress(&na,bs1);
      marshallCredit(c,bs1);
      PERDIO_DEBUG1(MSG_SENT,"MSG_SENT:BORROW_CREDIT %d",c);
      int ret=reliableSend0(rsite,bs1);
      Assert(ret==PROCEED); // TODO
      delete bs1;
      break;
    }
  case OWNER_CREDIT:  
    {
      ByteStream *bs=new ByteStream(msg+1,len-1);
      int index=unmarshallNumber(bs);
      Credit c=unmarshallCredit(bs);
      PERDIO_DEBUG1(MSG_RECEIVED,"MSG_RECEIVED:OWNER_CREDIT %d",c);
      bs->endCheck();
      ownerTable->receiveReturnCredit(index,c);
      break;
    }
  case BORROW_CREDIT:  
    {
      ByteStream *bs=new ByteStream(msg+1,len-1);
      int sd=unmarshallSiteId(bs);
      Assert(sd>=0);
      int si=unmarshallNumber(bs);
      Credit c=unmarshallCredit(bs);
      NetAddress na=NetAddress(sd,si);
      PERDIO_DEBUG1(MSG_RECEIVED,"MSG_RECEIVED:BORROW_CREDIT %d",c);
      bs->endCheck();
      BorrowEntry *b=borrowTable->find(&na);
      Assert(b!=NULL);
      b->addAskCredit(c);
      break;
    }
  default:
    OZ_fail("siteReceive: unknown message %d\n",msg[0]);
    printf("\n--\n%s\n--\n",msg);
    break;
  }
}


/* ********************************************************************** */
/* ********************************************************************** */
/*              BUILTINS                                                  */
/* ********************************************************************** */
/* ********************************************************************** */

void domarshallTerm(int sd,OZ_Term t, ByteStream *bs)
{
  refCounter = 0;
  marshallTerm(sd,t,bs,debtRec);
  refTrail->unwind();
}

inline
int reliableSend0(int sd, ByteStream *bs){
  int ret=reliableSend(sd,bs->getPtr(),bs->getLen());
  if (ret != 0) {
    return OZ_raiseC("ip",2,OZ_atom("send"),
		     OZ_atom(OZ_unixError(lastIpError())));
  }
  return PROCEED;
}


/* engine-interface */
int remoteSend(PortProxy *p, TaggedRef msg) {
  BorrowEntry *b= borrowTable->getBorrow(p->getIndex());
  NetAddress *na = b->getNetAddress();
  ByteStream *bs = new ByteStream();
  PendEntry *pe;
  int site = na->site;
  int index = na->index;

  if(!(b->getOneCredit())){
      PERDIO_DEBUG(DEBT_MAIN,"DEBT_MAIN:remoteSend");
      pe= pendEntryManager->newPendEntry();
      pe->initialize(bs,site,b);
      b->inDebtMain(pe);}
  else pe=NULL;

  bs->put(PORTSEND);                    
  marshallNumber(index,bs);               
  domarshallTerm(site,msg,bs);
  if(pe==NULL){
    if(debtRec->isEmpty()){
      int ret = reliableSend0(site,bs);
      delete bs;
      return ret;}
    PERDIO_DEBUG(DEBT_SEC,"DEBT_SEC:remoteSend");
    pe=pendEntryManager->newPendEntry();
    pe->initialize(bs,site);
    debtRec->handler(pe);
    return PROCEED;}
  if(debtRec->isEmpty()){
    return PROCEED;}
  debtRec->handler(pe);  
  return PROCEED;
}

int sitesend(int sd,OZ_Term t){
  ByteStream *bs = new ByteStream();
  bs->put(SITESEND);
  domarshallTerm(sd,t,bs);
  if(debtRec->isEmpty()){
    int ret = reliableSend0(sd,bs);
    delete bs;
    return ret;
  }
  PendEntry *pe;
  pe= pendEntryManager->newPendEntry();
  pe->initialize(bs,sd);
  debtRec->handler(pe);
  return PROCEED;
}

/* ********************************************************************** */
/*              BUILTINS themselves                                       */
/* ********************************************************************** */

OZ_C_proc_begin(BIreliableSend3,3)
{
  PERDIO_DEBUG(SEND_EMIT,"SEND_EMIT:reliable send3");
  OZ_declareIntArg(0,sd);
  OZ_declareArg(1,value);
  OZ_declareArg(2,out);

  if(sitesend(sd,value)==0) {
    PERDIO_DEBUG(SEND_DONE,"SEND_DONE:reliable send3");
    return OZ_unifyAtom(out,"true");}
  return OZ_unifyAtom(out,"false"); 
}
OZ_C_proc_end

OZ_C_proc_begin(BIreliableSend,2)
{
  PERDIO_DEBUG(SEND_EMIT,"SEND_EMIT:reliable send");
  OZ_declareIntArg(0,sd);
  OZ_declareArg(1,value);

  int ret = sitesend(sd,value);
  if (ret == PROCEED) {
    PERDIO_DEBUG(SEND_DONE,"SEND_DONE:reliable send");
  }
  return ret;
}
OZ_C_proc_end


OZ_C_proc_begin(BIunreliableSend,2)
{
  PERDIO_DEBUG(SEND_EMIT,"SEND_EMIT:unreliable send");
  OZ_declareIntArg(0,sd);
  OZ_declareArg(1,value);

  ByteStream *bs=new ByteStream();
  bs->put(SITESEND);
  domarshallTerm(sd,value,bs);
  if(debtRec->isEmpty()){
    int ret=unreliableSend(sd,bs->getPtr(),bs->getLen());
    delete bs;
    if(ret==0){
      PERDIO_DEBUG(SEND_DONE,"SEND_DONE:reliable send");
      return PROCEED;}
    return OZ_raiseC("unreliableSend",1);
  }
  PERDIO_DEBUG(DEBT,"DEBT:unreliableSend");
  PendEntry *pe;
  pe=pendEntryManager->newPendEntry();
  pe->initialize(bs,sd);
  debtRec->handler(pe);
  return PROCEED;
}
OZ_C_proc_end


OZ_C_proc_begin(BIstartSite,2)
{
  OZ_declareIntArg(0,p);
  OZ_declareArg(1,stream);

  if (ozport!=0) {
    return OZ_raise(OZ_mkTupleC("perdio",1,OZ_atom("site already started")));
  }
  ozport = makeTaggedConst(new PortWithStream(am.rootBoard, stream));
  if (ipInit(p,siteReceive) < 0) {
    ozport=0;
    return OZ_raiseC("ip",2,OZ_atom("ip init failed"),
		     OZ_atom(OZ_unixError(lastIpError())));
  }
  return PROCEED;
}
OZ_C_proc_end

OZ_C_proc_begin(BIstartSite3,3)
{
  OZ_declareIntArg(0,p);
  OZ_declareArg(1,stream);
  OZ_declareArg(2,out);

  if (ozport!=0) {
    OZ_unifyAtom(out,"false");
    return PROCEED;
  }
  ozport = makeTaggedConst(new PortWithStream(am.rootBoard, stream));
  if (ipInit(p,siteReceive) < 0) {
    OZ_unifyAtom(out,"false");
    return PROCEED;}
  OZ_unifyAtom(out,"true");
  return PROCEED;
}
OZ_C_proc_end

OZ_C_proc_begin(BIlookupSite,4)
{
  OZ_declareVirtualStringArg(0,host);
  OZ_declareIntArg(1,port);
  OZ_declareIntArg(2,timestamp);
  OZ_declareArg(3,out);

  return OZ_unifyInt(out,lookupSite(host,port,timestamp));
}
OZ_C_proc_end

BIspec perdioSpec[] = {
  {"reliableSend",   2, BIreliableSend, 0},
  {"reliableSend3",  3, BIreliableSend3, 0},
  {"unreliableSend", 2, BIunreliableSend, 0},
  {"startSite",      2, BIstartSite, 0},
  {"startSite3",     3, BIstartSite3, 0},
  {"lookupSite",     4, BIlookupSite, 0},
  {0,0,0,0}
};


void BIinitPerdio()
{
  BIaddSpec(perdioSpec);
  
  refTable = new RefTable();
  refTrail = new RefTrail();
  ownerTable = new OwnerTable(DEFAULT_OWNER_TABLE_SIZE);
  borrowTable = new BorrowTable(DEFAULT_BORROW_TABLE_SIZE);
  debtRec= new DebtRec(); 
  pendLinkManager = new PendLinkManager();
  pendEntryManager = new PendEntryManager();
  /* TODO: gname-table */
#ifdef DEBUG_PERDIO
  dvset();
#endif
}

#endif
