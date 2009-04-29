/*
 *  Main authors:
 *     Alejandro Arbelaez (aarbelaez@cic.puj.edu.co)
 *
 *  Contributing authors:
 *
 *  Copyright:
 *    Alejandro Arbelaez, 2006-2007
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

#ifndef __MOZ_PROP_CC__
#define __MOZ_PROP_CC__

#include "MozProp.hh"

//--------------------------------------------------------------------------------
Gecode::ExecStatus WatchMaxProp::propagate(Gecode::Space *s, Gecode::ModEventDelta) {
  if(x0.max() <= x2 && x0.min() >= Gecode::Int::Limits::min) {
    GECODE_ME_CHECK(x1.eq(s,0));
    return Gecode::ES_SUBSUMED(this,s);
  }
  if(x0.min() <= Gecode::Int::Limits::max && x0.min() >= x2+1) {
    GECODE_ME_CHECK(x1.eq(s,1));
    return Gecode::ES_SUBSUMED(this,s);
  }
  return Gecode::ES_FIX;
}

Gecode::ExecStatus WatchMinProp::propagate(Gecode::Space *s, Gecode::ModEventDelta) {
  if(x0.max() <= getX2() && x0.min() >= Gecode::Int::Limits::min) {
    GECODE_ME_CHECK(x1.eq(s,1));
    return Gecode::ES_SUBSUMED(this,s);
  }
  if(x0.min() >= getX2() && x0.max() <= Gecode::Int::Limits::max) {
    GECODE_ME_CHECK(x1.eq(s,0));
    return Gecode::ES_SUBSUMED(this,s);
  }
  return Gecode::ES_FIX;
}

Gecode::ExecStatus WatchSizeProp::propagate(Gecode::Space *s, Gecode::ModEventDelta) {
  if(x0.max() - x0.min() + 1 >= getX2()) {
    GECODE_ME_CHECK(x1.eq(s,1));
    return Gecode::ES_SUBSUMED(this,s);
  }
  return ES_FIX;
}

//--------------------------------------------------------------------------------
//This propagators are insperated in the same ones of Mozart-oz

Gecode::ExecStatus DisjointProp::propagate(Gecode::Space *s, Gecode::ModEventDelta) {
  int xl = x0.min(), xu = x0.max();
  int yl = x1.min(), yu = x1.max();
  

  if(xu + xd <= yl) return Gecode::ES_SUBSUMED(this,s);
  if(yu + yd <= xl) return Gecode::ES_SUBSUMED(this,s);
  
  if(xl + xd > yu) {
    /*GECODE_AUTOARRAY(Gecode::Int::Linear::Term, t, 2);
    t[0].a = 1; t[1].a = -1;
    t[0].x = x0; t[1].x = x1;
    Gecode::Int::Linear::post(s,t,2,Gecode::IRT_GQ,yd);*/
    return Gecode::ES_SUBSUMED(this,s);
  }
  if(yl + yd > xu) {
    /*GECODE_AUTOARRAY(Gecode::Int::Linear::Term,t,2);
    t[0].a = -1; t[1].a = 1;
    t[0].x = x0; t[1].x = x1;
    Gecode::Int::Linear::post(s,t,2,Gecode::IRT_GQ,xd);*/
    return Gecode::ES_SUBSUMED(this,s);
  }
  
  int lowx = yu-xd+1, lowy = xu-yd+1;
  int upx = yl+yd-1, upy = xl+xd-1;
  
  if(lowx <= upx) {
    Gecode::IntVar Tmp(s,lowx,upx);
    Gecode::Int::ViewRanges<IntView> la(Tmp);
    GECODE_ME_CHECK(x0.minus_r(s,la));
  }
  
  if(lowy <= upy) {
    Gecode::IntVar Tmp(s,lowy,upy);
    Gecode::Int::ViewRanges<IntView> la(Tmp);
    GECODE_ME_CHECK(x1.minus_r(s,la));
  }
  
  return Gecode::ES_FIX;
}

////// DisjointC propagation methods

void DisjointCProp::LessEqY(Gecode::Space *s) {
  GECODE_AUTOARRAY(Gecode::Int::Linear::Term<Gecode::Int::IntView>, t, 2);
  
  t[0].a = 1; t[1].a = -1;
  t[0].x = x0; t[1].x = x1;
  Gecode::Int::Linear::post(s,t,2,Gecode::IRT_GQ,yd);
}

void DisjointCProp::LessEqX(Gecode::Space *s) {
  GECODE_AUTOARRAY(Gecode::Int::Linear::Term<Gecode::Int::IntView>,t,2);
  t[0].a = -1; t[1].a = 1;
  t[0].x = x0; t[1].x = x1;
  Gecode::Int::Linear::post(s,t,2,Gecode::IRT_GQ,xd);
}

Gecode::ExecStatus DisjointCProp::propagate(Gecode::Space *s, Gecode::ModEventDelta) {
  int xl = x0.min(), xu = x0.max();
  int yl = x1.min(), yu = x1.max();
    
  if(xu + xd <= yl) {
    GECODE_ME_CHECK(x2.eq(s,0));
    return Gecode::ES_SUBSUMED(this,s);
  }

  if(yu + yd <= xl) {
    GECODE_ME_CHECK(x2.eq(s,1));
    return Gecode::ES_SUBSUMED(this,s);
  }

  if(xl + xd > yu) {
    GECODE_ME_CHECK(x2.eq(s,1));
    LessEqY(s);
    return Gecode::ES_SUBSUMED(this,s);
  }

  if(yl + yd > xu) {
    GECODE_ME_CHECK(x2.eq(s,0));
    LessEqX(s);
    return Gecode::ES_SUBSUMED(this,s);
  }
  
  if(x2.zero()) {
    LessEqX(s);
    return Gecode::ES_SUBSUMED(this,s);
  }

  if(x2.one()) {
    LessEqY(s);
    return Gecode::ES_SUBSUMED(this,s);
  }

  int lowx, lowy, upx, upy;
  
  lowx = yu-xd+1; lowy = xu-yd+1;
  upx = yl+yd-1; upy = xl+xd-1;

  if (lowx <= upx) {
    Gecode::IntSet is(lowx,upx);
    Gecode::IntSetRanges isr(is);
    GECODE_ME_CHECK(x0.minus_r(s, isr));
  }
  
  if (lowy <= upy) {
    Gecode::IntSet is(lowy,upy);
    Gecode::IntSetRanges isr(is);
    GECODE_ME_CHECK(x1.minus_r(s, isr));
  }

  return Gecode::ES_FIX;
}



//--------------------------------------------------------------------------------
Gecode::ExecStatus ReifiedIntProp::propagate(Gecode::Space *s, Gecode::ModEventDelta) {
  //    Gecode::IntVarRanges v1(x0);
  //Gecode::IntVar tmp(s, d);
  //Gecode::Int::IntView d2(tmp);
  Gecode::Int::IntView d2(d);
  ViewRanges<IntView> v2(d2);
  ViewRanges<IntView> v1(x0);
  
  if(b.zero()) {
    GECODE_ME_CHECK(x0.minus_r(s,v2));
    return Gecode::ES_SUBSUMED(this,s);
  }
  if(b.one()) {
    //    GECODE_ME_CHECK(Gecode::Int::Rel::EqDom<IntView,IntView>::post(s,x0,d2));
    //    GECODE_ME_CHECK((Gecode::Int::Rel::EqDom<IntView,IntView>::post(s,x0,d2)));
    if(Gecode::Int::Rel::EqDom<IntView,IntView>::post(s,x0,d2) == ES_FAILED) 
      return Gecode::ES_FAILED;
    return Gecode::ES_SUBSUMED(this,s);
  }
  bool stateDom = true;
  for(;v1() && v2(); ++v1, ++v2){
    if( v1.min() != v2.min() || v1.max() != v2.max() ) {
      stateDom = false;
    }
  }
  if(stateDom) {
    GECODE_ME_CHECK(b.eq(s,1));
    return Gecode::ES_SUBSUMED(this,s);
  }
  return Gecode::ES_FIX;
}

//-----------------------------------------------------------------------------

Gecode::ExecStatus ReifiedCardProp::propagate(Gecode::Space *s, Gecode::ModEventDelta){

  int size = x.size();
  
  if(size == 0)
    return ES_FAILED;

  int i, ones, zeroes, possibles;
  int yLow = y.min(); int yUp = y.max();
  int zLow = z.min(); int zUp = z.max();

  
  if(bo.assigned()){
    if(bo.one()){
      GECODE_ME_CHECK(y.lq(s, zUp));
      GECODE_ME_CHECK(z.gq(s, yLow));
      yUp = y.max();
      zLow = z.min();
    }
  } else if (yLow > zUp) {
    GECODE_ME_CHECK(bo.zero(s));
    return ES_FIX;
  }

  ones = 0;
  zeroes = 0;
  
  for (i = size; i--; ) 
    if (x[i].assigned())
      if (x[i].zero())
	zeroes++;
      else
	ones++;
  
  possibles = size - zeroes;
  

  if ((ones > zUp) || (possibles < yLow)) {
    GECODE_ME_CHECK(bo.zero(s));
    return ES_FIX;
  }
  
  if ((ones >= yUp) && (possibles <= zLow)) {
    GECODE_ME_CHECK(bo.one(s));
    return ES_FIX;
  }
  
  if(bo.assigned()){
    if (bo.one()) {
      if ((ones == zUp) && (possibles - ones > 0)) {
	// impose negatively
	for (i = size; i--; ) 
	  if (!x[i].assigned()) 
	    GECODE_ME_CHECK(x[i].zero(s));
	return ES_FIX;
      }
      
      if ((possibles == yLow) && (possibles - ones > 0)) {
	// impose positively
	for (i = size; i--; ) 
	  if (!x[i].assigned())
	    GECODE_ME_CHECK(x[i].one(s));
	return ES_FIX;
      }
      GECODE_ME_CHECK(y.lq(s, possibles));
      GECODE_ME_CHECK(z.gq(s, ones));
    } else if (bo.zero()) {
      if ((ones == yLow - 1) && (possibles <= zLow) && (possibles - ones > 0)) {
	// impose negatively
	for (i = size; i--; ) 
	  if (!x[i].assigned()) 
	    GECODE_ME_CHECK(x[i].zero(s));
	return ES_FIX;
      }
      
      if ((possibles == zLow + 1) && (ones >= yUp) && (possibles - ones > 0)) {
	// impose positively
	for (i = size; i--; ) 
	  if (!x[i].assigned())
	    GECODE_ME_CHECK(x[i].one(s));
	return ES_FIX;
      }
      
      if (yUp <= ones) 
	GECODE_ME_CHECK(z.lq(s, possibles -1));
      if (zLow >= possibles) 
	GECODE_ME_CHECK(y.gq(s, ones + 1));
    }
  } 
 
  if(y.assigned() && z.assigned() && bo.assigned()){
    for(i=0; i<size; i++){
      if(!x[i].assigned()) return ES_NOFIX;
    }
    return ES_FIX;
  }
  
  return ES_NOFIX;
  
}


//--------------------------------------------------------------------------------

/**
 * \brief Create a propagator with the same behavior that FD.watch.max in Mozart-oz
 * @param s Space
 * @param a IntVar
 * @param b BoolVar
 * @param Num int
 */


//void WatchMax(Gecode::Space *s, Gecode::IntVar a, Gecode::BoolVar b, int Num) {
void WatchMax(Gecode::Space *s, Gecode::IntVar a, Gecode::IntVar b, int Num) {
  if(s->failed()) return;
  (void) new(s) WatchMaxProp(s,a,b,Num);
}

/**
 * \brief Create a propagator (FD.watch.min)
 * @param s Space
 * @param a IntVar
 * @param b BoolVar
 * @param Num int
 */


//void WatchMin(Gecode::Space *s, IntVar a, BoolVar b, int Num) {
void WatchMin(Gecode::Space *s, IntVar a, IntVar b, int Num) {  
	if(s->failed()) return;
	(void) new(s) WatchMinProp(s,a,b,Num);
}

/**
 * \brief Create a propagator (FD.watch.size)
 * @param s Space
 * @param a IntVar
 * @param b BoolVar
 * @param Num int
 */

//void WatchSize(Gecode::Space *s, IntVar a, BoolVar b, int Num) {
void WatchSize(Gecode::Space *s, IntVar a, IntVar b, int Num) {
  if(s->failed()) return;

  (void) new(s) WatchSizeProp(s,a,b,Num);
}

/**
 * \brief Create a propagator (FD.disjoint)
 * @param s Space
 * @param D1 IntVar
 * @param I1 int
 * @param D2 IntVar
 * @param I2 int 
 */
void Disjoint(Gecode::Space *s, IntVar D1, int I1, IntVar D2, int I2) {
  if(s->failed()) return;
  (void) new(s) DisjointProp(s,D1,D2,I1,I2);
}

/**
 * \brief Create a propagator (FD.disjointC)
 * @param s Space
 * @param D1 IntVar
 * @param I1 int
 * @param D2 IntVar
 * @param I2 int 
 * @param D3 BoolVar
 */


 void DisjointC(Gecode::Space *s, IntVar D1, int I1, IntVar D2, int I2, BoolVar D3) {

  if(s->failed()) return;
  (void) new(s) DisjointCProp(s,D1,D2,D3,I2,I2);
  }

/**
 * \brief Create a propagator that when x \in d implique  b = 1 other case b = 0
 * @param s Space
 * @param x IntVar
 * @param b BoolVar
 * @param d const IntSet
 */
void ReifiedInt(Gecode::Space *s, Gecode::IntVar x, Gecode::BoolVar b, const Gecode::IntVar d) {
  if(s->failed()) return;
  (void) new(s) ReifiedIntProp(s, x, b,d);
}


/**
 * \brief Create a propagator 
 * @param s Space
 * @param x IntVar
 * @param b BoolVarArgs
 * @param z IntVar
 * @param bo BoolVar
 */
void ReifiedCard(Gecode::Space *s, IntView x, ViewArray<BoolView> y, IntView z, BoolView bo) {
  if(s->failed()) return;
  (void) new(s) ReifiedCardProp(s, x, y, z, bo);
}

#endif