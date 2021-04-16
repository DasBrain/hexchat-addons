/***************************************************************************
                           tclplugin.c  -  Tcl plugin for xchat 1.9.x / 2.x.x
                           -------------------------------------------------s
    begin                : Sat Nov 19 17:31:20 MST 2002
    copyright            : Copyright 2002-2012 Daniel P. Stasinski
    email                : daniel@GenericInbox.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

static char RCSID[] = "$Id: tclplugin.c,v 1.65 2012/07/26 20:02:12 mooooooo Exp $";

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>
#include <ctype.h>
#include <stdarg.h>
#include <tcl.h>
#include <tclDecls.h>
#include <sys/stat.h>

#ifdef WIN32
#define strcasecmp _stricmp
#endif

#ifdef WIN32
#define strncasecmp _strnicmp
#endif

#ifdef WIN32
#include <windows.h>
#include "win32/typedef.h"
#define bzero(mem, sz) memset((mem), 0, (sz))
#define bcopy(src, dest, count) memmove((dest), (src), (count))
#else
#include <unistd.h>
#endif

#include "hexchat-plugin.h"
#include "tclplugin.h"
#include "printevents.h"

static int nexttimerid = 0;
static int nexttimerindex = 0;
static timer timers[MAX_TIMERS];

static char VERSION[16];

static int initialized = 0;
static int reinit_tried = 0;
static Tcl_Interp *interp = NULL;
static hexchat_plugin *ph;
static hexchat_hook *raw_line_hook;
static hexchat_hook *Command_TCL_hook;
static hexchat_hook *Command_Source_hook;
static hexchat_hook *Command_Reload_hook;
static hexchat_hook *Command_Load_hook;
static hexchat_hook *Event_Handler_hook;
static hexchat_hook *Null_Command_hook;

static int complete_level = 0;
static t_complete complete[MAX_COMPLETES + 1];
static Tcl_HashTable cmdTablePtr;
static Tcl_HashTable aliasTablePtr;

static int nextprocid = 0x1000;
#define PROCPREFIX "::__xctcl_"

static char unknown[] = {
"rename unknown iunknown\n"
"proc ::unknown {args} {\n"
"  global errorInfo errorCode\n"
"  if { [string index [lindex $args 0] 0] == \"/\" } {\n"
"    command \"[string range [join $args \" \"] 1 end]\"\n"
"  } else {\n"
"    set code [catch {uplevel iunknown $args} msg]\n"
"    if {$code == 1} {\n"
"      set new [split $errorInfo \\n]\n"
"      set new [join [lrange $new 0 [expr {[llength $new] - 8}]] \\n]\n"
"      return -code error -errorcode $errorCode -errorinfo $new $msg\n"
"    } else {\n"
"      return -code $code $msg\n"
"    }\n"
"  }\n"
"}\n"
"proc unsupported0 {from to {bytes \"\"}} {\n"
"  set b [expr {$bytes == \"\" ? \"\" : \"-size [list $bytes]\"}]\n"
"  eval [list fcopy $from $to] $b\n"
"}\n"
};

/* don't pollute the filesystem with script files, this only causes misuse of the folders
 * only use ~/.config/hexchat/addons/ and %APPDATA%\HexChat\addons */
static char sourcedirs[] = {
    "set files [lsort [glob -nocomplain -directory [configdir] \"/addons/*.tcl\"]]\n"
        "set init [lsearch -glob $files \"*/init.tcl\"]\n"
        "if { $init > 0 } {\n"
        "set initfile [lindex $files $init]\n"
        "set files [lreplace $files $init $init]\n"
        "set files [linsert $files 0 $initfile]\n" "}\n" "foreach f $files {\n" "if { [catch { source $f } errMsg] } {\n" "puts \"Tcl plugin\\tError sourcing \\\"$f\\\" ($errMsg)\"\n" "} else {\n" "puts \"Tcl plugin\\tSourced \\\"$f\\\"\"\n" "}\n" "}\n"
};

static char inlinetcl[] = {
"proc splitsrc { } {\n"
"uplevel \"scan \\$_src \\\"%\\\\\\[^!\\\\\\]!%\\\\\\[^@\\\\\\]@%s\\\" _nick _ident _host\"\n"
"}\n"

"proc ::exit { } {\n"
"puts \"Using 'exit' is bad\"\n"
"}\n"

"proc ::away { args } { return [eval [join [list getinfo $args away]]] }\n"
"proc ::channel { args } { return [eval [join [list getinfo $args channel]]] }\n"
"proc ::tab { args } { return [eval [join [list getinfo $args channel]]] }\n"
"proc ::charset { args } { return [eval [join [list getinfo $args charset]]] }\n"
"proc ::host { args } { return [eval [join [list getinfo $args host]]] }\n"
"proc ::inputbox { args } { return [eval [join [list getinfo $args inputbox]]] }\n"
"proc ::libdirfs { args } { return [eval [join [list getinfo $args libdirfs]]] }\n"
"proc ::network { args } { return [eval [join [list getinfo $args network]]] }\n"
"proc ::nickserv { args } { return [eval [join [list getinfo $args nickserv]]] }\n"
"proc ::server { args } { return [eval [join [list getinfo $args server]]] }\n"
"proc ::version { args } { return [eval [join [list getinfo $args version]]] }\n"
"proc ::win_status { args } { return [eval [join [list getinfo $args win_status]]] }\n"
"proc ::configdir { args } { return [eval [join [list getinfo $args configdir]]] }\n"

"proc ::color { {arg {}} } { return \"\\003$arg\" }\n"
"proc ::bold { } { return \"\\002\" }\n"
"proc ::underline { } { return \"\\037\" }\n"
"proc ::reverse { } { return \"\\026\" }\n"
"proc ::reset { } { return \"\\017\" }\n"

"proc ::__xctcl_errorInfo { } {\n"
"      set text [split $::errorInfo \\n]\n"
"      puts [string trim [join [lrange $text 0 [expr {[llength $text] - 4}]] \\n]]\n"
"}\n"

"proc ::bgerror { message } {\n"
"      set text [split $::errorInfo \\n]\n"
"      puts [string trim [join [lrange $text 0 [expr {[llength $text] - 4}]] \\n]]\n"
"}\n"
};

static void NiceErrorInfo ()
{
    Tcl_Eval(interp, "::__xctcl_errorInfo");
}

static char *InternalProcName(int value)
{
    static char result[32];
    sprintf(result, "%s%08x", PROCPREFIX, value);
    return result;
}

static int SourceInternalProc(int id, char *args, char *source)
{
    Tcl_DString ds;
    int result;
    Tcl_DStringInit(&ds);

    Tcl_DStringAppend(&ds, "proc ", -1);
    Tcl_DStringAppend(&ds, InternalProcName(id), -1);
    Tcl_DStringAppend(&ds, " { ", -1);
    Tcl_DStringAppend(&ds, args, -1);
    Tcl_DStringAppend(&ds, " } {\n", -1);
    Tcl_DStringAppend(&ds, source, -1);
    Tcl_DStringAppend(&ds, "\n}\n\n", -1);

    result = Tcl_Eval(interp, ds.string);

    Tcl_DStringFree(&ds);

    return result;
}

static int EvalInternalProc(const char *procname, int ct, ...)
{
    Tcl_DString ds;
    int result;
    va_list ap;
    char *buf;

    Tcl_DStringInit(&ds);

    Tcl_DStringAppend(&ds, procname, -1);

    if (ct) {
        va_start(ap, ct);
        while (ct--) {
            if ((buf = va_arg(ap, char *)) != NULL)
                 Tcl_DStringAppendElement(&ds, buf);
            else
                Tcl_DStringAppend(&ds, " \"\"", -1);
        }
        va_end(ap);
    }

    result = Tcl_Eval(interp, ds.string);

    Tcl_DStringFree(&ds);

    return result;
}


static void DeleteInternalProc(const char *proc)
{
    Tcl_DeleteCommand(interp, proc);
}

static char *StrDup(const char *string, int *length)
{
    char *result;

    if (string == NULL)
        return NULL;

    *length = (int) strlen(string);
    result = Tcl_Alloc((*length) + 1);
    strncpy(result, string, (size_t) (*length) + 1L);

    return result;
}

static hexchat_context *objtoctx(Tcl_Obj *objPtr)
{
    Tcl_WideInt result;

    if (!objPtr)
        return NULL;

    if (Tcl_GetWideIntFromObj(NULL, objPtr, &result) == TCL_OK)
        return (hexchat_context *) (void *) result;
    else
        return NULL;
}

static hexchat_context *xchat_smart_context(Tcl_Obj *oarg1, Tcl_Obj *arg2)
{
    const char *server, *s, *n, *arg1;
    hexchat_context *result = NULL;
    hexchat_context *ctx = NULL;
    hexchat_context *actx = NULL;
    hexchat_list *list;

    if (oarg1 == NULL)
        return hexchat_get_context(ph);;

    if (oarg1 && arg2) {
        result = hexchat_find_context(ph, Tcl_GetString(oarg1), Tcl_GetString(arg2));
        if (result == NULL)
            result = hexchat_find_context(ph, Tcl_GetString(arg2), Tcl_GetString(oarg1));
        return result;
    } else {

        actx = objtoctx(oarg1);
        arg1 = Tcl_GetString(oarg1);

        server = hexchat_get_info(ph, "server");

        list = hexchat_list_get(ph, "channels");

        if (list != NULL) {

            while (hexchat_list_next(ph, list)) {

                ctx = (hexchat_context *)hexchat_list_str(ph, list, "context");

                if (actx) {
                    if (ctx == actx) {
                        result = ctx;
                        break;
                    }
                } else {

                    s = hexchat_list_str(ph, list, "server");

                    if (hexchat_list_int(ph, list, "type") == 1) {
                        if (strcasecmp(arg1, s) == 0) {
                            result = ctx;
                            break;
                        }
                        n = hexchat_list_str(ph, list, "network");
                        if (n) {
                            if (strcasecmp(arg1, n) == 0) {
                                result = ctx;
                                break;
                            }
                        }
                    } else {
                        if ((strcasecmp(server, s) == 0) && (strcasecmp(arg1, hexchat_list_str(ph, list, "channel")) == 0)) {
                            result = ctx;
                            break;
                        }
                    }
                }
            }

            hexchat_list_free(ph, list);
        }

    }

    return result;
}

static void queue_nexttimer()
{
    int x;
    time_t then;

    nexttimerindex = 0;
    then = (time_t) 0x7fffffff;

    for (x = 1; x < MAX_TIMERS; x++) {
        if (timers[x].timerid) {
            if (timers[x].timestamp < then) {
                then = timers[x].timestamp;
                nexttimerindex = x;
            }
        }
    }
}

static int insert_timer(int seconds, int count, Tcl_Obj *script)
{
    int x;
    int dummy;
    time_t now;
    int id;

    if (script == NULL)
        return (-1);

    id = (nextprocid++ % INT_MAX) + 1;

    now = time(NULL);

    for (x = 1; x < MAX_TIMERS; x++) {
        if (timers[x].timerid == 0) {
            if (SourceInternalProc(id, "", Tcl_GetString(script)) == TCL_ERROR) {
                hexchat_printf(ph, "\0039TCL plugin\003\tERROR (timer %d) ", timers[x].timerid);
                NiceErrorInfo ();
                return (-1);
            }
            timers[x].timerid = (nexttimerid++ % INT_MAX) + 1;
            timers[x].timestamp = now + seconds;
            timers[x].count = count;
            timers[x].seconds = seconds;
            timers[x].procPtr = StrDup(InternalProcName(id), &dummy);
            queue_nexttimer();
            return (timers[x].timerid);
        }
    }

    return (-1);
}

static void do_timer()
{
    hexchat_context *origctx;
    time_t now;
    int index;

    if (!nexttimerindex)
        return;

    now = time(NULL);

    if (now < timers[nexttimerindex].timestamp)
        return;

    index = nexttimerindex;
    origctx = hexchat_get_context(ph);
    if (EvalInternalProc(timers[index].procPtr, 0) == TCL_ERROR) {
        hexchat_printf(ph, "\0039TCL plugin\003\tERROR (timer %d) ", timers[index].timerid);
        NiceErrorInfo ();
    }
    hexchat_set_context(ph, origctx);

    if (timers[index].count != -1)
      timers[index].count--;

    if (timers[index].count == 0) {
      timers[index].timerid = 0;
      if (timers[index].procPtr != NULL) {
          DeleteInternalProc(timers[index].procPtr);
          Tcl_Free(timers[index].procPtr);
      }
      timers[index].procPtr = NULL;
    } else {
      timers[index].timestamp += timers[index].seconds;
    }

    queue_nexttimer();

    return;

}

static int Server_raw_line(char *word[], char *word_eol[], void *userdata)
{
    char *src, *cmd, *dest, *rest;
    char *chancmd = NULL;
    char *procList;
    Tcl_HashEntry *entry = NULL;
    hexchat_context *origctx;
    int len;
    int dummy;
    char *string = NULL;
    int ctcp = 0;
    int count;
    int list_argc, proc_argc;
    const char **list_argv, **proc_argv;
    int private = 0;

    if (word[0][0] == 0)
        return HEXCHAT_EAT_NONE;

    if (complete_level == MAX_COMPLETES)
        return HEXCHAT_EAT_NONE;

    complete_level++;
    complete[complete_level].defresult = HEXCHAT_EAT_NONE;     /* HEXCHAT_EAT_PLUGIN; */
    complete[complete_level].result = HEXCHAT_EAT_NONE;
    complete[complete_level].word = word;
	complete[complete_level].word_eol = word_eol;

    if (word[1][0] == ':') {
        src = word[1];
        cmd = word[2];
        dest = word[3];
        rest = word_eol[4];
    } else {
        src = "";
        cmd = word[1];
        if (word_eol[2][0] == ':') {
            dest = "";
            rest = word_eol[2];
        } else {
            dest = word[2];
            rest = word_eol[3];
        }
    }

    if (src[0] == ':')
        src++;
    if (dest[0] == ':')
        dest++;
    if (rest[0] == ':')
        rest++;

    if (rest[0] == 0x01) {
        rest++;
        if (strcasecmp("PRIVMSG", cmd) == 0) {
            if (strncasecmp(rest, "ACTION ", 7) == 0) {
                cmd = "ACTION";
                rest += 7;
            } else
                cmd = "CTCP";
        } else if (!strcasecmp("NOTICE", cmd))
            cmd = "CTCR";
        ctcp = 1;
    } else if (!strcasecmp("NOTICE", cmd) && (strchr(src, '!') == NULL)) {
        cmd = "SNOTICE";
    } else if (rest[0] == '!') {
        chancmd = word[4] + 1;
    }

    if (chancmd != NULL) {
        string = StrDup(chancmd, &dummy);
        Tcl_UtfToUpper(string);
        if ((entry = Tcl_FindHashEntry(&cmdTablePtr, string)) == NULL) {
            Tcl_Free(string);
        } else {
            cmd = chancmd;
            rest = word_eol[5];
        }
    }

    if (entry == NULL) {
        string = StrDup(cmd, &dummy);
        Tcl_UtfToUpper(string);
        entry = Tcl_FindHashEntry(&cmdTablePtr, string);
    }

    if (entry != NULL) {

        procList = Tcl_GetHashValue(entry);

        if (isalpha(dest[0]))
            private = 1;

        rest = StrDup(rest, &len);
        if (ctcp) {
            if (rest != NULL) {
                if ((len > 1) && (rest[len - 1] == 0x01))
                    rest[len - 1] = 0;
            }
        }

        if (Tcl_SplitList(interp, procList, &list_argc, &list_argv) == TCL_OK) {

            for (count = 0; count < list_argc; count++) {

                if (Tcl_SplitList(interp, list_argv[count], &proc_argc, &proc_argv) != TCL_OK)
                    continue;

                origctx = hexchat_get_context(ph);
                if (EvalInternalProc(proc_argv[1], 7, src, dest, cmd, rest, word_eol[1], proc_argv[0], private ? "1" : 0) == TCL_ERROR) {
                    hexchat_printf(ph, "\0039TCL plugin\003\tERROR (on %s %s) ", cmd, proc_argv[0]);
                    NiceErrorInfo ();
                }
                hexchat_set_context(ph, origctx);

                Tcl_Free((char *) proc_argv);

                if ((complete[complete_level].result ==  HEXCHAT_EAT_PLUGIN) || (complete[complete_level].result == HEXCHAT_EAT_ALL))
                    break;

            }

            Tcl_Free((char *) list_argv);

        }

        Tcl_Free(rest);

    }

    if (string)
        Tcl_Free(string);

    return complete[complete_level--].result;

}

static int Print_Hook(char *word[], void *userdata)
{
    char *procList;
    Tcl_HashEntry *entry;
    hexchat_context *origctx;
    int count;
    int list_argc, proc_argc;
    const char **list_argv, **proc_argv;
    Tcl_DString ds;
    int x;

    if (complete_level == MAX_COMPLETES)
        return HEXCHAT_EAT_NONE;

    complete_level++;
    complete[complete_level].defresult = HEXCHAT_EAT_NONE;     /* HEXCHAT_EAT_PLUGIN; */
    complete[complete_level].result = HEXCHAT_EAT_NONE;
    complete[complete_level].word = word;
	complete[complete_level].word_eol = word;

    if ((entry = Tcl_FindHashEntry(&cmdTablePtr, xc[(int)(intptr_t) userdata].event)) != NULL) {

        procList = Tcl_GetHashValue(entry);

        if (Tcl_SplitList(interp, procList, &list_argc, &list_argv) == TCL_OK) {

            for (count = 0; count < list_argc; count++) {

                if (Tcl_SplitList(interp, list_argv[count], &proc_argc, &proc_argv) != TCL_OK)
                    continue;

                origctx = hexchat_get_context(ph);

                Tcl_DStringInit(&ds);

                if ((intptr_t) userdata == CHAT) {
                    Tcl_DStringAppend(&ds, word[3], -1);
                    Tcl_DStringAppend(&ds, "!*@", 3);
                    Tcl_DStringAppend(&ds, word[1], -1);
                    if (EvalInternalProc(proc_argv[1], 7, ds.string, word[2], xc[(intptr_t) userdata].event, word[4], "", proc_argv[0], "0") == TCL_ERROR) {
                        hexchat_printf(ph, "\0039TCL plugin\003\tERROR (on %s %s) ", xc[(intptr_t) userdata].event, proc_argv[0]);
                        NiceErrorInfo ();
                    }
                } else {
                    if (xc[(intptr_t) userdata].argc > 0) {
                        for (x = 0; x <= xc[(intptr_t) userdata].argc; x++)
                            Tcl_DStringAppendElement(&ds, word[x]);
                    }
                    if (EvalInternalProc(proc_argv[1], 7, "", "", xc[(intptr_t) userdata].event, "", ds.string, proc_argv[0], "0") == TCL_ERROR) {
                        hexchat_printf(ph, "\0039Tcl plugin\003\tERROR (on %s %s) ", xc[(intptr_t) userdata].event, proc_argv[0]);
                        NiceErrorInfo ();
                    }
                }

                Tcl_DStringFree(&ds);

                hexchat_set_context(ph, origctx);

                Tcl_Free((char *) proc_argv);

                if ((complete[complete_level].result ==  HEXCHAT_EAT_PLUGIN) || (complete[complete_level].result ==  HEXCHAT_EAT_ALL))
                    break;

            }

            Tcl_Free((char *) list_argv);

        }
    }

    return complete[complete_level--].result;
}


static int tcl_timerexists(ClientData cd, Tcl_Interp * irp, int argc, Tcl_Obj *const argv[])
{
    int x;
    int timerid;

    BADARGS(2, 2, " schedid");

    if (Tcl_GetIntFromObj(irp, argv[1], &timerid) != TCL_OK) {
        Tcl_AppendResult(irp, "Invalid timer id", NULL);
        return TCL_ERROR;
    }

    if (timerid) {
        for (x = 1; x < MAX_TIMERS; x++) {
            if (timers[x].timerid == timerid) {
                Tcl_AppendResult(irp, "1", NULL);
                return TCL_OK;
            }
        }
    }

    Tcl_AppendResult(irp, "0", NULL);

    return TCL_OK;
}

static int tcl_killtimer(ClientData cd, Tcl_Interp * irp, int argc, Tcl_Obj *const argv[])
{
    int x;
    int timerid;

    BADARGS(2, 2, " timerid");

    if (Tcl_GetIntFromObj(irp, argv[1], &timerid) != TCL_OK) {
        Tcl_AppendResult(irp, "Invalid timer id", NULL);
        return TCL_ERROR;
    }

    if (timerid) {
        for (x = 1; x < MAX_TIMERS; x++) {
            if (timers[x].timerid == timerid) {
                timers[x].timerid = 0;
                if (timers[x].procPtr != NULL) {
                    DeleteInternalProc(timers[x].procPtr);
                    Tcl_Free(timers[x].procPtr);
                }
                timers[x].procPtr = NULL;
                break;
            }
        }
    }

    queue_nexttimer();

    return TCL_OK;
}

static int tcl_timers(ClientData cd, Tcl_Interp * irp, int argc, Tcl_Obj *const argv[])
{
    int x;
    Tcl_Obj * result;
    time_t now;

    BADARGS(1, 1, "");

    now = time(NULL);

    result = Tcl_NewListObj(0, NULL);

    for (x = 1; x < MAX_TIMERS; x++) {
        if (timers[x].timerid) {
            Tcl_Obj * sublist = Tcl_NewListObj(0, NULL);
            Tcl_ListObjAppendElement(irp, sublist, Tcl_NewIntObj(timers[x].timerid));
            Tcl_ListObjAppendElement(irp, sublist, Tcl_NewWideIntObj((Tcl_WideInt)timers[x].timestamp - now));
            Tcl_ListObjAppendElement(irp, sublist, Tcl_NewStringObj(timers[x].procPtr, -1));
            Tcl_ListObjAppendElement(irp, sublist, Tcl_NewIntObj(timers[x].seconds));
            Tcl_ListObjAppendElement(irp, sublist, Tcl_NewIntObj(timers[x].count));
            Tcl_ListObjAppendElement(irp, result, sublist);
        }
    }

    Tcl_SetObjResult(irp, result);

    return TCL_OK;
}

static int tcl_timer(ClientData cd, Tcl_Interp * irp, int argc, Tcl_Obj *const argv[])
{
    int seconds;
    int timerid;
    int repeat = 0;
    int count = 0;
    int first = 1;

    BADARGS(3, 6, " ?-repeat? ?-count times? seconds {script | procname ?args?}");

    while (argc--) {
        if (strcasecmp(Tcl_GetString(argv[first]), "-repeat") == 0) {
            repeat++;
        } else if (strcasecmp(Tcl_GetString(argv[first]), "-count") == 0) {
            if (Tcl_GetIntFromObj(irp, argv[++first], &count) != TCL_OK)
                return TCL_ERROR;
        } else {
            break;
        }
        first++;
    }

    if (repeat && !count)
      count = -1;

    if (!count)
      count = 1;

    if (Tcl_GetIntFromObj(irp, argv[first++], &seconds) != TCL_OK)
        return TCL_ERROR;

    if ((timerid = insert_timer(seconds, count, argv[first++])) == -1) {
        Tcl_AppendResult(irp, "0", NULL);
        return TCL_ERROR;
    }

    Tcl_SetObjResult(irp, Tcl_NewIntObj(timerid));

    queue_nexttimer();

    return TCL_OK;
}

static int tcl_on(ClientData cd, Tcl_Interp * irp, int argc, Tcl_Obj *const argv[])
{
    int newentry;
    char *procList;
    Tcl_HashEntry *entry;
    char *token;
    int dummy;
    Tcl_DString ds;
    int index;
    int count;
    int list_argc, proc_argc;
    int id;
    const char **list_argv, **proc_argv;

    BADARGS(4, 4, " token label {script | procname ?args?}");

    id = (nextprocid++ % INT_MAX) + 1;

    if (SourceInternalProc(id, "_src _dest _cmd _rest _raw _label _private", Tcl_GetString(argv[3])) == TCL_ERROR) {
        hexchat_printf(ph, "\0039Tcl plugin\003\tERROR (on %s:%s) ", Tcl_GetString(argv[1]), Tcl_GetString(argv[2]));
        NiceErrorInfo ();
        return TCL_OK;
    }

    token = StrDup(Tcl_GetString(argv[1]), &dummy);
    Tcl_UtfToUpper(token);

    Tcl_DStringInit(&ds);

    entry = Tcl_CreateHashEntry(&cmdTablePtr, token, &newentry);
    if (!newentry) {
        procList = Tcl_GetHashValue(entry);
        if (Tcl_SplitList(interp, procList, &list_argc, &list_argv) == TCL_OK) {
            for (count = 0; count < list_argc; count++) {
                if (Tcl_SplitList(interp, list_argv[count], &proc_argc, &proc_argv) != TCL_OK)
                    continue;
                if (strcmp(proc_argv[0], Tcl_GetString(argv[2]))) {
                    Tcl_DStringStartSublist(&ds);
                    Tcl_DStringAppendElement(&ds, proc_argv[0]);
                    Tcl_DStringAppendElement(&ds, proc_argv[1]);
                    Tcl_DStringEndSublist(&ds);
                } else {
                    DeleteInternalProc(proc_argv[1]);
                }
                Tcl_Free((char *) proc_argv);
            }
            Tcl_Free((char *) list_argv);
        }
        Tcl_Free(procList);
    }

    Tcl_DStringStartSublist(&ds);
    Tcl_DStringAppendElement(&ds, Tcl_GetString(argv[2]));
    Tcl_DStringAppendElement(&ds, InternalProcName(id));
    Tcl_DStringEndSublist(&ds);

    procList = StrDup(ds.string, &dummy);

    Tcl_SetHashValue(entry, procList);

    if ((strncmp(token, "XC_", 3) == 0) || (strcmp(token, "CHAT") == 0)) {
        for (index = 0; index < XC_SIZE; index++) {
            if (strcmp(xc[index].event, token) == 0) {
                if (xc[index].hook == NULL) {
                    xc[index].hook = hexchat_hook_print(ph, xc[index].emit, HEXCHAT_PRI_NORM, Print_Hook, (void *)(uintptr_t)index);
                    break;
                }
            }
        }
    }

    Tcl_Free(token);
    Tcl_DStringFree(&ds);

    return TCL_OK;
}

static int tcl_off(ClientData cd, Tcl_Interp * irp, int argc, Tcl_Obj *const argv[])
{
    char *procList;
    Tcl_HashEntry *entry;
    char *token;
    int dummy;
    Tcl_DString ds;
    int index;
    int count;
    int list_argc, proc_argc;
    const char **list_argv, **proc_argv;

    BADARGS(2, 3, " token ?label?");

    token = StrDup(Tcl_GetString(argv[1]), &dummy);
    Tcl_UtfToUpper(token);

    Tcl_DStringInit(&ds);

    if ((entry = Tcl_FindHashEntry(&cmdTablePtr, token)) != NULL) {

        procList = Tcl_GetHashValue(entry);

        if (argc == 3) {
            if (Tcl_SplitList(interp, procList, &list_argc, &list_argv) == TCL_OK) {
                for (count = 0; count < list_argc; count++) {
                    if (Tcl_SplitList(interp, list_argv[count], &proc_argc, &proc_argv) != TCL_OK)
                        continue;
                    if (strcmp(proc_argv[0], Tcl_GetString(argv[2]))) {
                        Tcl_DStringStartSublist(&ds);
                        Tcl_DStringAppendElement(&ds, proc_argv[0]);
                        Tcl_DStringAppendElement(&ds, proc_argv[1]);
                        Tcl_DStringEndSublist(&ds);
                    } else
                        DeleteInternalProc(proc_argv[1]);
                    Tcl_Free((char *) proc_argv);
                }
                Tcl_Free((char *) list_argv);
            }
        }

        Tcl_Free(procList);

        if (ds.length) {
            procList = StrDup(ds.string, &dummy);
            Tcl_SetHashValue(entry, procList);
        } else {
            Tcl_DeleteHashEntry(entry);
        }

        if (!ds.length) {
            if ((strncmp(token, "XC_", 3) == 0) || (strcmp(token, "CHAT") == 0)) {
                for (index = 0; index < XC_SIZE; index++) {
                    if (strcmp(xc[index].event, token) == 0) {
                        if (xc[index].hook != NULL) {
                            hexchat_unhook(ph, xc[index].hook);
                            xc[index].hook = NULL;
                            break;
                        }
                    }
                }
            }
        }
    }

    Tcl_Free(token);
    Tcl_DStringFree(&ds);

    return TCL_OK;
}

static int tcl_alias(ClientData cd, Tcl_Interp * irp, int argc, Tcl_Obj *const argv[])
{
    int newentry;
    alias *aliasPtr;
    Tcl_HashEntry *entry;
    char *string;
    const char *help = NULL;
    int dummy;
    int id;

    BADARGS(3, 4, " name ?help? {script | procname ?args?}");

    string = StrDup(Tcl_GetString(argv[1]), &dummy);
    Tcl_UtfToUpper(string);

    if (Tcl_GetCharLength(argv[argc - 1])) {

        if (argc == 4)
            help = Tcl_GetString(argv[2]);

        id = (nextprocid++ % INT_MAX) + 1;

        if (SourceInternalProc(id, "_cmd _rest", Tcl_GetString(argv[argc - 1])) == TCL_ERROR) {
            hexchat_printf(ph, "\0039Tcl plugin\003\tERROR (alias %s) ", Tcl_GetString(argv[1]));
            NiceErrorInfo ();
            return TCL_OK;
        }

        entry = Tcl_CreateHashEntry(&aliasTablePtr, string, &newentry);
        if (newentry) {
            aliasPtr = (alias *) Tcl_Alloc(sizeof(alias));
            if (string[0] == '@')
                aliasPtr->hook = NULL;
            else
                aliasPtr->hook = hexchat_hook_command(ph, string, HEXCHAT_PRI_NORM, Command_Alias, help, 0);
        } else {
            aliasPtr = Tcl_GetHashValue(entry);
            DeleteInternalProc(aliasPtr->procPtr);
            Tcl_Free(aliasPtr->procPtr);
        }

        aliasPtr->procPtr = StrDup(InternalProcName(id), &dummy);

        Tcl_SetHashValue(entry, aliasPtr);

    } else {

        if ((entry = Tcl_FindHashEntry(&aliasTablePtr, string)) != NULL) {
            aliasPtr = Tcl_GetHashValue(entry);
            DeleteInternalProc(aliasPtr->procPtr);
            Tcl_Free(aliasPtr->procPtr);
            if (aliasPtr->hook)
                hexchat_unhook(ph, aliasPtr->hook);
            Tcl_Free((char *) aliasPtr);
            Tcl_DeleteHashEntry(entry);
        }
    }

    Tcl_Free(string);

    return TCL_OK;
}

static int tcl_complete(ClientData cd, Tcl_Interp * irp, int argc, Tcl_Obj *const argv[])
{
    BADARGS(1, 2, " ?EAT_NONE|EAT_XCHAT|EAT_PLUGIN|EAT_ALL?");

    if (argc == 2) {
        if (Tcl_GetIntFromObj(NULL, argv[1], &complete[complete_level].result) != TCL_OK) {
            const char* arg = Tcl_GetString(argv[1]);
            if (strcasecmp("EAT_NONE", arg) == 0)
                complete[complete_level].result = HEXCHAT_EAT_NONE;
            else if (strcasecmp("EAT_XCHAT", arg) == 0)
                complete[complete_level].result = HEXCHAT_EAT_HEXCHAT;
            else if (strcasecmp("EAT_PLUGIN", arg) == 0)
                complete[complete_level].result = HEXCHAT_EAT_PLUGIN;
            else if (strcasecmp("EAT_ALL", arg) == 0)
                complete[complete_level].result = HEXCHAT_EAT_ALL;
            else
                BADARGS(1, 2, " ?EAT_NONE|EAT_XCHAT|EAT_PLUGIN|EAT_ALL?");
        }
    } else
        complete[complete_level].result = complete[complete_level].defresult;

    return TCL_RETURN;
}

static int tcl_command(ClientData cd, Tcl_Interp * irp, int argc, Tcl_Obj *const argv[])
{
    hexchat_context *origctx;
    hexchat_context *ctx = NULL;
    const char *string = NULL;

    BADARGS(2, 4, " ?server|network|context? ?#channel|nick? text");

    origctx = hexchat_get_context(ph);

    switch (argc) {
    case 2:
        ctx = origctx;
        break;
    case 3:
        ctx = xchat_smart_context(argv[1], NULL);
        break;
    case 4:
        ctx = xchat_smart_context(argv[1], argv[2]);
        break;
    default:;
    }

    CHECKCTX(ctx);

    string = Tcl_GetString(argv[argc - 1]);

    if (string[0] == '/')
        string++;

    hexchat_set_context(ph, ctx);
    hexchat_command(ph, string);
    hexchat_set_context(ph, origctx);

    return TCL_OK;
}

static int tcl_raw(ClientData cd, Tcl_Interp * irp, int argc, Tcl_Obj *const argv[])
{
    hexchat_context *origctx;
    hexchat_context *ctx = NULL;
    const char *string = NULL;

    BADARGS(2, 4, " ?server|network|context? ?#channel|nick? text");

    origctx = hexchat_get_context(ph);

    switch (argc) {
    case 2:
        ctx = origctx;
        break;
    case 3:
        ctx = xchat_smart_context(argv[1], NULL);
        break;
    case 4:
        ctx = xchat_smart_context(argv[1], argv[2]);
        break;
    default:;
    }

    CHECKCTX(ctx);

    string = Tcl_GetString(argv[argc - 1]);

    hexchat_set_context(ph, ctx);
    hexchat_commandf(ph, "RAW %s", string);
    hexchat_set_context(ph, origctx);

    return TCL_OK;
}


static int tcl_prefs(ClientData cd, Tcl_Interp * irp, int argc, Tcl_Obj *const argv[])
{
    int i;
    const char *str;

    BADARGS(2, 2, " name");

    switch (hexchat_get_prefs (ph, Tcl_GetString(argv[1]), &str, &i)) {
        case 1:
            Tcl_SetObjResult(irp, Tcl_NewStringObj(str, -1));
            break;
        case 2:
        case 3:
            Tcl_SetObjResult(irp, Tcl_NewIntObj(i));
            break;
        default:
            Tcl_AppendResult(irp, NULL);
    }

    return TCL_OK;
}

static int tcl_info(ClientData cd, Tcl_Interp * irp, int argc, Tcl_Obj *const argv[], char *id)
{
    const char *result;
    int max_argc;
    hexchat_context *origctx, *ctx;

    if (id == NULL) {
        BADARGS(2, 3, " ?server|network|context? id");
        max_argc = 3;
    } else {
        BADARGS(1, 2, " ?server|network|context?");
        max_argc = 2;
    }

    origctx = hexchat_get_context(ph);

    if (argc == max_argc) {
        ctx = xchat_smart_context(argv[1], NULL);
        CHECKCTX(ctx);
        hexchat_set_context(ph, ctx);
    }

    if (id == NULL)
      id = Tcl_GetString(argv[argc-1]);

    if ((result = hexchat_get_info(ph, id)) == NULL)
        result = "";

    if (strcasecmp(id, "win_ptr")) {
        Tcl_SetObjResult(irp, Tcl_NewWideIntObj((Tcl_WideInt)result));
    }
    else {
        Tcl_AppendResult(irp, result, NULL);
    }

    hexchat_set_context(ph, origctx);

    return TCL_OK;
}

static int tcl_me(ClientData cd, Tcl_Interp * irp, int argc, Tcl_Obj *const argv[])
{
    return tcl_info(cd, irp, argc, argv, "nick");
}

static int tcl_getinfo(ClientData cd, Tcl_Interp * irp, int argc, Tcl_Obj *const argv[])
{
    return tcl_info(cd, irp, argc, argv, NULL);
}

static int tcl_getlist(ClientData cd, Tcl_Interp * irp, int argc, Tcl_Obj *const argv[])
{
    hexchat_list *list = NULL;
    const char *name = NULL;
    const char * const *fields;
    const char *field;
    const char *sattr;
    int iattr;
    int i;
    time_t t;
    hexchat_context *origctx;
    hexchat_context *ctx = NULL;

    origctx = hexchat_get_context(ph);

    BADARGS(1, 2, " list");
    Tcl_Obj* result = Tcl_NewListObj(0, NULL);

    fields = hexchat_list_fields(ph, "lists");

    if (argc == 1) {
        for (i = 0; fields[i] != NULL; i++) {
            Tcl_ListObjAppendElement(irp, result, Tcl_NewStringObj(fields[i], -1));            
        }
        goto done;
    }

    for (i = 0; fields[i] != NULL; i++) {
        if (strcmp(fields[i], Tcl_GetString(argv[1])) == 0) {
            name = fields[i];
            break;
        }
    }

    if (name == NULL)
        goto done;

    list = hexchat_list_get(ph, name);
    if (list == NULL)
        goto done;

    fields = hexchat_list_fields(ph, name);

    Tcl_Obj* sublist = Tcl_NewListObj(0, NULL);
    for (i = 0; fields[i] != NULL; i++) {
        field = fields[i] + 1;
        Tcl_ListObjAppendElement(irp, sublist, Tcl_NewStringObj(field, -1));
    }
    Tcl_ListObjAppendElement(irp, result, sublist);

    while (hexchat_list_next(ph, list)) {

        sublist = Tcl_NewListObj(0, NULL);

        for (i = 0; fields[i] != NULL; i++) {

            field = fields[i] + 1;

            switch (fields[i][0]) {
            case 's':
                sattr = hexchat_list_str(ph, list, (char *) field);
                Tcl_ListObjAppendElement(irp, sublist, Tcl_NewStringObj(sattr, -1));
                break;
            case 'i':
                iattr = hexchat_list_int(ph, list, (char *) field);
                Tcl_ListObjAppendElement(irp, sublist, Tcl_NewIntObj(iattr));
                break;
            case 't':
                t = hexchat_list_time(ph, list, (char *) field);
                Tcl_ListObjAppendElement(irp, sublist, Tcl_NewWideIntObj((Tcl_WideInt)t));
                break;
            case 'p':
                sattr = hexchat_list_str(ph, list, (char *) field);
                Tcl_ListObjAppendElement(irp, sublist, Tcl_NewWideIntObj((Tcl_WideInt)sattr));
                break;
            default:
                Tcl_ListObjAppendElement(irp, sublist, Tcl_NewStringObj("*", -1));
                break;
            }
        }

        Tcl_ListObjAppendElement(irp, result, sublist);

    }

    hexchat_list_free(ph, list);

  done:

    hexchat_set_context(ph, origctx);

    Tcl_SetObjResult(irp, result);

    return TCL_OK;
}

/*
 * tcl_xchat_puts - stub for tcl puts command
 * This is modified from the original internal "puts" command.  It redirects
 * stdout to the current context, while still allowing all normal puts features
 */
// TODO: REPLACE with an custom channel
static int tcl_xchat_puts(ClientData cd, Tcl_Interp * irp, int argc, Tcl_Obj *const argv[])
{
    Tcl_Channel chan;
    Tcl_Obj *string;
    int newline;
    Tcl_Obj *channelId = NULL;
    int result;
    int mode;
    int trap_stdout = 0;

    switch (argc) {

    case 2:
        string = argv[1];
        newline = 1;
        trap_stdout = 1;
        break;

    case 3:
        if (strcmp(Tcl_GetString(argv[1]), "-nonewline") == 0) {
            newline = 0;
            trap_stdout = 1;
        } else {
            newline = 1;
            channelId = argv[1];
        }
        string = argv[2];
        break;

    case 4:
        if (strcmp(Tcl_GetString(argv[1]), "-nonewline") == 0) {
            channelId = argv[2];
            string = argv[3];
        } else {
            if (strcmp(Tcl_GetString(argv[3]), "nonewline") != 0) {
                Tcl_AppendResult(interp, "bad argument \"", Tcl_GetString(argv[3]), "\": should be \"nonewline\"", (char *) NULL);
                return TCL_ERROR;
            }
            channelId = argv[1];
            string = argv[2];
        }
        newline = 0;
        break;

    default:
        Tcl_AppendResult(interp, argv, "?-nonewline? ?channelId? string", NULL);
        return TCL_ERROR;
    }

    if (!trap_stdout && (strcmp(Tcl_GetString(channelId), "stdout") == 0))
        trap_stdout = 1;

    if (trap_stdout) {
        if (newline)
            hexchat_printf(ph, "%s\n", Tcl_GetString(string));
        else
            hexchat_print(ph, Tcl_GetString(string));
        return TCL_OK;
    }

    chan = Tcl_GetChannel(interp, Tcl_GetString(channelId), &mode);
    if (chan == (Tcl_Channel) NULL) {
        return TCL_ERROR;
    }
    if ((mode & TCL_WRITABLE) == 0) {
        Tcl_AppendResult(interp, "channel \"", Tcl_GetString(channelId), "\" wasn't opened for writing", (char *) NULL);
        return TCL_ERROR;
    }

    result = Tcl_WriteObj(chan, string);
    if (result < 0) {
        goto error;
    }
    if (newline != 0) {
        result = Tcl_WriteChars(chan, "\n", 1);
        if (result < 0) {
            goto error;
        }
    }
    return TCL_OK;

  error:
    Tcl_AppendResult(interp, "error writing \"", channelId, "\": ", Tcl_PosixError(interp), (char *) NULL);

    return TCL_ERROR;
}

static int tcl_print(ClientData cd, Tcl_Interp * irp, int argc, Tcl_Obj *const argv[])
{
    hexchat_context *origctx;
    hexchat_context *ctx = NULL;
    const char *string = NULL;

    BADARGS(2, 4, " ?server|network|context? ?#channel|nick? text");

    origctx = hexchat_get_context(ph);

    switch (argc) {
    case 2:
        ctx = origctx;
        break;
    case 3:
        ctx = xchat_smart_context(argv[1], NULL);
        break;
    case 4:
        ctx = xchat_smart_context(argv[1], argv[2]);
        break;
    default:;
    }

    CHECKCTX(ctx);

    string = Tcl_GetString(argv[argc - 1]);

    hexchat_set_context(ph, ctx);
    hexchat_print(ph, string);
    hexchat_set_context(ph, origctx);

    return TCL_OK;
}

static int tcl_setcontext(ClientData cd, Tcl_Interp * irp, int argc, Tcl_Obj *const argv[])
{
    hexchat_context *ctx = NULL;

    BADARGS(2, 2, " context");

    ctx = xchat_smart_context(argv[1], NULL);

    CHECKCTX(ctx);

    hexchat_set_context(ph, ctx);

    return TCL_OK;
}

static int tcl_findcontext(ClientData cd, Tcl_Interp * irp, int argc, Tcl_Obj *const argv[])
{
    hexchat_context *ctx = NULL;

    BADARGS(1, 3, " ?server|network|context? ?channel?");

    switch (argc) {
    case 1:
        ctx = hexchat_find_context(ph, NULL, NULL);
        break;
    case 2:
        ctx = xchat_smart_context(argv[1], NULL);
        break;
    case 3:
        ctx = xchat_smart_context(argv[1], argv[2]);
        break;
    default:;
    }

    CHECKCTX(ctx);

    Tcl_SetObjResult(irp, Tcl_NewWideIntObj((Tcl_WideInt)ctx));

    return TCL_OK;
}

static int tcl_getcontext(ClientData cd, Tcl_Interp * irp, int argc, Tcl_Obj *const argv[])
{
    hexchat_context *ctx = NULL;

    BADARGS(1, 1, "");

    ctx = hexchat_get_context(ph);

    Tcl_SetObjResult(irp, Tcl_NewWideIntObj((Tcl_WideInt)ctx));

    return TCL_OK;
}

static int tcl_channels(ClientData cd, Tcl_Interp * irp, int argc, Tcl_Obj *const argv[])
{
    const char *server, *channel;
    hexchat_list *list;
    Tcl_Obj* result;
    hexchat_context *origctx;
    hexchat_context *ctx;

    origctx = hexchat_get_context(ph);

    BADARGS(1, 2, " ?server|network|context?");

    if (argc == 2) {
        ctx = xchat_smart_context(argv[1], NULL);
        CHECKCTX(ctx);
        hexchat_set_context(ph, ctx);
    }

    server = (char *) hexchat_get_info(ph, "server");

    result = Tcl_NewListObj(0, NULL);

    list = hexchat_list_get(ph, "channels");

    if (list != NULL) {
        while (hexchat_list_next(ph, list)) {
            if (hexchat_list_int(ph, list, "type") != 2)
                continue;
            if (strcasecmp(server, hexchat_list_str(ph, list, "server")) != 0)
                continue;
            channel = hexchat_list_str(ph, list, "channel");
            Tcl_ListObjAppendElement(irp, result, Tcl_NewStringObj(channel, -1));
        }
        hexchat_list_free(ph, list);
    }

    Tcl_SetObjResult(irp, result);

    hexchat_set_context(ph, origctx);

    return TCL_OK;
}

static int tcl_servers(ClientData cd, Tcl_Interp * irp, int argc, Tcl_Obj *const argv[])
{
    const char *server;
    hexchat_list *list;
    Tcl_Obj* result;

    BADARGS(1, 1, "");

    result = Tcl_NewListObj(0, NULL);

    list = hexchat_list_get(ph, "channels");

    if (list != NULL) {
        while (hexchat_list_next(ph, list)) {
            if (hexchat_list_int(ph, list, "type") == 1) {
                server = hexchat_list_str(ph, list, "server");
                Tcl_ListObjAppendElement(irp, result, Tcl_NewStringObj(server, -1));
            }
        }
        hexchat_list_free(ph, list);
    }

    Tcl_SetObjResult(irp, result);

    return TCL_OK;
}

static int tcl_queries(ClientData cd, Tcl_Interp * irp, int argc, Tcl_Obj *const argv[])
{
    const char *server, *channel;
    hexchat_list *list;
    Tcl_Obj* result;
    hexchat_context *origctx;
    hexchat_context *ctx;

    origctx = hexchat_get_context(ph);

    BADARGS(1, 2, " ?server|network|context?");

    if (argc == 2) {
        ctx = xchat_smart_context(argv[1], NULL);
        CHECKCTX(ctx);
        hexchat_set_context(ph, ctx);
    }

    server = (char *) hexchat_get_info(ph, "server");

    result = Tcl_NewListObj(0, NULL);

    list = hexchat_list_get(ph, "channels");

    if (list != NULL) {
        while (hexchat_list_next(ph, list)) {
            if (hexchat_list_int(ph, list, "type") != 3)
                continue;
            if (strcasecmp(server, hexchat_list_str(ph, list, "server")) != 0)
                continue;
            channel = hexchat_list_str(ph, list, "channel");
            Tcl_ListObjAppendElement(irp, result, Tcl_NewStringObj(channel, -1));
        }
        hexchat_list_free(ph, list);
    }

    Tcl_SetObjResult(irp, result);

    hexchat_set_context(ph, origctx);

    return TCL_OK;
}

static int tcl_users(ClientData cd, Tcl_Interp * irp, int argc, Tcl_Obj *const argv[])
{
    hexchat_context *origctx, *ctx = NULL;
    hexchat_list *list;
    Tcl_Obj* result;

    BADARGS(1, 3, " ?server|network|context? ?channel?");

    origctx = hexchat_get_context(ph);

    switch (argc) {
    case 1:
        ctx = origctx;
        break;
    case 2:
        ctx = xchat_smart_context(argv[1], NULL);
        break;
    case 3:
        ctx = xchat_smart_context(argv[1], argv[2]);
        break;
    default:;
    }

    CHECKCTX(ctx);

    hexchat_set_context(ph, ctx);

    result = Tcl_NewListObj(0, NULL);

    list = hexchat_list_get(ph, "users");

    if (list != NULL) {

        Tcl_Obj* sublist = Tcl_NewListObj(6, NULL);
        
        Tcl_ListObjAppendElement(irp, sublist, Tcl_NewStringObj("nick", -1));
        Tcl_ListObjAppendElement(irp, sublist, Tcl_NewStringObj("host", -1));
        Tcl_ListObjAppendElement(irp, sublist, Tcl_NewStringObj("prefix", -1));
        Tcl_ListObjAppendElement(irp, sublist, Tcl_NewStringObj("away", -1));
        Tcl_ListObjAppendElement(irp, sublist, Tcl_NewStringObj("lasttalk", -1));
        Tcl_ListObjAppendElement(irp, sublist, Tcl_NewStringObj("selected", -1));
        Tcl_ListObjAppendElement(irp, result, sublist);

        while (hexchat_list_next(ph, list)) {
            sublist = Tcl_NewListObj(6, NULL);
            Tcl_ListObjAppendElement(irp, sublist, Tcl_NewStringObj(hexchat_list_str(ph, list, "nick"), -1));
            Tcl_ListObjAppendElement(irp, sublist, Tcl_NewStringObj(hexchat_list_str(ph, list, "host"), -1));
            Tcl_ListObjAppendElement(irp, sublist, Tcl_NewStringObj(hexchat_list_str(ph, list, "prefix"), -1));
            Tcl_ListObjAppendElement(irp, sublist, Tcl_NewIntObj(hexchat_list_int(ph, list, "away")));
            Tcl_ListObjAppendElement(irp, sublist, Tcl_NewWideIntObj(hexchat_list_time(ph, list, "lasttalk")));
            Tcl_ListObjAppendElement(irp, sublist, Tcl_NewIntObj(hexchat_list_int(ph, list, "selected")));
            Tcl_ListObjAppendElement(irp, result, sublist);
        }

        hexchat_list_free(ph, list);
    }

    Tcl_SetObjResult(irp, result);

    hexchat_set_context(ph, origctx);

    return TCL_OK;
}

static int tcl_notifylist(ClientData cd, Tcl_Interp * irp, int argc, Tcl_Obj *const argv[])
{
    hexchat_list *list;
    Tcl_Obj* result;

    BADARGS(1, 1, "");

    result = Tcl_NewListObj(0, NULL);

    list = hexchat_list_get(ph, "notify");

    if (list != NULL) {

        Tcl_Obj* sublist = Tcl_NewListObj(6, NULL);
        Tcl_ListObjAppendElement(irp, sublist, Tcl_NewStringObj("nick", -1));
        Tcl_ListObjAppendElement(irp, sublist, Tcl_NewStringObj("flags", -1));
        Tcl_ListObjAppendElement(irp, sublist, Tcl_NewStringObj("on", -1));
        Tcl_ListObjAppendElement(irp, sublist, Tcl_NewStringObj("off", -1));
        Tcl_ListObjAppendElement(irp, sublist, Tcl_NewStringObj("seen", -1));
        Tcl_ListObjAppendElement(irp, sublist, Tcl_NewStringObj("networks", -1));
        Tcl_ListObjAppendElement(irp, result, sublist);

        while (hexchat_list_next(ph, list)) {
            sublist = Tcl_NewListObj(6, NULL);
            Tcl_ListObjAppendElement(irp, sublist, Tcl_NewStringObj(hexchat_list_str(ph, list, "nick"), -1));
            Tcl_ListObjAppendElement(irp, sublist, Tcl_NewStringObj(hexchat_list_str(ph, list, "flags"), -1));
            Tcl_ListObjAppendElement(irp, sublist, Tcl_NewWideIntObj(hexchat_list_time(ph, list, "on")));
            Tcl_ListObjAppendElement(irp, sublist, Tcl_NewWideIntObj(hexchat_list_time(ph, list, "off")));
            Tcl_ListObjAppendElement(irp, sublist, Tcl_NewWideIntObj(hexchat_list_time(ph, list, "seen")));
            Tcl_ListObjAppendElement(irp, sublist, Tcl_NewStringObj(hexchat_list_str(ph, list, "networks"), -1));
            Tcl_ListObjAppendElement(irp, result, sublist);
        }

        hexchat_list_free(ph, list);

    }

    Tcl_SetObjResult(irp, result);

    return TCL_OK;
}

static int tcl_chats(ClientData cd, Tcl_Interp * irp, int argc, Tcl_Obj *const argv[])
{
    hexchat_list *list;
    Tcl_Obj* result;

    BADARGS(1, 1, "");

    result = Tcl_NewListObj(0, NULL);

    list = hexchat_list_get(ph, "dcc");

    if (list != NULL) {
        while (hexchat_list_next(ph, list)) {
            switch (hexchat_list_int(ph, list, "type")) {
            case 2:
            case 3:
                if (hexchat_list_int(ph, list, "status") == 1)
                    Tcl_ListObjAppendElement(irp, result, Tcl_NewStringObj(hexchat_list_str(ph, list, "nick"), -1));
                break;
            }
        }
        hexchat_list_free(ph, list);
    }

    Tcl_SetObjResult(irp, result);

    return TCL_OK;
}

static int tcl_ignores(ClientData cd, Tcl_Interp * irp, int argc, Tcl_Obj *const argv[])
{
    hexchat_list *list;
    Tcl_Obj* result;
    int flags;

    BADARGS(1, 1, "");

    result = Tcl_NewListObj(0, NULL);

    list = hexchat_list_get(ph, "ignore");

    if (list != NULL) {

        while (hexchat_list_next(ph, list)) {
            Tcl_Obj* sublist = Tcl_NewListObj(2, NULL);
            Tcl_ListObjAppendElement(irp, sublist, Tcl_NewStringObj(hexchat_list_str(ph, list, "mask"), -1));
            Tcl_Obj* ssub = Tcl_NewListObj(0, NULL);
            flags = hexchat_list_int(ph, list, "flags");
            if (flags & 1)
                Tcl_ListObjAppendElement(irp, ssub, Tcl_NewStringObj("PRIVMSG", -1));
            if (flags & 2)
                Tcl_ListObjAppendElement(irp, ssub, Tcl_NewStringObj("NOTICE", -1));
            if (flags & 4)
                Tcl_ListObjAppendElement(irp, ssub, Tcl_NewStringObj("CHANNEL", -1));
            if (flags & 8)
                Tcl_ListObjAppendElement(irp, ssub, Tcl_NewStringObj("CTCP", -1));
            if (flags & 16)
                Tcl_ListObjAppendElement(irp, ssub, Tcl_NewStringObj("INVITE", -1));
            if (flags & 32)
                Tcl_ListObjAppendElement(irp, ssub, Tcl_NewStringObj("UNIGNORE", -1));
            if (flags & 64)
                Tcl_ListObjAppendElement(irp, ssub, Tcl_NewStringObj("NOSAVE", -1));
            Tcl_ListObjAppendElement(irp, sublist, ssub);
            Tcl_ListObjAppendElement(irp, result, sublist);
        }
        hexchat_list_free(ph, list);
    }

    Tcl_SetObjResult(irp, result);

    return TCL_OK;
}

static int tcl_dcclist(ClientData cd, Tcl_Interp * irp, int argc, Tcl_Obj *const argv[])
{
    hexchat_list *list;
    Tcl_Obj* result;
    int dcctype;

    BADARGS(1, 1, "");

    result = Tcl_NewListObj(0, NULL);

    list = hexchat_list_get(ph, "dcc");

    if (list != NULL) {

        while (hexchat_list_next(ph, list)) {
            Tcl_Obj* sublist = Tcl_NewListObj(10, NULL);
            dcctype = hexchat_list_int(ph, list, "type");
            switch (dcctype) {
            case 0:
                Tcl_ListObjAppendElement(irp, sublist, Tcl_NewStringObj("filesend", -1));
                break;
            case 1:
                Tcl_ListObjAppendElement(irp, sublist, Tcl_NewStringObj("filerecv", -1));
                break;
            case 2:
                Tcl_ListObjAppendElement(irp, sublist, Tcl_NewStringObj("chatrecv", -1));
                break;
            case 3:
                Tcl_ListObjAppendElement(irp, sublist, Tcl_NewStringObj("chatsend", -1));
                break;
            }
            switch (hexchat_list_int(ph, list, "status")) {
            case 0:
                Tcl_ListObjAppendElement(irp, sublist, Tcl_NewStringObj("queued", -1));
                break;
            case 1:
                Tcl_ListObjAppendElement(irp, sublist, Tcl_NewStringObj("active", -1));
                break;
            case 2:
                Tcl_ListObjAppendElement(irp, sublist, Tcl_NewStringObj("failed", -1));
                break;
            case 3:
                Tcl_ListObjAppendElement(irp, sublist, Tcl_NewStringObj("done", -1));
                break;
            case 4:
                Tcl_ListObjAppendElement(irp, sublist, Tcl_NewStringObj("connecting", -1));
                break;
            case 5:
                Tcl_ListObjAppendElement(irp, sublist, Tcl_NewStringObj("aborted", -1));
                break;
            }

            Tcl_ListObjAppendElement(irp, sublist, Tcl_NewStringObj(hexchat_list_str(ph, list, "nick"), -1));

            switch (dcctype) {
            case 0:
                Tcl_ListObjAppendElement(irp, sublist, Tcl_NewStringObj(hexchat_list_str(ph, list, "file"), -1));
                break;
            case 1:
                Tcl_ListObjAppendElement(irp, sublist, Tcl_NewStringObj(hexchat_list_str(ph, list, "destfile"), -1));
                break;
            }

            Tcl_ListObjAppendElement(irp, sublist, Tcl_NewIntObj(hexchat_list_int(ph, list, "size")));
            Tcl_ListObjAppendElement(irp, sublist, Tcl_NewIntObj(hexchat_list_int(ph, list, "resume")));
            Tcl_ListObjAppendElement(irp, sublist, Tcl_NewIntObj(hexchat_list_int(ph, list, "pos")));
            Tcl_ListObjAppendElement(irp, sublist, Tcl_NewIntObj(hexchat_list_int(ph, list, "cps")));
            Tcl_ListObjAppendElement(irp, sublist, Tcl_NewIntObj(hexchat_list_int(ph, list, "address32")));
            Tcl_ListObjAppendElement(irp, sublist, Tcl_NewIntObj(hexchat_list_int(ph, list, "port")));
            Tcl_ListObjAppendElement(irp, result, sublist);
        }
        hexchat_list_free(ph, list);
    }

    Tcl_SetObjResult(irp, result);

    return TCL_OK;
}


static int tcl_strip(ClientData cd, Tcl_Interp * irp, int argc, Tcl_Obj *const argv[])
{
    char *new_text;
    int flags = 1 | 2;

    BADARGS(2, 3, " text ?flags?");

    if (argc == 3) {
        if (Tcl_GetIntFromObj(irp, argv[2], &flags) != TCL_OK)
            return TCL_ERROR;
    }

    new_text = hexchat_strip(ph, Tcl_GetString(argv[1]), -1, flags);

    if(new_text) {
        Tcl_AppendResult(irp, new_text, NULL);
        hexchat_free(ph, new_text);
    }

    return TCL_OK;
}

static int tcl_topic(ClientData cd, Tcl_Interp * irp, int argc, Tcl_Obj *const argv[])
{
    hexchat_context *origctx, *ctx = NULL;
    BADARGS(1, 3, " ?server|network|context? ?channel?");

    origctx = hexchat_get_context(ph);

    switch (argc) {
    case 1:
        ctx = origctx;
        break;
    case 2:
        ctx = xchat_smart_context(argv[1], NULL);
        break;
    case 3:
        ctx = xchat_smart_context(argv[1], argv[2]);
        break;
    default:;
    }

    CHECKCTX(ctx);

    hexchat_set_context(ph, ctx);
    Tcl_AppendResult(irp, hexchat_get_info(ph, "topic"), NULL);
    hexchat_set_context(ph, origctx);

    return TCL_OK;
}

static int tcl_hexchat_nickcmp(ClientData cd, Tcl_Interp * irp, int argc, Tcl_Obj *const argv[])
{
    BADARGS(3, 3, " string1 string2");
    Tcl_SetObjResult(irp, Tcl_NewIntObj(hexchat_nickcmp(ph, Tcl_GetString(argv[1]), Tcl_GetString(argv[2]))));
    return TCL_OK;
}

static int tcl_word(ClientData cd, Tcl_Interp * irp, int argc, Tcl_Obj *const argv[])
{
    int index;

    BADARGS(2, 2, " index");

    if (Tcl_GetIntFromObj(irp, argv[1], &index) != TCL_OK)
        return TCL_ERROR;

    if (!index || (index > 31))
        Tcl_AppendResult(interp, "", NULL);
    else
        Tcl_AppendResult(interp, complete[complete_level].word[index], NULL);

    return TCL_OK;
}

static int tcl_word_eol(ClientData cd, Tcl_Interp * irp, int argc, Tcl_Obj *const argv[])
{
    int index;

    BADARGS(2, 2, " index");

    if (Tcl_GetIntFromObj(irp, argv[1], &index) != TCL_OK)
        return TCL_ERROR;

    if (!index || (index > 31))
        Tcl_AppendResult(interp, "", NULL);
    else
        Tcl_AppendResult(interp, complete[complete_level].word_eol[index], NULL);

    return TCL_OK;
}

static int Command_Alias(char *word[], char *word_eol[], void *userdata)
{
    alias *aliasPtr;
    Tcl_HashEntry *entry;
    hexchat_context *origctx;
    int dummy;
    char *string;

    if (complete_level == MAX_COMPLETES)
        return HEXCHAT_EAT_NONE;

    complete_level++;
    complete[complete_level].defresult = HEXCHAT_EAT_ALL;
    complete[complete_level].result = HEXCHAT_EAT_NONE;
    complete[complete_level].word = word;
	complete[complete_level].word_eol = word_eol;

    string = StrDup(word[1], &dummy);

    Tcl_UtfToUpper(string);

    if ((entry = Tcl_FindHashEntry(&aliasTablePtr, string)) != NULL) {
        aliasPtr = Tcl_GetHashValue(entry);
        origctx = hexchat_get_context(ph);
        if (EvalInternalProc(aliasPtr->procPtr, 2, string, word_eol[2]) == TCL_ERROR) {
            hexchat_printf(ph, "\0039Tcl plugin\003\tERROR (alias %s) ", string);
            NiceErrorInfo ();
        }
        hexchat_set_context(ph, origctx);
    }

    Tcl_Free(string);

    return complete[complete_level--].result;
}

static int Null_Command_Alias(char *word[], char *word_eol[], void *userdata)
{
    alias *aliasPtr;
    Tcl_HashEntry *entry;
    hexchat_context *origctx;
    int dummy;
    const char *channel;
    char *string;
    Tcl_DString ds;
    static int recurse = 0;

    if (recurse)
        return HEXCHAT_EAT_NONE;

    if (complete_level == MAX_COMPLETES)
        return HEXCHAT_EAT_NONE;

    complete_level++;
    complete[complete_level].defresult = HEXCHAT_EAT_ALL;
    complete[complete_level].result = HEXCHAT_EAT_NONE;
    complete[complete_level].word = word;
	complete[complete_level].word_eol = word_eol;

    recurse++;

    channel = hexchat_get_info(ph, "channel");
    Tcl_DStringInit(&ds);
    Tcl_DStringAppend(&ds, "@", 1);
    Tcl_DStringAppend(&ds, channel, -1);
    string = StrDup(ds.string, &dummy);
    Tcl_DStringFree(&ds);

    Tcl_UtfToUpper(string);

    if ((entry = Tcl_FindHashEntry(&aliasTablePtr, string)) != NULL) {
        aliasPtr = Tcl_GetHashValue(entry);
        origctx = hexchat_get_context(ph);
        if (EvalInternalProc(aliasPtr->procPtr, 2, string, word_eol[1]) == TCL_ERROR) {
            hexchat_printf(ph, "\0039Tcl plugin\003\tERROR (alias %s) ", string);
            NiceErrorInfo ();
        }
        hexchat_set_context(ph, origctx);
    }

    Tcl_Free(string);

    recurse--;

    return complete[complete_level--].result;
}

static int Command_TCL(char *word[], char *word_eol[], void *userdata)
{
    const char *errorInfo;

    complete_level++;
    complete[complete_level].word = word;
    complete[complete_level].word_eol = word_eol;

    if (Tcl_Eval(interp, word_eol[2]) == TCL_ERROR) {
        errorInfo = Tcl_GetVar(interp, "errorInfo", TCL_GLOBAL_ONLY);
        hexchat_printf(ph, "\0039Tcl plugin\003\tERROR: %s ", errorInfo);
    } else
        hexchat_printf(ph, "\0039Tcl plugin\003\tRESULT: %s ", Tcl_GetStringResult(interp));

    complete_level--;

    return HEXCHAT_EAT_ALL;
}

static int Command_Source(char *word[], char *word_eol[], void *userdata)
{
    const char *hexchatdir;
    Tcl_DString ds;
    struct stat dummy;
    size_t len;
    const char *errorInfo;

    if (!strlen(word_eol[2]))
        return HEXCHAT_EAT_NONE;

    complete_level++;
    complete[complete_level].word = word;
    complete[complete_level].word_eol = word_eol;

    len = strlen(word[2]);

    if (len > 4 && strcasecmp(".tcl", word[2] + len - 4) == 0) {

        hexchatdir = hexchat_get_info(ph, "configdir");

        Tcl_DStringInit(&ds);

        if (stat(word_eol[2], &dummy) == 0) {
            Tcl_DStringAppend(&ds, word_eol[2], -1);
        } else {
            if (!strchr(word_eol[2], '/')) {
                Tcl_DStringAppend(&ds, hexchatdir, -1);
                Tcl_DStringAppend(&ds, "/addons/", 8);
                Tcl_DStringAppend(&ds, word_eol[2], -1);
            }
        }

        if (Tcl_EvalFile(interp, ds.string) == TCL_ERROR) {
            errorInfo = Tcl_GetVar(interp, "errorInfo", TCL_GLOBAL_ONLY);
            hexchat_printf(ph, "\0039Tcl plugin\003\tERROR: %s ", errorInfo);
        } else
            hexchat_printf(ph, "\0039Tcl plugin\003\tSourced %s\n", ds.string);

        Tcl_DStringFree(&ds);

        complete_level--;

        return HEXCHAT_EAT_HEXCHAT;

    } else {
        complete_level--;
        return HEXCHAT_EAT_NONE;
    }

}

static int Command_Reloadall(char *word[], char *word_eol[], void *userdata)
{
    Tcl_Plugin_DeInit();
    Tcl_Plugin_Init();

    hexchat_print(ph, "\0039Tcl plugin\003\tRehashed\n");

    return HEXCHAT_EAT_ALL;
}

static int TCL_Event_Handler(void *userdata)
{
    Tcl_DoOneEvent(TCL_DONT_WAIT);
    do_timer();
    return 1;
}

static void Tcl_Plugin_Init()
{
    int x;
    const char *hexchatdir;

    interp = Tcl_CreateInterp();

    Tcl_FindExecutable(NULL);

    Tcl_Init(interp);

    nextprocid = 0x1000;

    Tcl_CreateObjCommand(interp, "alias", tcl_alias, NULL, NULL);
    Tcl_CreateObjCommand(interp, "channels", tcl_channels, NULL, NULL);
    Tcl_CreateObjCommand(interp, "chats", tcl_chats, NULL, NULL);
    Tcl_CreateObjCommand(interp, "command", tcl_command, NULL, NULL);
    Tcl_CreateObjCommand(interp, "complete", tcl_complete, NULL, NULL);
    Tcl_CreateObjCommand(interp, "dcclist", tcl_dcclist, NULL, NULL);
    Tcl_CreateObjCommand(interp, "notifylist", tcl_notifylist, NULL, NULL);
    Tcl_CreateObjCommand(interp, "findcontext", tcl_findcontext, NULL, NULL);
    Tcl_CreateObjCommand(interp, "getcontext", tcl_getcontext, NULL, NULL);
    Tcl_CreateObjCommand(interp, "getinfo", tcl_getinfo, NULL, NULL);
    Tcl_CreateObjCommand(interp, "getlist", tcl_getlist, NULL, NULL);
    Tcl_CreateObjCommand(interp, "ignores", tcl_ignores, NULL, NULL);
    Tcl_CreateObjCommand(interp, "killtimer", tcl_killtimer, NULL, NULL);
    Tcl_CreateObjCommand(interp, "me", tcl_me, NULL, NULL);
    Tcl_CreateObjCommand(interp, "on", tcl_on, NULL, NULL);
    Tcl_CreateObjCommand(interp, "off", tcl_off, NULL, NULL);
    Tcl_CreateObjCommand(interp, "nickcmp", tcl_hexchat_nickcmp, NULL, NULL);
    Tcl_CreateObjCommand(interp, "print", tcl_print, NULL, NULL);
    Tcl_CreateObjCommand(interp, "prefs", tcl_prefs, NULL, NULL);
    Tcl_CreateObjCommand(interp, "::puts", tcl_xchat_puts, NULL, NULL);
    Tcl_CreateObjCommand(interp, "queries", tcl_queries, NULL, NULL);
    Tcl_CreateObjCommand(interp, "raw", tcl_raw, NULL, NULL);
    Tcl_CreateObjCommand(interp, "servers", tcl_servers, NULL, NULL);
    Tcl_CreateObjCommand(interp, "setcontext", tcl_setcontext, NULL, NULL);
    Tcl_CreateObjCommand(interp, "strip", tcl_strip, NULL, NULL);
    Tcl_CreateObjCommand(interp, "timer", tcl_timer, NULL, NULL);
    Tcl_CreateObjCommand(interp, "timerexists", tcl_timerexists, NULL, NULL);
    Tcl_CreateObjCommand(interp, "timers", tcl_timers, NULL, NULL);
    Tcl_CreateObjCommand(interp, "topic", tcl_topic, NULL, NULL);
    Tcl_CreateObjCommand(interp, "users", tcl_users, NULL, NULL);
    Tcl_CreateObjCommand(interp, "word", tcl_word, NULL, NULL);
    Tcl_CreateObjCommand(interp, "word_eol", tcl_word_eol, NULL, NULL);

    Tcl_InitHashTable(&cmdTablePtr, TCL_STRING_KEYS);
    Tcl_InitHashTable(&aliasTablePtr, TCL_STRING_KEYS);

    bzero(timers, sizeof(timers));
    nexttimerid = 0;
    nexttimerindex = 0;

    for (x = 0; x < XC_SIZE; x++)
        xc[x].hook = NULL;

    hexchatdir = hexchat_get_info(ph, "configdir");

    if (Tcl_Eval(interp, unknown) == TCL_ERROR) {
        hexchat_printf(ph, "Error sourcing internal 'unknown' (%s)\n", Tcl_GetStringResult(interp));
    }

    if (Tcl_Eval(interp, inlinetcl) == TCL_ERROR) {
        hexchat_printf(ph, "Error sourcing internal 'inlinetcl' (%s)\n", Tcl_GetStringResult(interp));
    }

    if (Tcl_Eval(interp, sourcedirs) == TCL_ERROR) {
        hexchat_printf(ph, "Error sourcing internal 'sourcedirs' (%s)\n", Tcl_GetStringResult(interp));
    }

}

static void Tcl_Plugin_DeInit()
{
    int x;
    char *procPtr;
    alias *aliasPtr;
    Tcl_HashEntry *entry;
    Tcl_HashSearch search;

    /* Be sure to free all the memory for ON and ALIAS entries */

    entry = Tcl_FirstHashEntry(&cmdTablePtr, &search);
    while (entry != NULL) {
        procPtr = Tcl_GetHashValue(entry);
        Tcl_Free(procPtr);
        entry = Tcl_NextHashEntry(&search);
    }

    Tcl_DeleteHashTable(&cmdTablePtr);

    entry = Tcl_FirstHashEntry(&aliasTablePtr, &search);
    while (entry != NULL) {
        aliasPtr = Tcl_GetHashValue(entry);
        Tcl_Free(aliasPtr->procPtr);
        if (aliasPtr->hook)
            hexchat_unhook(ph, aliasPtr->hook);
        Tcl_Free((char *) aliasPtr);
        entry = Tcl_NextHashEntry(&search);
    }

    Tcl_DeleteHashTable(&aliasTablePtr);

    for (x = 1; x < MAX_TIMERS; x++) {
        if (timers[x].timerid) {
            timers[x].timerid = 0;
            if (timers[x].procPtr != NULL)
                Tcl_Free(timers[x].procPtr);
            timers[x].procPtr = NULL;
            break;
        }
    }

    for (x = 0; x < XC_SIZE; x++) {
        if (xc[x].hook != NULL) {
            hexchat_unhook(ph, xc[x].hook);
            xc[x].hook = NULL;
        }
    }

    Tcl_DeleteInterp(interp);
}

static void banner()
{
#if 0
    hexchat_printf(ph, "Tcl plugin for HexChat - Version %s\n", VERSION);
    hexchat_print(ph, "Copyright 2002-2012 Daniel P. Stasinski\n");
    hexchat_print(ph, "http://www.scriptkitties.com/tclplugin/\n");
#endif
}

int hexchat_plugin_init(hexchat_plugin * plugin_handle, char **plugin_name, char **plugin_desc, char **plugin_version, char *arg)
{
#ifdef WIN32
    HINSTANCE lib;
#endif

    strncpy(VERSION, &RCSID[19], 5);

    ph = plugin_handle;

#ifdef WIN32
    lib = LoadLibraryA(TCL_DLL);
    if (!lib) {
        hexchat_print(ph, "You must have ActiveTCL 8.5 installed in order to run Tcl scripts.\n" "http://www.activestate.com/activetcl/downloads\n" "Make sure Tcl's bin directory is in your PATH.\n");
        return 0;
    }
    FreeLibrary(lib);
#endif

    if (initialized != 0) {
        banner();
        hexchat_print(ph, "Tcl interface already loaded");
        reinit_tried++;
        return 0;
    }
    initialized = 1;

    *plugin_name = "Tcl";
    *plugin_desc = "Tcl scripting interface";
    *plugin_version = VERSION;

    Tcl_Plugin_Init();

    raw_line_hook = hexchat_hook_server(ph, "RAW LINE", HEXCHAT_PRI_NORM, Server_raw_line, NULL);
    Command_TCL_hook = hexchat_hook_command(ph, "tcl", HEXCHAT_PRI_NORM, Command_TCL, 0, 0);
    Command_Source_hook = hexchat_hook_command(ph, "source", HEXCHAT_PRI_NORM, Command_Source, 0, 0);
    Command_Reload_hook = hexchat_hook_command(ph, "reloadall", HEXCHAT_PRI_NORM, Command_Reloadall, 0, 0);
    Command_Load_hook = hexchat_hook_command(ph, "LOAD", HEXCHAT_PRI_NORM, Command_Source, 0, 0);
    Event_Handler_hook = hexchat_hook_timer(ph, 100, TCL_Event_Handler, 0);
    Null_Command_hook = hexchat_hook_command(ph, "", HEXCHAT_PRI_NORM, Null_Command_Alias, "", 0);

    banner();
    hexchat_print(ph, "Tcl interface loaded\n");

    return 1;                   /* return 1 for success */
}

int hexchat_plugin_deinit()
{
    if (reinit_tried) {
        reinit_tried--;
        return 1;
    }

    hexchat_unhook(ph, raw_line_hook);
    hexchat_unhook(ph, Command_TCL_hook);
    hexchat_unhook(ph, Command_Source_hook);
    hexchat_unhook(ph, Command_Reload_hook);
    hexchat_unhook(ph, Command_Load_hook);
    hexchat_unhook(ph, Event_Handler_hook);
    hexchat_unhook(ph, Null_Command_hook);

    Tcl_Plugin_DeInit();

    hexchat_print(ph, "Tcl interface unloaded\n");
    initialized = 0;

    return 1;
}

void hexchat_plugin_get_info(char **name, char **desc, char **version, void **reserved)
{
   strncpy(VERSION, &RCSID[19], 5);
   *name = "tclplugin";
   *desc = "Tcl plugin for HexChat";
   *version = VERSION;
   if (reserved)
      *reserved = NULL;
}

