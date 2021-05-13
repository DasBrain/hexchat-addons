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


int hexchat_plugin_init(hexchat_plugin * plugin_handle, char **plugin_name, char **plugin_desc, char **plugin_version, char *arg);
int hexchat_plugin_deinit();
