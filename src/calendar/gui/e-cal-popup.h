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

#ifndef __E_CAL_POPUP_H__
#define __E_CAL_POPUP_H__

#include <glib-object.h>
#include <libedataserverui/e-source-selector.h>

#include <e-util/e-popup.h>
#include "dialogs/comp-editor.h"
#include "e-cal-model.h"

G_BEGIN_DECLS

/**
 * enum _e_cal_popup_target_t - A list of mail popup target types.
 *
 * @E_CAL_POPUP_TARGET_SELECT: A selection of cards
 * @E_CAL_POPUP_TARGET_SOURCE: A source selection.
 *
 * Defines the value of the targetid for all ECalPopup target types.
 **/
enum _e_cal_popup_target_t {
	E_CAL_POPUP_TARGET_SELECT,
	E_CAL_POPUP_TARGET_SOURCE,
	E_CAL_POPUP_TARGET_ATTACHMENTS
};

/**
 * enum _e_cal_popup_target_select_t - ECalPopupTargetSelect qualifiers.
 *
 * @E_CAL_POPUP_SELECT_ONE: Only one item is selected.
 * @E_CAL_POPUP_SELECT_MANY: More than one item selected.
 * @E_CAL_POPUP_SELECT_ANY: One ore more items are selected.
 * @E_CAL_POPUP_SELECT_EDITABLE: The selection is editable.
 * @E_CAL_POPUP_SELECT_RECURRING: Is a recurring event.
 * @E_CAL_POPUP_SELECT_NONRECURRING: Is not a recurring event.
 * @E_CAL_POPUP_SELECT_INSTANCE: This is an instance event.
 * @E_CAL_POPUP_SELECT_ORGANIZER: The user is the organiser of the event.
 * @E_CAL_POPUP_SELECT_NOTEDITING: The event is not being edited already.  Not implemented.
 * @E_CAL_POPUP_SELECT_NOTMEETING: The event is not a meeting.
 * @E_CAL_POPUP_SELECT_ASSIGNABLE: An assignable task.
 * @E_CAL_POPUP_SELECT_HASURL: A task that contains a URL.
 **/
enum _e_cal_popup_target_select_t {
	E_CAL_POPUP_SELECT_ONE = 1<<0,
	E_CAL_POPUP_SELECT_MANY = 1<<1,
	E_CAL_POPUP_SELECT_ANY = 1<<2,
	E_CAL_POPUP_SELECT_EDITABLE = 1<<3,
	E_CAL_POPUP_SELECT_RECURRING = 1<<4,
	E_CAL_POPUP_SELECT_NONRECURRING = 1<<5,
	E_CAL_POPUP_SELECT_INSTANCE = 1<<6,

	E_CAL_POPUP_SELECT_ORGANIZER = 1<<7,
	E_CAL_POPUP_SELECT_NOTEDITING = 1<<8,
	E_CAL_POPUP_SELECT_NOTMEETING = 1<<9,

	E_CAL_POPUP_SELECT_ASSIGNABLE = 1<<10,
	E_CAL_POPUP_SELECT_HASURL = 1<<11,
	E_CAL_POPUP_SELECT_MEETING = 1 <<12,
	E_CAL_POPUP_SELECT_DELEGATABLE = 1<<13,
	E_CAL_POPUP_SELECT_ACCEPTABLE = 1<<14,
	E_CAL_POPUP_SELECT_NOTCOMPLETE = 1<<15,
	E_CAL_POPUP_SELECT_NOSAVESCHEDULES = 1<<16,
	E_CAL_POPUP_SELECT_COMPLETE = 1<<17
};

/**
 * enum _e_cal_popup_target_source_t - ECalPopupTargetSource qualifiers.
 *
 * @E_CAL_POPUP_SOURCE_PRIMARY: Has a primary selection.
 * @E_CAL_POPUP_SOURCE_SYSTEM: Is a 'system' folder.
 *
 **/
enum _e_cal_popup_target_source_t {
	E_CAL_POPUP_SOURCE_PRIMARY = 1<<0,
	E_CAL_POPUP_SOURCE_SYSTEM = 1<<1,	/* system folder */
	E_CAL_POPUP_SOURCE_USER = 1<<2, /* user folder (!system) */
	E_CAL_POPUP_SOURCE_OFFLINE = 1 <<3,
	E_CAL_POPUP_SOURCE_NO_OFFLINE = 1 <<4,
	E_CAL_POPUP_SOURCE_DELETE = 1<<5,
	E_CAL_POPUP_SOURCE_NO_DELETE = 1<<6
};

/**
 * enum _e_cal_popup_target_attachments_t - ECalPopupTargetAttachments qualifiers.
 *
 * @E_CAL_POPUP_ATTACHMENTS_ONE: There is one and only one attachment selected.
 * @E_CAL_POPUP_ATTACHMENTS_MANY: There is one or more attachments selected.
 *
 **/
enum _e_cal_popup_target_attachments_t {
	E_CAL_POPUP_ATTACHMENTS_ONE = 1<<0, /* only 1 selected */
	E_CAL_POPUP_ATTACHMENTS_MANY = 1<<1, /* one or more selected */
	E_CAL_POPUP_ATTACHMENTS_MODIFY = 1<<2, /* check for modify operation */
	E_CAL_POPUP_ATTACHMENTS_MULTIPLE = 1<<3,
	E_CAL_POPUP_ATTACHMENTS_IMAGE = 1<<4
};

typedef struct _ECalPopupTargetSelect ECalPopupTargetSelect;
typedef struct _ECalPopupTargetSource ECalPopupTargetSource;
typedef struct _ECalPopupTargetAttachments ECalPopupTargetAttachments;

/**
 * struct _ECalPopupTargetSelect - A list of address cards.
 *
 * @target: Superclass.  target.widget is an ECalendarView.
 * @model: The ECalModel.
 * @events: The selected events.  These are ECalModelComponent's.
 *
 * Used to represent a selection of appointments as context for a popup
 * menu.
 *
 * TODO: For maximum re-usability references to the view could be removed
 * from this structure.
 **/
struct _ECalPopupTargetSelect {
	EPopupTarget target;

	ECalModel *model;
	GPtrArray *events;
};

/**
 * struct _ECalPopupTargetSource - A source target.
 *
 * @target: Superclass.
 * @selector: Selector holding the source selection.
 *
 * This target is used to represent a source selection.
 **/
struct _ECalPopupTargetSource {
	EPopupTarget target;

	ESourceSelector *selector;
};

/**
 * struct _ECalPopupTargetAttachments - A list of calendar attachments.
 *
 * @target: Superclass.
 * @attachments: A GSList list of CalAttachments.
 *
 * This target is used to represent a selected list of attachments in
 * the calendar attachment area.
 **/
struct _ECalPopupTargetAttachments {
	EPopupTarget target;
	GSList *attachments;
};

typedef struct _EPopupItem ECalPopupItem;

typedef struct _ECalPopup ECalPopup;
typedef struct _ECalPopupClass ECalPopupClass;
typedef struct _ECalPopupPrivate ECalPopupPrivate;

/* The object */
struct _ECalPopup {
	EPopup popup;

	ECalPopupPrivate *priv;
};

struct _ECalPopupClass {
	EPopupClass popup_class;
};

GType e_cal_popup_get_type(void);

ECalPopup *e_cal_popup_new(const gchar *menuid);

ECalPopupTargetSelect *e_cal_popup_target_new_select(ECalPopup *eabp, ECalModel *model, GPtrArray *events);
ECalPopupTargetSource *e_cal_popup_target_new_source(ECalPopup *eabp, ESourceSelector *selector);

/* ********************************************************************** */

typedef struct _ECalPopupHook ECalPopupHook;
typedef struct _ECalPopupHookClass ECalPopupHookClass;

struct _ECalPopupHook {
	EPopupHook hook;
};

struct _ECalPopupHookClass {
	EPopupHookClass hook_class;
};

GType e_cal_popup_hook_get_type(void);

G_END_DECLS

#endif /* __E_CAL_POPUP_H__ */
