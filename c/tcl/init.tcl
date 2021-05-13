package provide hexchat [::hexchat::getinfo version]

namespace eval ::hexchat {
	namespace export command findcontext getcontext getinfo getlist list_fields nickcmp print prefs setcontext strip hook_command hook_server hook_print hook_timer unregister_hook emit_print
	namespace export pluginpref_get pluginpref_set pluginpref_delete pluginpref_list

	variable EAT_NONE 0
	variable EAT_HEXCHAT 1
	variable EAT_PLUGIN 2
	variable EAT_ALL 3

	variable msgprefix "\0039Tcl plugin\003\t"

}

proc ::hexchat::UplevelError opt {
	variable msgprefix
	set ei [join [lrange [split [dict get $opt -errorinfo] \n] 0 end-3] \n]
	::hexchat::print "${msgprefix}ERROR: $ei"
}

proc ::hexchat::command_tcl {words words_eol} {
	variable msgprefix
	try {
		::hexchat::print "${msgprefix}RESULT: [uplevel #0 [lindex $words_eol 2]]"
	} on error {- opt} {
		UplevelError $opt
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
		variable msgprefix
		try {
			uplevel #0 [list source -encoding utf-8 $file]
			::hexchat::print "${msgprefix}Sourced $file\n"
		} on error {- opt} {
			UplevelError $opt
		}
		return $::hexchat::EAT_ALL
	} else {
		return $::hexchat::EAT_NONE
	}
}

set ::hexchat::command_tcl [::hexchat::hook_command "tcl" ::hexchat::command_tcl]
set ::hexchat::command_source [::hexchat::hook_command "source" ::hexchat::command_source]
set ::hexchat::command_load [::hexchat::hook_command "LOAD" ::hexchat::command_source]

proc ::bgerror {msg} {
	::hexchat::print "${::hexchat::msgprefix}${::errorInfo}"
}

namespace eval ::hexchat::util {
	namespace export withctx
}

proc ::hexchat::util::withctx {ctx script} {
	set oldctx [::hexchat::getcontext]
	::hexchat::setcontext $ctx
	catch {uplevel 1 $script} res opt
	::hexchat::setcontext $oldctx
	dict incr opt -level
	return -options $opt $res
}

apply {{} {
	variable msgprefix
	set files [lsort [glob -nocomplain -directory [::hexchat::getinfo configdir] addons/*.tcl]]
	foreach first {init compat} {
		set idx [lsearch -glob $files */${first}.tcl]
		if {$idx != -1} {
			set f [lindex $files $idx]
			set files [lreplace $files $idx $idx]
			set files [linsert $files 0 $f]
		}
	}
	foreach f $files {
		try {
			uplevel #0 [list source -encoding utf-8 $f]
			::hexchat::print "${msgprefix}Sourced \"$f\""
		} on error {- opt} {
			UplevelError $opt
		}
	}
} ::hexchat}