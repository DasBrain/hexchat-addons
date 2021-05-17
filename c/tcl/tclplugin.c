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

static char RCSID[] = "$Id: tclplugin.c,v 2.0 2021/05/13 20:02:12 dasbrain Exp $";

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
#else
#include <unistd.h>
#endif

#include "hexchat-plugin.h"
#include "tclplugin.h"

#ifndef _DEBUG
static const char init_tcl[] = {
    "package provide hexchat [::hexchat::getinfo version]\n"
    "\n"
    "namespace eval ::hexchat {\n"
    "       namespace export command findcontext getcontext getinfo getlist list_fields nickcmp print prefs setcontext strip hook_command hook_server hook_print hook_timer unregister_hook emit_print\n"
    "       namespace export pluginpref_get pluginpref_set pluginpref_delete pluginpref_list\n"
    "\n"
    "       variable EAT_NONE 0\n"
    "       variable EAT_HEXCHAT 1\n"
    "       variable EAT_PLUGIN 2\n"
    "       variable EAT_ALL 3\n"
    "\n"
    "       variable msgprefix \"\\0039Tcl plugin\\003\\t\"\n"
    "\n"
    "}\n"
    "\n"
    "proc ::hexchat::UplevelError opt {\n"
    "       variable msgprefix\n"
    "       set ei [join [lrange [split [dict get $opt -errorinfo] \\n] 0 end-3] \\n]\n"
    "       ::hexchat::print \"${msgprefix}ERROR: $ei\"\n"
    "}\n"
    "\n"
    "proc ::hexchat::command_tcl {words words_eol} {\n"
    "       variable msgprefix\n"
    "       try {\n"
    "               ::hexchat::print \"${msgprefix}RESULT: [uplevel #0 [lindex $words_eol 2]]\"\n"
    "       } on error {- opt} {\n"
    "               UplevelError $opt\n"
    "       }\n"
    "       return $::hexchat::EAT_ALL\n"
    "}\n"
    "\n"
    "proc ::hexchat::command_source {words words_eol} {\n"
    "       if {[lindex $words_eol 2] eq {}} {\n"
    "               return $::hexchat::EAT_NONE\n"
    "       }\n"
    "       set file [lindex $words 2]\n"
    "       if {[string match *.tcl $file] || [string match *.tm $file]} {\n"
    "               if {![file exists $file]} {\n"
    "                       set file [file join [::hexchat::getinfo configdir] addons $file]\n"
    "               }\n"
    "               variable msgprefix\n"
    "               try {\n"
    "                       uplevel #0 [list source -encoding utf-8 $file]\n"
    "                       ::hexchat::print \"${msgprefix}Sourced $file\\n\"\n"
    "               } on error {- opt} {\n"
    "                       UplevelError $opt\n"
    "               }\n"
    "               return $::hexchat::EAT_ALL\n"
    "       } else {\n"
    "               return $::hexchat::EAT_NONE\n"
    "       }\n"
    "}\n"
    "\n"
    "set ::hexchat::command_tcl [::hexchat::hook_command \"tcl\" ::hexchat::command_tcl]\n"
    "set ::hexchat::command_source [::hexchat::hook_command \"source\" ::hexchat::command_source]\n"
    "set ::hexchat::command_load [::hexchat::hook_command \"LOAD\" ::hexchat::command_source]\n"
    "\n"
    "proc ::bgerror {msg} {\n"
    "       ::hexchat::print \"${::hexchat::msgprefix}${::errorInfo}\"\n"
    "}\n"
    "\n"
    "namespace eval ::hexchat::util {\n"
    "       namespace export withctx\n"
    "}\n"
    "\n"
    "proc ::hexchat::util::withctx {ctx script} {\n"
    "       set oldctx [::hexchat::getcontext]\n"
    "       ::hexchat::setcontext $ctx\n"
    "       catch {uplevel 1 $script} res opt\n"
    "       ::hexchat::setcontext $oldctx\n"
    "       dict incr opt -level\n"
    "       return -options $opt $res\n"
    "}\n"
    "\n"
    "apply {{} {\n"
    "       variable msgprefix\n"
    "       set files [lsort [glob -nocomplain -directory [::hexchat::getinfo configdir] addons/*.tcl]]\n"
    "       foreach first {init compat} {\n"
    "               set idx [lsearch -glob $files */${first}.tcl]\n"
    "               if {$idx != -1} {\n"
    "                       set f [lindex $files $idx]\n"
    "                       set files [lreplace $files $idx $idx]\n"
    "                       set files [linsert $files 0 $f]\n"
    "               }\n"
    "       }\n"
    "       foreach f $files {\n"
    "               try {\n"
    "                       uplevel #0 [list source -encoding utf-8 $f]\n"
    "                       ::hexchat::print \"${msgprefix}Sourced \\\"$f\\\"\"\n"
    "               } on error {- opt} {\n"
    "                       UplevelError $opt\n"
    "               }\n"
    "       }\n"
    "} ::hexchat}\n"
};
#endif

static char VERSION[16];

static int initialized = 0;
static int reinit_tried = 0;
static Tcl_Interp *interp = NULL;
static hexchat_plugin *ph;
static hexchat_hook *Command_Reload_hook;
static hexchat_hook *Event_Handler_hook;

static Tcl_HashTable hooks;
static Tcl_Obj* empty_obj;


static void RegisterHook(hexchat_hook* hook, Tcl_Obj* data) {
    int newentry = 0;
    Tcl_IncrRefCount(data);
    Tcl_HashEntry* entry = Tcl_CreateHashEntry(&hooks, hook, &newentry);
    if (!newentry) {
        // An element with that key already exists - strange.
        Tcl_DecrRefCount(Tcl_GetHashValue(entry));
    }
    Tcl_SetHashValue(entry, data);
}

static void UnregisterHookEntry(Tcl_HashEntry* entry) {
    hexchat_unhook(ph, Tcl_GetHashKey(&hooks, entry));
    Tcl_DecrRefCount(Tcl_GetHashValue(entry));
    Tcl_DeleteHashEntry(entry);
}

#define HEXCHAT_WORD_LENGTH 32
static Tcl_Obj* word2TclObj(char const* const word[]) {
    Tcl_Obj* result = Tcl_NewListObj(HEXCHAT_WORD_LENGTH - 1, NULL);
    Tcl_ListObjAppendElement(NULL, result, empty_obj);
    for (int i = 1; i < HEXCHAT_WORD_LENGTH; i++) {
        if (word[i] == NULL || word[i][0] == 0) {
            Tcl_ListObjAppendElement(NULL, result, empty_obj);
        } else {
            Tcl_ListObjAppendElement(NULL, result, Tcl_NewStringObj(word[i], -1));
        }
    }
    return result;
}

static int CallHook(Tcl_Obj* command, int default_return) {
    int result = Tcl_EvalObjEx(interp, command, TCL_EVAL_GLOBAL | TCL_EVAL_DIRECT);
    int hexchat_eat = default_return;
    if (result != TCL_OK) {
        Tcl_BackgroundException(interp, result);
        return hexchat_eat;
    }
    Tcl_Obj* returnData = Tcl_GetReturnOptions(interp, result);
    Tcl_Obj* eatOpt = Tcl_GetObjResult(interp);
    if (eatOpt != NULL) {
        result = Tcl_GetIntFromObj(interp, eatOpt, &hexchat_eat);
        if (result != TCL_OK) {
            if (Tcl_GetCharLength(eatOpt) == 0) {
                Tcl_SetResult(interp, "", NULL);
                return default_return;
            }
            Tcl_BackgroundException(interp, result);
            return hexchat_eat;
        }
    }
    return hexchat_eat;
}

static int CommandHook(char* word[], char* word_eol[], void* user_data) {
    Tcl_Obj* command = user_data;
    Tcl_Obj* objword = word2TclObj(word);
    Tcl_Obj* objword_eol = word2TclObj(word_eol);
    command = Tcl_DuplicateObj(command);
    Tcl_IncrRefCount(command);
    Tcl_ListObjAppendElement(interp, command, objword);
    Tcl_ListObjAppendElement(interp, command, objword_eol);
    int result = CallHook(command, HEXCHAT_EAT_ALL);
    Tcl_DecrRefCount(command);
    return result;
}

static int PrintHook(char* word[], void* user_data) {
    Tcl_Obj* command = user_data;
    Tcl_Obj* objword = word2TclObj(word);
    command = Tcl_DuplicateObj(command);
    Tcl_IncrRefCount(command);
    Tcl_ListObjAppendElement(interp, command, objword);
    int result = CallHook(command, HEXCHAT_EAT_NONE);
    Tcl_DecrRefCount(command);
    return result;
}

static int ServerHook(char* word[], char* word_eol[], void* user_data) {
    Tcl_Obj* command = user_data;
    Tcl_Obj* objword = word2TclObj(word);
    Tcl_Obj* objword_eol = word2TclObj(word_eol);
    command = Tcl_DuplicateObj(command);
    Tcl_IncrRefCount(command);
    Tcl_ListObjAppendElement(interp, command, objword);
    Tcl_ListObjAppendElement(interp, command, objword_eol);
    int result = CallHook(command, HEXCHAT_EAT_NONE);
    Tcl_DecrRefCount(command);
    return result;
}

static int TimerHook(void* user_data) {
    return CallHook(user_data, 0);
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



//{ -- TCL API


static int tcl_unregister_hook(ClientData cd, Tcl_Interp* irp, int argc, Tcl_Obj* const argv[]) {
    BADARGS(2, 2, "hookptr");

    Tcl_WideInt wint;
    if (Tcl_GetWideIntFromObj(irp, argv[1], &wint) != TCL_OK) {
        return TCL_ERROR;
    }

    Tcl_HashEntry* entry = Tcl_FindHashEntry(&hooks, wint);
    if (entry == NULL) {
        Tcl_AppendResult(irp, "Invalid hook pointer passed");
        return TCL_ERROR;
    }

    UnregisterHookEntry(entry);
    return TCL_OK;
}

static int tcl_hook_command(ClientData cd, Tcl_Interp* irp, int argc, Tcl_Obj* const argv[]) {
    BADARGS(3, 7, "?-option ...? name tcl_command");

    char* name = Tcl_GetString(argv[argc - 2]);
    char* help = NULL;
    int priority = HEXCHAT_PRI_NORM;

    static const char* const options[] = {
        "-priority", "-help"
    };
    enum options { OPT_PRIORITY, OPT_HELP };
    if (argc > 3) {
        int i = 1;
        while (i + 2 < argc) {
            int opt = 0;
            if (Tcl_GetIndexFromObj(irp, argv[i], options, "option", 0, &opt) != TCL_OK) {
                return TCL_ERROR;
            }
            i++;
            switch (opt) {
            case OPT_PRIORITY:
                if (Tcl_GetIntFromObj(irp, argv[i], &priority) != TCL_OK) {
                    return TCL_ERROR;
                }
                i++;
                break;
            case OPT_HELP:
                help = Tcl_GetString(argv[i]);
                i++;
                break;
            }
        }
    }
    Tcl_Obj* command = argv[argc - 1];
    hexchat_hook* hook = hexchat_hook_command(ph, name, priority, CommandHook, help, command);
    RegisterHook(hook, command);
    Tcl_SetObjResult(irp, Tcl_NewWideIntObj((intptr_t)hook));
    return TCL_OK;
}

static int tcl_hook_print(ClientData cd, Tcl_Interp* irp, int argc, Tcl_Obj* const argv[]) {
    BADARGS(3, 5, "?-option ...? name tcl_command");

    char* name = Tcl_GetString(argv[argc - 2]);
    int priority = HEXCHAT_PRI_NORM;

    static const char* const options[] = {
        "-priority"
    };
    enum options { OPT_PRIORITY };
    if (argc > 3) {
        int i = 1;
        while (i + 2 < argc) {
            int opt = 0;
            if (Tcl_GetIndexFromObj(irp, argv[i], options, "option", 0, &opt) != TCL_OK) {
                return TCL_ERROR;
            }
            i++;
            switch (opt) {
            case OPT_PRIORITY:
                if (Tcl_GetIntFromObj(irp, argv[i], &priority) != TCL_OK) {
                    return TCL_ERROR;
                }
                i++;
                break;
            }
        }
    }
    Tcl_Obj* command = argv[argc - 1];

    hexchat_hook* hook = hexchat_hook_print(ph, name, priority, PrintHook, command);
    RegisterHook(hook, command);
    Tcl_SetObjResult(irp, Tcl_NewWideIntObj((intptr_t)hook));
    return TCL_OK;
}

static int tcl_hook_server(ClientData cd, Tcl_Interp* irp, int argc, Tcl_Obj* const argv[]) {
    BADARGS(3, 5, "?-option ...? name tcl_command");

    char* name = Tcl_GetString(argv[argc - 2]);
    int priority = HEXCHAT_PRI_NORM;

    static const char* const options[] = {
        "-priority"
    };
    enum options { OPT_PRIORITY };
    if (argc > 3) {
        int i = 1;
        while (i + 2 < argc) {
            int opt = 0;
            if (Tcl_GetIndexFromObj(irp, argv[i], options, "option", 0, &opt) != TCL_OK) {
                return TCL_ERROR;
            }
            i++;
            switch (opt) {
            case OPT_PRIORITY:
                if (Tcl_GetIntFromObj(irp, argv[i], &priority) != TCL_OK) {
                    return TCL_ERROR;
                }
                i++;
                break;
            }
        }
    }
    Tcl_Obj* command = argv[argc - 1];

    hexchat_hook* hook = hexchat_hook_server(ph, name, priority, ServerHook, command);
    RegisterHook(hook, command);
    Tcl_SetObjResult(irp, Tcl_NewWideIntObj((intptr_t)hook));
    return TCL_OK;
}

static int tcl_hook_timer(ClientData cd, Tcl_Interp* irp, int argc, Tcl_Obj* const argv[]) {
    BADARGS(3, 3, " timeout command");
    int timeout = -1;
    if (Tcl_GetIntFromObj(irp, argv[1], &timeout) != TCL_OK) {
        return TCL_ERROR;
    }
    Tcl_Obj* command = argv[2];
    hexchat_hook* hook = hexchat_hook_timer(ph, timeout, TimerHook, command);
    RegisterHook(hook, command);
    Tcl_SetObjResult(irp, Tcl_NewWideIntObj((intptr_t)hook));
    return TCL_OK;
}

static int tcl_command(ClientData cd, Tcl_Interp * irp, int argc, Tcl_Obj *const argv[]) {
    BADARGS(2, 2, " text");
    const char* string = Tcl_GetString(argv[1]);
    hexchat_command(ph, string);
    return TCL_OK;
}

static int tcl_prefs(ClientData cd, Tcl_Interp * irp, int argc, Tcl_Obj *const argv[])
{
    int i = 0;
    const char *str = NULL;

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

static int tcl_getinfo(ClientData cd, Tcl_Interp * irp, int argc, Tcl_Obj *const argv[])
{
    const char* result;

    BADARGS(2, 2, " id");

    char* id = Tcl_GetString(argv[1]);

    if ((result = hexchat_get_info(ph, id)) == NULL)
        result = "";

    if (strcasecmp(id, "win_ptr") == 0 || strcasecmp(id, "gtkwin_ptr") == 0) {
        Tcl_SetObjResult(irp, Tcl_NewWideIntObj((Tcl_WideInt)result));
    }
    else {
        Tcl_AppendResult(irp, result, NULL);
    }

    return TCL_OK;
}

static int tcl_list_fields(ClientData cd, Tcl_Interp* irp, int argc, Tcl_Obj* const argv[]) {
    hexchat_list* list = NULL;
    const char* name = NULL;
    const char* const* fields;
    const char* field;
    int i;

    BADARGS(1, 2, " list");
    Tcl_Obj* result = Tcl_NewListObj(0, NULL);

    fields = hexchat_list_fields(ph, "lists");

    if (argc == 1) {
        for (i = 0; fields[i] != NULL; i++) {
            Tcl_ListObjAppendElement(irp, result, Tcl_NewStringObj(fields[i], -1));
        }
        return TCL_OK;
    }

    for (i = 0; fields[i] != NULL; i++) {
        if (strcmp(fields[i], Tcl_GetString(argv[1])) == 0) {
            name = fields[i];
            break;
        }
    }

    if (name == NULL) {
        Tcl_AppendResult(irp, "list not found");
        return TCL_ERROR;
    }

    fields = hexchat_list_fields(ph, name);

    for (i = 0; fields[i] != NULL; i++) {
        field = fields[i] + 1;
        Tcl_ListObjAppendElement(irp, result, Tcl_NewStringObj(field, -1));
    }
    Tcl_SetObjResult(irp, result);
    return TCL_OK;
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

    BADARGS(1, 2, " list");
    Tcl_Obj* result = Tcl_NewListObj(0, NULL);

    fields = hexchat_list_fields(ph, "lists");

    if (argc == 1) {
        for (i = 0; fields[i] != NULL; i++) {
            Tcl_ListObjAppendElement(irp, result, Tcl_NewStringObj(fields[i], -1));            
        }
        return TCL_OK;
    }

    for (i = 0; fields[i] != NULL; i++) {
        if (strcmp(fields[i], Tcl_GetString(argv[1])) == 0) {
            name = fields[i];
            break;
        }
    }

    if (name == NULL) {
        Tcl_AppendResult(irp, "list not found");
        return TCL_ERROR;
    }

    list = hexchat_list_get(ph, name);
    if (list == NULL) {
        Tcl_AppendResult(irp, "invalid list");
        return TCL_ERROR;
    }

    fields = hexchat_list_fields(ph, name);

    while (hexchat_list_next(ph, list)) {

        Tcl_Obj* sublist = Tcl_NewListObj(0, NULL);

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
    Tcl_SetObjResult(irp, result);

    return TCL_OK;
}

static int tcl_print(ClientData cd, Tcl_Interp * irp, int argc, Tcl_Obj *const argv[])
{
    BADARGS(2, 2, " text");
    const char* string = Tcl_GetString(argv[argc - 1]);
    hexchat_print(ph, string);
    return TCL_OK;
}

static int tcl_setcontext(ClientData cd, Tcl_Interp * irp, int argc, Tcl_Obj *const argv[])
{
    hexchat_context *ctx = NULL;
    BADARGS(2, 2, " context");
    Tcl_WideInt wint = 0;
    if (Tcl_GetWideIntFromObj(irp, argv[1], &wint) != TCL_OK) {
        return TCL_ERROR;
    }
    ctx = (void*)(intptr_t)wint;
    
    hexchat_list* list = hexchat_list_get(ph, "channels");
    while (hexchat_list_next(ph, list)) {
        if ((void*)hexchat_list_str(ph, list, "context") == ctx) {
            hexchat_set_context(ph, ctx);
            return TCL_OK;
        }
    }

    Tcl_SetResult(irp, "Invalid context", TCL_STATIC);
    return TCL_ERROR;
}

static int tcl_findcontext(ClientData cd, Tcl_Interp * irp, int argc, Tcl_Obj *const argv[])
{
    hexchat_context *ctx = NULL;

    BADARGS(1, 3, " ?server|network? ?channel?");

    switch (argc) {
    case 1:
        ctx = hexchat_find_context(ph, NULL, NULL);
        break;
    case 2:
        ctx = hexchat_find_context(ph, Tcl_GetString(argv[1]), NULL);
        break;
    case 3:
        ctx = hexchat_find_context(ph, Tcl_GetString(argv[1]), Tcl_GetString(argv[2]));
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

static int tcl_hexchat_nickcmp(ClientData cd, Tcl_Interp * irp, int argc, Tcl_Obj *const argv[])
{
    BADARGS(3, 3, " string1 string2");
    Tcl_SetObjResult(irp, Tcl_NewIntObj(hexchat_nickcmp(ph, Tcl_GetString(argv[1]), Tcl_GetString(argv[2]))));
    return TCL_OK;
}

static int tcl_hexchat_pluginpref_get(ClientData cd, Tcl_Interp* irp, int argc, Tcl_Obj* const argv[]) {
    BADARGS(2, 3, "?-int? name");
    if (argc == 3) {
        if (strcmp(Tcl_GetString(argv[1]), "-int") == 0) {
            int result = hexchat_pluginpref_get_int(ph, Tcl_GetString(argv[2]));
            Tcl_SetObjResult(irp, Tcl_NewIntObj(result));
            return TCL_OK;
        } else {
            Tcl_WrongNumArgs(irp, 1, argv, "?-int? name");
            return TCL_ERROR;
        }
    } else {
        char buffer[512];
        int result = hexchat_pluginpref_get_str(ph, Tcl_GetString(argv[1]), buffer);
        if (result == 1) {
            Tcl_SetObjResult(irp, Tcl_NewStringObj(buffer, -1));
            return TCL_OK;
        } else {
            Tcl_SetResult(irp, "could not get plugin preference", TCL_STATIC);
            return TCL_ERROR;
        }
    }
}

static int tcl_hexchat_pluginpref_set(ClientData cd, Tcl_Interp* irp, int argc, Tcl_Obj* const argv[]) {
    BADARGS(3, 3, "name value");
    int result = hexchat_pluginpref_set_str(ph, Tcl_GetString(argv[1]), Tcl_GetString(argv[2]));
    if (result == 1) {
        return TCL_OK;
    } else {
        Tcl_SetResult(irp, "could not set plugin preference", TCL_STATIC);
        return TCL_ERROR;
    }
}

static int tcl_hexchat_pluginpref_delete(ClientData cd, Tcl_Interp* irp, int argc, Tcl_Obj* const argv[]) {
    BADARGS(2, 2, "name");
    int result = hexchat_pluginpref_delete(ph, Tcl_GetString(argv[1]));
    if (result == 1) {
        return TCL_OK;
    } else {
        Tcl_SetResult(irp, "could not delete plugin preference", TCL_STATIC);
        return TCL_ERROR;
    }
}

static int tcl_hexchat_pluginpref_list(ClientData cd, Tcl_Interp* irp, int argc, Tcl_Obj* const argv[]) {
    BADARGS(1, 1, "");
    char buffer[4096];
    int result = hexchat_pluginpref_list(ph, buffer);
    if (result == 1) {
        Tcl_SetObjResult(irp, Tcl_NewStringObj(buffer, -1));
        return TCL_OK;
    } else {
        Tcl_SetResult(irp, "could not list the plugin preferences", TCL_STATIC);
        return TCL_ERROR;
    }
}

static int tcl_hexchat_emit_print_helper(Tcl_Interp* irp, int result) {
    if (!result) {
        Tcl_SetResult(interp, "Could not emit print", TCL_STATIC);
        return TCL_ERROR;
    } else {
        return TCL_OK;
    }
}

static int tcl_hexchat_emit_print(ClientData cd, Tcl_Interp* irp, int argc, Tcl_Obj* const argv[]) {
    if (argc < 2) {
        Tcl_WrongNumArgs(irp, 1, argv, "event ?args...?");
        return TCL_ERROR;
    }
    switch (argc) {
        case 2: return tcl_hexchat_emit_print_helper(irp, hexchat_emit_print(ph, Tcl_GetString(argv[1]), NULL));
        case 3: return tcl_hexchat_emit_print_helper(irp, hexchat_emit_print(ph, Tcl_GetString(argv[1]), Tcl_GetString(argv[2]), NULL));
        case 4: return tcl_hexchat_emit_print_helper(irp, hexchat_emit_print(ph, Tcl_GetString(argv[1]), Tcl_GetString(argv[2]), Tcl_GetString(argv[3]), NULL));
        case 5: return tcl_hexchat_emit_print_helper(irp, hexchat_emit_print(ph, Tcl_GetString(argv[1]), Tcl_GetString(argv[2]), Tcl_GetString(argv[3]), Tcl_GetString(argv[4]), NULL));
        case 6: return tcl_hexchat_emit_print_helper(irp, hexchat_emit_print(ph, Tcl_GetString(argv[1]), Tcl_GetString(argv[2]), Tcl_GetString(argv[3]), Tcl_GetString(argv[4]), Tcl_GetString(argv[5]), NULL));
        case 7: return tcl_hexchat_emit_print_helper(irp, hexchat_emit_print(ph, Tcl_GetString(argv[1]), Tcl_GetString(argv[2]), Tcl_GetString(argv[3]), Tcl_GetString(argv[4]), Tcl_GetString(argv[5]), Tcl_GetString(argv[6]), NULL));
        case 8: return tcl_hexchat_emit_print_helper(irp, hexchat_emit_print(ph, Tcl_GetString(argv[1]), Tcl_GetString(argv[2]), Tcl_GetString(argv[3]), Tcl_GetString(argv[4]), Tcl_GetString(argv[5]), Tcl_GetString(argv[6]), Tcl_GetString(argv[7]), NULL));
        default: Tcl_SetResult(irp, "Only up to 6 aruments are supported for hexchat::emit_print", TCL_STATIC); return TCL_ERROR;
    }
    // Unreachable
}



//}

static int Command_Reloadall(char* word[], char* word_eol[], void* userdata);
static int TCL_Event_Handler(void *userdata)
{
    Tcl_DoOneEvent(TCL_DONT_WAIT);
    return 1;
}

static void Tcl_Plugin_Init()
{

    Tcl_FindExecutable(NULL);

    interp = Tcl_CreateInterp();

    Tcl_Init(interp);

    Tcl_CreateObjCommand(interp, "::hexchat::command", tcl_command, NULL, NULL);
    Tcl_CreateObjCommand(interp, "::hexchat::findcontext", tcl_findcontext, NULL, NULL);
    Tcl_CreateObjCommand(interp, "::hexchat::getcontext", tcl_getcontext, NULL, NULL);
    Tcl_CreateObjCommand(interp, "::hexchat::getinfo", tcl_getinfo, NULL, NULL);
    Tcl_CreateObjCommand(interp, "::hexchat::getlist", tcl_getlist, NULL, NULL);
    Tcl_CreateObjCommand(interp, "::hexchat::list_fields", tcl_list_fields, NULL, NULL);
    Tcl_CreateObjCommand(interp, "::hexchat::nickcmp", tcl_hexchat_nickcmp, NULL, NULL);
    Tcl_CreateObjCommand(interp, "::hexchat::print", tcl_print, NULL, NULL);
    Tcl_CreateObjCommand(interp, "::hexchat::prefs", tcl_prefs, NULL, NULL);
    Tcl_CreateObjCommand(interp, "::hexchat::setcontext", tcl_setcontext, NULL, NULL);
    Tcl_CreateObjCommand(interp, "::hexchat::strip", tcl_strip, NULL, NULL);
    Tcl_CreateObjCommand(interp, "::hexchat::hook_server", tcl_hook_server, NULL, NULL);
    Tcl_CreateObjCommand(interp, "::hexchat::hook_command", tcl_hook_command, NULL, NULL);
    Tcl_CreateObjCommand(interp, "::hexchat::hook_print", tcl_hook_print, NULL, NULL);
    Tcl_CreateObjCommand(interp, "::hexchat::hook_timer", tcl_hook_timer, NULL, NULL);
    Tcl_CreateObjCommand(interp, "::hexchat::unregister_hook", tcl_unregister_hook, NULL, NULL);
    Tcl_CreateObjCommand(interp, "::hexchat::pluginpref_get", tcl_hexchat_pluginpref_get, NULL, NULL);
    Tcl_CreateObjCommand(interp, "::hexchat::pluginpref_set", tcl_hexchat_pluginpref_set, NULL, NULL);
    Tcl_CreateObjCommand(interp, "::hexchat::pluginpref_delete", tcl_hexchat_pluginpref_delete, NULL, NULL);
    Tcl_CreateObjCommand(interp, "::hexchat::pluginpref_list", tcl_hexchat_pluginpref_list, NULL, NULL);
    Tcl_CreateObjCommand(interp, "::hexchat::emit_print", tcl_hexchat_emit_print, NULL, NULL);

    Tcl_InitHashTable(&hooks, TCL_ONE_WORD_KEYS);

    empty_obj = Tcl_NewObj();
    Tcl_IncrRefCount(empty_obj);

#ifdef _DEBUG
    Tcl_SetVar2(interp, "pluginsrc", NULL, __FILE__, TCL_GLOBAL_ONLY);
    if (Tcl_Eval(interp, "source [file join [file dirname $pluginsrc] init.tcl]") != TCL_OK) {
        hexchat_print(ph, Tcl_GetVar(interp, "errorInfo", 0));
    };
#else
    Tcl_Eval(interp, init_tcl);
#endif // DEBUG
}

static void Tcl_Plugin_DeInit()
{
    Tcl_HashEntry *entry;
    Tcl_HashSearch search;

    /* Be sure to free all the memory for ON and ALIAS entries */
    entry = Tcl_FirstHashEntry(&hooks, &search);
    while (entry != NULL) {
        UnregisterHookEntry(entry);
        entry = Tcl_NextHashEntry(&search);
    }
    Tcl_DeleteHashTable(&hooks);
    Tcl_DecrRefCount(empty_obj);

    Tcl_DeleteInterp(interp);
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
        hexchat_print(ph, "You must have Tcl 8.6 installed in order to run Tcl scripts.\n" "https://www.magicsplat.com/tcl-installer/index.html\n" "Make sure Tcl's bin directory is in your PATH.\n");
        return 0;
    }
    FreeLibrary(lib);
#endif

    if (initialized != 0) {
        hexchat_print(ph, "Tcl interface already loaded");
        reinit_tried++;
        return 0;
    }
    initialized = 1;

    *plugin_name = "Tcl";
    *plugin_desc = "Tcl scripting interface";
    *plugin_version = VERSION;

    Tcl_Plugin_Init();

    Command_Reload_hook = hexchat_hook_command(ph, "reloadall", HEXCHAT_PRI_NORM, Command_Reloadall, 0, 0);
    Event_Handler_hook = hexchat_hook_timer(ph, 100, TCL_Event_Handler, 0);

    hexchat_print(ph, "Tcl interface loaded\n");

    return 1;                   /* return 1 for success */
}

static int Command_Reloadall(char* word[], char* word_eol[], void* userdata) {
    Tcl_Plugin_DeInit();
    Tcl_Plugin_Init();

    hexchat_print(ph, "\0039Tcl plugin\003\tRehashed\n");

    return HEXCHAT_EAT_ALL;
}

int hexchat_plugin_deinit()
{
    if (reinit_tried) {
        reinit_tried--;
        return 1;
    }

    hexchat_unhook(ph, Command_Reload_hook);
    hexchat_unhook(ph, Event_Handler_hook);

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

