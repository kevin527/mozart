%%%
%%% Authors:
%%%   Martin Henz (henz@iscs.nus.edu.sg)
%%%   Christian Schulte (schulte@dfki.de)
%%%
%%% Copyright:
%%%   Martin Henz, 1997
%%%   Christian Schulte, 1997
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


declare
   Dictionary
   IsDictionary
   NewDictionary
in

NewDictionary = {`Builtin` 'NewDictionary' 1}
IsDictionary  = {`Builtin` 'IsDictionary'  2}

local
   Entries = {`Builtin` 'Dictionary.entries'   2}
in
   Dictionary =
   dictionary(new:          NewDictionary
	      is:           IsDictionary
	      put:          {`Builtin` 'Dictionary.put'          3}
	      get:          {`Builtin` 'Dictionary.get'          3}
	      condGet:      {`Builtin` 'Dictionary.condGet'      4}
	      keys:         {`Builtin` 'Dictionary.keys'         2}
	      entries:      Entries
	      items:        {`Builtin` 'Dictionary.items'        2}
	      remove:       {`Builtin` 'Dictionary.remove'       2}
	      removeAll:    {`Builtin` 'Dictionary.removeAll'    1}
	      clone:        {`Builtin` 'Dictionary.clone'        2}
	      member:       {`Builtin` 'Dictionary.member'       3}
	      toRecord:     fun {$ L D}
			       {`record` L {Entries D}}
			    end)
end
