functor prop once
import
   System.valueToVirtualString
   SA
   CodeGen
   ImAConstruction
   ImAValueNode
   ImAVariableOccurrence
   ImAToken
export
   FlattenSequence

   % abstract syntax:
   Statement
   StepPoint
   Declaration
   SkipNode
   Equation
   Construction
   Definition
   FunctionDefinition
   ClauseBody
   Application
   BoolCase
   BoolClause
   PatternCase
   PatternClause
   RecordPattern
   EquationPattern
   AbstractElse
   ElseNode
   NoElse
   ThreadNode
   TryNode
   LockNode
   ClassNode
   Method
   MethodWithDesignator
   MethFormal
   MethFormalOptional
   MethFormalWithDefault
   ObjectLockNode
   GetSelf
   FailNode
   IfNode
   ChoicesAndDisjunctions
   OrNode
   DisNode
   ChoiceNode
   Clause
   ValueNode
   AtomNode
   IntNode
   FloatNode
   Variable
   RestrictedVariable
   VariableOccurrence
   PatternVariableOccurrence

   % token representations:
   Token
   NameToken
   BuiltinToken
   ProcedureToken
   CellToken
   ChunkToken
   ArrayToken
   DictionaryToken
   ClassToken
   ObjectToken
   LockToken
   PortToken
   ThreadToken
   SpaceToken
   BitArrayToken

   % token instances:
   TrueToken
   FalseToken
body
   local
      \insert Annotate
   in
      \insert CoreLanguage
   end
end
