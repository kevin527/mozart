%%%
%%% Authors:
%%%   Christian Schulte <schulte@dfki.de>
%%%
%%% Copyright:
%%%   Christian Schulte, 1998
%%%
%%% Last change:
%%%   $Date$ by $Author$
%%%   $Revision$
%%%
%%% This file is part of Mozart, an implementation
%%% of Oz 3
%%%    http://mozart.ps.uni-sb.de
%%%
%%% See the file "LICENSE" or
%%%    http://mozart.ps.uni-sb.de/LICENSE.html
%%% for information on usage and redistribution
%%% of this file, and for a DISCLAIMER OF ALL
%%% WARRANTIES.
%%%

functor

import
   Tk TkTools

   Edit(page)
   Compute(page)

define

   T = {New Tk.toplevel      tkInit(title:    'Glass Plates'
                                    withdraw: true)}
   B = {New TkTools.notebook tkInit(parent: T)}

   E = {New Edit.page        init(parent: B)}
   C = {New Compute.page     init(parent: B
                                  edit:   E)}

in

   {B add(E)} {B add(C)}

   {Tk.batch [pack(B) update(idletasks) wm(deiconify T)]}

end
