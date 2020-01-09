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

#ifndef __EAB_MENU_H__
#define __EAB_MENU_H__

#include <glib-object.h>
#include <libebook/e-book.h>

#include "e-util/e-menu.h"

G_BEGIN_DECLS

typedef struct _EABMenu EABMenu;
typedef struct _EABMenuClass EABMenuClass;

/* Current target description */
enum _eab_menu_target_t {
	EAB_MENU_TARGET_SELECT
};

/**
 * enum _eab_menu_target_select_t - EABMenuTargetSelect qualifiers.
 *
 * @EAB_MENU_SELECT_ONE: Only one item is selected.
 * @EAB_MENU_SELECT_MANY: More than one item selected.
 * @EAB_MENU_SELECT_ANY: One or more items selected.
 * @EAB_MENU_SELECT_EDITABLE: Editable addressbook.
 * @EAB_MENU_SELECT_EMAIL: Has an email address.
 **/
enum _eab_menu_target_select_t {
	EAB_MENU_SELECT_ONE = 1<<0,
	EAB_MENU_SELECT_MANY = 1<<1,
	EAB_MENU_SELECT_ANY = 1<<2,
	EAB_MENU_SELECT_EDITABLE = 1<<3,
	EAB_MENU_SELECT_EMAIL = 1<<4
};

typedef struct _EABMenuTargetSelect EABMenuTargetSelect;

struct _EABMenuTargetSelect {
	EMenuTarget target;

	struct _EBook *book;
	GPtrArray *cards;
};

typedef struct _EMenuItem EABMenuItem;

/* The object */
struct _EABMenu {
	EMenu menu;

	struct _EABMenuPrivate *priv;
};

struct _EABMenuClass {
	EMenuClass menu_class;
};

GType eab_menu_get_type(void);

EABMenu *eab_menu_new(const gchar *menuid);

EABMenuTargetSelect *eab_menu_target_new_select(EABMenu *eabp, struct _EBook *book, gint readonly, GPtrArray *cards);

/* ********************************************************************** */

typedef struct _EABMenuHook EABMenuHook;
typedef struct _EABMenuHookClass EABMenuHookClass;

struct _EABMenuHook {
	EMenuHook hook;
};

struct _EABMenuHookClass {
	EMenuHookClass hook_class;
};

GType eab_menu_hook_get_type(void);

G_END_DECLS

#endif /* __EAB_MENU_H__ */
