functor
   %% ACTIONS
   %%     1. write a package entry either using the makefile or a given package
   %%     2. write contact entries
   %%     3. list mogul info using makefile or given package
   %%     4. list mogul database
   %%     5. write section entries for current database
   %%     6. delete entries in current database
   %% ozmake --mogul-update
   %% ozmake -M update
export
   'class' : Mogul
prepare
   IsPrefix = List.isPrefix
   VS2S = VirtualString.toString
   fun {List2VSX Accu X} Accu#X end
   fun {List2VS L} {FoldL L List2VSX nil} end
   fun {NewQueue}
      C=local L in {NewCell L|L} end
      proc {Put X} L1 L2 in
	 {Exchange C L1|(X|L2) L1|L2}
      end
      proc {ToList L} {Exchange C L|nil unit} end
   in
      queue(put:Put toList:ToList)
   end
import
   Path at 'Path.ozf'
   Utils at 'Utils.ozf'
define
   %% here we keep track of the user's MOGUL offerings and
   %% automate the management of the user's MOGUL section
   class Mogul
      attr
	 DB : unit
	 Dashes : unit
	 EntryFiles : unit
	 RootID : unit

      meth mogul_read()
	 if @DB==unit then
	    F={self get_moguldatabase($)}
	    D={self databaselib_read(F $)}
	 in
	    if D\=unit then
	       DB <- D
	    elseif {self get_moguldatabase_given($)} then
	       raise ozmake(mogul:filenotfound(F)) end
	    else
	       {self trace('no mogul database')}
	       DB <- {NewDictionary}
	    end
	 end
      end

      meth mogul_save()
	 if @DB\=unit then
	    {self databaselib_save(
		     {self get_moguldatabase($)}
		     @DB)}
	 end
      end

      meth mogul_get_rootid($)
	 if @RootID==unit then
	    RootID <- {self get_mogulrootid($)}
	    if @RootID==unit then
	       %% guess it using the mogul id from the first
	       %% entry in the mogul.db if any
	       {self mogul_read}
	       case {Dictionary.keys @DB}
	       of K|_ then
		  case {Path.toNonBaseURL K}.path
		  of H|_ then
		     RootID <- {Path.toAtom 'mogul:/'#H}
		     {self trace('guessed root MOGUL id: '#@RootID)}
		  else skip end
	       else skip end
	    end
	 end
	 @RootID
      end

      meth ToMogulPackageEntry(R VS)
	 Q={NewQueue}
      in
	 {Q.put 'type:           package\n'}
	 {Q.put 'id:             '#R.mogul#'\n'}
	 if {HasFeature R version} then
	    {Q.put 'version:        '#R.version#'\n'}
	 end
	 if {HasFeature R released} then
	    {Q.put 'released:       '#R.released#'\n'}
	 end
	 for A in {CondSelect R author nil} do
	    {Q.put 'author:         '#A#'\n'}
	 end
	 if {HasFeature R blurb} then
	    case {String.tokens {VS2S R.blurb} &\n}
	    of nil then skip
	    [] [nil] then skip
	    [] H|T then
	       {Q.put 'blurb:          '#H}
	       for X in T do {Q.put '\n                '#X} end
	       {Q.put '\n'}
	    end
	 end
	 for C in {CondSelect R categories nil} do
	    {Q.put 'category:       '#C#'\n'}
	 end
	 if {CondSelect R tar nil}\=nil then
	    for EXT in R.tar do
	       {Q.put  'url-pkg:        '#
		{Path.resolve {self get_mogulpkgurl($)}
		 {Utils.mogulToFilename R.mogul}#'.'#EXT}#'\n'}
	    end
	 else
	    {Q.put 'url-pkg:        '#
	     {Path.resolve {self get_mogulpkgurl($)}
	      {Utils.mogulToPackagename R.mogul}}#'\n'}
	 end
	 case {CondSelect R doc unit} of F|_ then
	    {Q.put 'url-doc:        '#
	     {Path.resolve {self get_moguldocurl($)}
	      {Utils.mogulToFilename R.mogul}#'/'#F}#'\n'}
	 else skip end
	 for T in {CondSelect R lib nil} do
	    {Q.put 'provides:       '#
	     {Path.resolve R.uri T}#'\n'}
	 end
	 for T in {CondSelect R bin nil} do
	    {Q.put 'provides:       '#T#'\n'}
	 end
	 if {HasFeature R info_html} then
	    {Q.put 'content-type:   text/html\n\n'}
	    {Q.put R.info_html}
	 elseif {HasFeature R info_text} then
	    {Q.put 'content-type:   text/plain\n\n'}
	    {Q.put R.info_text}
	 end
	 {List2VS {Q.toList} VS}
      end

      meth ToMogulContactEntry(R VS)
	 Q={NewQueue}
      in
	 {Q.put 'type:           contact\n'}
	 {Q.put 'id:             '#R.mogul}
	 {Q.put '\nname:           '#R.name}
	 if {HasFeature R name_for_index} then
	    {Q.put '\nname-for-index: '#R.name_for_index}
	 end
	 if {HasFeature R email} then
	    {Q.put '\nemail:          '#R.email}
	 end
	 if {HasFeature R www} then
	    {Q.put '\nwww:            '#R.www}
	 end
	 {List2VS {Q.toList} VS}
      end

      meth HasPackageContribs(R $)
	 {CondSelect R lib nil}\=nil orelse
	 {CondSelect R bin nil}\=nil orelse
	 {CondSelect R doc nil}\=nil
      end

      meth HasContactContribs(R $)
	 {CondSelect R contact nil}\=nil
      end

      meth mogul_put()
	 %% here we update the MOGUL entry for this package
	 {self makefile_read_maybe_from_package}
	 R  = {self makefile_to_record($ relax:true)}
	 PkgB = {self HasPackageContribs(R $)}
	 ConB = {self HasContactContribs(R $)}
      in
	 if PkgB then {self mogul_put_package(R)} end
	 if ConB then
	    for C in R.contact do {self mogul_put_contact(C)} end
	 end
	 if {Not (PkgB orelse ConB)} then
	    {self trace('package contributes no MOGUL entries')}
	 else
	    {self mogul_save}
	 end
      end

      meth mogul_put_package(R)
	 {self mogul_read}
	 {self mogul_validate_id(R.mogul)}
	 D = {NewDictionary}
      in
	 D.type := package
	 D.mogul := R.mogul
	 case {CondSelect R author nil}
	 of nil  then skip
	 [] unit then skip
	 [] L    then D.author := L end
	 case {CondSelect R blurb nil}
	 of nil  then skip
	 [] unit then skip
	 [] S    then D.blurb := S end
	 case {CondSelect R uri unit}
	 of nil  then skip
	 [] unit then skip
	 [] S    then D.uri := S end
	 D.doc := {CondSelect R doc nil}
	 D.lib := {CondSelect R lib nil}
	 D.bin := {CondSelect R bin nil}
	 case {CondSelect R tar unit}
	 of nil  then skip
	 [] unit then skip
	 [] L    then D.tar := L end
	 case {CondSelect R info_html nil}
	 of nil  then skip
	 [] unit then skip
	 [] S    then D.info_html := S end
	 case {CondSelect R info_text nil}
	 of nil  then skip
	 [] unit then skip
	 [] S    then D.info_text := S end
	 case {CondSelect R released unit}
	 of unit then skip
	 [] S    then D.released := S end
	 case {CondSelect R version unit}
	 of unit then skip
	 [] S    then D.version := S end
	 case {CondSelect R categories unit}
	 of nil  then skip
	 [] unit then skip
	 [] L    then D.categories := L end
	 local P = {Dictionary.toRecord package D} in
	    @DB.(R.mogul) := P
	    {self trace('updated entry for '#R.mogul)}
	    {self mogul_trace_package(P)}
	 end
      end

      meth mogul_put_contact(R)
	 {self mogul_read}
	 {self mogul_validate_id(R.mogul)}
	 D = {NewDictionary}
      in
	 D.type  := contact
	 D.mogul := R.mogul
	 D.name  := R.name
	 if {HasFeature R name_for_index} then
	    D.name_for_index := R.name_for_index
	 end
	 if {HasFeature R email} then
	    D.email := R.email
	 end
	 if {HasFeature R www} then
	    D.www := R.www
	 end
	 local C = {Dictionary.toRecord contact D} in
	    @DB.(R.mogul) := C
	    {self trace('updated entry for '#R.mogul)}
	    {self mogul_trace_contact(C)}
	 end
      end

      meth mogul_export_package(R)
	 VS = {self ToMogulPackageEntry(R $)}
	 F  = {Utils.mogulToFilename R.mogul}#'.mogul'
	 D  = {self get_moguldbdir($)}
	 FF = {Path.resolve D F}
      in
	 {self exec_write_to_file(VS FF)}
	 {self mogul_trace_package(R vs:VS file:F)}
      end

      meth mogul_trace_package(P vs:VS0<=unit file:F0<=unit print:PR<=false)
	 VS = if VS0==unit then {self ToMogulPackageEntry(P $)} else VS0 end
	 F  = if F0==unit then nil else ' [ '#F0#' ] ' end
	 TR = if PR then print else ptrace end
      in
	 {self TR({self format_title(F $)})}
	 {self TR(VS)}
	 {self TR({self format_dashes($)})}
      end

      meth mogul_export_contact(C)
	 D  = {self get_moguldbdir($)}
	 VS = {self ToMogulContactEntry(C $)}
	 F  = {Utils.mogulToFilename C.mogul}#'.mogul'
	 FF = {Path.resolve D F}
      in
	 {self exec_write_to_file(VS FF)}
	 {self mogul_trace_contact(C vs:VS file:F)}
      end

      meth mogul_trace_contact(C vs:VS0<=unit file:F0<=unit print:PR<=false)
	 VS = if VS0==unit then {self ToMogulContactEntry(C $)} else VS0 end
	 F  = if F0==unit then nil else ' [ '#F0#' ] ' end
	 TR = if PR then print else ptrace end
      in
	 {self TR({self format_title(F $)})}
	 {self TR(VS)}
	 {self TR({self format_dashes($)})}
      end

      meth mogul_validate_id(ID)
	 ROOT = {self mogul_get_rootid($)}
      in
	 if ROOT\=unit then
	    R = {Path.toNonBaseURL ROOT}
	    P = {Path.toNonBaseURL ID}
	 in
	    if {Not {IsPrefix R.path P.path}} then
	       raise ozmake(mogul:validate(ROOT ID)) end
	    end
	 end
      end

      meth ToMogulSectionEntry(ID L VS)
	 Q={NewQueue}
      in
	 {Q.put 'type:           section\n'}
	 {Q.put 'id:             '#ID#'\n\n'}
	 for ID2 in L do
	    {Q.put ID2#': '#{Utils.mogulToFilename {Path.resolve ID ID2}}#'.mogul\n'}
	 end
	 {List2VS {Q.toList} VS}
      end

      meth mogul_export_section(ID L)
	 %% ID is the mogul id of the section
	 %% and L is the list of relative ids of its contents
	 VS = {self ToMogulSectionEntry(ID L $)}
	 F  = {Utils.mogulToFilename ID}#'.mogul'
	 D  = {self get_moguldbdir($)}
	 FF = {Path.resolve D F}
      in
	 {self exec_write_to_file(VS FF)}
	 {self ptrace({self format_title(' [ '#F#' ] ' $)})}
	 {self ptrace(VS)}
	 {self ptrace({self format_dashes($)})}
      end

      meth mogul_export
	 {self mogul_read}
	 %% for consistency, first validate all entries
	 {self trace('validating all ids in mogul database')}
	 IDS = {Dictionary.keys @DB}
	 for K in IDS do {self mogul_validate_id(K)} end
	 %% now build the sections.
	 Table={NewDictionary}
	 ROOT={self mogul_get_rootid($)}
	 proc {Enter ID}
	    if ID\=ROOT andthen {Not {Utils.isMogulRootID ID}} then
	       PID = {Path.dirnameAtom ID}
	       KEY = {Path.basenameAtom ID}
	    in
	       {Enter PID}
	       Table.PID := KEY|{CondSelect Table PID nil}
	    end
	 end
	 for K in IDS do {Enter K} end
	 Entries = {Sort {Dictionary.items @DB}
		    fun {$ E1 E2} E1.mogul < E2.mogul end}
      in
	 %% export all MOGUL entries
	 %% by type, and sorted - for user convenience when tracing
	 {self trace('exporting MOGUL section entries')}
	 {self incr}
	 for K in {Sort {Dictionary.keys Table} Value.'<'} do
	    {self mogul_export_section(K Table.K)}
	 end
	 {self decr}
	 {self trace('exporting MOGUL contact entries')}
	 {self incr}
	 for C in Entries do
	    if C.type==contact then
	       {self mogul_export_contact(C)}
	    end
	 end
	 {self decr}
	 {self trace('exporting MOGUL package entries')}
	 {self incr}
	 for P in Entries do
	    if P.type==package then
	       {self mogul_export_package(P)}
	    end
	 end
	 {self decr}
      end

      meth mogul(Targets)
	 case {self get_mogul_action($)}
	 of put      then {self mogul_put}
	 [] delete   then {self mogul_del(Targets)}
	 [] list     then {self mogul_list(Targets)}
	 [] 'export' then {self mogul_export}
	 end
      end

      meth mogul_validate_action(S $)
	 case for A in [put delete list 'export'] collect:C do
		 if {IsPrefix S {AtomToString A}} then {C A} end
	      end
	 of nil then raise ozmake(mogul:unknownaction(S)) end
	 [] [A] then A
	 []  L  then raise ozmake(mogul:ambiguousaction(S L)) end
	 end
      end

      meth format_title(T $)
	 N  = {self get_linewidth($)} - {VirtualString.length T}
	 N1 = N div 2
	 N2 = N - N1
      in
	 for I in 1..N1 collect:COL do {COL &-} end#T#
	 for I in 1..N2 collect:COL do {COL &-} end
      end

      meth format_dashes($)
	 if @Dashes==unit then
	    Dashes <-
	    for I in 1..{self get_linewidth($)} collect:C do {C &-} end
	 end
	 @Dashes
      end

      meth mogul_del(Targets)
	 {self mogul_del_targets(
		  if Targets==nil then
		     {self makefile_read_maybe_from_package}
		     [{self get_mogul($)}]
		  else Targets end)}
      end

      meth mogul_del_targets(Targets) Save in
	 {self mogul_read}
	 for T in Targets do
	    if {HasFeature @DB T} then
	       {self trace('deleting entry : '#T)}
	       Save=unit
	       {Dictionary.remove @DB T}
	    else
	       {self xtrace('entry not found: '#T)}
	    end
	 end
	 if {IsDet Save} then {self mogul_save} end
      end

      meth mogul_list(Targets)
	 {self mogul_read}
	 Keys = if Targets==nil then L={Dictionary.keys @DB} in
		   if L==nil then
		      {self xtrace('your mogul database is empty')}
		      nil
		   else
		      {Sort L Value.'<'}
		   end
		else
		   {Sort
		    for T in Targets collect:COL do
		       if {HasFeature @DB T} then {COL T} else
			  {self xtrace('entry not found: '#T)}
		       end
		    end Value.'<'}
		end
      in
	 for K in Keys do E=@DB.K in
	    case E.type
	    of contact then {self mogul_trace_contact(E print:true)}
	    [] package then {self mogul_trace_package(E print:true)}
	    end
	 end
      end

      meth publish
	 %% ozmake --publish
	 %% is essentially equivalent to the following sequence:
	 %%
	 %% 1. ozmake --install --package=PKG --includedocs --excludelibs --excludebins --docdir=DOCDIR/MOG
	 %%    where DOCDIR is given by --moguldocdir and MOG is the package's MOGUL id
	 %%
	 %% 2. ozmake --create --package PKG
	 %%    where PKG's directory is given by --mogulpkgdir and PKG's
	 %%    filename is computed from the package's MOGUL id
	 %%
	 %% 3. ozmake --mogul=put --package PKG
	 %%
	 %% 4. ozmake --mogul=export
	 %%
	 {self set_database_ignore(true)}
	 {self makefile_read}
	 %% install docs if necessary
	 local L={self get_doc_targets($)} in
	    if L\=nil then
	       {self set_docdir({Path.resolve
				 {self get_moguldocdir($)}
				 {Utils.mogulToFilename {self get_mogul($)}}})}
	       {self install(L)}
	    end
	 end
	 %% create and install package if necessary
	 if {self get_tar_targets($)}\=nil then
	    %% if we count on the tarballs, just check that they exist
	    Tarballs =
	    for EXT in {self get_tar_targets($)} collect:COL do
	       F = {Path.resolve {self get_builddir($)}
		    {Utils.mogulToFilename {self get_mogul($)}}#'.'#EXT}
	    in
	       if {Path.exists F} then {COL F}
	       else raise ozmake(mogul:tarballnotfound(F)) end end
	    end
	    DIR = {self get_mogulpkgdir($)}
	 in
	    %% copy tarballs into mogulpkgdir
	    {self exec_mkdir(DIR)}
	    for F in Tarballs do
	       {self exec_cp(F {Path.resolve DIR {Path.basename F}})}
	    end
	 elseif
	    {self get_bin_targets($)}\=nil orelse
	    {self get_lib_targets($)}\=nil orelse
	    {self get_doc_targets($)}\=nil
	 then
	    {self set_package(
		     {Path.resolve
		      {self get_mogulpkgdir($)}
		      {Utils.mogulToPackagename {self get_mogul($)}}})}
	    {self create}
	 end
	 %% create all mogul entries
	 {self mogul_put}
	 {self mogul_export}
      end
   end
end
