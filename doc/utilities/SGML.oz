%%%
%%% Authors:
%%%   Leif Kornstaedt <kornstae@ps.uni-sb.de>
%%%   Denys Duchier <duchier@ps.uni-sb.de>
%%%
%%% Copyright:
%%%   Leif Kornstaedt, 1998
%%%   Denys Duchier, 1998
%%%
%%% Last change:
%%%   $Date$ by $Author$
%%%   $Revision$
%%%
%%% This file is part of Mozart, an implementation of Oz 3:
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
   Property(get)
   System(printError)
   Parser at 'x-oz://contrib/doc/sgml/Parser'
export
   Parse
   namePI: PI
   GetSubtree
   IsOfClass
define
   PI = {NewName}
   fun {Parse File} Res Errors in
      {Parser.object
       process([File] ?Res
               catalog:{Property.get 'ozdoc.catalog'}
               casefold:lower
               error:Errors)}
      case Errors of nil then {Transform Res.docElem}
      else
         {List.is Errors _}   % request lazily computed list
         {System.printError Errors}
         raise error end
      end
   end
   fun {GetSubtree M L ?Mr}
      if {IsTuple M} then
         case {Record.toList M} of (X=L(...))|Xr then
            Mr = {List.toTuple {Label M} Xr}
            X
         else
            Mr = M
            unit
         end
      else
         {GetSubtree
          {List.toRecord {Label M}
           {Filter {Record.toListInd M} fun {$ X#_} {IsInt X} end}} L ?Mr}
      end
   end
   fun {IsOfClass M C}
      {Member C {CondSelect M 'class' nil}}
   end
   fun {Transform E}
      case E
      of element(tag:T attributes:A linkAttributes:L children:C) then
         {List.toRecord T
          {Append {List.mapInd C fun {$ I C} I#{Transform C} end}
           {FoldR L TransformAttribute
            {FoldR A TransformAttribute nil}}}}
      elseof pi(Name) then PI(Name)
      elseof data(Bytes) then {ByteString.toString Bytes}
      end
   end
   fun {TransformAttribute A L}
      Prop = A.name Kind = A.kind Value = A.value
   in
      if Kind=='IMPLIED' then L
      else
         (Prop#if Prop=='class' then Value
               elsecase Value of [V] then
                  if {ByteString.is V}
                  then {ByteString.toString V}
                  else V end
               end)|L
      end
   end
end
