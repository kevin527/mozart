%%%
%%% Authors:
%%%   Michael Mehl (mehl@dfki.de)
%%%   Christian Schulte (schulte@dfki.de)
%%%
%%% Copyright:
%%%   Michael Mehl, 1997
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

declare
   String IsString StringToAtom StringToInt StringToFloat
in


%%
%% Global
%%
IsString      = {`Builtin` 'IsString'      2}
StringToAtom  = {`Builtin` 'StringToAtom'  2}
StringToInt   = {`Builtin` 'StringToInt'   2}
StringToFloat = {`Builtin` 'StringToFloat' 2}


%%
%% Module
%%

local
   proc {Token Is J ?T ?R}
      case Is of nil then T=nil R=nil
      [] I|Ir then
         case I==J then T=nil R=Ir
         else Tr in T=I|Tr {Token Ir J Tr R}
         end
      end
   end

   fun {Tokens Is C Js Jr}
      case Is of nil then Jr=nil
         case Js of nil then nil else [Js] end
      [] I|Ir then
         case I==C then NewJs in
            Jr=nil Js|{Tokens Ir C NewJs NewJs}
         else NewJr in
            Jr=I|NewJr {Tokens Ir C Js NewJr}
         end
      end
   end

in
   String = string(is:      IsString
                   isAtom:  {`Builtin` 'String.isAtom'  2}
                   toAtom:  StringToAtom
                   isInt:   {`Builtin` 'String.isInt'   2}
                   toInt:   StringToInt
                   isFloat: {`Builtin` 'String.isFloat' 2}
                   toFloat: StringToFloat
                   token:   Token
                   tokens:  fun {$ S C}
                               Ss in {Tokens S C Ss Ss}
                            end)
end
