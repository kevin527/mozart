/*
  Hydra Project, DFKI Saarbruecken,
  Stuhlsatzenhausweg 3, D-66123 Saarbruecken, Phone (+49) 681 302-5312
  Author: tmueller
  Last modified: $Date$ from $Author$
  Version: $Revision$
  State: $State$

  ------------------------------------------------------------------------
*/

#ifndef __REIFIED_HH__
#define __REIFIED_HH__

#include "fsstd.hh"
#include "telling.hh"

class IncludeRPropagator : public Propagator_S_D_D {
private:
  static OZ_CFun header;
public:
  IncludeRPropagator(OZ_Term s, OZ_Term d, OZ_Term r)
    : Propagator_S_D_D(s, d, r) {}

  virtual OZ_Return propagate(void);
  
  virtual OZ_CFun getHeaderFunc(void) const {
    return header;
  }
};

//-----------------------------------------------------------------------------

class IsInRPropagator : public Propagator_S_I_D {
private:
  static OZ_CFun spawner;
public:
  IsInRPropagator(OZ_Term v, OZ_Term i, OZ_Term b)
    : Propagator_S_I_D(v, i, b) { }

  virtual OZ_Return propagate(void);
  
  virtual OZ_CFun getHeaderFunc(void) const {
    return spawner;
  }
};


#endif /* __REIFIED_HH__ */

//-----------------------------------------------------------------------------
// eof
