/*
 *  Authors:
 *    Christian Schulte <schulte@ps.uni-sb.de>
 *
 *  Copyright:
 *    Christian Schulte, 1997
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

#if defined(INTERFACE)
#pragma implementation "distributor.hh"
#endif

#include "distributor.hh"

// PERFORMANCE PROBLEMS
DistBag * DistBag::clean(void) {

  if (!this)
    return this;

  if (getDist()->isAlive()) {
    setNext(getNext()->clean());
    return this;
  } else {
    return getNext()->clean();
  }

}
