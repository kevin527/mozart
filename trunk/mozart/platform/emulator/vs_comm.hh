/*
 *  Authors:
 *    Konstantin Popov
 * 
 *  Contributors:
 *
 *  Copyright:
 *    Konstantin Popov 1997-1998
 * 
 *  Last change:
 *    $Date$ by $Author$
 *    $Revision$
 * 
 *  This file is part of Mozart, an implementation 
 *  of Oz 3:
 *     $MOZARTURL$
 * 
 *  See the file "LICENSE" or
 *     $LICENSEURL$
 *  for information on usage and redistribution 
 *  of this file, and for a DISCLAIMER OF ALL 
 *  WARRANTIES.
 *
 */

#ifndef __VS_COMM_HH
#define __VS_COMM_HH

#ifdef INTERFACE  
#pragma interface
#endif

#include "base.hh"
#include "comm.hh"
#include "vs_msgbuffer.hh"
#include "vs_mailbox.hh"

//
// The 'VirtualInfo' class serves two purposes:
// (a) identifies the virtual sites group (the id of the master 
//     in this implementation).
// (b) contains information for receiving/sending messages to 
//     the site.
class VirtualInfo {
  friend Site::isInMyVSGroup(VirtualInfo *vi);
  friend Site::initVirtualInfoArg(VirtualInfo *vi);
private:
  // The "street address" of the master virtual site
  // ("virtual site group id");
  ip_address address;
  port_t port;
  time_t timestamp;

  // "box" of the site itself;
  key_t mailboxKey;

  //
private:
  //
  void setAddress(ip_address aIn) { address = aIn; }
  void setPort(port_t pIn) { port = pIn; }
  void setTimeStamp(time_t tsIn) { timestamp = tsIn; }

  //
  ip_address getAddress() { return (address); }
  port_t getPort() { return (port); }
  time_t getTimeStamp() { return (timestamp); }

  //
public:
  //
  // When a "plain" site declares itself as a virtual one (when it
  // creates a first slave site), both the mailbox is already there:
  VirtualInfo(Site *bs, key_t mboxkIn)
    : mailboxKey(mboxkIn)
  {
    bs->initVirtualInfoArg(this);
  }

  //
  // "virtual info" in a mailbox being created for a slave is copied
  // from the master's one (passed in the mailbox):
  VirtualInfo(VirtualInfo *vii)
    : address(vii->address), port(vii->port), timestamp(vii->timestamp),
      mailboxKey(vii->mailboxKey)
  {}

  //
  // When the 'mySite' of a slave site is extended for the virtual
  // info (by 'BIVSinitServer'), then the mailbox key is given (the
  // only 'BIVSInitServer's argument), and the "virtual site group id"
  // is taken from the mailbox'es virtual info:
  VirtualInfo(VirtualInfo *mvi, key_t mboxkIn)
    : address(mvi->address), port(mvi->port), timestamp(mvi->timestamp),
      mailboxKey(mboxkIn)
  {}

  //
  // There is free list management of virtual info"s;
  ~VirtualInfo() { error("VirtualInfo is destroyed?"); }
  // There is nothing to be done when disposed;
  void destroy() {
    DebugCode(address = (ip_address) 0);
    DebugCode(port = (port_t) 0);
    DebugCode(timestamp = (time_t) 0);
    DebugCode(mailboxKey = (key_t) 0);
  }

  //
  // Another type of initialization - unmarshalliing:
  VirtualInfo(MsgBuffer *mb) {
    Assert(sizeof(ip_address) <= sizeof(unsigned int));
    Assert(sizeof(port_t) <= sizeof(unsigned short));
    Assert(sizeof(time_t) <= sizeof(unsigned int));
    Assert(sizeof(key_t) <= sizeof(unsigned int));

    //
    address = (ip_address) unmarshalNumber(mb);
    port = (port_t) unmarshalShort(mb);  
    timestamp = (time_t) unmarshalNumber(mb);
    //
    mailboxKey = (key_t) unmarshalNumber(mb);
  }

  // 
  void marshal(MsgBuffer *mb) {
    Assert(sizeof(ip_address) <= sizeof(unsigned int));
    Assert(sizeof(port_t) <= sizeof(unsigned short));
    Assert(sizeof(time_t) <= sizeof(unsigned int));
    Assert(sizeof(key_t) <= sizeof(unsigned int));

    //
    marshalNumber(address, mb);
    marshalShort(port, mb);  
    marshalNumber(timestamp, mb);
    //
    marshalNumber(mailboxKey, mb);
  }

  //
  // There are NO public 'get' methods for address/port/timestamp!
  key_t getMailboxKey() { return (mailboxKey); }
  void setMailboxKey(key_t mbkIn) { mailboxKey = mbkIn; }
};

//
// Throw away the "virtual info" object representation from a message
// buffer;
void unmarshalUselessVirtualInfo(MsgBuffer *mb);
class VSMessage;

//
// Messages are queued up using the field of the message itself, 
// which is declared in its superclass:
class VSMsgQueueNode {
  friend class VSFreeMessagePool;
  friend class VSMsgQueue;
private:
  VSMessage *next;

  //
protected:
  VSMessage* getNext() { return (next); }
  void setNext(VSMessage *n) { next = n; }

public:
  VSMsgQueueNode() : next((VSMessage *) 0) {}
  ~VSMsgQueueNode() { error("VSMsgQueueNode destroyed??"); }
};

//
// "Messages" are necessary for the case when a message cannot be
// delivered (i.e. put in a mailbox) immediately (when e.g. that
// mailbox is locked or full);
class VSMessage : private VSMsgQueueNode {
  friend class VSFreeMessagePool;
  friend class VSMsgQueue;
private:
  VSMsgBuffer *mb;
  MessageType msgType;
  // 'Site*' is not needed (stored in the message buffer);
  int storeIndex;

  //
public:
  VSMessage(VSMsgBuffer *mbIn, MessageType mtIn, Site *sIn, int stIn)
    : mb(mbIn), msgType(mtIn), storeIndex(stIn)
  {
    Assert(mbIn->getSite() == sIn);
  }
  ~VSMessage() { error("VSMessage destroyed??"); }

  //
  VSMsgBuffer* getMsgBuffer() { return (mb); }
  MessageType getMessageType() { return (msgType); }
  Site *getSite() { return (mb->getSite()); }
  int getStoreIndex() { return (storeIndex); }
};

//
// A list of free messages;
class VSFreeMessagePool {
private:
  VSMessage *freeList;

  //
public:
  VSFreeMessagePool() : freeList((VSMessage *) 0) {}
  ~VSFreeMessagePool() {}

  //
  VSMessage *allocate() {
    if (freeList) {
      VSMessage *b = freeList;
      freeList = freeList->getNext();
      DebugCode(b->setNext((VSMessage *) 0));
      return (b);
    } else {
      return ((VSMessage *) malloc(sizeof(VSMessage)));
    }
  }
  void dispose(VSMessage *b) {
    Assert(b->getNext() == (VSMessage *) 0);
    b->setNext(freeList);
    freeList = b;
  }
};

//
// A queue of messages waiting for delivery (an element of a "virtual
// site" object);
class VSMsgQueue {
private:
  VSMessage *first, *last;

  //
public:
  VSMsgQueue() : first((VSMessage *) 0), last((VSMessage *) 0) {}
  ~VSMsgQueue() { error("VSMsgQueue destroyed??"); }

  //
  Bool isNotEmpty() { return ((Bool) first); }

  //
  void enqueue(VSMessage *n) {
    if (last) {
      Assert(last->getNext() == (VSMessage *) 0);
      last->setNext(n);
    }
    last = n;
    if (first == (VSMessage *) 0) 
      first = last;
  }

  //
  VSMessage* dequeue() {
    VSMessage *r = first;
    Assert(first != (VSMessage *) 0);
    if (first == last) { 
      first = last = (VSMessage *) 0;
    } else {
      first = first->getNext();
    }
    DebugCode(r->setNext((VSMessage *) 0));
    return (r);
  }
};

//
// Virtual sites that are unable to accept messages at the moment
// (thus, whose mailboxes locked) are kept in a list;
//
// I (kost@) have choosen the 'two-layer' scheme for keeping unsent
// messages, since (a) each VS has to know what's going on with its
// messages anyway (e.g. for the needs of statistic/control), and (b)
// the implementations of both queues can be changed separately.
class VSSiteQueue;
class VSSiteQueueNode {
  friend class VSSiteQueue;
private:
  Bool isInFlag;		// Every site is stored at most once;
  VirtualSite *nextSiteInQueue;

  //
private:
  VirtualSite* getNext() { return (nextSiteInQueue); }
  void setNext(VirtualSite *n) { nextSiteInQueue = n; }
  //
  Bool isIn() { return (isInFlag); }
  void setIn() { isInFlag = OK; }
  void setOut() { isInFlag = NO; }

  //
public:
  VSSiteQueueNode()
    : isInFlag(NO), nextSiteInQueue((VirtualSite *) 0) {}
  ~VSSiteQueueNode() { error("VSSiteQueueNode destroyed?"); }
};

//
// Internal state of a virtual site, i.e. details that are (must be)
// hidden from the communication layer (i.e. the 'Site' object);
#define VS_PENDING_CLOSE       0x1
#define VS_PENDING_UNMAP_MBOX  0x2

//
// That's the thing which is referenced by a site object denoting a
// virtual site, and created whenever something is to be sent there;
class VirtualSite : private VSSiteQueueNode, public VSMsgQueue  {
  friend class VSSiteQueue;
private:
  Site *site;			// backward (cross) reference;
  SiteStatus status;		// (values;)
  int vsStatus;			// (flags;)

  //
  VSMailboxManager *mboxMgr;	// ... of that virtual site;

  //
  // Message bodies are allocated from this pool:
  VSFreeMessagePool *fmp;

  // If 'sendTo' fails to deliver something 'inline', it puts itself
  // (the virtual site) into the queue:
  VSSiteQueue *sq;

  //
private:
  //
  void setStatus(SiteStatus statusIn) { status = statusIn; }
  void setVSFlag(int flag) { vsStatus |= flag; }
  void clearVSFlag(int flag) { vsStatus &= ~flag; }

  //
  Bool isSetVSFlag(int flag) { return ((Bool) vsStatus & flag); }

  //
  // All currently unsent messages to a site being disconnected
  // will be sent ('VS_PENDING_UNMAP_MBOX');
  void connect();
  void disconnect();

  //
public:
  VirtualSite(Site *s, VSFreeMessagePool *fmpIn, VSSiteQueue *sqIn);
  ~VirtualSite() { Assert(status == SITE_PERM); }

  //
  SiteStatus getSiteStatus() {
    Assert(status != SITE_PERM);
    return (status);
  }

  // 'drop' means that the virtual site will not be used anymore 
  // since it is known to be dead (e.g. from a third party). 
  void drop();

  //
  // The message type, store site and store index parameters 
  // are opaque data (just stored);
  int sendTo(VSMsgBuffer *mb, MessageType mt,
	     Site *storeSite, int storeIndex);
  // ... resend it;
  int tryToSendToAgain(VSMessage *vsm);

  //
  // The mailbox is mapped/unmapped on these events:
  void zeroReferences() { disconnect(); }

  //
  // If 'sendTo' could say "unsent, stored as #msgNum", then with this
  // method we could try to remove that message from the queue:
  int discardUnsentMessage(int msgNum) { return (MSG_SENT); }
  //
  // ... again, the queue is considered to be opaque by now:
  int getQueueStatus(int &noMsgs) {
    noMsgs = 0; 
    return (0);
  }
};

//
// A queue of sites that have some pending messages for delivery;
//
// An object of this class is tried to empty (i.e. to send out pending
// messages to those sites) at regular intervals (tasks; see
// virtual.cc and am.*);
//
// (We cannot use templates for 'VSMsgQueue' and 'VSSiteQueue'
// implementations since their corresponding "queue node" classes 
// should define those queue classes as "friends";)
class VSSiteQueue {
private:
  VirtualSite *first, *last;

  //
public:
  VSSiteQueue() : 
    first((VirtualSite *) 0), last((VirtualSite *) 0) 
  {}
  ~VSSiteQueue() {}

  //
  Bool isEmpty() { return (first == (VirtualSite *) 0); }
  Bool isNotEmpty() { return ((Bool) first); }

  //
  void enqueue(VirtualSite *n) {
    if (!n->isIn()) {
      n->setIn();

      //
      if (last) {
	Assert(last->getNext() == (VirtualSite *) 0);
	last->setNext(n);
      }
      last = n;
      if (first == (VirtualSite *) 0) 
	first = last;
    }
  }

  //
  VirtualSite* dequeue() {
    VirtualSite *r = first;
    Assert(r->isIn());
    r->setOut();

    //
    Assert(first != (VirtualSite *) 0);
    if (first == last) { 
      first = last = (VirtualSite *) 0;
    } else {
      first = first->getNext();
    }
    DebugCode(r->setNext((VirtualSite *) 0));

    //
    return (r);
  }
};

#endif // __VS_COMM_HH
