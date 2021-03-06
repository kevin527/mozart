###
### Authors:
###   Denys Duchier <duchier@ps.uni-sb.de>
###   Christian Schulte <schulte@ps.uni-sb.de>
###
### Copyright:
###   Denys Duchier, 1998
###   Christian Schulte, 1998
###
### Last change:
###   $Date$ by $Author$
###   $Revision$
###
### This file is part of Mozart, an implementation 
### of Oz 3:
###    http://www.mozart-oz.org
###
### See the file "LICENSE" or
###    http://www.mozart-oz.org/LICENSE.html
### for information on usage and redistribution 
### of this file, and for a DISCLAIMER OF ALL 
### WARRANTIES.
###

%builtins_all =
(
    'save'		=> { in     => ['value','+virtualString'],
			     out    => [],
			     BI     => BIsave},

    'saveCompressed'	=> { in     => ['value','+virtualString',
			                '+int'],
			     out    => [],
			     BI     => BIsaveCompressed},

    'saveWithHeader'	=> { in     => ['value','+virtualString',
			                '+virtualString','+int'],
			     out    => [],
			     BI     => BIsaveWithHeader},

    'saveWithCells'	=> { in     => ['value','+virtualString',
			                '+virtualString','+int'],
			     out    => [],
			     BI     => BIsaveWithCells},

    'load'		=> { in     => ['value','value'],
			     out    => [],
			     BI     => BIload},

    'loadWithHeader'	=> { in     => ['+virtualString'],
			     out    => ['value'],
			     BI     => BIloadWithHeader},

    'pack'	        => { in     => ['+value'],
                             out    => ['+byteString'],
                             BI     => BIpicklePack},

    'packWithCells'	=> { in     => ['+value'],
			     out    => ['+byteString'],
			     BI     => BIpicklePackWithCells},

    'unpack'	        => { in     => ['+virtualString','value'],
                             out    => [],
                             BI     => BIpickleUnpack},

 );
