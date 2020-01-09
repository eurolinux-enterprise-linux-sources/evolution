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

#ifndef __EM_POPUP_H__
#define __EM_POPUP_H__

#include <glib-object.h>
#include <camel/camel-folder.h>

#include "e-util/e-popup.h"

G_BEGIN_DECLS

typedef struct _EMPopup EMPopup;
typedef struct _EMPopupClass EMPopupClass;

/**
 * enum _em_popup_target_t - A list of mail popup target types.
 *
 * @EM_POPUP_TARGET_SELECT: A selection of messages.
 * @EM_POPUP_TARGET_URI: A URI.
 * @EM_POPUP_TARGET_PART: A CamelMimePart message part.
 * @EM_POPUP_TARGET_FOLDER: A folder URI.
 *
 * Defines the value of the targetid for all EMPopup target types.
 **/
enum _em_popup_target_t {
	EM_POPUP_TARGET_SELECT,
	EM_POPUP_TARGET_URI,
	EM_POPUP_TARGET_PART,
	EM_POPUP_TARGET_FOLDER
};

/**
 * enum _em_popup_target_select_t - EMPopupTargetSelect qualifiers.
 *
 * @EM_POPUP_SELECT_ONE: Only one item is selected.
 * @EM_POPUP_SELECT_MANY: One ore more items are selected.
 * @EM_POPUP_SELECT_MARK_READ: Message(s) are unseen and can be
 * marked seen.
 * @EM_POPUP_SELECT_MARK_UNREAD: Message(s) are seen and can be
 * marked unseen.
 * @EM_POPUP_SELECT_DELETE: Message(s) are undeleted and can
 * be marked deleted.
 * @EM_POPUP_SELECT_UNDELETE: Message(s) are deleted and can be
 * undeleted.
 * @EM_POPUP_SELECT_MAILING_LIST: If one message is selected, and it
 * contains a message list tag.
 * @EM_POPUP_SELECT_EDIT: The message can be opened for editing (the
 * folder is a sent folder).
 * @EM_POPUP_SELECT_MARK_IMPORTANT: Message(s) are not marked
 * important.
 * @EM_POPUP_SELECT_MARK_UNIMPORTANT: Message(s) are marked
 * important.
 * @EM_POPUP_SELECT_FLAG_FOLLOWUP: Message(s) are not flagged for
 * followup.
 * @EM_POPUP_SELECT_FLAG_COMPLETED: Message(s) are not flagged completed.
 * @EM_POPUP_SELECT_FLAG_CLEAR: Message(s) are flagged for followup.
 * @EM_POPUP_SELECT_ADD_SENDER: The message contains sender addresses
 * which might be added to the addressbook. i.e. it isn't a message in
 * the Sent or Drafts folders.
 * @EM_POPUP_SELECT_FOLDER: A folder is set on the selection.
 * @EM_POPUP_SELECT_LAST: The last bit used, can be used to add
 * additional types from derived application code.
 *
 **/
enum _em_popup_target_select_t {
	EM_POPUP_SELECT_ONE                = 1<<1,
	EM_POPUP_SELECT_MANY               = 1<<2,
	EM_POPUP_SELECT_MARK_READ          = 1<<3,
	EM_POPUP_SELECT_MARK_UNREAD        = 1<<4,
	EM_POPUP_SELECT_DELETE             = 1<<5,
	EM_POPUP_SELECT_UNDELETE           = 1<<6,
	EM_POPUP_SELECT_MAILING_LIST       = 1<<7,
	EM_POPUP_SELECT_EDIT               = 1<<8,
	EM_POPUP_SELECT_MARK_IMPORTANT     = 1<<9,
	EM_POPUP_SELECT_MARK_UNIMPORTANT   = 1<<10,
	EM_POPUP_SELECT_FLAG_FOLLOWUP      = 1<<11,
	EM_POPUP_SELECT_FLAG_COMPLETED     = 1<<12,
	EM_POPUP_SELECT_FLAG_CLEAR         = 1<<13,
	EM_POPUP_SELECT_ADD_SENDER         = 1<<14,
	EM_POPUP_SELECT_FOLDER             = 1<<15,     /* do we have any folder at all? */
	EM_POPUP_SELECT_JUNK               = 1<<16,
	EM_POPUP_SELECT_NOT_JUNK           = 1<<17,
	EM_POPUP_SELECT_LAST               = 1<<18
};

/**
 * enum _em_popup_target_uri_t - EMPopupTargetURI qualifiers.
 *
 * @EM_POPUP_URI_HTTP: This is a HTTP or HTTPS url.
 * @EM_POPUP_URI_MAILTO: This is a MAILTO url.
 * @EM_POPUP_URI_NOT_MAILTO: This is not a MAILTO url.
 *
 **/
enum _em_popup_target_uri_t {
	EM_POPUP_URI_HTTP = 1<<0,
	EM_POPUP_URI_MAILTO = 1<<1,
	EM_POPUP_URI_NOT_MAILTO = 1<<2
};

/**
 * enum _em_popup_target_part_t - EMPopupTargetPart qualifiers.
 *
 * @EM_POPUP_PART_MESSAGE: This is a message type.
 * @EM_POPUP_PART_IMAGE: This is an image type.
 *
 **/
enum _em_popup_target_part_t {
	EM_POPUP_PART_MESSAGE = 1<<0,
	EM_POPUP_PART_IMAGE = 1<<1
};

/**
 * enum _em_popup_target_folder_t - EMPopupTargetFolder qualifiers.
 *
 * @EM_POPUP_FOLDER_FOLDER: This is a normal folder.
 * @EM_POPUP_FOLDER_STORE: This is a store.
 * @EM_POPUP_FOLDER_INFERIORS: This folder may have child folders.
 * @EM_POPUP_FOLDER_DELETE: This folder can be deleted or renamed.
 * @EM_POPUP_FOLDER_SELECT: This folder exists and can be selected or
 * opened.
 *
 **/
enum _em_popup_target_folder_t {
	EM_POPUP_FOLDER_FOLDER = 1<<0, /* normal folder */
	EM_POPUP_FOLDER_STORE = 1<<1, /* root/nonselectable folder, i.e. store */
	EM_POPUP_FOLDER_INFERIORS = 1<<2, /* folder can have children */
	EM_POPUP_FOLDER_DELETE = 1<<3, /* folder can be deleted/renamed */
	EM_POPUP_FOLDER_SELECT = 1<<4, /* folder can be selected/opened */
	EM_POPUP_FOLDER_OUTBOX = 1<<5, /* Outbox folder */
	EM_POPUP_FOLDER_NONSTATIC = 1<<6 /* Except static folders like Outbox.*/
};

typedef struct _EMPopupTargetSelect EMPopupTargetSelect;
typedef struct _EMPopupTargetURI EMPopupTargetURI;
typedef struct _EMPopupTargetPart EMPopupTargetPart;
typedef struct _EMPopupTargetFolder EMPopupTargetFolder;

/**
 * struct _EMPopupTargetURI - An inline URI.
 *
 * @target: Superclass.
 * @uri: The encoded URI to which this target applies.
 *
 * Used to represent popup-menu context on any URI object.
 **/
struct _EMPopupTargetURI {
	EPopupTarget target;
	gchar *uri;
};

/**
 * struct _EMPopupTargetSelect - A list of messages.
 *
 * @target: Superclass.
 * @folder: The CamelFolder of the selected messages.
 * @uri: The encoded URI represending this folder.
 * @uids: An array of UID strings of messages within @folder.
 *
 * Used to represent a selection of messages as context for a popup
 * menu.  All items may be NULL if the current view has no active
 * folder selected.
 **/
struct _EMPopupTargetSelect {
	EPopupTarget target;
	CamelFolder *folder;
	gchar *uri;
	GPtrArray *uids;
};

/**
 * struct _EMPopupTargetPart - A Camel object.
 *
 * @target: Superclass.
 * @mime_type: MIME type of the part.  This may be a calculated type
 * not matching the @part's MIME type.
 * @part: A CamelMimePart representing a message or attachment.
 *
 * Used to represent a message part as context for a popup menu.  This
 * is used for both attachments and inline-images.
 **/
struct _EMPopupTargetPart {
	EPopupTarget target;
	gchar *mime_type;
	CamelMimePart *part;
};

/**
 * struct _EMPopupTargetFolder - A folder uri.
 *
 * @target: Superclass.
 * @uri: A folder URI.
 *
 * This target is used to represent folder context.
 **/
struct _EMPopupTargetFolder {
	EPopupTarget target;
	gchar *uri;
};

typedef struct _EPopupItem EMPopupItem;

/* The object */
struct _EMPopup {
	EPopup popup;

	struct _EMPopupPrivate *priv;
};

struct _EMPopupClass {
	EPopupClass popup_class;
};

GType em_popup_get_type(void);

EMPopup *em_popup_new(const gchar *menuid);

EMPopupTargetURI *em_popup_target_new_uri(EMPopup *emp, const gchar *uri);
EMPopupTargetSelect *em_popup_target_new_select(EMPopup *emp, CamelFolder *folder, const gchar *folder_uri, GPtrArray *uids);
EMPopupTargetPart *em_popup_target_new_part(EMPopup *emp, CamelMimePart *part, const gchar *mime_type);
EMPopupTargetFolder *em_popup_target_new_folder(EMPopup *emp, const gchar *uri, guint32 info_flags, guint32 popup_flags);

/* ********************************************************************** */

typedef struct _EMPopupHook EMPopupHook;
typedef struct _EMPopupHookClass EMPopupHookClass;

struct _EMPopupHook {
	EPopupHook hook;
};

struct _EMPopupHookClass {
	EPopupHookClass hook_class;
};

GType em_popup_hook_get_type(void);

G_END_DECLS

#endif /* __EM_POPUP_H__ */
