%%%
%%% Author:
%%%   Leif Kornstaedt <kornstae@ps.uni-sb.de>
%%%
%%% Copyright:
%%%   Leif Kornstaedt, 1997
%%%
%%% Last change:
%%%   $Date$ by $Author$
%%%   $Revision$
%%%
%%% This file is part of Mozart, an implementation of Oz 3:
%%%    $MOZARTURL$
%%%
%%% See the file "LICENSE" or
%%%    $LICENSEURL$
%%% for information on usage and redistribution
%%% of this file, and for a DISCLAIMER OF ALL
%%% WARRANTIES.
%%%

local
   ParserLib = {Foreign.staticLoad 'libparser.so'}
   ParseFile          = ParserLib.parser_parseFile
   ParseVirtualString = ParserLib.parser_parseVirtualString
in
   fun {ParseOzFile FileName Reporter GetSwitch Defines}
      Res VS
   in
      Res = {ParseFile FileName
	     options(showInsert: {GetSwitch showinsert}
		     gumpSyntax: {GetSwitch gump}
		     systemVariables: {GetSwitch system}
		     defines: Defines
		     errorOutput: ?VS)}
      case Res of fileNotFound then
	 {Reporter error(kind: 'compiler directive error'
			 msg: ('could not open file "'#FileName#
			       '" for reading'))}
      [] parseErrors(N) then
	 {Reporter addErrors(N)}
	 {Reporter userInfo(VS)}
      else
	 {Reporter userInfo(VS)}
      end
      Res
   end

   fun {ParseOzVirtualString VS Reporter GetSwitch Defines}
      Res VS2
   in
      Res = {ParseVirtualString VS
	     options(showInsert: {GetSwitch showinsert}
		     gumpSyntax: {GetSwitch gump}
		     systemVariables: {GetSwitch system}
		     defines: Defines
		     errorOutput: ?VS2)}
      case Res of parseErrors(N) then
	 {Reporter addErrors(N)}
      else skip
      end
      {Reporter userInfo(VS2)}
      Res
   end
end
