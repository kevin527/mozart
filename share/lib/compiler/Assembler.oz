%%%
%%% Authors:
%%%   Leif Kornstaedt <kornstae@ps.uni-sb.de>
%%%   Ralf Scheidhauer <scheidhr@ps.uni-sb.de>
%%%
%%% Copyright:
%%%   Leif Kornstaedt and Ralf Scheidhauer, 1997
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

%%
%% This file defines the procedure `Assemble' which takes a list of
%% machine instructions, applies peephole optimizations and returns
%% an AssemblerClass object.  Its methods provide for the output,
%% feeding and loading of assembled machine code.
%%
%% Notes:
%% -- The code may contain no backward-only references to labels that
%%    are not reached during a forward-scan through the code.
%% -- The definition(...) and definitionCopy(...) instructions differ
%%    from the format expected by the assembler proper:  An additional
%%    argument stores the code for the definition's body.  This way,
%%    less garbage is produced during code generation.
%%

local
   local
      GetOpcode               = {`Builtin` getOpcode               2}
      GetInstructionSize      = {`Builtin` getInstructionSize      2}
      NewCodeBlock            = {`Builtin` newCodeBlock            2}
      MakeProc                = {`Builtin` makeProc                3}
      AddDebugInfo            = {`Builtin` addDebugInfo            3}
      StoreOpcode             = {`Builtin` storeOpcode             2}
      StoreNumber             = {`Builtin` storeNumber             2}
      StoreLiteral            = {`Builtin` storeLiteral            2}
      StoreFeature            = {`Builtin` storeFeature            2}
      StoreConstant           = {`Builtin` storeConstant           2}
      StoreInt                = {`Builtin` storeInt                2}
      StoreVariablename       = {`Builtin` storeVariablename       2}
      StoreRegisterIndex      = {`Builtin` storeRegisterIndex      2}
      StorePredicateRef       = {`Builtin` storePredicateRef       2}
      StoreRecordArity        = {`Builtin` storeRecordArity        2}
      StoreGRegRef            = {`Builtin` storeGRegRef            2}
      StoreLocation           = {`Builtin` storeLocation           2}
      StoreCache              = {`Builtin` storeCache              2}

      local
         BIStoreBuiltinname = {`Builtin` storeBuiltinname 2}
      in
         proc {StoreBuiltinname CodeBlock Builtin}
            {BIStoreBuiltinname CodeBlock {`Builtin` Builtin ~1}}
         end
      end

      proc {StoreRegister CodeBlock Register}
         {StoreRegisterIndex CodeBlock Register.1}
      end

      proc {StoreXRegisterIndex CodeBlock x(Index)}
         {StoreRegisterIndex CodeBlock Index}
      end

      proc {StoreYRegisterIndex CodeBlock y(Index)}
         {StoreRegisterIndex CodeBlock Index}
      end

      local
         BIStoreLabel = {`Builtin` storeLabel 2}
      in
         proc {StoreLabel CodeBlock Lbl LabelDict}
            {BIStoreLabel CodeBlock {Dictionary.get LabelDict Lbl}}
         end
      end

      local
         BIStorePredId = {`Builtin` storePredId 6}
         BIpredIdFlags = {`Builtin` predIdFlags 2}
         NativeFlag
         CopyOnceFlag
      in
         {BIpredIdFlags CopyOnceFlag NativeFlag}
         proc {StorePredId CodeBlock pid(Name Arity File Line CopyOnce Native)}
            CopyFlag = case CopyOnce then CopyOnceFlag else 0 end
            NatFlag  = case Native   then NativeFlag else 0 end
         in
            {BIStorePredId CodeBlock Name Arity File Line NatFlag+CopyFlag}
         end
      end

      local
         BINewHashTable  = {`Builtin` newHashTable  4}
         BIStoreHTScalar = {`Builtin` storeHTScalar 4}
         BIStoreHTRecord = {`Builtin` storeHTRecord 5}
      in
         proc {StoreHashTableRef CodeBlock ht(ElseLabel List) LabelDict}
            Addr = {Dictionary.get LabelDict ElseLabel}
            HTable = {BINewHashTable CodeBlock {Length List} Addr}
         in
            {ForAll List
             proc {$ Entry}
                case Entry of onScalar(NumOrLit Lbl) then Addr in
                   Addr = {Dictionary.get LabelDict Lbl}
                   {BIStoreHTScalar CodeBlock HTable NumOrLit Addr}
                [] onRecord(Label RecordArity Lbl) then Addr in
                   Addr = {Dictionary.get LabelDict Lbl}
                   {BIStoreHTRecord CodeBlock HTable Label RecordArity Addr}
                end
             end}
         end
      end

      local
         BIStoreGenCallInfo  = {`Builtin` storeGenCallInfo 6}
      in
         proc {StoreGenCallInfo CodeBlock
               gci(g(Index) IsMethod Name IsTail RecordArity)}
            {BIStoreGenCallInfo
             CodeBlock Index IsMethod Name IsTail RecordArity}
         end
      end

      local
         BIStoreApplMethInfo = {`Builtin` storeApplMethInfo 3}
      in
         proc {StoreApplMethInfo CodeBlock ami(Name RecordArity)}
            {BIStoreApplMethInfo CodeBlock Name RecordArity}
         end
      end

      \insert compiler-Opcodes

      local
         IsUniqueName           = {`Builtin` 'isUniqueName'           2}
         IsCopyableName         = {`Builtin` 'isCopyableName'         2}
         IsCopyablePredicateRef = {`Builtin` 'isCopyablePredicateRef' 2}
         ForeignPointerToInt    = {`Builtin` 'ForeignPointerToInt'    2}

         fun {ListToVirtualString Vs In FPToIntMap}
            case Vs of V|Vr then
               {ListToVirtualString Vr
                In#' '#{MyValueToVirtualString V FPToIntMap} FPToIntMap}
            [] nil then In
            end
         end

         fun {TupleSub I N In Value FPToIntMap}
            case I =< N then
               {TupleSub I + 1 N
                In#' '#{MyValueToVirtualString Value.I FPToIntMap}
                Value FPToIntMap}
            else In
            end
         end

         fun {TupleToVirtualString Value FPToIntMap}
            {TupleSub 2 {Width Value}
             {Label Value}#'('#{MyValueToVirtualString Value.1 FPToIntMap}
             Value FPToIntMap}#')'
         end

         fun {MyValueToVirtualString Value FPToIntMap}
            case {IsName Value} then
               case Value of true then 'true'
               [] false then 'false'
               [] unit then 'unit'
               elsecase {IsUniqueName Value} then
                  %--** these only work if the name's print name is friendly
                  %--** and all names' print names are distinct
                  '<U: '#{System.printName Value}#'>'
               elsecase {IsCopyableName Value} then
                  '<M: '#{System.printName Value}#'>'
               else
                  '<N: '#{System.printName Value}#'>'
               end
            elsecase {IsAtom Value} then
               % the atom must not be mistaken for a token
               case {HasFeature InstructionSizes Value} then '\''#Value#'\''
               elsecase Value of lbl then '\'lbl\''
               [] pid then '\'pid\''
               [] ht then '\'ht\''
               [] onScalar then '\'onScalar\''
               [] onRecord then '\'onRecord\''
               [] gci then '\'gci\''
               [] ami then '\'ami\''
               else
                  {System.valueToVirtualString Value 0 0}
               end
            elsecase {Foreign.pointer.is Value} then I in
               % foreign pointers are assigned increasing integers
               % in order of appearance so that diffs are sensible
               I = {ForeignPointerToInt Value}
               case {IsCopyablePredicateRef Value} then '<Q: '
               else '<P: '
               end#
               case {Dictionary.condGet FPToIntMap I unit} of unit then N in
                  N = {Dictionary.get FPToIntMap 0} + 1
                  {Dictionary.put FPToIntMap 0 N}
                  {Dictionary.put FPToIntMap I N}
                  N
               elseof V then
                  V
               end#'>'
            elsecase Value of V1|Vr then
               {ListToVirtualString Vr
                '['#{MyValueToVirtualString V1 FPToIntMap} FPToIntMap}#']'
            [] V1#V2 then
               {MyValueToVirtualString V1 FPToIntMap}#"#"#
               {MyValueToVirtualString V2 FPToIntMap}
            elsecase {IsTuple Value} then
               {TupleToVirtualString Value FPToIntMap}
            else
               {System.valueToVirtualString Value 1000 1000}
            end
         end
      in
         fun {InstrToVirtualString Instr FPToIntMap}
            case {IsAtom Instr} then
               Instr
            else
               {TupleToVirtualString Instr FPToIntMap}
            end
         end
      end
   in
      class AssemblerClass
         prop final
         attr InstrsHd InstrsTl LabelDict Size
         feat Profile DebugInfoControl
         meth init(ProfileSwitch DebugInfoControlSwitch)
            InstrsHd <- 'skip'|@InstrsTl
            LabelDict <- {NewDictionary}
            Size <- InstructionSizes.'skip'
            % Code must not start at address 0, since this is interpreted as
            % NOCODE by the emulator - thus the dummy instruction 'skip'.
            self.Profile = ProfileSwitch
            self.DebugInfoControl = DebugInfoControlSwitch
         end
         meth newLabel(?L)
            L = {NewName}
            {Dictionary.put @LabelDict L _}
         end
         meth declareLabel(L)
            case {Dictionary.member @LabelDict L} then skip
            else {Dictionary.put @LabelDict L _}
            end
         end
         meth isLabelUsed(I $)
            {Dictionary.member @LabelDict I}
         end
         meth setLabel(L)
            case {Dictionary.member @LabelDict L} then
               {Dictionary.get @LabelDict L} = @Size
            else
               {Dictionary.put @LabelDict L @Size}
            end
         end
         meth append(Instr) NewTl NewInstr in
            case Instr
            of definition(_ L _ _ _) then AssemblerClass, declareLabel(L)
            [] definitionCopy(_ L _ _ _) then AssemblerClass, declareLabel(L)
            [] endDefinition(L) then AssemblerClass, declareLabel(L)
            [] branch(L) then AssemblerClass, declareLabel(L)
            [] 'thread'(L) then AssemblerClass, declareLabel(L)
            [] exHandler(L) then AssemblerClass, declareLabel(L)
            [] createCond(L _) then AssemblerClass, declareLabel(L)
            [] nextClause(L) then AssemblerClass, declareLabel(L)
            [] shallowGuard(L _) then AssemblerClass, declareLabel(L)
            [] inlinePlus1(X1 X2 NLiveRegs) then
               case self.DebugInfoControl then
                  NewInstr = callBI('+1' [X1]#[X2] NLiveRegs)
               else skip
               end
            [] inlineMinus1(X1 X2 NLiveRegs) then
               case self.DebugInfoControl then
                  NewInstr = callBI('-1' [X1]#[X2] NLiveRegs)
               else skip
               end
            [] inlinePlus(X1 X2 X3 NLiveRegs) then
               case self.DebugInfoControl then
                  NewInstr = callBI('+' [X1 X2]#[X3] NLiveRegs)
               else skip
               end
            [] inlineMinus(X1 X2 X3 NLiveRegs) then
               case self.DebugInfoControl then
                  NewInstr = callBI('-' [X1 X2]#[X3] NLiveRegs)
               else skip
               end
            [] testBI(_ _ L _) then AssemblerClass, declareLabel(L)
            [] testLT(X1 X2 X3 L NLiveRegs) then
               AssemblerClass, declareLabel(L)
               case self.DebugInfoControl then
                  NewInstr = testBI('<' [X1 X2]#[X3] L NLiveRegs)
               else skip
               end
            [] testLE(X1 X2 X3 L NLiveRegs) then
               AssemblerClass, declareLabel(L)
               case self.DebugInfoControl then
                  NewInstr = callBI('=<' [X1 X2]#[X3] L NLiveRegs)
               else skip
               end
            [] testLiteral(_ _ L1 L2 _) then
               AssemblerClass, declareLabel(L1)
               AssemblerClass, declareLabel(L2)
            [] testNumber(_ _ L1 L2 _) then
               AssemblerClass, declareLabel(L1)
               AssemblerClass, declareLabel(L2)
            [] testBool(_ L1 L2 L3 _) then
               AssemblerClass, declareLabel(L1)
               AssemblerClass, declareLabel(L2)
               AssemblerClass, declareLabel(L3)
            [] testRecord(_ _ _ L1 L2 _) then
               AssemblerClass, declareLabel(L1)
               AssemblerClass, declareLabel(L2)
            [] testList(_ L1 L2 _) then
               AssemblerClass, declareLabel(L1)
               AssemblerClass, declareLabel(L2)
            [] match(_ HT NLiveRegs) then ht(L Cases) = HT in
               AssemblerClass, declareLabel(L)
               {ForAll Cases
                proc {$ Case}
                   case Case
                   of onScalar(_ L) then AssemblerClass, declareLabel(L)
                   [] onRecord(_ _ L) then AssemblerClass, declareLabel(L)
                   end
                end}
            [] lockThread(L _ _) then AssemblerClass, declareLabel(L)
            else skip
            end
            case {IsFree NewInstr} then NewInstr = Instr else skip end
            @InstrsTl = NewInstr|NewTl
            InstrsTl <- NewTl
            Size <- @Size + InstructionSizes.{Label NewInstr}
            case NewInstr of definition(_ _ _ _ _) then
               case self.Profile then
                  AssemblerClass, append(profileProc)
               else skip
               end
            [] definitionCopy(_ _ _ _ _) then
               case self.Profile then
                  AssemblerClass, append(profileProc)
               else skip
               end
            else skip
            end
         end
         meth output($) AddrToLabelMap FPToIntMap in
            AssemblerClass, MarkEnd()
            AddrToLabelMap = {NewDictionary}
            FPToIntMap = {NewDictionary}
            {Dictionary.put FPToIntMap 0 0}
            {ForAll {Dictionary.entries @LabelDict}
             proc {$ Label#Addr}
                case {IsDet Addr} then
                   {Dictionary.put AddrToLabelMap Addr Label}
                else skip
                end
             end}
            '%% Code Size:\n'#@Size#' % words\n'#
            AssemblerClass, OutputSub(@InstrsHd AddrToLabelMap FPToIntMap 0 $)
         end
         meth OutputSub(Instrs AddrToLabelMap FPToIntMap Addr ?VS)
            case Instrs of Instr|Ir then LabelVS NewInstr VSRest NewAddr in
               LabelVS = case {Dictionary.member AddrToLabelMap Addr} then
                            'lbl('#Addr#')'#
                            case Addr < 100 then '\t\t' else '\t' end
                         else '\t\t'
                         end
               AssemblerClass, TranslateInstrLabels(Instr ?NewInstr)
               VS = (LabelVS#{InstrToVirtualString NewInstr FPToIntMap}#'\n'#
                     VSRest)
               NewAddr = Addr + InstructionSizes.{Label Instr}
               AssemblerClass, OutputSub(Ir AddrToLabelMap FPToIntMap NewAddr
                                         ?VSRest)
            [] nil then
               VS = ""
            end
         end
         meth load(Globals ?P) CodeBlock in
            AssemblerClass, MarkEnd()
            CodeBlock = {NewCodeBlock @Size}
            {ForAll @InstrsHd
             proc {$ Instr} {StoreInstr Instr CodeBlock @LabelDict} end}
            P = {MakeProc CodeBlock Globals}
         end
         meth MarkEnd()
            @InstrsTl = nil
         end

         meth TranslateInstrLabels(Instr $)
            case Instr of definition(X1 L X2 X3 X4) then A in
               A = {Dictionary.get @LabelDict L}
               definition(X1 A X2 X3 X4)
            [] definitionCopy(X1 L X2 X3 X4) then A in
               A = {Dictionary.get @LabelDict L}
               definitionCopy(X1 A X2 X3 X4)
            [] endDefinition(L) then A in
               A = {Dictionary.get @LabelDict L}
               endDefinition(A)
            [] branch(L) then A in
               A = {Dictionary.get @LabelDict L}
               branch(A)
            [] 'thread'(L) then A in
               A = {Dictionary.get @LabelDict L}
               'thread'(A)
            [] exHandler(L) then A in
               A = {Dictionary.get @LabelDict L}
               exHandler(A)
            [] createCond(L X) then A in
               A = {Dictionary.get @LabelDict L}
               createCond(A X)
            [] nextClause(L) then A in
               A = {Dictionary.get @LabelDict L}
               nextClause(A)
            [] shallowGuard(L X) then A in
               A = {Dictionary.get @LabelDict L}
               shallowGuard(A X)
            [] testBI(X1 X2 L X3) then A in
               A = {Dictionary.get @LabelDict L}
               testBI(X1 X2 A X3)
            [] testLT(X1 X2 X3 L X4) then A in
               A = {Dictionary.get @LabelDict L}
               testLT(X1 X2 X3 A X4)
            [] testLE(X1 X2 X3 L X4) then A in
               A = {Dictionary.get @LabelDict L}
               testLE(X1 X2 X3 A X4)
            [] testLiteral(X1 X2 L1 L2 X3) then A1 A2 in
               A1 = {Dictionary.get @LabelDict L1}
               A2 = {Dictionary.get @LabelDict L2}
               testLiteral(X1 X2 A1 A2 X3)
            [] testNumber(X1 X2 L1 L2 X3) then A1 A2 in
               A1 = {Dictionary.get @LabelDict L1}
               A2 = {Dictionary.get @LabelDict L2}
               testNumber(X1 X2 A1 A2 X3)
            [] testRecord(X1 X2 X3 L1 L2 X4) then A1 A2 in
               A1 = {Dictionary.get @LabelDict L1}
               A2 = {Dictionary.get @LabelDict L2}
               testRecord(X1 X2 X3 A1 A2 X4)
            [] testList(X1 L1 L2 X2) then A1 A2 in
               A1 = {Dictionary.get @LabelDict L1}
               A2 = {Dictionary.get @LabelDict L2}
               testList(X1 A1 A2 X2)
            [] testBool(X1 L1 L2 L3 X2) then A1 A2 A3 in
               A1 = {Dictionary.get @LabelDict L1}
               A2 = {Dictionary.get @LabelDict L2}
               A3 = {Dictionary.get @LabelDict L3}
               testBool(X1 A1 A2 A3 X2)
            [] match(X HT NLiveRegs) then ht(L Cases) = HT A NewCases in
               A = {Dictionary.get @LabelDict L}
               NewCases = {Map Cases
                           fun {$ Case}
                              case Case of onScalar(X L) then A in
                                 A = {Dictionary.get @LabelDict L}
                                 onScalar(X A)
                              [] onRecord(X1 X2 L) then A in
                                 A = {Dictionary.get @LabelDict L}
                                 onRecord(X1 X2 A)
                              end
                           end}
               match(X ht(A NewCases) NLiveRegs)
            [] lockThread(L X1 X2) then A in
               A = {Dictionary.get @LabelDict L}
               lockThread(A X1 X2)
            else
               Instr
            end
         end
      end
   end

   fun {RecordArityWidth RecordArity}
      case {IsInt RecordArity} then RecordArity
      else {Length RecordArity}
      end
   end

   proc {GetClears Instrs ?Clears ?Rest}
      case Instrs of I1|Ir then
         case I1 of clear(_) then Cr in
            Clears = I1|Cr
            {GetClears Ir ?Cr ?Rest}
         else
            Clears = nil
            Rest = Instrs
         end
      [] nil then
         Clears = nil
         Rest = nil
      end
   end

   proc {SetVoids Instrs InI ?OutI ?Rest}
      case Instrs of I1|Ir then
         case I1 of setVoid(J) then
            {SetVoids Ir InI + J ?OutI ?Rest}
         else
            OutI = InI
            Rest = Instrs
         end
      [] nil then
         OutI = InI
         Rest = nil
      end
   end

   proc {UnifyVoids Instrs InI ?OutI ?Rest}
      case Instrs of I1|Ir then
         case I1 of unifyVoid(J) then
            {UnifyVoids Ir InI + J ?OutI ?Rest}
         else
            OutI = InI
            Rest = Instrs
         end
      [] nil then
         OutI = InI
         Rest = nil
      end
   end

   proc {GetVoids Instrs InI ?OutI ?Rest}
      case Instrs of I1|Ir then
         case I1 of getVoid(J) then
            {GetVoids Ir InI + J ?OutI ?Rest}
         else
            OutI = InI
            Rest = Instrs
         end
      [] nil then
         OutI = InI
         Rest = nil
      end
   end

   proc {MakeDeAllocate I Assembler}
      case I of 0 then skip
      [] 1 then {Assembler append(deAllocateL1)}
      [] 2 then {Assembler append(deAllocateL2)}
      [] 3 then {Assembler append(deAllocateL3)}
      [] 4 then {Assembler append(deAllocateL4)}
      [] 5 then {Assembler append(deAllocateL5)}
      [] 6 then {Assembler append(deAllocateL6)}
      [] 7 then {Assembler append(deAllocateL7)}
      [] 8 then {Assembler append(deAllocateL8)}
      [] 9 then {Assembler append(deAllocateL9)}
      [] 10 then {Assembler append(deAllocateL10)}
      else {Assembler append(deAllocateL)}
      end
   end

   fun {SkipDeadCode Instrs Assembler}
      case Instrs of I1|Rest then
         case I1 of lbl(I) then
            case {Assembler isLabelUsed(I $)} then Instrs
            else {SkipDeadCode Rest Assembler}
            end
         [] endDefinition(_) then Instrs
         [] globalVarname(_) then Instrs
         [] localVarname(_) then Instrs
         else {SkipDeadCode Rest Assembler}
         end
      [] nil then nil
      end
   end

   proc {EliminateDeadCode Instrs Assembler}
      {Peephole {SkipDeadCode Instrs Assembler} Assembler}
   end

   proc {Peephole Instrs Assembler}
      case Instrs of I1|Rest then
         case I1 of lbl(I) then
            {Assembler setLabel(I)}
            {Peephole Rest Assembler}
         [] definition(Register Label PredId PredicateRef GRegRef Code) then
            {Assembler
             append(definition(Register Label PredId PredicateRef GRegRef))}
            {Peephole Code Assembler}
            {Peephole Rest Assembler}
         [] definitionCopy(Register Label PredId PredicateRef GRegRef Code)
         then
            {Assembler
             append(definitionCopy(Register Label PredId PredicateRef
                                   GRegRef))}
            {Peephole Code Assembler}
            {Peephole Rest Assembler}
         [] clear(_) then Clears Rest in
            {GetClears Instrs ?Clears ?Rest}
            case Rest of deAllocateL(_)|_ then skip
            else
               {ForAll Clears
                proc {$ clear(Y)}
                   {Assembler append(clearY(Y))}
                end}
            end
            {Peephole Rest Assembler}
         [] move(_ _) then
            case Instrs
            of move(X1=x(_) Y1=y(_))|move(X2=x(_) Y2=y(_))|Rest then
               {Assembler append(moveMoveXYXY(X1 Y1 X2 Y2))}
               {Peephole Rest Assembler}
            elseof move(Y1=y(_) X1=x(_))|move(Y2=y(_) X2=x(_))|Rest then
               {Assembler append(moveMoveYXYX(Y1 X1 Y2 X2))}
               {Peephole Rest Assembler}
            elseof move(X1=x(_) Y1=y(_))|move(Y2=y(_) X2=x(_))|Rest then
               {Assembler append(moveMoveXYYX(X1 Y1 Y2 X2))}
               {Peephole Rest Assembler}
            elseof move(Y1=y(_) X1=x(_))|move(X2=x(_) Y2=y(_))|Rest then
               {Assembler append(moveMoveYXXY(Y1 X1 X2 Y2))}
               {Peephole Rest Assembler}
            else
               {Assembler append(I1)}
               {Peephole Rest Assembler}
            end
         [] createVariable(_) then
            case Instrs
            of createVariable(R)|move(R X=x(_))|Rest then
               {Peephole createVariableMove(R X)|Rest Assembler}
            elseof createVariable(X=x(_))|move(X R)|Rest then
               {Peephole createVariableMove(R X)|Rest Assembler}
            else
               {Assembler append(I1)}
               {Peephole Rest Assembler}
            end
         [] putRecord(L A R) then
            case L == '|' andthen A == 2 then
               {Assembler append(putList(R))}
            else
               {Assembler append(I1)}
            end
            {Peephole Rest Assembler}
         [] setVoid(I) then OutI Rest1 in
            {SetVoids Rest I ?OutI ?Rest1}
            {Assembler append(setVoid(OutI))}
            {Peephole Rest1 Assembler}
         [] getRecord(L A R) then
            case L == '|' andthen A == 2 then
               case R of X1=x(_) then
                  case Rest of unifyValue(R)|unifyVariable(X2=x(_))|Rest then
                     {Assembler append(getListValVar(X1 R X2))}
                     {Peephole Rest Assembler}
                  else
                     {Assembler append(getList(R))}
                     {Peephole Rest Assembler}
                  end
               else
                  {Assembler append(getList(R))}
                  {Peephole Rest Assembler}
               end
            else
               {Assembler append(I1)}
               {Peephole Rest Assembler}
            end
         [] unifyValue(R1) then
            case Rest of unifyVariable(R2)|Rest then
               {Assembler append(unifyValVar(R1 R2))}
               {Peephole Rest Assembler}
            else
               {Assembler append(I1)}
               {Peephole Rest Assembler}
            end
         [] unifyVoid(I) then OutI Rest1 in
            {UnifyVoids Rest I ?OutI ?Rest1}
            {Assembler append(unifyVoid(OutI))}
            {Peephole Rest1 Assembler}
         [] allocateL(I) then
            case I of 0 then skip
            [] 1 then {Assembler append(allocateL1)}
            [] 2 then {Assembler append(allocateL2)}
            [] 3 then {Assembler append(allocateL3)}
            [] 4 then {Assembler append(allocateL4)}
            [] 5 then {Assembler append(allocateL5)}
            [] 6 then {Assembler append(allocateL6)}
            [] 7 then {Assembler append(allocateL7)}
            [] 8 then {Assembler append(allocateL8)}
            [] 9 then {Assembler append(allocateL9)}
            [] 10 then {Assembler append(allocateL10)}
            else {Assembler append(allocateL(I))}
            end
            {Peephole Rest Assembler}
         [] deAllocateL(I) then
            case Rest of return|(Rest2=lbl(_)|deAllocateL(!I)|return|_) then
               {Peephole Rest2 Assembler}
            else
               {MakeDeAllocate I Assembler}
               {Peephole Rest Assembler}
            end
         [] failure then
            case Rest of deAllocateL(I)|Rest then
               {MakeDeAllocate I Assembler}
               {Assembler append(failure)}
               {EliminateDeadCode Rest Assembler}
            else
               {Assembler append(failure)}
               {EliminateDeadCode Rest Assembler}
            end
         [] branch(L) then Rest1 in
            {Assembler declareLabel(L)}
            Rest1 = {SkipDeadCode Rest Assembler}
            case Rest1 of lbl(!L)|_ then skip
            else {Assembler append(branch(L))}
            end
            {Peephole Rest1 Assembler}
         [] return then
            {Assembler append(return)}
            {EliminateDeadCode Rest Assembler}
         [] callBI(Builtinname Args NLiveRegs) then
            BIInfo = {GetBuiltinInfo Builtinname}
         in
            case Builtinname of '+1' then [X1]#[X2] = Args in
               {Assembler append(inlinePlus1(X1 X2 NLiveRegs))}
            [] '-1' then [X1]#[X2] = Args in
               {Assembler append(inlineMinus1(X1 X2 NLiveRegs))}
            [] '+' then [X1 X2]#[X3] = Args in
               {Assembler append(inlinePlus(X1 X2 X3 NLiveRegs))}
            [] '-' then [X1 X2]#[X3] = Args in
               {Assembler append(inlineMinus(X1 X2 X3 NLiveRegs))}
            [] '>' then [X1 X2]#Out = Args in
               {Assembler append(callBI('<' [X2 X1]#Out NLiveRegs))}
            [] '>=' then [X1 X2]#Out = Args in
               {Assembler append(callBI('=<' [X2 X1]#Out NLiveRegs))}
            [] '^' then [X1 X2]#[X3] = Args in
               {Assembler append(inlineUparrow(X1 X2 X3 NLiveRegs))}
            else
               {Assembler append(I1)}
            end
            case {CondSelect BIInfo doesNotReturn false} then
               {EliminateDeadCode Rest Assembler}
            else
               {Peephole Rest Assembler}
            end
         [] genCall(GCI Arity) then
            Arity = 0
            case Rest of deAllocateL(I)|return|Rest then
               {MakeDeAllocate I Assembler}
               {Assembler append(genCall({AdjoinAt GCI 4 true} Arity))}
               {EliminateDeadCode Rest Assembler}
            elseof lbl(_)|deAllocateL(I)|_ then
               {MakeDeAllocate I Assembler}
               {Assembler append(genCall({AdjoinAt GCI 4 true} Arity))}
               {Peephole Rest Assembler}
            else
               {Assembler append(I1)}
               {Peephole Rest Assembler}
            end
         [] call(R Arity) then NewR in
            % this test is needed to ensure correctness, since the emulator
            % does not save X registers with indices > Arity:
            case {Label R} == x andthen R.1 > Arity then
               {Assembler append(move(R NewR=x(Arity)))}
            else
               NewR = R
            end
            case Rest of deAllocateL(I)|return|Rest then NewNewR in
               case NewR of y(_) then
                  {Assembler append(move(NewR NewNewR=x(Arity)))}
               else
                  NewNewR = NewR
               end
               {MakeDeAllocate I Assembler}
               {Assembler append(tailCall(NewNewR Arity))}
               {EliminateDeadCode Rest Assembler}
            elseof lbl(_)|deAllocateL(I)|_ then NewNewR in
               case NewR of y(_) then
                  {Assembler append(move(NewR NewNewR=x(Arity)))}
               else
                  NewNewR = NewR
               end
               {MakeDeAllocate I Assembler}
               {Assembler append(tailCall(NewNewR Arity))}
               {Peephole Rest Assembler}
            else
               {Assembler append(call(NewR Arity))}
               {Peephole Rest Assembler}
            end
         [] genFastCall(PredicateRef ArityAndIsTail) then
            case ArityAndIsTail mod 2 == 1 then
               {Assembler append(I1)}
               {Peephole Rest Assembler}
            elsecase Rest of deAllocateL(I)|return|Rest then
               {MakeDeAllocate I Assembler}
               {Assembler append(genFastCall(PredicateRef ArityAndIsTail + 1))}
               {EliminateDeadCode Rest Assembler}
            elseof lbl(_)|deAllocateL(I)|_ then
               {MakeDeAllocate I Assembler}
               {Assembler append(genFastCall(PredicateRef ArityAndIsTail + 1))}
               {Peephole Rest Assembler}
            else
               {Assembler append(I1)}
               {Peephole Rest Assembler}
            end
         [] marshalledFastCall(Abstraction ArityAndIsTail) then
            case {IsDet Abstraction} andthen {IsProcedure Abstraction}
               andthen (ArityAndIsTail mod 2 == 0)
            then
               case Rest of deAllocateL(I)|return|Rest then
                  {MakeDeAllocate I Assembler}
                  {Assembler
                   append(marshalledFastCall(Abstraction ArityAndIsTail + 1))}
                  {EliminateDeadCode Rest Assembler}
               elseof lbl(_)|deAllocateL(I)|_ then
                  {MakeDeAllocate I Assembler}
                  {Assembler
                   append(marshalledFastCall(Abstraction ArityAndIsTail + 1))}
                  {Peephole Rest Assembler}
               else
                  {Assembler append(I1)}
                  {Peephole Rest Assembler}
               end
            else
               {Assembler append(I1)}
               {Peephole Rest Assembler}
            end
         [] sendMsg(Literal R RecordArity Cache) then NewR Arity in
            % this test is needed to ensure correctness, since the emulator
            % does not save X registers with indices > Arity:
            Arity = {RecordArityWidth RecordArity}
            case {Label R} == x andthen R.1 > Arity then
               {Assembler append(move(R NewR=x(Arity)))}
            else
               NewR = R
            end
            case Rest of deAllocateL(I)|return|Rest then NewNewR in
               case NewR of y(_) then
                  {Assembler append(move(NewR NewNewR=x(Arity)))}
               else
                  NewNewR = NewR
               end
               {MakeDeAllocate I Assembler}
               {Assembler
                append(tailSendMsg(Literal NewNewR RecordArity Cache))}
               {EliminateDeadCode Rest Assembler}
            elseof lbl(_)|deAllocateL(I)|_ then NewNewR in
               case NewR of y(_) then
                  {Assembler append(move(NewR NewNewR=x(Arity)))}
               else
                  NewNewR = NewR
               end
               {MakeDeAllocate I Assembler}
               {Assembler
                append(tailSendMsg(Literal NewNewR RecordArity Cache))}
               {Peephole Rest Assembler}
            else
               {Assembler append(sendMsg(Literal NewR RecordArity Cache))}
               {Peephole Rest Assembler}
            end
         [] applMeth(AMI R) then
            case Rest of deAllocateL(I)|return|Rest then NewR in
               case R of y(_) then
                  NewR = x({RecordArityWidth AMI.2})
                  {Assembler append(move(R NewR))}
               else
                  NewR = R
               end
               {MakeDeAllocate I Assembler}
               {Assembler append(tailApplMeth(AMI NewR))}
               {EliminateDeadCode Rest Assembler}
            elseof lbl(_)|deAllocateL(I)|_ then NewR in
               case R of y(_) then
                  NewR = x({RecordArityWidth AMI.2})
                  {Assembler append(move(R NewR))}
               else
                  NewR = R
               end
               {MakeDeAllocate I Assembler}
               {Assembler append(tailApplMeth(AMI NewR))}
               {Peephole Rest Assembler}
            else
               {Assembler append(I1)}
               {Peephole Rest Assembler}
            end
         [] clause then
            case Rest of wait|Rest then
               {Assembler append(emptyClause)}
               {Peephole Rest Assembler}
            else
               {Assembler append(clause)}
               {Peephole Rest Assembler}
            end
         [] shallowGuard(L1 NLiveRegs) then
            case Rest of shallowThen|Rest then
               {Peephole Rest Assembler}
            elseof getNumber(Number R)|branch(L)|lbl(L)|shallowThen|Rest
            then L2 in
               {Assembler newLabel(?L2)}
               {Assembler append(testNumber(R Number L2 L1 NLiveRegs))}
               {Assembler setLabel(L2)}
               {Peephole Rest Assembler}
            elseof getLiteral(Literal R)|branch(L)|lbl(L)|shallowThen|Rest
            then L2 in
               {Assembler newLabel(?L2)}
               {Assembler append(testLiteral(R Literal L2 L1 NLiveRegs))}
               {Assembler setLabel(L2)}
               {Peephole Rest Assembler}
            else
               {Assembler append(I1)}
               {Peephole Rest Assembler}
            end
         [] testBI(Builtinname Args L1 NLiveRegs) then
            case Builtinname of '<' then [X1 X2]#[X3] = Args in
               {Assembler append(testLT(X1 X2 X3 L1 NLiveRegs))}
            [] '=<' then [X1 X2]#[X3] = Args in
               {Assembler append(testLE(X1 X2 X3 L1 NLiveRegs))}
            [] '>='then [X1 X2]#[X3] = Args in
               {Assembler append(testLE(X2 X1 X3 L1 NLiveRegs))}
            [] '>' then [X1 X2]#[X3] = Args in
               {Assembler append(testLT(X2 X1 X3 L1 NLiveRegs))}
            else
               {Assembler append(I1)}
            end
            {Peephole Rest Assembler}
         [] testLiteral(_ _ _ _ _) then
            {Assembler append(I1)}
            {EliminateDeadCode Rest Assembler}
         [] testNumber(_ _ _ _ _) then
            {Assembler append(I1)}
            {EliminateDeadCode Rest Assembler}
         [] testRecord(R Literal Arity L1 L2 NLiveRegs) then
            case Literal == '|' andthen Arity == 2 then
               {Assembler append(testList(R L1 L2 NLiveRegs))}
            else
               {Assembler append(I1)}
            end
            {EliminateDeadCode Rest Assembler}
         [] testList(_ _ _ _) then
            {Assembler append(I1)}
            {EliminateDeadCode Rest Assembler}
         [] testBool(_ _ _ _ _) then
            {Assembler append(I1)}
            {EliminateDeadCode Rest Assembler}
         [] match(R HT NLiveRegs) then ht(ElseL Cases) = HT in
            case Cases of [onScalar(true TrueL) onScalar(false FalseL)] then
               {Assembler append(testBool(R TrueL FalseL ElseL NLiveRegs))}
            elseof [onScalar(false FalseL) onScalar(true TrueL)] then
               {Assembler append(testBool(R TrueL FalseL ElseL NLiveRegs))}
            elseof [onScalar(X L)] then
               case {IsNumber X} then
                  {Assembler append(testNumber(R X L ElseL NLiveRegs))}
               elsecase {IsLiteral X} then
                  {Assembler append(testLiteral(R X L ElseL NLiveRegs))}
               end
            elseof [onRecord(Label RecordArity L)] then
               case Label == '|' andthen RecordArity == 2 then
                  {Assembler
                   append(testList(R L ElseL NLiveRegs))}
               else
                  {Assembler
                   append(testRecord(R Label RecordArity L ElseL NLiveRegs))}
               end
            else
               {Assembler append(I1)}
            end
            {EliminateDeadCode Rest Assembler}
         [] getVariable(R1) then
            case Rest of getVariable(R2)|Rest then
               {Assembler append(getVarVar(R1 R2))}
               {Peephole Rest Assembler}
            else
               {Assembler append(I1)}
               {Peephole Rest Assembler}
            end
         [] getVoid(I) then OutI Rest1 in
            {GetVoids Rest I ?OutI ?Rest1}
            case Rest1 of getVariable(_)|_ then
               {Assembler append(getVoid(OutI))}
            else skip
            end
            {Peephole Rest1 Assembler}
         else
            {Assembler append(I1)}
            {Peephole Rest Assembler}
         end
      [] nil then skip
      end
   end
in
   proc {Assemble Code ProfileSwitch DebugInfoControlSwitch ?Assembler}
      Assembler = {New AssemblerClass
                   init(ProfileSwitch DebugInfoControlSwitch)}
      {Peephole Code Assembler}
   end
end
