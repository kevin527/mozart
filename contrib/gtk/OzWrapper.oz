%%%
%%% Author:
%%%   Thorsten Brunklaus <bruni@ps.uni-sb.de>
%%%
%%% Copyright:
%%%   Thorsten Brunklaus, 2001
%%%
%%% Last Change:
%%%   $Date$ by $Author$
%%%   $Revision$
%%%
%%% This file is part of Mozart, an implementation of Oz 3:
%%%   http://www.mozart-oz.org
%%%
%%% See the file "LICENSE" or
%%%   http://www.mozart-oz.org/LICENSE.html
%%% for information on usage and redistribution
%%% of this file, and for a DISCLAIMER OF ALL
%%% WARRANTIES.
%%%

functor $
import
   Word at 'x-oz://boot/Word.ozf'
   Open
   Util
export
   'create' : Create
define
   class TextFile from Open.file Open.text end

   GtkFilePrepend = ["%%"
                     "%% This file is generated. Please do not edit."
                     "%%"
                     ""
                     "functor $"
                     "import"
                     "   GtkNative                             at 'GtkNative.so{native}'"
                     "   GOZCoreComponent('GOZCore' : GOZCore) at 'GOZCore.ozf'"
                     "export"]

   GdkFilePrepend = ["%%"
                     "%% This file is generated. Please do not edit."
                     "%%"
                     ""
                     "functor $"
                     "import"
                     "   GdkNative                             at 'GdkNative.so{native}'"
                     "   GOZCoreComponent('GOZCore' : GOZCore) at 'GOZCore.ozf'"
                     "export"
                     "   GetEvent"]

   CanvasFilePrepend = ["%%"
                        "%% This file is generated. Please do not edit."
                        "%%"
                        ""
                        "functor $"
                        "import"
                        "   GtkCanvasNative                       at 'GtkCanvasNative.so{native}'"
                        "   GOZCoreComponent('GOZCore' : GOZCore) at 'GOZCore.ozf'"
                        "   GTK                                   at 'GTK.ozf'"
                        "export"]

   GdkInitStub = ["define"
                  "   OzBase   = GOZCore.ozBase"
                  "   P2O      = GOZCore.pointerToObject"
                  "   O2P      = GOZCore.objectToPointer"
                  "   GetEvent = GOZCore.getGdkEvent"
                  "   class OzColorBase from OzBase"
                  "      meth new(R G B)"
                  "         @object = {GOZCore.allocColor R G B}"
                  "      end"
                  "   end"]

   GtkInitStub = ["define"
                  "   OzBase = GOZCore.ozBase"
                  "   P2O    = GOZCore.pointerToObject"
                  "   O2P    = GOZCore.objectToPointer"]

   CanvasInitStub = ["define"
                     "   P2O    = GOZCore.pointerToObject"
                     "   O2P    = GOZCore.objectToPointer"
                     "   Widget = GTK.widget"
                     "   Layout = GTK.layout"
                     "   Object = GTK.object"
                     "\\insert 'OzCanvasBase.oz'"]

   GtkEnd   = ["end"]

   class ClassRegistry
      attr
         classes
      meth new
         @classes = {Dictionary.new}
      end
      meth add(ClassKey ClassName)
         {Dictionary.put @classes {Util.toAtom ClassKey} ClassName}
      end
      meth member(Class $)
         case {Dictionary.condGet @classes Class nil}
         of nil then false
         [] _   then true
         end
      end
      meth className(Class $)
         {Dictionary.get @classes Class}
      end
   end

   ClassReg = {New ClassRegistry new}

   fun {IsStruct _#Entry}
      case Entry
      of struct(...) then true
      [] _           then false
      end
   end

   local
      fun {Forbidden NameS Ps}
         case Ps
         of Prefix|Pr then
            {Util.checkPrefix Prefix NameS} orelse {Forbidden NameS Pr}
         [] nil then false
         end
      end

      fun {NoVArgs Args}
         case {Reverse Args}
         of arg(type("va_list" "") _)|_ then false
         [] _                           then true
         end
      end
   in
      fun {FuncPrefix P Fs}
         fun {$ _#X}
            case X
            of function(Name _ Args) then
               NameS = {Util.toString Name}
            in
               {Util.checkPrefix P NameS} andthen {Not {Forbidden NameS Fs}}
               andthen {NoVArgs Args}
            [] _ then false
            end
         end
      end
   end

   local
      fun {MakeClassPrefixIter Ss NewSs Flag}
         case Ss
         of S|Sr then
            if {Char.isLower S}
            then {MakeClassPrefixIter Sr S|NewSs true}
            elseif Flag
            then {MakeClassPrefixIter Sr {Char.toLower S}|&_|NewSs false}
            else {MakeClassPrefixIter Sr {Char.toLower S}|NewSs true}
            end
         [] nil  then {Reverse &_|NewSs}
         end
      end
   in
      fun {MakeClassPrefix Ss}
         {MakeClassPrefixIter Ss nil true}
      end
   end

   fun {MakeArgs N I}
      case I
      of 0 then nil
      [] 1 then "A"#N
      else "A"#N#" "#{MakeArgs (N + 1) (I - 1)}
      end
   end

   local
      fun {Resolve D K}
         case {Dictionary.condGet D {Util.toAtom K} K}
         of alias(Name Ptrs) then {Resolve D {Util.toString Name#Ptrs}}
         [] type(Name Ptrs)  then {Util.toString Name#Ptrs}
         [] V                then V
         end
      end
      fun {IsCoreType Type}
         case Type
         of "gboolean"     then true
         [] "gchar"        then true
         [] "guchar"       then true
         [] "gint"         then true
         [] "int"          then true
         [] "guint"        then true
         [] "gshort"       then true
         [] "gushort"      then true
         [] "glong"        then true
         [] "gulong"       then true
         [] "gint8"        then true
         [] "guint8"       then true
         [] "gint16"       then true
         [] "guint16"      then true
         [] "gint32"       then true
         [] "guint32"      then true
         [] "gint64"       then true
         [] "guint64"      then true
         [] "gfloat"       then true
         [] "gdouble"      then true
         [] "double"       then true
         [] "gsize"        then true
         [] "gssize"       then true
         [] "gchar*"       then true
         [] "char*"        then true
         [] "const gchar*" then true
         [] "const char*"  then true
%        [] "..."          then true
         [] _              then false
         end
      end
   in
      fun {IsBasicType D Type}
         if {IsCoreType Type}
         then true
         elsecase {Dictionary.condGet D {Util.toAtom Type} nil}
         of alias(Name Ptrs) then {IsBasicType D {Util.toString Name#Ptrs}}
         [] type(Name Ptrs)  then {IsCoreType {Util.toString Name#Ptrs}}
         [] enum(...)        then true
         [] _                then false
         end
      end
   end

   fun {CheckFirstArg Prefix Args}
      case Args
      of arg(type(T P) _)|_ then
         PrefName = {Util.toString Prefix#"*"}
         Name     = {Util.firstLower {Util.toString T#P}}
      in
         PrefName == Name
      [] _ then false
      end
   end

   fun {GetConstKeys Prefix D}
      {Filter {Dictionary.keys D}
       fun {$ Key}
          case {Dictionary.get D Key}
          of enum(...) then {Util.checkPrefix Prefix {Util.toString Key}}
          [] _         then false
          end
       end}
   end

   local
      Constrs = ["new"           %% Default
                 "newWithLabel"  %% GtkButton(Box)
                 "newWithValues" %% GdkGC
                 "load"          %% GkdFont
                 "getSystem"     %% GdkColormap
                ]

      Containers = [%% All Container
                    "gtkContainerAdd"
                    %% GtkBox and Childs
                    "gtkBoxPackStart"
                    "gtkBoxPackEnd"
                    "gtkBoxPackStartDefaults"
                    "gtkBoxPackEndDefaults"
                    %% GtkPane and Childs
                    "gtkPanedPack1"
                    "gtkPanedPack2"
                    %% GtkNotebook and Childs
                    "gtkNotebookInsertPage"
                    "gtkNotebookInsertPageMenu"
                    "gtkNotebookPrependPage"
                    "gtkNotebookPrependPageMenu"
                    "gtkNotebookAppendPage"
                    "gtkNotebookAppendPageMenu"
                   ]
   in
      fun {IsConstructor S}
         {Member S Constrs}
      end
      fun {IsContainerAdd C}
         {Member C Containers}
      end
   end

   local
      fun {Search V Is}
         case Is
         of item(Name Val)|Ir then
            if {Util.toString V} == {Util.toString Name} then Val else {Search V Ir} end
         [] nil               then ({Pow 2 32} - 1)
         end
      end
   in
      fun {ComputeVal V Items}
         if {IsInt V}
         then V
         elseif {IsAtom V}
         then {ComputeVal {Search V Items} Items}
         elsecase V
         of 'inclor(expr, expr)'(...) then
            Vs = {Record.toList V}
         in
            {Word.toInt
             {FoldL Vs
              fun {$ E X}
                 {Word.orb E {Word.make 32 {ComputeVal X Items}}}
              end {Word.make 32 0}}}
         [] 'un_op cast_exp'('-' A) then
            if A == '1' then ({Pow 2 32} - 1) else 1 end
         end
      end
   end

   fun {QuoteName Name}
      case Name
      of "raise" then "'raise'"
      [] Name    then Name
      end
   end

   class GtkClasses from TextFile
      attr
         types      %% Type Dictionary
         allTypes   %% All Types Dictionary
         entries    %% Gtk Entries
         classes    %% Class Dictionary
         module     %% Module Name
         stdPrefix  %% Standard Prefix (Gtk, Gdk)
         enumPrefix %% Enum Prefix (GTK_, GDK_)
         filePrep   %% FilePrepend
         initStub   %% File Init Stub
         fileEnd    %% File End
      meth init(Types AllTypes Name)
         @types      = Types
         @allTypes   = AllTypes
         @entries    = {Dictionary.entries Types}
         @classes    = {Dictionary.new}
         @module     = "GtkNative"
         @stdPrefix  = "Gtk"
         @enumPrefix = "GTK_"
         @filePrep   = GtkFilePrepend
         @initStub   = GtkInitStub
         @fileEnd    = GtkEnd
         TextFile, init(name: Name
                        flags:[write create truncate])
         GtkClasses, collect({Filter @entries IsStruct})
         GtkClasses, emit
      end
      meth resolve(S $)
         case {Dictionary.condGet @types S nil}
         of nil         then raise resolve(unknown_type S) end
         [] alias(S _)  then GtkClasses, resolve(S $)
         [] Struct      then
            case Struct
            of struct(...) then Struct
            else raise resolve(unknown_data Struct) end
            end
         end
      end
      meth addClass(Parent Class Forbidden)
         ClassForbidden = {Map Forbidden
                           fun {$ X}
                              {MakeClassPrefix {Util.firstLower {Util.toString X}}}
                           end}
         ClassPrefix    = {MakeClassPrefix {Util.firstLower {Util.toString Class}}}
         Methods        = {Filter @entries {FuncPrefix ClassPrefix ClassForbidden}}
         ClassS         = {Util.toString Class}
      in
         {ClassReg add({Util.toString ClassS#"*"} {Util.cutPrefix @stdPrefix ClassS})}
         {Dictionary.put @classes Class
          'class'(anchestor: Parent
                  methods: Methods)}
      end
      meth searchAnchor(Keys $)
         case Keys
         of Key|Kr then
            Item = {Dictionary.get @classes Key}
         in
            case Item
            of anchor(...) then GtkClasses, searchAnchor(Kr $)
            [] _           then {Dictionary.put @classes Key anchor(Item)} Key
            end
         [] nil then nil
         end
      end
      meth removeAnchors(Keys)
         case Keys
         of Key|Kr then
            Classes = @classes
         in
            case {Dictionary.get Classes Key}
            of anchor(Item) then {Dictionary.put Classes Key Item}
            [] _            then skip
            end
            GtkClasses, removeAnchors(Kr)
         [] nil then skip
         end
      end
      meth collectSingleChilds(Key Ss)
         case Ss
         of S|Sr then
            case S
            of SN#struct(Is) then
               case Is
               of S|_ then
                  case S
                  of item(text(T) nil _) then
                     if Key == T
                     then
                        NewSN = {Util.stripPrefix {Util.toString SN}}
                     in
                        GtkClasses, addClass({Util.toAtom Key} {Util.toAtom NewSN} nil)
                     end
                  [] _ then skip
                  end
               [] _ then skip
               end
            [] _ then skip
            end
            GtkClasses, collectSingleChilds(Key Sr)
         [] nil then skip
         end
      end
      meth collectChilds(Ss)
         case GtkClasses, searchAnchor({Dictionary.keys @classes} $)
         of nil  then GtkClasses, removeAnchors({Dictionary.keys @classes})
         [] Root then
            GtkClasses, collectSingleChilds({Util.toString Root} Ss)
            GtkClasses, collectChilds(Ss)
         end
      end
      meth collect(Ss)
         GtkClasses, addClass('GtkOzBase' 'GtkObject' nil)
         GtkClasses, collectChilds(Ss)
      end
      meth emitClasses(Keys)
         case Keys
         of Class|Kr then
            ClassSN = {Util.toString Class}
         in
            case {Dictionary.get @classes Class}
            of 'class'(anchestor: Anchestor methods: Methods) then
               From    = " from "#{Util.cutPrefix @stdPrefix {Util.toString Anchestor}}
               ValName = {Util.cutPrefix @stdPrefix ClassSN}
            in
               TextFile, putS({Util.indent 1}#ValName#" = {Value.byNeed fun {$} class $"#From)
               GtkClasses, emitMethods({Util.firstLower ClassSN} Methods)
               TextFile, putS({Util.indent 1}#"end end}")
            end
            GtkClasses, emitClasses(Kr)
         [] nil then skip
         end
      end
      meth emitMethods(Prefix Ms)
         case Ms
         of (_#function(Name RetType Args))|Mr then
            GtkClasses, emitSingleMeth(Prefix Name RetType Args)
            GtkClasses, emitMethods(Prefix Mr)
         [] nil then skip
         end
      end
      meth emitSingleMeth(Prefix RawName RetType Args)
         Name      = {Util.translateName RawName}
         ShortName = {Util.firstLower {Util.cutPrefix {Util.firstLower Prefix} Name}}
         IsVoid    = case Args of [arg(type("void" "") _)] then true else false end
         IsNew     = {IsConstructor ShortName}
         IsCont    = {IsContainerAdd Name}
         HasSelf   = {CheckFirstArg Prefix Args}
         IA        = if IsVoid then 0 else ({Length Args} - (if HasSelf then 1 else 0 end)) end
         OA        = case RetType
                     of type("void" _) then 0
                     [] _              then 1
                     end
         ArgStr   = {MakeArgs 0 IA}#if IsNew then "" elseif OA == 0 then ""
                                    elseif IA == 0 then "Res" else " Res" end
         CallArgs =  if IsVoid then nil
                     else
                        PArgs = if {Not HasSelf} then Args
                                elsecase Args of _|Ar then Ar else Args end
                     in
                        GtkClasses, prepareArgs(PArgs 0 nil $)
                     end
         CallStr  = GtkClasses, callStr(CallArgs $)
         CallDef  = GtkClasses, callDef(CallArgs $)
         CheckRes = if OA == 0 then nil else GtkClasses, checkResStart(RetType $) end
         ResStart = if IsNew then "@object = " elseif OA == 1
                    then "Res = "#CheckRes else "" end
         ResEnd   = if IsNew orelse CheckRes == nil then "" else "}" end
         Self     = if HasSelf then "@object " else "" end
      in
         TextFile, putS({Util.indent 2}#"meth "#{QuoteName ShortName}#"("#ArgStr#")")
         if CallDef \= nil
         then
            TextFile, putS(CallDef)
            TextFile, putS({Util.indent 2}#"in")
         end
         TextFile, putS({Util.indent 3}#ResStart#"{"#@module#"."#Name#" "#Self#CallStr#"}"#ResEnd)
         GtkClasses, handleOutArgs(CallArgs)
         if Name == "gtkWindowNew"
         then
            TextFile, putS({Util.indent 3}#"if A0 == WINDOW_TOPLEVEL then gcMode <- toplevel end")
         end
         if IsCont then TextFile, putS({Util.indent 3}#"children <- A0|@children") end
         TextFile, putS({Util.indent 2}#"end")
      end
      meth prepareArgs(InArgs I OutArgs $)
         case InArgs
         of arg(type(Base Ptrs) _)|Ar then
            TStr = {Util.toString Base#Ptrs}
            Type = case TStr
                   of "int*"     then 'OUT'('Int')
                   [] "gint*"    then 'OUT'('Int')
                   [] "double*"  then 'OUT'('Double')
                   [] "gdouble*" then 'OUT'('Double')
                   [] _          then 'IN'
                   end
            Conv = if {IsBasicType @allTypes TStr} then 'NONE' else 'OBJECT' end
         in
            GtkClasses, prepareArgs(Ar (I + 1) (I#Type#Conv)|OutArgs $)
         [] nil then {Reverse OutArgs}
         end
      end
      meth callStr(Args $)
         case Args
         of Arg|Ar then
            Str = case Arg
                  of I#'OUT'(...)#_  then "AO"#I
                  [] I#'IN'#'NONE'   then "A"#I
                  [] I#'IN'#'OBJECT' then "{O2P A"#I#"}"
                  end
            Space = case Ar of nil then "" else " " end
         in
            (Str#Space)#GtkClasses, callStr(Ar $)
         [] nil then nil
         end
      end
      meth callDef(Args $)
         case Args
         of Arg|Ar then
            case Arg
            of I#'OUT'(Type)#_ then
               {Util.indent 3}#"AO"#I#" = {GOZCore.alloc"#Type#" unit}\n"#GtkClasses, callDef(Ar $)
            [] _ then GtkClasses, callDef(Ar $)
            end
         [] nil then nil
         end
      end
      meth handleOutArgs(Args)
         case Args
         of Arg|Ar then
            case Arg
            of I#'OUT'(Type)#_ then
               TextFile, putS({Util.indent 3}#"A"#I#" = {GOZCore.get"#Type#" AO"#I#"}")
               TextFile, putS({Util.indent 3}#"{GOZCore.freeData AO"#I#"}")
            [] _ then skip
            end
            GtkClasses, handleOutArgs(Ar)
         [] nil then skip
         end
      end
      meth checkResStart(Type $)
         case Type
         of type(Base Ptrs) then
            TypeS = {Util.toString Base#Ptrs}
            TypeA = {Util.toAtom TypeS}
         in
            if {IsBasicType @allTypes TypeS}
            then nil
            elseif {ClassReg member(TypeA $)}
            then "{P2O "#{ClassReg className(TypeA $)}#" "
            else nil %% Former Pointer Registration is no longer necessary
            end
         [] _ then nil
         end
      end
      meth emitInterface(Keys)
         case Keys
         of Key|Kr then
            Id = {Util.cutPrefix @stdPrefix {Util.toString Key}}
         in
            TextFile, putS({Util.indent 1}#Id)
            GtkClasses, emitInterface(Kr)
         [] nil then skip
         end
      end
      meth emitConstInterface(Keys)
         case Keys
         of Key|Kr then
            case {Dictionary.get @types Key}
            of enum(Items) then GtkClasses, emitSingleCI(Items)
            end
            GtkClasses, emitConstInterface(Kr)
         [] nil then skip
         end
      end
      meth emitSingleCI(Items)
         case Items
         of item(Name _)|Ir then
            NewName = case {Util.cutPrefix @enumPrefix {Util.toString Name}}
                      of "2BUTTON_PRESS" then "TWO_BUTTON_PRESS"
                      [] "3BUTTON_PRESS" then "THREE_BUTTON_PRESS"
                      [] Id then Id
                      end
         in
            TextFile, putS({Util.indent 1}#NewName)
            GtkClasses, emitSingleCI(Ir)
         [] nil then skip
         end
      end
      meth emitConstants(Keys)
         case Keys
         of Key|Kr then
            case {Dictionary.get @types Key}
            of enum(Items) then GtkClasses, emitSingleI(Items Items)
            end
            GtkClasses, emitConstants(Kr)
         [] nil then skip
         end
      end
      meth emitSingleI(Items All)
         case Items
         of item(Name Val)|Ir then
            NewName = case {Util.cutPrefix @enumPrefix {Util.toString Name}}
                      of "2BUTTON_PRESS" then "TWO_BUTTON_PRESS"
                      [] "3BUTTON_PRESS" then "THREE_BUTTON_PRESS"
                      [] Id then Id
                      end
            NewVal  = {ComputeVal Val All}
         in
            TextFile, putS({Util.indent 1}#NewName#" = "#NewVal)
            GtkClasses, emitSingleI(Ir All)
         [] nil then skip
         end
      end
      meth emit
         Keys      = {List.sort {Dictionary.keys @classes} Value.'<'}
         ConstKeys = {List.sort {GetConstKeys @stdPrefix @types} Value.'<'}
      in
         {ForAll @filePrep proc {$ L} TextFile, putS(L) end}
         GtkClasses, emitInterface(Keys)
         GtkClasses, emitConstInterface(ConstKeys)
         {ForAll @initStub proc {$ L} TextFile, putS(L) end}
         GtkClasses, emitClasses(Keys)
         GtkClasses, emitConstants(ConstKeys)
         {ForAll @fileEnd proc {$ L} TextFile, putS(L) end}
      end
   end

   GdkClassList = ["GdkColor" "GdkColorContext" "GdkColor" "GdkColormap"
                   "GdkDrawable" "GdkFont" "GdkGC" "GdkImage" "GdkImlib"]

   class GdkClasses from GtkClasses
      meth init(Types AllTypes Name)
         @types      = Types
         @allTypes   = AllTypes
         @entries    = {Dictionary.entries Types}
         @classes    = {Dictionary.new}
         @module     = "GdkNative"
         @stdPrefix  = "Gdk"
         @enumPrefix = "GDK_"
         @filePrep   = GdkFilePrepend
         @initStub   = GdkInitStub
         @fileEnd    = GtkEnd
         TextFile, init(name: Name
                        flags:[write create truncate])
         GdkClasses, collect(GdkClassList)
         GtkClasses, emit
      end
      meth collect(Keys)
         case Keys
         of Key|Kr then
            Base = if Key == "GdkColor" then "GdkOzColorBase" else "GdkOzBase" end
         in
            GtkClasses, addClass({Util.toAtom Base} {Util.toAtom Key} nil)
            GdkClasses, collect(Kr)
         [] nil then skip
         end
      end
   end

   CanvasClassList = ["GtkOzCanvasBase"#"GtkCanvas"#["GtkCanvasItem" "GtkCanvasGroup"]
                      "GtkObject"#"GtkCanvasItem"#nil
                      "GtkCanvasItem"#"GtkCanvasGroup"#nil]

   class CanvasClasses from GtkClasses
      meth init(Types AllTypes Name)
         @types      = Types
         @allTypes   = AllTypes
         @entries    = {Dictionary.entries Types}
         @classes    = {Dictionary.new}
         @module     = "GtkCanvasNative"
         @stdPrefix  = "Gtk"
         @enumPrefix = "GTK_"
         @filePrep   = CanvasFilePrepend
         @initStub   = CanvasInitStub
         @fileEnd    = GtkEnd
         TextFile, init(name: Name
                        flags:[write create truncate])
         CanvasClasses, collect(CanvasClassList)
         GtkClasses, emit
      end
      meth collect(Keys)
         case Keys
         of (Parent#Key#Forbidden)|Kr then
            GtkClasses, addClass({Util.toAtom Parent} {Util.toAtom Key} Forbidden)
            CanvasClasses, collect(Kr)
         [] nil then skip
         end
      end
   end

   local
      fun {FilterGtkTypes Type}
         if {IsName Type}
         then false
         else
            TypeS = {Util.translateName {Util.toString Type}}
         in
            if {Util.checkPrefix "gdk" TypeS}
            then false
            elseif {Util.checkPrefix "gtkCanvas" TypeS}
            then false
            else true
            end
         end
      end

      fun {FilterGdkTypes Type}
         if {IsName Type}
         then false
         else
            TypeS = {Util.translateName {Util.toString Type}}
         in
            {Util.checkPrefix "gdk" TypeS}
         end
      end

      fun {FilterCanvasTypes Type}
         if {IsName Type}
         then false
         else
            TypeS = {Util.translateName {Util.toString Type}}
         in
            {Util.checkPrefix "gtkCanvas" TypeS}
         end
      end

      fun {CopyTypes SourceD Keys DestD}
         case Keys
         of Key|Kr then
            {Dictionary.put DestD Key {Dictionary.get SourceD Key}}
            {CopyTypes SourceD Kr DestD}
         [] nil then DestD
         end
      end
   in
      proc {Create AllTypes}
         AllKeys = {Dictionary.keys AllTypes}
      in
         {ForAll [FilterGtkTypes#GtkClasses#"GTK.oz"
                  FilterGdkTypes#GdkClasses#"GDK.oz"
                  FilterCanvasTypes#CanvasClasses#"GTKCANVAS.oz"]
          proc {$ Fil#Class#File}
             Keys  = {Filter AllKeys Fil}
             Types = {CopyTypes AllTypes Keys {Dictionary.new}}
             Obj   = {New Class init(Types AllTypes File)}
          in
             {Obj close}
          end}
      end
   end
end
