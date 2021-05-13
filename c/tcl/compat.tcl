if {[::hexchat::pluginpref_get -int hexchat_tcl_compat] <> 0} {
	namespace eval ::hexchat::compat {
		namespace export alias away channel channels chats command complete dcclist findcontext getcontext getinfo getlist ignores killtimer me network nickcmp off on
		namespace import ::hexchat::getcontext ::hexchat::nickcmp
	}
	
	set ::hexchat::compat::aliases [dict create]
	set ::hexchat::compat::nextprocid 0x1000
	set ::hexchat::compat::complete [dict create]
	set ::hexchat::compat::timers [dict create]
	set ::hexchat::compat::nexttimerid 0
	set ::hexchat::compat::hooks [dict create]
	
	set ::hexchat::compat::printevents {
		CHAT {name {DCC Chat Text} argc -1}
		XC_APPFOCUS {name {Focus Window} argc -3}
		XC_TABOPEN {name {Open Context} argc -2}
		XC_TABCLOSE {name {Close Context} argc -2}
		XC_TABFOCUS {name {Focus Tab} argc -2}
		XC_KEYPRESS {name {Key Press} argc 4}
		XC_ADDNOTIFY {name {Add Notify} argc 1}
		XC_BANLIST {name {Ban List} argc 4}
		XC_BANNED {name Banned argc 1}
		XC_BEEP {name Beep argc 0}
		XC_CHANGENICK {name {Change Nick} argc 2}
		XC_CHANACTION {name {Channel Action} argc 3}
		XC_HCHANACTION {name {Channel Action Hilight} argc 3}
		XC_CHANBAN {name {Channel Ban} argc 2}
		XC_CHANDATE {name {Channel Creation} argc 2}
		XC_CHANDEHOP {name {Channel DeHalfOp} argc 2}
		XC_CHANDEOP {name {Channel DeOp} argc 2}
		XC_CHANDEVOICE {name {Channel DeVoice} argc 2}
		XC_CHANEXEMPT {name {Channel Exempt} argc 2}
		XC_CHANHOP {name {Channel Half-Operator} argc 2}
		XC_CHANINVITE {name {Channel INVITE} argc 2}
		XC_CHANLISTHEAD {name {Channel List} argc 0}
		XC_CHANMSG {name {Channel Message} argc 4}
		XC_CHANMODEGEN {name {Channel Mode Generic} argc 4}
		XC_CHANMODES {name {Channel Modes} argc 2}
		XC_HCHANMSG {name {Channel Msg Hilight} argc 4}
		XC_CHANNOTICE {name {Channel Notice} argc 3}
		XC_CHANOP {name {Channel Operator} argc 2}
		XC_CHANRMEXEMPT {name {Channel Remove Exempt} argc 2}
		XC_CHANRMINVITE {name {Channel Remove Invite} argc 2}
		XC_CHANRMKEY {name {Channel Remove Keyword} argc 1}
		XC_CHANRMLIMIT {name {Channel Remove Limit} argc 1}
		XC_CHANSETKEY {name {Channel Set Key} argc 2}
		XC_CHANSETLIMIT {name {Channel Set Limit} argc 2}
		XC_CHANUNBAN {name {Channel UnBan} argc 2}
		XC_CHANVOICE {name {Channel Voice} argc 2}
		XC_CONNECTED {name Connected argc 0}
		XC_CONNECT {name Connecting argc 3}
		XC_CONNFAIL {name {Connection Failed} argc 1}
		XC_CTCPGEN {name {CTCP Generic} argc 2}
		XC_CTCPGENC {name {CTCP Generic to Channel} argc 3}
		XC_CTCPSEND {name {CTCP Send} argc 2}
		XC_CTCPSND {name {CTCP Sound} argc 2}
		XC_CTCPSNDC {name {CTCP Sound to Channel} argc 3}
		XC_DCCCHATABORT {name {DCC CHAT Abort} argc 1}
		XC_DCCCONCHAT {name {DCC CHAT Connect} argc 2}
		XC_DCCCHATF {name {DCC CHAT Failed} argc 4}
		XC_DCCCHATOFFER {name {DCC CHAT Offer} argc 1}
		XC_DCCCHATOFFERING {name {DCC CHAT Offering} argc 1}
		XC_DCCCHATREOFFER {name {DCC CHAT Reoffer} argc 1}
		XC_DCCCONFAIL {name {DCC Conection Failed} argc 3}
		XC_DCCGENERICOFFER {name {DCC Generic Offer} argc 2}
		XC_DCCHEAD {name {DCC Header} argc 0}
		XC_MALFORMED {name {DCC Malformed} argc 2}
		XC_DCCOFFER {name {DCC Offer} argc 3}
		XC_DCCIVAL {name {DCC Offer Not Valid} argc 0}
		XC_DCCRECVABORT {name {DCC RECV Abort} argc 2}
		XC_DCCRECVCOMP {name {DCC RECV Complete} argc 4}
		XC_DCCCONRECV {name {DCC RECV Connect} argc 3}
		XC_DCCRECVERR {name {DCC RECV Failed} argc 4}
		XC_DCCFILEERR {name {DCC RECV File Open Error} argc 2}
		XC_DCCRENAME {name {DCC Rename} argc 2}
		XC_DCCRESUMEREQUEST {name {DCC RESUME Request} argc 3}
		XC_DCCSENDABORT {name {DCC SEND Abort} argc 2}
		XC_DCCSENDCOMP {name {DCC SEND Complete} argc 3}
		XC_DCCCONSEND {name {DCC SEND Connect} argc 3}
		XC_DCCSENDFAIL {name {DCC SEND Failed} argc 3}
		XC_DCCSENDOFFER {name {DCC SEND Offer} argc 4}
		XC_DCCSTALL {name {DCC Stall} argc 3}
		XC_DCCTOUT {name {DCC Timeout} argc 3}
		XC_DELNOTIFY {name {Delete Notify} argc 1}
		XC_DISCON {name Disconnected argc 1}
		XC_FOUNDIP {name {Found IP} argc 1}
		XC_GENMSG {name {Generic Message} argc 2}
		XC_IGNOREADD {name {Ignore Add} argc 1}
		XC_IGNORECHANGE {name {Ignore Changed} argc 1}
		XC_IGNOREFOOTER {name {Ignore Footer} argc 0}
		XC_IGNOREHEADER {name {Ignore Header} argc 0}
		XC_IGNOREREMOVE {name {Ignore Remove} argc 1}
		XC_IGNOREEMPTY {name {Ignorelist Empty} argc 0}
		XC_INVITE {name Invite argc 1}
		XC_INVITED {name Invited argc 3}
		XC_JOIN {name Join argc 3}
		XC_KEYWORD {name Keyword argc 1}
		XC_KICK {name Kick argc 4}
		XC_KILL {name Killed argc 2}
		XC_MSGSEND {name {Message Send} argc 2}
		XC_MOTD {name Motd argc 1}
		XC_MOTDSKIP {name {MOTD Skipped} argc 0}
		XC_NICKCLASH {name {Nick Clash} argc 2}
		XC_NICKFAIL {name {Nick Failed} argc 0}
		XC_NODCC {name {No DCC} argc 0}
		XC_NOCHILD {name {No Running Process} argc 0}
		XC_NOTICE {name Notice argc 2}
		XC_NOTICESEND {name {Notice Send} argc 2}
		XC_NOTIFYEMPTY {name {Notify Empty} argc 0}
		XC_NOTIFYHEAD {name {Notify Header} argc 0}
		XC_NOTIFYNUMBER {name {Notify Number} argc 1}
		XC_NOTIFYOFFLINE {name {Notify Offline} argc 3}
		XC_NOTIFYONLINE {name {Notify Online} argc 3}
		XC_OPENDIALOG {name {Open Dialog} argc 0}
		XC_PART {name Part argc 3}
		XC_PARTREASON {name {Part with Reason} argc 4}
		XC_PINGREP {name {Ping Reply} argc 2}
		XC_PINGTIMEOUT {name {Ping Timeout} argc 1}
		XC_PRIVMSG {name {Private Message} argc 3}
		XC_DPRIVMSG {name {Private Message to Dialog} argc 3}
		XC_ALREADYPROCESS {name {Process Already Running} argc 0}
		XC_QUIT {name Quit argc 3}
		XC_RAWMODES {name {Raw Modes} argc 2}
		XC_WALLOPS {name {Receive Wallops} argc 2}
		XC_RESOLVINGUSER {name {Resolving User} argc 2}
		XC_SERVERCONNECTED {name {Server Connected} argc 0}
		XC_SERVERERROR {name {Server Error} argc 1}
		XC_SERVERLOOKUP {name {Server Lookup} argc 1}
		XC_SERVNOTICE {name {Server Notice} argc 2}
		XC_SERVTEXT {name {Server Text} argc 2}
		XC_STOPCONNECT {name {Stop Connection} argc 1}
		XC_TOPIC {name Topic argc 2}
		XC_NEWTOPIC {name {Topic Change} argc 3}
		XC_TOPICDATE {name {Topic Creation} argc 3}
		XC_UKNHOST {name {Unknown Host} argc 0}
		XC_USERLIMIT {name {User Limit} argc 1}
		XC_USERSONCHAN {name {Users On Channel} argc 2}
		XC_WHOIS_AUTH {name {WhoIs Authenticated} argc 3}
		XC_WHOIS5 {name {WhoIs Away Line} argc 2}
		XC_WHOIS2 {name {WhoIs Channel/Oper Line} argc 2}
		XC_WHOIS6 {name {WhoIs End} argc 1}
		XC_WHOIS_ID {name {WhoIs Identified} argc 2}
		XC_WHOIS4 {name {WhoIs Idle Line} argc 2}
		XC_WHOIS4T {name {WhoIs Idle Line with Signon} argc 3}
		XC_WHOIS1 {name {WhoIs Name Line} argc 4}
		XC_WHOIS_REALHOST {name {WhoIs Real Host} argc 4}
		XC_WHOIS3 {name {WhoIs Server Line} argc 2}
		XC_WHOIS_SPECIAL {name {WhoIs Special} argc 3}
		XC_UJOIN {name {You Join} argc 3}
		XC_UKICK {name {You Kicked} argc 4}
		XC_UPART {name {You Part} argc 3}
		XC_UPARTREASON {name {You Part with Reason} argc 4}
		XC_UACTION {name {Your Action} argc 3}
		XC_UINVITE {name {Your Invitation} argc 3}
		XC_UCHANMSG {name {Your Message} argc 4}
		XC_UCHANGENICK {name {Your Nick Changing} argc 2}
	}
	
	set ::hexchat::compat::raw_line_hook [::hexchat::hook_server {RAW LINE} [list ::hexchat::compat::ServerRawHook]]
	set ::hexchat::compat::null_alias_hook [::hexchat::hook_command "" [list ::hexchat::compat::NullCommandHook]]
	
	proc ::hexchat::compat::nextprocid {} {
		variable nextprocid
		return [format "::__xctcl_%08x" [incr nextprocid]]
	}
	
	proc ::hexchat::compat::smartctx {args} {
		if {[llength $args] == 0} {
			return [::hexchat::getcontext]
		}
		
		if {[llength $args] == 2} {
			try {
				return [::hexchat::findcontext [lindex $args 0] [lindex $args 1]]
			} on error - {
				return [::hexchat::findcontext [lindex $args 1] [lindex $args 0]]
			}
			
		}
		
		set arg [lindex $args 0]
		
		set currsid [::hexchat::prefs id]
		set cfields [::hexchat::list_fields channels]
		set chans [::hexchat::getlist channels]
		
		set sididx [lsearch $cfields id]
		set ctxidx [lsearch $cfields context]
		set typeidx [lsearch $cfields type]
		set serveridx [lsearch $cfields server]
		set networkidx [lsearch $cfields network]
		set channelidx [lsearch $cfields channel]
		
		foreach c $channels {
			set ctx [lindex $c $ctxidx]
			if {$ctx eq $arg} {
				return $ctx
			}
			
			set type [lindex $c $typeidx]
			if {$type == 1} {
				set server [lindex $c $serveridx]
				if {[string equals -nocase $arg $server]} {
					return $ctx
				}
				set network [lindex $c $networkidx]
				if {[string equal -nocase $arg $network]} {
					return $ctx
				}
			} else {
				set sid [lindex $c $sididx]
				set channel [lindex $c $channelidx]
				if {$sid == $currsid && [::hexchat::nickcmp $channel $arg] == 0} {
					return $ctx
				}
			}
		}
		return {}
	}
	
	proc ::hexchat::compat::alias {args} {
		switch -- [llength $args] {
			2 {
				lassign $args name script
				set help {}
			}
			3 {
				lassign $args name help script
			}
			default {
				return -code error "wrong # args: should be \"[lindex [info level 0] 0] name ?help? script\""
			}
		}
		variable aliases
		set name [string toupper $name]
		if {[string length $script] > 0} {
			# Add alias
			set pid [nextprocid]
			
			if {[dict exists $aliases $name]} {
				rename [dict get $aliases $name proc] {}
			} else {
				if {[string index $name 0] ne {@}} {
					if {$help ne {}} {
						set help [list -help $help]
					}
					dict set aliases $name hook [::hexchat::hook_command {*}$help $name ::hexchat::compat::AliasHook]
				}
			}
			dict set aliases $name proc $pid
			proc $pid {_cmd _rest} $source
			
		} else {
			# Remove alias
			if {[dict exists $aliases $name]} {
				rename [dict get $aliases $name proc] {}
				if {[dict exists $aliases $name hook]} {
					::hexchat::unregister_hook [dict get $aliases $name hook]
				}
				dict unset aliases $name
			}
		}
	}
	
	proc ::hexchat::compat::AliasHook {word word_eol} {
		variable complete
		variable aliases
		set oldcomplete $complete
		set complete [dict create word $word word_eol $word_eol defresult $::hexchat::EAT_ALL result $::hexchat::EAT_NONE]
		
		set name [string toupper [lindex $word 1]]
		if {[dict exists $aliases $name]} {
			try {
				[dict get $aliases $name proc] $name [lindex $word_eol 2]
			} on error {res opt} {
				::hexchat::print "${::hexchat::msgprefix}ERROR (alias $name) [dict get $opt -errorinfo]"
			}
		}
		set newcomplete $complete
		set complete $oldcomplete
		return [dict get $newcomplete result]
	}
	
	proc ::hexchat::compat::Info {what args} {
		if {[llength $args] ni {0 1 2}} {
			return -code error "wrong # args: should be \"[lindex [info level -1] 0] ?server|context? ?channel?\""
		}
		set oldctx [::hexchat::getcontext]
		::hexchat::setcontext [smartctx {*}$args]
		try {
			return [::hexchat::getinfo $what]
		} finally {
			hexchat::setcontext $oldctx
		}
	}
	
	proc ::hexchat::compat::InfoAlias {name what} {
		interp alias {} ::hexchat::compat::$name {} [list ::hexchat::compat::Info $what]
	}
	
	::hexchat::compat::InfoAlias away away
	::hexchat::compat::InfoAlias channel channel
	
	proc ::hexchat::compat::channels {args} {
		if {[llength $args] ni {0 1}} {
			return -code error "wrong # args: should be \"[lindex [info level 0] 0] ?server|network|context?\""
		}
		set oldctx [::hexchat::getcontext]
		::hexchat::setcontext [smartctx {*}$args]
		
		set cfields [::hexchat::list_fields channels]
		set currsid [::hexchat::prefs id]
		set chans [::hexchat::getlist channels]
		
		set typeidx [lsearch $cfields type]
		set channelidx [lsearch $cfields channel]
		set sididx [lsearch $cfields id]
		
		set result [list]
		
		foreach c $chans {
			if {[lindex $c $sididx] == $currsid && [lindex $c $typeidx] == 2} {
				lappend result [lindex $c $channelidx]
			}
		}
		::hexchat::setcontext $oldctx
		return $result
	}
	
	proc ::hexchat::compat::chats {} {
		set dfields [::hexchat::list_fields dcc]
		set dccs [::hexchat::getlist dcc]
		
		set typeidx [lsearch $dfields type]
		set statusidx [lsearch $dfields status]
		set nickidx [lsearch $dfields nick]
		
		set result [list]
		foreach d $dccs {
			if {[lindex $d $typeidx] in {2 3} && [lindex $d $statusidx] == 1} {
				lappend result [lindex $d $nickidx]
			}
		}
		return $result
	}
	
	proc ::hexchat::compat::command {args} {
		if {[llength $args] ni {1 2 3}} {
			return -code error "wrong # args: should be \"[lindex [info level 0] 0] ?server|network|context? ?#channel|nick? text\""
		}
		set text [lindex $args end]
		set oldctx [::hexchat::getcontext]
		::hexchat::setcontext [smartctx {*}[lrange $args 0 end-1]]
		::hexchat::command $text
		::hexchat::setcontext $oldctx
	}
	
	proc ::hexchat::compat::complete {how {}} {
		variable complete
		if {[string is integer -strict $how]} {
			dict set complete result $how
		} else {
			switch -nocase -- $how {
				{} {dict set complete result [dict get $complete defresult]}
				EAT_NONE {dict set complete result $::hexchat::EAT_NONE}
				EAT_HEXCHAT {dict set complete result $::hexchat::EAT_HEXCHAT}
				EAT_PLUGIN {dict set complete result $::hexchat::EAT_PLUGIN}
				EAT_ALL {dict set complete result $::hexchat::EAT_ALL}
				default {return -code error "wrong # args: should be \"[lindex [info level 0] 0] ?EAT_NONE|EAT_XCHAT|EAT_PLUGIN|EAT_ALL?\""}
			}
		}
		return -code return
	}
	
	proc ::hexchat::compat::dcclist {} {
		set dccs [::hexchat::getlist dcc]
		set dfields [::hexchat::list_fields dcc]
		
		set typeidx [lsearch $dfields type]
		set statusidx [lsearch $dfields status]
		set nickidx [lsearch $dfields nick]
		set fileidx [lsearch $dfields file]
		set destfileidx [lsearch $dfields destfile]
		set sizeidx [lsearch $dfields size]
		set resumeidx [lsearch $dfields resume]
		set posidx [lsearch $dfields pos]
		set cpsidx [lsearch $dfields cps]
		set address32idx [lsearch $dfields address32]
		set portidx [lsearch $dfields port]
		
		set result [list]
		foreach d $dccs {
			set item [list]
			set type [lindex $d $typeidx]
			lappend item [lindex {filesend filerecv chatsend chatrecv} $type]
			lappend item [lindex {queued active failed done connecting aborted} [lindex $d $statusidx]]
			lappend item [lindex $d $nickidx]
			switch -- $type {
				0 {lappend item [lindex $d $fileidx]}
				1 {lappend item [lindex $d $destfileidx]}
			}
			lappend item [lindex $d $sizeidx]
			lappend item [lindex $d $resumeidx]
			lappend item [lindex $d $posidx]
			lappend item [lindex $d $cpsidx]
			lappend item [lindex $d $address32idx]
			lappend item [lindex $d $portidx]
			
			lappend result $item
		}
		return $result
	}
	
	proc ::hexchat::compat::findcontext {args} {
		switch -- [llength $args] {
			0 {return [::hexchat::findcontext]}
			1 -
			2 {return [smartctx {*}$args]}
			default {return -code error "wrong # args: should be \"[lindex [info level 0] 0] ?server|network|context? ?channel?"}
		}
	}
	
	proc ::hexchat::compat::getinfo {args} {
		switch -- [llength $args] {
			1 {return [::hexchat::getinfo [lindex $args 0]]}
			2 -
			3 {
				set oldctx [::hexchat::getcontext]
				::hexchat::setcontext [smartctx {*}[lrange $args 0 end-1]]
				try {
					return [::hexchat::getinfo [lindex $args end]]
				} finally {
					::hexchat::setcontext $oldctx
				}
			}
		}
	}
	
	proc ::hexchat::compat::getlist {{list {}}} {
		if {$list eq {}} {
			return [::hexchat::getlist]
		} else {
			return [linsert [::hexchat::getlist $list] 0 [::hexchat::list_fields $list]]
		}
	}
	
	::hexchat::compat::InfoAlias host host
	
	proc ::hexchat::compat::ignores {} {
		set ignores [::hexchat::getlist ignore]
		set ifields [::hexchat::list_fields ignore]
		
		set maskidx [lsearch $ifields mask]
		set flagsidx [lsearch $ifields flags]
		
		set flagenum {PRIVMSG NOTICE CHANNEL CTCP INVITE UNIGNORE NOSAVE}
		
		set result [list]
		foreach i $ignores {
			set item [list]
			lappend item [lindex $i $maskidx]
			set flags [lindex $i $flagsidx]
			set flaglist [list]
			for {set j 0} {$j < [llength $flagenum]} {incr j} {
				if {($flags & (1 << $j)) != 0} {
					lappend flaglist [lindex $flagenum $j]
				}
			}
			lappend item $flaglist
			
			lappend result $item
		}
		return $result
	}
	
	proc ::hexchat::compat::killtimer {timerid} {
		variable timers
		if {[info exists $timers $timerid]} {
			::hexchat::unregister_hook [dict get $timers $timerid hook]
			dict unset timers $timerid
		} else {
			return -code error "Invalid timer id"
		}
	}
	
	::hexchat::compat::InfoAlias me nick
	::hexchat::compat::InfoAlias network network
	
	proc ::hexchat::compat::off {args} {
		switch -- [llength $args] {
			1 {lassign $args token}
			2 {lassign $args token label}
			default {return -code error "wrong # args: should be \"[lindex [info level 0] 0] token ?label?"}
		}
		set token [string toupper $token]
		variable hooks
		if {[dict exists $hooks $token]} {
			switch -- [llength $args] {
				2 {
					dict for {- proc} [dict get $hooks $token procs] {
						rename $proc {}
					}
					dict set hooks $token procs {}
				}
				3 {
					if {[dict exists $hooks $token procs $label]} {
						rename [dict get $hooks $token procs $label] {}
						dict unset hooks $token procs $label
					}
				}
			}
			if {[dict size $hooks $token procs] > 0} {
				if {[dict exists $hooks $token hook]} {
					::hexchat::unregister_hook [dict get $hooks $token hook]
				}
				dict unset $hooks $token
			}
		}
	}
	
	proc ::hexchat::compat::on {token label script} {
		set procid [nextprocid]
		proc $procid {_src _dest _cmd _rest _raw _label _private} $script
		
		set token [string toupper $token]
		
		variable hooks
		
		if {[dict exists $hooks $token procs $label]} {
			rename [dict get $hooks $token procs $label] {}
		}
		dict set hooks $token procs $label $procid
		
		variable printevents
		if {[dict exists $printevents $token] && ![dict exists $hooks $token hook]} {
			dict set hooks $token hook [::hexchat::hook_print [dict get $printevents $token name]  [list ::hexchat::compat::PrintHook $token]]
		}
	}
	
	proc ::hexchat::compat::PrintHook {token word} {
		variable complete
		set oldcomplete $complete
		set complete [dict create word $word word_eol $word defresult $::hexchat::EAT_NONE result $::hexchat::EAT_NONE]
		
		set origctx [::hexchat::getcontext]
		# _src _dest _cmd _rest _raw _label _private
		if {$token eq {CHAT}} {
			set _src [lindex $word 3]!*@[lindex $word 1]
			set _dest [lindex $word 2]
			set _cmd $token
			set _rest [lindex $word 4]
			set _raw ""
			set _private 0
		} else {
			variable printevents
			set _src ""
			set _dest ""
			set _cmd $token
			set _rest ""
			if {[dict get $printevents $token argc] > 0} {
				set _raw [linsert [lrange $word 1 [dict get $printevents $token argc]] 0 [dict get $printevents $token name]]
			} else {
				set _raw ""
			}
			set _private 0
		}
		
		variable hooks
		if {[dict exists $hooks $token procs]} {
			dict for {label proc} [dict get $hooks $token procs] {
				try {
					$proc $_src $_dest $_cmd $_rest $_raw $label $_private
				} on error {- opt} {
					::hexchat::print "${::hexchat::msgprefix}ERROR (on $token $label) [dict get $opt -errorinfo]"
				}
				::hexchat::setcontext $origctx
				
				set cplvl [dict get $complete result]
				if {$cplvl == $::hexchat::EAT_PLUGIN || $cplvl == $::hexchat::EAT_ALL} {
					break
				}
			}
		}
		
		set newcomplete $complete
		set complete $oldcomplete
		return [dict get $newcomplete result]
	}
	
	proc ::hexchat::compat::ServerRawHook {word word_eol} {
		variable complete
		variable hooks
		
		if {[string index [lindex $word 1] 0] eq {:}} {
			set src [lindex $word 1]
			set cmd [lindex $word 2]
			set dest [lindex $word 3]
			set rest [lindex $word_eol 4]
		} else {
			set src ""
			set cmd [lindex $word 1]
			if {[string index [lindex $word_eol 2] 0] eq {:}} {
				set dest ""
				set rest [lindex $word_eol 2]
			} else {
				set dest [lindex $word 2]
				set rest [lindex $word_eol 3]
			}
		}
		
		set realcmd $cmd
		
		if {[string length $cmd] == 0} {
			return ::hexchat::EAT_NONE
		}
		
		if {[string index $src 0] eq {:}} {
			set src [string range $src 1 end]
		}
		if {[string index $dest 0] eq {:}} {
			set dest [string range $dest 1 end]
		}
		if {[string index $rest 0] eq {:}} {
			set rest [string range $rest 1 end]
		}
		
		set oldcomplete $complete
		set complete [dict create word $word word_eol $word_eol defresult $::hexchat::EAT_NONE result $::hexchat::EAT_NONE]
		
		set hasentry 0
		set entry {}
		
		if {[string index $rest 0] eq "\0x01"} {
			set rest [string range $rest 1 end]
			if {[string equal -nocase $cmd {PRIVMSG}]} {
				if {[string match -nocase {ACTION *} $rest]} {
					set cmd ACTION
					set rest [string range $rest 7 end]
				} else {
					set cmd CTCP
				}
			} elseif {[string equal -nocase $cmd {NOTICE}]} {
				set cmd CTCR
			}
			set ctcp 1
		} elseif {[string equal -nocase $cmd {NOTICE}] && [string first ! $src] != -1} {
			set cmd SNOTICE
		} elseif {[string index $rest 0] eq {!}} {
			set chancmd [string range [lindex $word 4] 1 end]
			if {[dict exists $hooks $chancmd]} {
				set entry [dict get $hooks $chancmd]
				set hasentry 1
				set rest [lindex $word_eol 5]
			}
		}
		
		if {!$hasentry} {
			if {[dict exists $hooks $cmd]} {
				set entry [dict get $hooks $cmd]
				set hasentry 1
			}
		}
		
		if {$hasentry} {
			set private [string is alpha [string index $dest 0]]
			if {$ctcp} {
				if {[string index $rest end] eq "\0x01"} {
					set rest [string range $rest 0 end-1]
				}
			}
		
			set origctx [::hexchat::getcontext]
			dict for {label proc} [dict get $entry procs] {
				# _src _dest _cmd _rest _raw _label _private
				try {
					$proc $src $dest $cmd $rest [lindex $word_eol 1] $label $private
				} on error {- opt} {
					::hexchat::print "${::hexchat::msgprefix}ERROR (on $token $label) [dict get $opt -errorinfo]"
				}
				::hexchat::setcontext $origctx
				
				set cplvl [dict get $complete result]
				if {$cplvl == $::hexchat::EAT_ALL || $cplvl == $::hexchat::EAT_PLUGIN} {
					break
				}
			}
		}
		
		set newcomplete $complete
		set complete $oldcomplete
		return [dict get $newcomplete result]
	}
	
	set ::hexchat::compat::NullCommandRecurse 0
	proc ::hexchat::compat::NullCommandHook {word word_eol} {
		variable NullCommandRecurse
		if {$NullCommandRecurse} {
			return $::hexchat::EAT_NOTE
		}
		
		variable hooks
		if {[dict exists $hooks @[::hexchat::getinfo channel]]} {
			incr NullCommandRecurse
			
			variable complete
			set oldcomplete $complete
			set complete [dict create word $word word_eol $word_eol defresult $::hexchat::EAT_ALL result $::hexchat::EAT_NONE]
			
			set origctx [::hexchat::getcontext]

			dict for {label proc} [dict get $entry procs] {
				# _src _dest _cmd _rest _raw _label _private
				try {
					$proc $src $dest $cmd $rest [lindex $word_eol 1] $label $private
				} on error {- opt} {
					::hexchat::print "${::hexchat::msgprefix}ERROR (on $token $label) [dict get $opt -errorinfo]"
				}
				::hexchat::setcontext $origctx
				
				set cplvl [dict get $complete result]
				if {$cplvl == $::hexchat::EAT_ALL || $cplvl == $::hexchat::EAT_PLUGIN} {
					break
				}
			}
			
			incr NullCommandRecurse -1
			
			set newcomplete $complete
			set complete $oldcomplete
			return [dict get $newcomplete result]
		} else {
			return $::hexchat::EAT_NONE
		}
	}
}