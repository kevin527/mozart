%  Programming Systems Lab, University of Saarland,
%  Geb. 45, Postfach 15 11 50, D-66041 Saarbruecken.
%  Author: Konstantin Popov & Co.
%  (i.e. all people who make proposals, advices and other rats at all:))
%  Last modified: $Date$ by $Author$
%  Version: $Revision$

%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%
%%%
%%%
%%% Object(s) of this class cache(s) information about the
%%% availability of some resources in a current X11 server, for
%%% instance (and primarily!) - fonts. (Note i don't mean here
%%% 'resources' in the sense 'X11 resources').
%%%
%%% Note that it makes sense to keep such an object(s) persistent in a
%%% running Oz system, e.g. between Browser starts (and it's done so
%%% in the Browser case);
%%%
%%% This is implemented using the Oz Tcl/Tk interface(too);
%%%
%%% The idea belongs to Joachim (Niehren) - Thanks!
%%%
%%%

%%
local
   WhileLoop                            %  handmade;
in
   %%
   fun {WhileLoop ValueIn Fun IncFun}
      case {Fun ValueIn} then {WhileLoop {IncFun ValueIn} Fun IncFun}
      else ValueIn
      end
   end

   %%
   class X11ResourceCacheClass from Object.base
      %%
      %%
      feat
         Toplevel               %  toplevel frame wich is withdrawn;
         TestTW                 %  a test text widget used for checks;
      %% This one keeps a mapping between X11 fonts and {True,False};
      %% If a given is not (yet) there, it will be checked and the
      %% result is cached in;
         FontsCache
      %%  ... between X11 fonts and their resolution (i.e. pairs "x#y");
         FontResCache
      %% curosr shapes;
         CursorsCache

      %%
      attr
         smallestFont

      %%
      meth init
         %%
         %% various caches;
         self.FontsCache = {Dictionary.new}
         self.FontResCache = {Dictionary.new}
         self.CursorsCache = {Dictionary.new}
         smallestFont <- InitValue

         %%
         self.Toplevel = {New Tk.toplevel tkInit(withdraw: True)}
         %%
         %% Parameters are essential since they influence figuring
         %% out a font's resolution;
         self.TestTW =
         {New Tk.text tkInit(parent:self.Toplevel width:1 height:1 bd:0
                             exportselection:0 highlightthickness:0
                             padx:0 pady:0 selectborderwidth:0
                             spacing1:0 spacing2:0 spacing3:0)}
      end

      %%
      %%  ... though it's never used;
      meth close
         {self.Toplevel close}
         Object.base , close
      end

      %%
      %% Yields 'True' if the font exists, and updates the cache
      %% if needed;
      %%
      %% 'Font' must be an atom;
      %%
      meth tryFont(Font $)
         case {Dictionary.condGet self.FontsCache Font InitValue}
         of !True        then True
         [] !False       then False
         [] !InitValue   then R in
            R = {Tk.returnInt 'catch'(q(self.TestTW conf(font: Font)))} == 0

            %%
            {Dictionary.put self.FontsCache Font R}
            touch

            %%
            R
         else
            {Show '*******************************************************'}
            {Show 'X11ResourceCacheClass::tryFont: error!'}
            {Show '*******************************************************'}
            False
         end
      end

      %%
      %% Yields height and width of the font given (or zeros if it
      %% doesn't exist at all);
      %%
      meth getFontRes(Font ?XRes ?YRes)
         case {Dictionary.condGet self.FontResCache Font InitValue}
         of R = _#_    then XRes#YRes = R
         [] !InitValue then
            %%
            case X11ResourceCacheClass , tryFont(Font $) then
               {self.TestTW tk(conf font:Font)}
               %%
               %%
               YRes = {Tk.returnInt winfo(reqheight self.TestTW)}
               XRes = {Tk.returnInt winfo(reqwidth self.TestTW)}

               %%
               {Wait XRes}
               {Wait YRes}
            else
               %%
               YRes = XRes = 0
            end

            %%
            {Dictionary.put self.FontResCache Font XRes#YRes}
            %%  ... is essential (because of 'tryFont');
            touch
         else
            {Show '*******************************************************'}
            {Show 'X11ResourceCacheClass::getFontRes: error!'}
            {Show '*******************************************************'}
            XRes = YRes = 0
         end
      end

      %%
      %% Try to figure out the smallest possible font
      %% (with the smallest "pixel size", to be precise);
      meth getSmallestFont(?Font ?PixSize)
         case @smallestFont \= InitValue then
            @smallestFont = font(font:Font pixSize:PixSize)
         else
            %%
            %%  Theoretically, this can blow up
            %% (when we don't have any fonts at all :-));
            PixSize =
            {WhileLoop 2                % it seems to be a minimal pixelsize;
             fun {$ N} FN in
                FN = '-*-*-*-*-*-*-' # N # '-*-*-*-*-*-*-*'
                {Tk.returnInt 'catch'(q(self.TestTW conf(font: FN)))} \= 0
             end
             fun {$ N} N + 1 end}

            %%
            Font = '-*-*-*-*-*-*-' # PixSize # '-*-*-*-*-*-*-*'
            smallestFont <- font(font:Font pixSize:PixSize)
         end
      end

      %%
      meth tryCursor(CName $)
         case {Dictionary.condGet self.CursorsCache CName InitValue}
         of !True        then True
         [] !False       then False
         [] !InitValue   then R in
            R =
            {Tk.returnInt 'catch'(q(self.TestTW conf(cursor: CName)))} == 0

            %%
            {Dictionary.put self.CursorsCache CName R}
            touch

            %%
            R
         else
            {Show '*******************************************************'}
            {Show 'X11ResourceCacheClass::tryCursor: error!'}
            {Show '*******************************************************'}
            False
         end
      end

      %%
   end

   %%
end
