%
% Authors:
%   Andreas Simon (2000)
%
% Copyright:
%   Andreas Simon (2000)
%
% Last change:
%   $Date$
%   $Revision$
%
% This file is part of Mozart, an implementation
% of Oz 3:
%   http://www.mozart-oz.org
%
% See the file "LICENSE" or
%   http://www.mozart-oz.org/LICENSE.html
% for information on usage and redistribution
% of this file, and for a DISCLAIMER OF ALL
% WARRANTIES.
%

functor

import
   Native at 'GTK.so{native}'
   GTK
   System

export
   \insert 'gdkexports.oz'

define
   
   RegisterNativeObject = GTK.registerNativeObject
   RegisterObject       = GTK.registerObject
   UnRegisterObject     = GTK.unregisterObject
   GetObject            = GTK.getObject

% -----------------------------------------------------------------------------
% Autogenerated stuff 
% -----------------------------------------------------------------------------

   \insert 'gdkclasses.oz'

% -----------------------------------------------------------------------------
% Finale 
% -----------------------------------------------------------------------------

end % functor
