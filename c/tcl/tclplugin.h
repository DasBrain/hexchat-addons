/***************************************************************************
                           tclplugin.h  -  TCL plugin header file
                           -------------------------------------------------
    begin                : Sat Nov  9 17:31:20 MST 2002
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

#define BADARGS(nl,nh,example) \
    if ((argc<(nl)) || (argc>(nh))) { \
        Tcl_WrongNumArgs(irp, 1, argv, (example)); \
        return TCL_ERROR; \
    }

#define CHECKCTX(ctx) \
    if (ctx == NULL) { \
        Tcl_AppendResult(irp, "No such server/channel/nick", NULL); \
        return TCL_ERROR; \
    }

typedef struct {
    Tcl_Obj *procPtr;
    hexchat_hook *hook;
} alias;

typedef struct {
    int timerid;
    time_t timestamp;
    Tcl_Obj *procPtr;
    int count;
    int seconds;
} timer;

typedef struct {
    int result;
    int defresult;
    char **word;
    char **word_eol;
} t_complete;

#define MAX_TIMERS 512
#define MAX_COMPLETES 128

static hexchat_context* xchat_smart_context(Tcl_Obj *arg1, Tcl_Obj *arg2);
static int tcl_command(ClientData cd, Tcl_Interp * irp, int argc, Tcl_Obj *const argv[]);
static int tcl_print(ClientData cd, Tcl_Interp * irp, int argc, Tcl_Obj *const argv[]);
static int tcl_hexchat_nickcmp(ClientData cd, Tcl_Interp * irp, int argc, Tcl_Obj *const argv[]);
static int tcl_strip(ClientData cd, Tcl_Interp * irp, int argc, Tcl_Obj *const argv[]);
/* static int Command_Reload(char *word[], char *word_eol[], void *userdata); */
static int TCL_Event_Handler(void *userdata);
static void Tcl_Plugin_Init();
static void Tcl_Plugin_DeInit();
int hexchat_plugin_init(hexchat_plugin * plugin_handle, char **plugin_name, char **plugin_desc, char **plugin_version, char *arg);
int hexchat_plugin_deinit();
