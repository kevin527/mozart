%%%
%%% Authors:
%%%   Christian Schulte (schulte@dfki.de)
%%%
%%% Copyright:
%%%   Christian Schulte, 1997
%%%
%%% Last change:
%%%   $Date$ by $Author$
%%%   $Revision$
%%%
%%% This file is part of Mozart, an implementation
%%% of Oz 3
%%%    $MOZARTURL$
%%%
%%% See the file "LICENSE" or
%%%    $LICENSEURL$
%%% for information on usage and redistribution
%%% of this file, and for a DISCLAIMER OF ALL
%%% WARRANTIES.
%%%

\ifdef LILO

functor $

import
   SP.{Show   = 'Show'
       System = 'System'}

   WP.{Tk      = 'Tk'
       TkTools = 'TkTools'}

   Browser.{Browse = 'Browse'}

export
   'ExplorerClass': ExplorerClass
   'Explorer':      Explorer
   'ExploreOne':    ExploreOne
   'ExploreAll':    ExploreAll
   'ExploreBest':   ExploreBest

body

   \insert 'explorer/main.oz'

   Explorer = {New ExplorerClass init}

   proc {ExploreOne P}
      {Explorer one(P)}
   end

   proc {ExploreAll P}
      {Explorer all(P)}
   end

   proc {ExploreBest P O}
      {Explorer all(P O)}
   end

end

\else

fun instantiate {$ IMPORT}
   \insert 'SP.env'
       = IMPORT.'SP'
   \insert 'WP.env'
       = IMPORT.'WP'
   \insert 'Browser.env'
       = IMPORT.'Browser'

   \insert 'explorer/main.oz'

   Explorer = {New ExplorerClass init}

   proc {ExploreOne P}
      {Explorer one(P)}
   end

   proc {ExploreAll P}
      {Explorer all(P)}
   end

   proc {ExploreBest P O}
      {Explorer all(P O)}
   end

in
   \insert 'Explorer.env'
end

\endif
