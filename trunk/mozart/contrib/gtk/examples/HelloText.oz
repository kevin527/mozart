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
   Application
   System(show)
   GDK at 'x-oz://system/gtk/GDK.ozf'
   GTK at 'x-oz://system/gtk/GTK.ozf'
define
   %% Create Toplevel window class
   class MyToplevel from GTK.window
      meth new
	 GTK.window, new(GTK.'WINDOW_TOPLEVEL')
	 GTK.window, signalConnect('delete-event' deleteEvent _)
	 GTK.window, setBorderWidth(10)
	 GTK.window, setTitle("Hello GTK")
      end
      meth deleteEvent(Args)
	 %% Caution: At this time, the underlying GTK object
	 %% Caution: has been destroyed already
	 %% Caution: Destruction also includes all attached child objects.
	 %% Caution: This event is solely intended to do OZ side cleanups.
	 {Application.exit 0}
      end
   end

   %% Set up the Colors
   %% 1. Obtain the system colormap
   %% 2. Allocate the color structure with R, G, B preset
   %% 3. Try to alloc appropriate system colors,
   %%    non-writeable and with best-match
   %% 4. Use color black
   Colormap = {New GDK.colormap getSystem}
   Black    = {New GDK.color new(0 0 0)}
   White    = {New GDK.color new(65535 65535 65535)}
   {Colormap allocColor(Black 0 1 _)}
   {Colormap allocColor(White 0 1 _)}

   %% Create TextWidget class
   class MyText from GTK.text
      meth new
	 GTK.text, new(unit unit)
      end
   end

   Toplevel = {New MyToplevel new}
   Text     = {New MyText new}

   %% Make TextWidget child of Toplevel
   {Toplevel add(Text)}
   {Text insert(unit Black White "Hallo, Leute!" ~1)}
   %% Make it all visible
   {Toplevel showAll}
end
