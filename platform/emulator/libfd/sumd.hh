/*
 *  Authors:
 *    Author's name (Author's email address)
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
 *     $MOZARTURL$
 * 
 *  See the file "LICENSE" or
 *     $LICENSEURL$
 *  for information on usage and redistribution 
 *  of this file, and for a DISCLAIMER OF ALL 
 *  WARRANTIES.
 *
 */
/*
  Hydra Project, DFKI Saarbruecken,
  Stuhlsatzenhausweg 3, D-66123 Saarbruecken, Phone (+49) 681 302-5312
  Author: loeckelt
  Last modified: $Date$ from $Author$
  Version: $Revision$
  State: $State$

  ------------------------------------------------------------------------
*/

#include "std.hh"

//-----------------------------------------------------------------------------

class isumProp : public Propagator_VI_VD_I {
public: 
  isumProp(OZ_Term a, OZ_Term x, OZ_Term c, OZ_Boolean is_lin = OZ_TRUE) 
    : Propagator_VI_VD_I(a, x, c, is_lin) { };
  isumProp(const Propagator_VI_VD_I_D &o) 
    : Propagator_VI_VD_I(o) {}
  isumProp(int sz, int sizes[], int single_var[], int a[], OZ_Term x[], int c) 
    : Propagator_VI_VD_I(sz, sizes, single_var, a, x, c) {}
  isumProp(OZ_Term x, OZ_Term d)
    : Propagator_VI_VD_I(x, d) {}
  isumProp(int ax, OZ_Term x, int ay, OZ_Term y, int az, OZ_Term z, int c)
    : Propagator_VI_VD_I(ax, x, ay, y, az, z, c) {}

  int getNextInterval(OZ_FDIntVar F, int position, int &lower, int &upper);
  void trimRemainders(int factor, int &lo, int &hi);
  int sum (OZ_FiniteDomain &AuxDom, OZ_FDIntVar var[], int except,  
	    int pos, int losum, int hisum, int typ);
};

class iLinEqProp : public isumProp {
public:
  iLinEqProp(OZ_Term a, OZ_Term x, OZ_Term c, OZ_Boolean is_lin = OZ_TRUE) 
    : isumProp(a, x, c, is_lin) {};
  iLinEqProp(const Propagator_VI_VD_I_D &o) 
    : isumProp(o) {}
  iLinEqProp(int sz, int sizes[], int single_var[], int a[], 
	     OZ_Term x[], int c) 
    : isumProp(sz, sizes, single_var, a, x, c) {}
  iLinEqProp(OZ_Term x, OZ_Term d)
    : isumProp(x, d) {}

  virtual OZ_Return propagate(void);
  virtual OZ_Term getParameters(void) const 
    { return Propagator_VI_VD_I::getParameters(SUM_OP_EQ); }
};


//-----------------------------------------------------------------------------

class iLinLessEqProp : public isumProp {
public:
  iLinLessEqProp(OZ_Term a, OZ_Term x, OZ_Term c, 
		      OZ_Boolean is_lin = OZ_TRUE) 
    : isumProp(a, x, c, is_lin) { };
  iLinLessEqProp(const Propagator_VI_VD_I_D &o) 
    : isumProp(o) {}
  iLinLessEqProp(int sz, int sizes[], int single_var[], 
		      int a[], OZ_Term x[], int c) 
    : isumProp(sz, sizes, single_var, a, x, c) {}
  iLinLessEqProp(int ax, OZ_Term x, int ay, OZ_Term y, int az,
		      OZ_Term z, int c)
    : isumProp(ax, x, ay, y, az, z, c) {}
  iLinLessEqProp(OZ_Term x, OZ_Term d)
    : isumProp(x, d) {}

  virtual OZ_Return propagate(void);
  virtual OZ_Term getParameters(void) const 
    { return Propagator_VI_VD_I::getParameters(SUM_OP_LEQ); }
};

//-----------------------------------------------------------------------------

#define LEQ_TO_GT \
{ reg_c += 1; for (int i = reg_sz; i--; ) reg_a[i] = - reg_a[i]; }

#define LEQ_TO_GEQ \
{ for (int i = reg_sz; i--; ) reg_a[i] = - reg_a[i]; }

#define LEQ_TO_LT \
{ reg_c += 1; }
 
//-----------------------------------------------------------------------------

class iLinNEqProp : public isumProp {
public:
  iLinNEqProp(OZ_Term a, OZ_Term x, OZ_Term c, OZ_Boolean is_lin = OZ_TRUE) 
    : isumProp(a, x, c, is_lin) {};
  iLinNEqProp(const Propagator_VI_VD_I_D &o) 
    : isumProp(o) {}
  iLinNEqProp(int sz, int sizes[], int single_var[], int a[], 
	     OZ_Term x[], int c) 
    : isumProp(sz, sizes, single_var, a, x, c) {}
  iLinNEqProp(OZ_Term x, OZ_Term d)
    : isumProp(x, d) {}

  virtual OZ_Return propagate(void);
  virtual OZ_Term getParameters(void) const 
    { return Propagator_VI_VD_I::getParameters(SUM_OP_NEQ); }
};



//=============================================================================

class isumEqProp : public iLinEqProp {
private:
  static OZ_CFunHeader spawner;
public:
  isumEqProp(OZ_Term x, OZ_Term d) : iLinEqProp(x, d) {}
  virtual OZ_CFunHeader * getHeader(void) const { return &spawner; }
};

class isumNEqProp : public iLinNEqProp {
private:
  static OZ_CFunHeader spawner;
public:
  isumNEqProp(OZ_Term x, OZ_Term d) : iLinNEqProp(x, d) {}
  virtual OZ_CFunHeader * getHeader(void) const { return &spawner; }
};

class isumLeqProp : public iLinLessEqProp {
private:
  static OZ_CFunHeader spawner;
public:
  isumLeqProp(OZ_Term x, OZ_Term d) : iLinLessEqProp(x, d) {}
  virtual OZ_CFunHeader * getHeader(void) const { return &spawner; }
};

class isumLtProp : public iLinLessEqProp {
private:
  static OZ_CFunHeader spawner;
public:
  isumLtProp(OZ_Term x, OZ_Term d) : iLinLessEqProp(x, d) LEQ_TO_LT
  virtual OZ_CFunHeader * getHeader(void) const { return &spawner; }
};

class isumGeqProp : public iLinLessEqProp {
private:
  static OZ_CFunHeader spawner;
public:
  isumGeqProp(OZ_Term x, OZ_Term d) : iLinLessEqProp(x, d) LEQ_TO_GEQ
  virtual OZ_CFunHeader * getHeader(void) const { return &spawner; }
};

class isumGtProp : public iLinLessEqProp {
private:
  static OZ_CFunHeader spawner;
public:
  isumGtProp(OZ_Term x, OZ_Term d) : iLinLessEqProp(x, d) LEQ_TO_GT
  virtual OZ_CFunHeader * getHeader(void) const { return &spawner; }
};

//-----------------------------------------------------------------------------

class isumcEqProp : public iLinEqProp {
private:
  static OZ_CFunHeader spawner;
public:
  isumcEqProp(OZ_Term a, OZ_Term x, OZ_Term d) : iLinEqProp(a, x, d) {}
  virtual OZ_CFunHeader * getHeader(void) const { return &spawner; }
};

class isumcNEqProp : public iLinNEqProp {
private:
  static OZ_CFunHeader spawner;
public:
  isumcNEqProp(OZ_Term a, OZ_Term x, OZ_Term d) : iLinNEqProp(a, x, d) {}
  virtual OZ_CFunHeader * getHeader(void) const { return &spawner; }
};


class isumcLeqProp : public iLinLessEqProp {
public:
  isumcLeqProp(OZ_Term a, OZ_Term x, OZ_Term d) : iLinLessEqProp(a, x, d) {}
private:
  static OZ_CFunHeader spawner;
  virtual OZ_CFunHeader * getHeader(void) const { return &spawner; }
};

class isumcLtProp : public iLinLessEqProp {
private:
  static OZ_CFunHeader spawner;
public:
  isumcLtProp(OZ_Term a, OZ_Term x, OZ_Term d) : iLinLessEqProp(a, x, d) 
       LEQ_TO_LT
  virtual OZ_CFunHeader * getHeader(void) const { return &spawner; }
};

class isumcGeqProp : public iLinLessEqProp {
private:
  static OZ_CFunHeader spawner;
public:
  isumcGeqProp(OZ_Term a, OZ_Term x, OZ_Term d) 
    : iLinLessEqProp(a, x, d) LEQ_TO_GEQ
  virtual OZ_CFunHeader * getHeader(void) const { return &spawner; }
};

class isumcGtProp : public iLinLessEqProp {
private:
  static OZ_CFunHeader spawner;
public:
  isumcGtProp(OZ_Term a, OZ_Term x, OZ_Term d) 
    : iLinLessEqProp(a, x, d) LEQ_TO_GT
  virtual OZ_CFunHeader * getHeader(void) const { return &spawner; }
};

//-----------------------------------------------------------------------------






