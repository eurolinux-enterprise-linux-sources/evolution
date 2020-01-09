/*
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
 *		Chris Toshok <toshok@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <glib/gi18n.h>

#include "eab-editor.h"
#include "addressbook/gui/widgets/eab-gui-util.h"
#include "e-util/e-util.h"

static void eab_editor_default_show  (EABEditor *editor);
static void eab_editor_default_raise (EABEditor *editor);
static void eab_editor_default_close (EABEditor *editor);
static void eab_editor_class_init    (EABEditorClass *klass);
static void eab_editor_init          (EABEditor *editor);

/* Signal IDs */
enum {
	CONTACT_ADDED,
	CONTACT_MODIFIED,
	CONTACT_DELETED,
	EDITOR_CLOSED,
	LAST_SIGNAL
};

static GSList *all_editors = NULL;

static GtkObjectClass *parent_class = NULL;

static guint editor_signals[LAST_SIGNAL];

GType
eab_editor_get_type (void)
{
	static GType editor_type = 0;

	if (!editor_type) {
		static const GTypeInfo editor_info =  {
			sizeof (EABEditorClass),
			NULL,           /* base_init */
			NULL,           /* base_finalize */
			(GClassInitFunc) eab_editor_class_init,
			NULL,           /* class_finalize */
			NULL,           /* class_data */
			sizeof (EABEditor),
			0,             /* n_preallocs */
			(GInstanceInitFunc) eab_editor_init,
		};

		editor_type = g_type_register_static (G_TYPE_OBJECT, "EABEditor", &editor_info, 0);
	}

	return editor_type;
}

static void
eab_editor_default_show (EABEditor *editor)
{
	g_warning ("abstract eab_editor_show called");
}

static void
eab_editor_default_close (EABEditor *editor)
{
	g_warning ("abstract eab_editor_close called");
}

static void
eab_editor_default_raise (EABEditor *editor)
{
	g_warning ("abstract eab_editor_raise called");
}

static void
eab_editor_default_save_contact (EABEditor *editor, gboolean should_close)
{
	g_warning ("abstract eab_editor_save_contact called");
}

static gboolean
eab_editor_default_is_valid (EABEditor *editor)
{
	g_warning ("abstract eab_editor_is_valid called");
	return FALSE;
}

static gboolean
eab_editor_default_is_changed (EABEditor *editor)
{
	g_warning ("abstract eab_editor_is_changed called");
	return FALSE;
}

static GtkWindow*
eab_editor_default_get_window (EABEditor *editor)
{
	g_warning ("abstract eab_editor_get_window called");
	return NULL;
}

static void
eab_editor_class_init (EABEditorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_ref (G_TYPE_OBJECT);

	klass->show = eab_editor_default_show;
	klass->close = eab_editor_default_close;
	klass->raise = eab_editor_default_raise;
	klass->save_contact = eab_editor_default_save_contact;
	klass->is_valid = eab_editor_default_is_valid;
	klass->is_changed = eab_editor_default_is_changed;
	klass->get_window = eab_editor_default_get_window;

	editor_signals[CONTACT_ADDED] =
		g_signal_new ("contact_added",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EABEditorClass, contact_added),
			      NULL, NULL,
			      e_marshal_NONE__INT_OBJECT,
			      G_TYPE_NONE, 2,
			      G_TYPE_INT, G_TYPE_OBJECT);

	editor_signals[CONTACT_MODIFIED] =
		g_signal_new ("contact_modified",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EABEditorClass, contact_modified),
			      NULL, NULL,
			      e_marshal_NONE__INT_OBJECT,
			      G_TYPE_NONE, 2,
			      G_TYPE_INT, G_TYPE_OBJECT);

	editor_signals[CONTACT_DELETED] =
		g_signal_new ("contact_deleted",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EABEditorClass, contact_deleted),
			      NULL, NULL,
			      e_marshal_NONE__INT_OBJECT,
			      G_TYPE_NONE, 2,
			      G_TYPE_INT, G_TYPE_OBJECT);

	editor_signals[EDITOR_CLOSED] =
		g_signal_new ("editor_closed",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_FIRST,
			      G_STRUCT_OFFSET (EABEditorClass, editor_closed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
}

static void
eab_editor_init (EABEditor *editor)
{
}



void
eab_editor_show (EABEditor *editor)
{
	g_return_if_fail (EAB_IS_EDITOR (editor));

	if (EAB_EDITOR_GET_CLASS(editor)->show)
		EAB_EDITOR_GET_CLASS(editor)->show (editor);
}

void
eab_editor_close (EABEditor *editor)
{
	g_return_if_fail (EAB_IS_EDITOR (editor));

	if (EAB_EDITOR_GET_CLASS(editor)->close)
		EAB_EDITOR_GET_CLASS(editor)->close (editor);
}

void
eab_editor_raise (EABEditor *editor)
{
	g_return_if_fail (EAB_IS_EDITOR (editor));

	if (EAB_EDITOR_GET_CLASS(editor)->raise)
		EAB_EDITOR_GET_CLASS(editor)->raise (editor);
}

void
eab_editor_save_contact (EABEditor *editor, gboolean should_close)
{
	g_return_if_fail (EAB_IS_EDITOR (editor));

	if (EAB_EDITOR_GET_CLASS(editor)->save_contact)
		EAB_EDITOR_GET_CLASS(editor)->save_contact (editor, should_close);
}

gboolean
eab_editor_is_changed (EABEditor *editor)
{
	g_return_val_if_fail (EAB_IS_EDITOR (editor), FALSE);

	if (EAB_EDITOR_GET_CLASS(editor)->is_changed)
		return EAB_EDITOR_GET_CLASS(editor)->is_changed (editor);
	else
		return FALSE;
}

gboolean
eab_editor_is_valid (EABEditor *editor)
{
	g_return_val_if_fail (EAB_IS_EDITOR (editor), FALSE);

	if (EAB_EDITOR_GET_CLASS(editor)->is_valid)
		return EAB_EDITOR_GET_CLASS(editor)->is_valid (editor);
	else
		return FALSE;
}

GtkWindow*
eab_editor_get_window (EABEditor *editor)
{
	g_return_val_if_fail (EAB_IS_EDITOR (editor), NULL);

	if (EAB_EDITOR_GET_CLASS(editor)->get_window)
		return EAB_EDITOR_GET_CLASS(editor)->get_window (editor);
	else
		return NULL;
}
/* This function prompts for saving if editor conents are in changed state and
   save or discards or cancels(just returns with out doing anything) according to user input.
   Editor gets destoryed in case of save and discard case.
   */

gboolean
eab_editor_prompt_to_save_changes (EABEditor *editor, GtkWindow *window)
{
	if (!eab_editor_is_changed (editor)) {
		eab_editor_close (EAB_EDITOR (editor));
		return TRUE;
	}

	switch (eab_prompt_save_dialog (window)) {
	case GTK_RESPONSE_YES:
		if (!eab_editor_is_valid (editor)) {
			return FALSE;
		}
		eab_editor_save_contact (editor, TRUE);
		return TRUE;
	case GTK_RESPONSE_NO:
		eab_editor_close (EAB_EDITOR (editor));
		return TRUE;
	case GTK_RESPONSE_CANCEL:
	default:
		return FALSE;
	}
}

gboolean
eab_editor_request_close_all (void)
{
	GSList *p;
	GSList *pnext;
	gboolean retval;

	retval = TRUE;
	for (p = all_editors; p != NULL; p = pnext) {
		EABEditor *editor = EAB_EDITOR (p->data);
		GtkWindow *window = eab_editor_get_window (editor);

		pnext = p->next;

		eab_editor_raise (editor);
		if (! eab_editor_prompt_to_save_changes (editor, window)) {
			retval = FALSE;
			break;
		}
	}

	return retval;
}

const GSList*
eab_editor_get_all_editors (void)
{
	return all_editors;
}

gboolean
eab_editor_confirm_delete (GtkWindow *parent, gboolean plural, gboolean is_list, gchar *name)
{
	GtkWidget *dialog;
	gint result;
	gchar *msg;

	if (is_list) {
		/* contact list(s) */
		if (!plural)
			msg = g_strdup_printf (_("Are you sure you want\nto delete contact list (%s)?"),
						name?name:"");
		else
			msg = g_strdup (_("Are you sure you want\nto delete these contact lists?"));
	}
	else {
		/* contact(s) */
		if (!plural)
			msg = g_strdup_printf (_("Are you sure you want\nto delete contact (%s)?"),
						name?name:"");
		else
			msg = g_strdup (_("Are you sure you want\nto delete these contacts?"));
	}

	dialog = gtk_message_dialog_new (parent,
					 0,
					 GTK_MESSAGE_QUESTION,
					 GTK_BUTTONS_NONE,
					 "%s",
					 msg);
	g_free (msg);

	gtk_dialog_add_buttons (GTK_DIALOG (dialog),
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				GTK_STOCK_DELETE, GTK_RESPONSE_ACCEPT,
				NULL);

	result = gtk_dialog_run(GTK_DIALOG (dialog));

	gtk_widget_destroy (dialog);

	return (result == GTK_RESPONSE_ACCEPT);
}

void
eab_editor_contact_added (EABEditor *editor, EBookStatus status, EContact *contact)
{
	g_return_if_fail (EAB_IS_EDITOR (editor));
	g_return_if_fail (E_IS_CONTACT (contact));

	g_signal_emit (editor, editor_signals[CONTACT_ADDED], 0,
		       status, contact);
}

void
eab_editor_contact_modified (EABEditor *editor, EBookStatus status, EContact *contact)
{
	g_return_if_fail (EAB_IS_EDITOR (editor));
	g_return_if_fail (E_IS_CONTACT (contact));

	g_signal_emit (editor, editor_signals[CONTACT_MODIFIED], 0,
		       status, contact);
}

void
eab_editor_contact_deleted (EABEditor *editor, EBookStatus status, EContact *contact)
{
	g_return_if_fail (EAB_IS_EDITOR (editor));
	g_return_if_fail (E_IS_CONTACT (contact));

	g_signal_emit (editor, editor_signals[CONTACT_DELETED], 0,
		       status, contact);
}

void
eab_editor_closed (EABEditor *editor)
{
	g_return_if_fail (EAB_IS_EDITOR (editor));

	g_signal_emit (editor, editor_signals[EDITOR_CLOSED], 0);
}

void
eab_editor_add (EABEditor *editor)
{
	g_return_if_fail (EAB_IS_EDITOR (editor));

	all_editors = g_slist_prepend (all_editors, editor);
}

void
eab_editor_remove (EABEditor *editor)
{
	g_return_if_fail (EAB_IS_EDITOR (editor));

	all_editors = g_slist_remove (all_editors, editor);
}
