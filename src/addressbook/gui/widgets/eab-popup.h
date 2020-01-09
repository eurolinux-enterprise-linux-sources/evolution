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
 *		Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef __EAB_POPUP_H__
#define __EAB_POPUP_H__

#include <glib-object.h>

#include "e-util/e-popup.h"

G_BEGIN_DECLS

#define ADAPTED_TO_E_NAME_SELECTOR 1

typedef struct _EABPopup EABPopup;
typedef struct _EABPopupClass EABPopupClass;

/**
 * enum _eab_popup_target_t - A list of mail popup target types.
 *
 * @EAB_POPUP_TARGET_SELECT: A selection of cards
 * @EAB_POPUP_TARGET_SOURCE: A source selection.
 *
 * Defines the value of the targetid for all EABPopup target types.
 **/
enum _eab_popup_target_t {
	EAB_POPUP_TARGET_SELECT,
        EAB_POPUP_TARGET_URI,
	EAB_POPUP_TARGET_SOURCE,
	EAB_POPUP_TARGET_SELECT_NAMES
};

/**
 * enum _eab_popup_target_select_t - EABPopupTargetSelect qualifiers.
 *
 * @EAB_POPUP_SELECT_ONE: Only one item is selected.
 * @EAB_POPUP_SELECT_MANY: Two or more items are selected.
 * @EAB_POPUP_SELECT_ANY: One or more items are selected.
 * @EAB_POPUP_SELECT_EDITABLE: Read/writable source.
 * @EAB_POPUP_SELECT_EMAIL: Has an email address.
 **/
enum _eab_popup_target_select_t {
	EAB_POPUP_SELECT_ONE = 1<<0,
	EAB_POPUP_SELECT_MANY = 1<<1,
	EAB_POPUP_SELECT_ANY = 1<<2,
	EAB_POPUP_SELECT_EDITABLE = 1<<3,
	EAB_POPUP_SELECT_EMAIL = 1<<4,
	EAB_POPUP_LIST = 1<<5,
	EAB_POPUP_CONTACT = 1<<6
};

enum _eab_popup_target_uri_t {
	EAB_POPUP_URI_HTTP = 1<<0,
	EAB_POPUP_URI_MAILTO = 1<<1,
	EAB_POPUP_URI_NOT_MAILTO = 1<<2
};
/**
 * enum _eab_popup_target_source_t - EABPopupTargetSource qualifiers.
 *
 * @EAB_POPUP_SOURCE_PRIMARY: Has a primary selection.
 * @EAB_POPUP_SOURCE_SYSTEM: Is a 'system' folder.
 *
 **/
enum _eab_popup_target_source_t {
	EAB_POPUP_SOURCE_PRIMARY = 1<<0,
	EAB_POPUP_SOURCE_SYSTEM = 1<<1,	/* system folder */
	EAB_POPUP_SOURCE_USER = 1<<2,	/* user folder (!system) */
	EAB_POPUP_SOURCE_DELETE = 1<<3,
	EAB_POPUP_SOURCE_NO_DELETE = 1<<4
};

typedef struct _EABPopupTargetSelect EABPopupTargetSelect;
typedef struct _EABPopupTargetSource EABPopupTargetSource;
typedef struct _EABPopupTargetSelectNames EABPopupTargetSelectNames;
typedef struct _EABPopupTargetURI EABPopupTargetURI;
/**
 * struct _EABPopupTargetSelect - A list of address cards.
 *
 * @target: Superclass.
 * @book: Book the cards belong to.
 * @cards: All selected cards.
 *
 * Used to represent a selection of cards as context for a popup
 * menu.
 **/
struct _EABPopupTargetSelect {
	EPopupTarget target;

	struct _EBook *book;
	GPtrArray *cards;
};

struct _EABPopupTargetURI {
	EPopupTarget target;
	gchar *uri;
};

/**
 * struct _EABPopupTargetSource - A source target.
 *
 * @target: Superclass.
 * @selector: Selector holding the source selection.
 *
 * This target is used to represent a source selection.
 **/
struct _EABPopupTargetSource {
	EPopupTarget target;

	struct _ESourceSelector *selector;
};

#ifdef ADAPTED_TO_E_NAME_SELECTOR

/**
 * struct _EABPopupTargetSelectNames - A select names target.
 *
 * @target: Superclass.
 * @model: Select names model.
 * @row: Row of item selected.
 *
 * This target is used to represent an item selected in an
 * ESelectNames model.
 **/
struct _EABPopupTargetSelectNames {
	EPopupTarget target;

	struct _ESelectNamesModel *model;
	gint row;
};

#endif

typedef struct _EPopupItem EABPopupItem;

/* The object */
struct _EABPopup {
	EPopup popup;

	struct _EABPopupPrivate *priv;
};

struct _EABPopupClass {
	EPopupClass popup_class;
};

GType eab_popup_get_type(void);

EABPopup *eab_popup_new(const gchar *menuid);

EABPopupTargetSelect *eab_popup_target_new_select(EABPopup *eabp, struct _EBook *book, gint readonly, GPtrArray *cards);
EABPopupTargetURI *eab_popup_target_new_uri(EABPopup *emp, const gchar *uri);
EABPopupTargetSource *eab_popup_target_new_source(EABPopup *eabp, struct _ESourceSelector *selector);

#ifdef ADAPTED_TO_E_NAME_SELECTOR

EABPopupTargetSelectNames *eab_popup_target_new_select_names(EABPopup *eabp, struct _ESelectNamesModel *model, gint row);

#endif

/* ********************************************************************** */

typedef struct _EABPopupHook EABPopupHook;
typedef struct _EABPopupHookClass EABPopupHookClass;

struct _EABPopupHook {
	EPopupHook hook;
};

struct _EABPopupHookClass {
	EPopupHookClass hook_class;
};

GType eab_popup_hook_get_type(void);

G_END_DECLS

#endif /* __EAB_POPUP_H__ */
