%%% $Id$
%%% Benjamin Lorenz <lorenz@ps.uni-sb.de>

local

   fun {B2F I B}
      frame(nr      : I
            file    : ''
            line    : 0
            name    : B.name
            args    : B.args
            env     : undef
            builtin : true)
   end

   fun {P2F I P D}
      frame(nr      : I
            file    : P.file
            line    : P.line
            name    : P.name
            args    : D.1.2
            env     : P.vars
            builtin : false)
   end

   local
      fun {Correct F}
         case F == nil then nil else
            case {Label F.1} == debug then
               {Correct F.2}
            else
               F
            end
         end
      end
      proc {DoStackForAllInd Xs I P}
         case Xs of nil then skip
         [] X|Y|Z then
            case X == toplevel then skip else
               {P I X Y}
               {DoStackForAllInd {Correct Z} I+1 P}
            end
         end
      end
   in
      proc {StackForAllInd Xs P}
         case {Label Xs.1} == builtin then
            {P 1 Xs.1 nil}
            {DoStackForAllInd Xs.2 2 P}
         else
            {DoStackForAllInd Xs 1 P}
         end
      end
   end

in

   class StackManager

      prop
         locking

      feat
         T              % the thread...
         I              % ...with it's ID
         W              % text widget for output
         D              % dictionary for stackframes

      attr
         Size           % current size of stack

      meth init(thr:Thr id:ID output:TextWidget)
         self.T = Thr
         self.I = ID
         self.W = TextWidget
         self.D = {Dictionary.new}
         Size <- 0
      end

      meth GetStack($)
         {Dbg.taskstack self.T MaxStackSize}
      end

      meth Reset
         CurrentStack = StackManager,GetStack($)
         OldKeys      = {Dkeys self.D}
      in
         lock
            {ForAll OldKeys proc {$ K} {Dremove self.D K} end}
            case CurrentStack \= nil then
               {StackForAllInd CurrentStack
                proc {$ Ind Proc Debug}
                   case Debug == nil then  % builtin
                      {Dput self.D 0         {B2F Ind Proc}}
                   else                    % procedure
                      {Dput self.D Debug.1.1 {P2F Ind Proc Debug}}
                   end
                end}
               Size <- {Length {Dkeys self.D}}
            else
               Size <- 0
            end
         end
         %{Browse {Dentries self.D}}
      end

      %% only print changed frames of stack
      meth update
         CurrentStack = StackManager,GetStack($)
      in
         skip
      end

      %% completely re-print the stack
      meth print
         skip /*
         StackManager,Reset
         local
            S = @Size
            Ack
         in
            case S > 0 then
               thread
                  {Ozcar printStack(id:self.I size:S stack:self.D ack:Ack)}
               end
               %{Ozcar stackStatus(S Ack)}
            else
               StackManager,clear
            end
         end */
      end

      meth Clear
         {ForAll [tk(conf state:normal)
                  tk(delete '0.0' 'end')] self.W}
      end

      meth Enable
         {self.W tk(conf state:normal)}
      end

      meth Disable
         {self.W tk(conf state:disabled)}
      end

      meth clear
         %% clear stack widget
         StackManager,Clear
         StackManager,Disable
         {self.W title(StackTitle)}
         %% clear env widgets
         {Ozcar printEnv(frame:0)}
      end

   end
end
