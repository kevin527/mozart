%%% oz-mode is stupid, so we use -*-text-mode-*-
%%% $Id$
%%% Benjamin Lorenz <lorenz@ps.uni-sb.de>

local

   MessageWidth = 380
   HelpTitle    = 'Profiler Help'
   OkButtonText = 'Aha'
   NoTopic      = 'No Help Available'
   NoHelp       = 'Feel free to ask the author.\n' #
                  'Send a mail to ' # EmailOfBenni

   HelpDict     = {Dictionary.new}

   {ForAll
    [
      nil # ('The Profiler Help System' #
        ('For most of the widgets in the Profiler GUI you can ' #
        'get some help.\nJust click ' #
        'with the right mouse button on the widget.'))

      StatusHelp # ('The Status Line' #
        ('Hey, this is just a boring status line!'))

      UpdateButtonText # ('Update' #
        ('Request new information from the Emulator.'))

      ResetButtonText # ('Reset' #
        ('Clear all accumulated information.'))

      SortButtonText # ('Sort Menu' #
        ('Here you can choose how to sort profile information:\n\n' #
        ' calls: How often a procedure has been called\n' #
        ' closures: How many closures a procedure has created\n' #
        ' samples: User time a procedure has spent\n' #
        ' heap: How much memory a procedure has used'))

      BarCanvasTitle # ('Procedure Bar Chart' #
        ('Procedures are presented as annotated bars. You can click ' #
        'on them to get further information in the `' # BarTextTitle #
        '\' window.'))

      BarTextTitle # ('Procedure Information' #
        ('Detailed information about the currently selected procedure ' #
        'is given here.'))

      GenTextTitle # ('Summary Information' #
        ('A summary of interesting profiling values is printed here.'))

    ]
    proc {$ S}
       {Dput HelpDict S.1 S.2}
    end}

   class HelpDialog from TkTools.dialog
      feat
         topic help
      meth init(master:Master)
         TkTools.dialog,tkInit(master:  Master
                               root:    pointer
                               title:   HelpTitle % self.topic
                               buttons: [OkButtonText # tkClose]
                               focus:   1
                               bg:      DefaultBackground
                               pack:    false
                               default: 1)
         Title = {New Tk.label tkInit(parent: self
                                      bg:     DefaultBackground
                                      text:   self.topic
                                      font:   HelpTitleFont)}
         Help = {New Tk.message
                 tkInit(parent: self
                        font:   HelpFont
                        bg:     DefaultBackground
                        width:  MessageWidth
                        text:   self.help)}
      in
         {Tk.send pack(Title Help side:top expand:1 pady:3 fill:x)}
         HelpDialog,tkPack
      end
   end

   class ProfilerHelp from HelpDialog
      meth init(master:Master topic:Topic)
         self.topic # self.help = {DcondGet HelpDict Topic NoTopic#NoHelp}
         HelpDialog,init(master:Master)
      end
   end

in

   class Help

      meth init
         skip
      end

      meth help(Topic)
         {Wait {New ProfilerHelp init(master: self.toplevel
                                      topic:  Topic)}.tkClosed}
      end

   end
end
