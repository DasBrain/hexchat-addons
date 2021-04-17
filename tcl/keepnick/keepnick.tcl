proc serverid {} {
	return [prefs id]
}

set wantednicks {}
on XC_NICKCLASH keepnick {
	global wantednicks
	set sid [serverid]
	if {![dict exists $wantednicks $sid]} {
		dict set wantednicks [serverid] [lindex $_raw 1]
	}
}

on 422 keepnick { # ERR_NOMOTD
	keepnick:connected
}

on 376 keepnick { # RPL_ENDOFMOTD
	keepnick:connected
}

proc keepnick:connected {} {
	global wantednicks
	set sid [serverid]
	if {[dict exists $wantednicks $sid]} {
		set wanted [dict get $wantednicks $sid]
		if {[nickcmp [me] $wanted] != 0} {
			command "GHOST $wanted [nickserv]"
			command "NICK $wanted"
		} else {
			dict unset wantednicks $sid
		}
	}
}

on XC_UCHANGENICK keepnick {
	global wantednicks
	set sid [serverid]
	if {[dict exists $wantednicks $sid]} {
		set newnick [lindex $_raw 2]
		if {[nickcmp $newnick [dict get $wantednicks $sid]] == 0} {
			dict unset wantednicks $sid
		}
	}
}

on QUIT keepnick {
	global wantednicks
	set sid [serverid]
	if {[dict exists $wantednicks $sid]} {
		set wanted [dict get $wantednicks $sid]
		splitsrc
		if {[nickcmp $_nick $wanted] == 0} {
			command "NICK $wanted"
		}
	}
}

on XC_DISCON keepnick {
	global wantednicks
	dict unset wantednicks [serverid]
}