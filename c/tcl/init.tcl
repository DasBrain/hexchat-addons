package provide hexchat [::hexchat::getinfo version]

namespace eval ::hexchat {
	namespace export command findcontext getcontext getinfo getlist getlist_fields nickcmp print prefs setcontext strip hook_command hook_server hook_print hook_timer 
	
	variable EAT_NONE 0
	variable EAT_HEXCHAT 1
	variable EAT_PLUGIN 2
	variable EAT_ALL 3
	
}

proc ::hexchat::command_tcl {words words_eol} {
	set code [catch {uplevel #0 [lindex $words_eol 2]} res opt]
	if {$code == 1} {
		::hexchat::print "\0039Tcl plugin\003\tERROR: [dict get $opt -errorinfo]"
	} else {
		::hexchat::print "\0039Tcl plugin\003\tRESULT: $res"
	}
	return $::hexchat::EAT_ALL
}

proc ::hexchat::command_source {words words_eol} {
	if {[lindex $words_eol 2] eq {}} {
		return $::hexchat::EAT_NONE
	}
	set file [lindex $word 2]
	if {[string match *.tcl $file] || [string match *.tm $file]} {
		if {![file exists $file]} {
			set file [file join [::hexchat::getinfo configdir] addons $file]
		}
		if {[catch {uplevel #0 [list source -encoding utf-8 $file]}] == 1} {
			::hexchat::print "\0039Tcl plugin\003\tERROR: [dict get $opt -errorinfo]"
		} else {
			::hexchat::print "\0039Tcl plugin\003\tSourced $file\n"
		}
		return $::hexchat::EAT_ALL
	} else {
		return $::hexchat::EAT_NONE
	}
}

set ::hexchat::command_tcl [::hexchat::hook_command "tcl" ::hexchat::command_tcl]
set ::hexchat::command_source [::hexchat::hook_command "source" ::hexchat::command_source]

