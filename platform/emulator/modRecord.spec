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

# -*-perl-*-

%builtins_all =
    (
     'is'	  => { in  => ['+value'],
		       out => ['+bool'],
		       bi  => BIisRecord},

     'make'	  => { in  => ['+literal','+[feature]'],
		       out => ['+record'],
		       bi  => BIrealMakeRecord},

     'clone'	  => { in  => ['+record'],
		       out => ['+record'],
		       bi  => BIcloneRecord},

     'isC'	  => { in  => ['+value'],
		       out => ['+bool'],
		       bi  => BIisRecordCB},

     'adjoin'	  => { in  => ['+record','+record'],
		       out => ['+record'],
		       bi  => BIadjoin},

     'adjoinList' => { in  => ['+record','+[feature#value]'],
		       out => ['+record'],
		       BI  => BIadjoinList},

     'arity'	  => { in  => ['+record'],
		       out => ['+[feature]'],
		       bi  => BIarity},

     'adjoinAt'	  => { in  => ['+record','+feature','value'],
		       out => ['+record'],
		       BI  => BIadjoinAt},

     'label'	  => { in  => ['*recordC'],
		       out => ['+literal'],
		       bi  => BIlabel},

     'hasLabel'	  => { in  => ['value'],
		       out => ['+bool'],
		       bi  => BIhasLabel},

     'tellRecord' => { in  => ['+literal','record'],
		       out => [],
		       BI  => BIrecordTell},

     'widthC'	  => { in  => ['*record','int'],
		       out => [],
		       BI  => BIwidthC},

     'monitorArity' => { in  => ['*recordC','value','[feature]'],
			out => [],
			BI  => BImonitorArity},

     'tellRecordSize'=> { in  => ['+literal','+int','record'],
			  out => [],
			  BI  => BIsystemTellSize},

     '^'	  => { in  => ['*recordCOrChunk','+feature'],
		       out => ['value'],
		       bi  => BIuparrowBlocking},

     'width'	  => { in  => ['+record'],
		       out => ['+int'],
		       bi  => BIwidth},

     'waitOr'	  => { in  => ['+record'],
		       out => ['value'],
		       BI  => BIwaitOrF},

     'test'	  => { in  => ['value','+literal','+[feature]'],
		       out => ['+bool'],
		       bi  => BItestRecord},

     'testLabel'  => { in  => ['value','+literal'],
		       out => ['+bool'],
		       bi  => BItestRecordLabel},

     'testFeature'=> { in  => ['value','+feature'],
		       out => ['+bool','value'],
		       bi  => BItestRecordFeature},

     'aritySublist'=> { in => ['+record','+record'],
			out=> ['+bool'],
			bi => BIaritySublist},

     'toDictionary'=> { in => ['+record'],
                        out=> ['+dictionary'],
                        bi => BIrecordToDictionary},

     );
1;;
