%%%  Programming Systems Lab, DFKI Saarbruecken,
%%%  Stuhlsatzenhausweg 3, D-66123 Saarbruecken, Phone (+49) 681 302-5312
%%%  Author: Christian Schulte
%%%  Email: schulte@dfki.uni-sb.de
%%%  Last modified: $Date$ by $Author$
%%%  Version: $Revision$


declare
   NewExplorer
in

fun
\ifdef NEWCOMPILER
   instantiate
\endif
   {NewExplorer IMPORT}
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
