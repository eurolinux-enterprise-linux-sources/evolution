/*
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with the program; if not, see <http://www.gnu.org/licenses/>
 *
 *
 * Authors:
 *		Michel Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef __EM_MENU_H__
#define __EM_MENU_H__

#include <glib-object.h>
#include <camel/camel-folder.h>

#include "e-util/e-menu.h"

G_BEGIN_DECLS

typedef struct _EMMenu EMMenu;
typedef struct _EMMenuClass EMMenuClass;

/* Current target description */
/* Types of popup tagets */
enum _em_menu_target_t {
	EM_MENU_TARGET_SELECT,
	EM_MENU_TARGET_WIDGET
};

/* Flags that describe a TARGET_SELECT */
enum {
	EM_MENU_SELECT_ONE                = 1<<1,
	EM_MENU_SELECT_MANY               = 1<<2,
	EM_MENU_SELECT_MARK_READ          = 1<<3,
	EM_MENU_SELECT_MARK_UNREAD        = 1<<4,
	EM_MENU_SELECT_DELETE             = 1<<5,
	EM_MENU_SELECT_UNDELETE           = 1<<6,
	EM_MENU_SELECT_MAILING_LIST       = 1<<7,
	EM_MENU_SELECT_EDIT               = 1<<8,
	EM_MENU_SELECT_MARK_IMPORTANT     = 1<<9,
	EM_MENU_SELECT_MARK_UNIMPORTANT   = 1<<10,
	EM_MENU_SELECT_FLAG_FOLLOWUP      = 1<<11,
	EM_MENU_SELECT_FLAG_COMPLETED     = 1<<12,
	EM_MENU_SELECT_FLAG_CLEAR         = 1<<13,
	EM_MENU_SELECT_ADD_SENDER         = 1<<14,
	EM_MENU_SELECT_MARK_JUNK          = 1<<15,
	EM_MENU_SELECT_MARK_NOJUNK        = 1<<16,
	EM_MENU_SELECT_FOLDER             = 1<<17,    /* do we have any folder at all? */
	EM_MENU_SELECT_LAST               = 1<<18     /* reserve 2 slots */
};

/* Flags that describe a TARGET_WIDGET (none)
   this should probably be a more specific target type */

typedef struct _EMMenuTargetSelect EMMenuTargetSelect;

struct _EMMenuTargetSelect {
	EMenuTarget target;
	CamelFolder *folder;
	gchar *uri;
	GPtrArray *uids;
};

typedef struct _EMMenuTargetWidget EMMenuTargetWidget;

struct _EMMenuTargetWidget {
	EMenuTarget target;
};

typedef struct _EMenuItem EMMenuItem;

/* The object */
struct _EMMenu {
	EMenu popup;

	struct _EMMenuPrivate *priv;
};

struct _EMMenuClass {
	EMenuClass popup_class;
};

GType em_menu_get_type(void);

EMMenu *em_menu_new(const gchar *menuid);

EMMenuTargetSelect *em_menu_target_new_select(EMMenu *emp, CamelFolder *folder, const gchar *folder_uri, GPtrArray *uids);
EMMenuTargetWidget *em_menu_target_new_widget(EMMenu *emp, GtkWidget *w);

/* ********************************************************************** */

typedef struct _EMMenuHook EMMenuHook;
typedef struct _EMMenuHookClass EMMenuHookClass;

struct _EMMenuHook {
	EMenuHook hook;
};

struct _EMMenuHookClass {
	EMenuHookClass hook_class;
};

GType em_menu_hook_get_type(void);

G_END_DECLS

#endif /* __EM_MENU_H__ */
