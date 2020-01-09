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

#include <string.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <gdk/gdkkeysyms.h>
#include <bonobo/bonobo-generic-factory.h>
#include <bonobo/bonobo-ui-util.h>
#include <bonobo/bonobo-exception.h>
#include <e-util/e-util.h>
#include <libedataserverui/e-source-selector.h>
#include <libedataserverui/e-passwords.h>

#include "e-util/e-error.h"
#include "e-util/e-request.h"
#include "misc/e-task-bar.h"
#include "misc/e-info-label.h"

#include "e-util/e-icon-factory.h"
#include "e-util/e-util-private.h"
#include "shell/e-user-creatable-items-handler.h"

#include "evolution-shell-component-utils.h"
#include "e-activity-handler.h"
#include "e-contact-editor.h"
#include "addressbook-config.h"
#include "addressbook.h"
#include "addressbook-view.h"
#include "addressbook-component.h"
#include "addressbook/gui/widgets/e-addressbook-view.h"
#include "addressbook/gui/widgets/eab-gui-util.h"
#include "addressbook/gui/merging/eab-contact-merging.h"
#include "addressbook/printing/e-contact-print.h"
#include "addressbook/util/eab-book-util.h"
#include "addressbook/gui/widgets/eab-popup.h"
#include "addressbook/gui/widgets/eab-menu.h"

#define PARENT_TYPE G_TYPE_OBJECT
static GObjectClass *parent_class = NULL;

#define d(x)

struct _AddressbookViewPrivate {
	GtkWidget *notebook;
	BonoboControl *folder_view_control;

	GtkWidget *statusbar_widget;
	EActivityHandler *activity_handler;

	GtkWidget *info_widget;
	GtkWidget *sidebar_widget;
	GtkWidget *selector;

	GConfClient *gconf_client;

	GHashTable *uid_to_view;
	GHashTable *uid_to_editor;

	EBook *book;
	guint activity_id;
	ESourceList *source_list;
	gchar *passwd;
	EUserCreatableItemsHandler *creatable_items_handler;

	EABMenu *menu;
};

enum DndTargetType {
	DND_TARGET_TYPE_VCARD_LIST,
	DND_TARGET_TYPE_SOURCE_VCARD_LIST
};
#define VCARD_TYPE        "text/x-vcard"
#define SOURCE_VCARD_TYPE "text/x-source-vcard"
static GtkTargetEntry drag_types[] = {
	{ (gchar *) SOURCE_VCARD_TYPE, 0, DND_TARGET_TYPE_SOURCE_VCARD_LIST },
	{ (gchar *) VCARD_TYPE, 0, DND_TARGET_TYPE_VCARD_LIST }
};
static gint num_drag_types = sizeof(drag_types) / sizeof(drag_types[0]);

static void set_status_message (EABView *eav, const gchar *message, AddressbookView *view);
static void search_result (EABView *eav, EBookViewStatus status, AddressbookView *view);

static void activate_source (AddressbookView *view, ESource *source);

static void addressbook_view_init	(AddressbookView      *view);
static void addressbook_view_class_init	(AddressbookViewClass *klass);
static void addressbook_view_dispose    (GObject *object);

static ESource *find_first_source  (ESourceList *source_list);
static ESource *get_primary_source (AddressbookView *view);

typedef struct {
	GtkWidget *editor;
	gchar *uid;
	AddressbookView *view;
} EditorUidClosure;

static void
editor_weak_notify (gpointer data, GObject *o)
{
	EditorUidClosure *closure = data;
	AddressbookViewPrivate *priv = closure->view->priv;

	g_hash_table_remove (priv->uid_to_editor,
			     closure->uid);
}

static EABView *
get_current_view (AddressbookView *view)
{
	AddressbookViewPrivate *priv = view->priv;

	return EAB_VIEW (gtk_notebook_get_nth_page (GTK_NOTEBOOK (priv->notebook),
						    gtk_notebook_get_current_page (GTK_NOTEBOOK (priv->notebook))));
}

static void
save_all_contacts_cb (BonoboUIComponent *uih, gpointer user_data, const gchar *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	EABView *v = get_current_view (view);

	if (v)
		eab_view_save_as (v, TRUE);
}

static void
save_contact_cb (BonoboUIComponent *uih, gpointer user_data, const gchar *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	EABView *v = get_current_view (view);
	if (v)
		eab_view_save_as(v, FALSE);
}

static void
view_contact_cb (BonoboUIComponent *uih, gpointer user_data, const gchar *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	EABView *v = get_current_view (view);
	if (v)
		eab_view_view(v);
}

static void
delete_contact_cb (BonoboUIComponent *uih, gpointer user_data, const gchar *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	EABView *v = get_current_view (view);
	if (v)
		eab_view_delete_selection(v, TRUE);
}

static void
print_cb (BonoboUIComponent *uih, gpointer user_data, const gchar *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	EABView *v = get_current_view (view);
	if (v)
		eab_view_print (v, GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG);
}

static void
print_preview_cb (BonoboUIComponent *uih, gpointer user_data, const gchar *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	EABView *v = get_current_view (view);
	if (v)
		eab_view_print (v, GTK_PRINT_OPERATION_ACTION_PREVIEW);
}

static void
stop_loading_cb (BonoboUIComponent *uih, gpointer user_data, const gchar *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	EABView *v = get_current_view (view);
	if (v)
		eab_view_stop(v);
}

static void
cut_contacts_cb (BonoboUIComponent *uih, gpointer user_data, const gchar *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	EABView *v = get_current_view (view);
	if (v)
		eab_view_cut(v);
}

static void
copy_contacts_cb (BonoboUIComponent *uih, gpointer user_data, const gchar *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	EABView *v = get_current_view (view);
	if (v)
		eab_view_copy(v);
}

static void
paste_contacts_cb (BonoboUIComponent *uih, gpointer user_data, const gchar *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	EABView *v = get_current_view (view);
	if (v)
		eab_view_paste(v);
}

static void
select_all_contacts_cb (BonoboUIComponent *uih, gpointer user_data, const gchar *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	EABView *v = get_current_view (view);
	if (v)
		eab_view_select_all (v);
}

static void
send_contact_cb (BonoboUIComponent *uih, gpointer user_data, const gchar *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	EABView *v = get_current_view (view);
	if (v)
		eab_view_send (v);
}

static void
send_contact_to_cb (BonoboUIComponent *uih, gpointer user_data, const gchar *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	EABView *v = get_current_view (view);
	if (v)
		eab_view_send_to (v);
}

static void
copy_all_contacts_to_cb (BonoboUIComponent *uih, gpointer user_data, const gchar *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	EABView *v = get_current_view (view);

	if (v)
		eab_view_copy_to_folder (v, TRUE);
}

static void
copy_contact_to_cb (BonoboUIComponent *uih, gpointer user_data, const gchar *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	EABView *v = get_current_view (view);
	if (v)
		eab_view_copy_to_folder (v, FALSE);
}

static void
move_all_contacts_to_cb (BonoboUIComponent *uih, gpointer user_data, const gchar *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	EABView *v = get_current_view (view);
	if (v)
		eab_view_move_to_folder (v, TRUE);
}

static void
move_contact_to_cb (BonoboUIComponent *uih, gpointer user_data, const gchar *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	EABView *v = get_current_view (view);
	if (v)
		eab_view_move_to_folder (v, FALSE);
}

static void
forget_passwords_cb (BonoboUIComponent *uih, gpointer user_data, const gchar *path)
{
	e_passwords_forget_passwords();
}

static void
new_addressbook_folder (AddressbookView *view)
{
	AddressbookViewPrivate *priv = view->priv;
	addressbook_config_create_new_source (gtk_widget_get_toplevel(priv->notebook));
}

static void
new_folder_cb (BonoboUIComponent *uih, gpointer user_data, const gchar *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	new_addressbook_folder (view);
}

static void
delete_addressbook_folder (AddressbookView *view)
{
	AddressbookViewPrivate *priv = view->priv;
	ESource *selected_source;
	EBook  *book;
	GError *error = NULL;
	GtkWindow *toplevel;

	selected_source = e_source_selector_peek_primary_selection (E_SOURCE_SELECTOR (priv->selector));
	if (!selected_source)
		return;
	toplevel = (GtkWindow *) gtk_widget_get_toplevel (priv->notebook);

	if (e_error_run (toplevel, "addressbook:ask-delete-addressbook",
			e_source_peek_name(selected_source), NULL) != GTK_RESPONSE_YES)
	return;

	/* Remove local data */
	book = e_book_new (selected_source, &error);
	if (book) {
		if (e_book_remove (book, NULL)) {
			if (e_source_selector_source_is_selected (E_SOURCE_SELECTOR (priv->selector),
								  selected_source))
				e_source_selector_unselect_source (E_SOURCE_SELECTOR (priv->selector),
								   selected_source);

			e_source_group_remove_source (e_source_peek_group (selected_source), selected_source);

			e_source_list_sync (priv->source_list, NULL);
		}
		else {
			e_error_run (toplevel, "addressbook:remove-addressbook", NULL);
		}
	g_object_unref (book);
	}
	else {
		g_warning ("error removing addressbook : %s", error->message);
		g_error_free (error);
	}
}

static void
delete_folder_cb (BonoboUIComponent *uih, gpointer user_data, const gchar *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	if (view)
		delete_addressbook_folder (view);

}

static void
edit_addressbook_folder (AddressbookView *view)
{
	AddressbookViewPrivate *priv = view->priv;
	ESource *selected_source;
	const gchar *uid;
	EditorUidClosure *closure;

	selected_source = e_source_selector_peek_primary_selection (E_SOURCE_SELECTOR (priv->selector));
	if (!selected_source)
		return;

	uid = e_source_peek_uid (selected_source);

	closure = g_hash_table_lookup (priv->uid_to_editor, uid);
	if (!closure) {
		gchar *uid_copy = g_strdup (uid);

		closure = g_new (EditorUidClosure, 1);
		closure->editor = addressbook_config_edit_source (gtk_widget_get_toplevel(priv->notebook), selected_source);
		closure->uid = uid_copy;
		closure->view = view;

		g_hash_table_insert (priv->uid_to_editor,
				     uid_copy,
				     closure);

		g_object_weak_ref (G_OBJECT (closure->editor),
				   editor_weak_notify, closure);
	}

	gtk_window_present (GTK_WINDOW (closure->editor));

}

static void
edit_folder_cb (BonoboUIComponent *uih, gpointer user_data, const gchar *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	if (view)
		edit_addressbook_folder (view);

}

static void
rename_addressbook_folder (AddressbookView *view)
{
	AddressbookViewPrivate *priv = view->priv;
	ESource *source;
	const gchar *old_name;
	gchar *prompt, *new_name;
	gboolean done = FALSE;

	source = e_source_selector_peek_primary_selection (E_SOURCE_SELECTOR (priv->selector));
	old_name = e_source_peek_name(source);
	prompt = g_strdup_printf (_("Rename the \"%s\" folder to:"), old_name);

	while (!done) {
		new_name = e_request_string (NULL, _("Rename Folder"), prompt, old_name);
		if (new_name == NULL || !strcmp (old_name, new_name)) {
			done = TRUE;
		} else if (strchr(new_name, '/') != NULL) {
			e_error_run (NULL,
					"addressbook:no-rename-folder", old_name, new_name, _("Folder names cannot contain '/'"), NULL);
			done = TRUE;
		} else if (e_source_group_peek_source_by_name(e_source_peek_group(source), new_name)) {
			e_error_run (NULL, "addressbook:no-rename-folder-exists", old_name, new_name, NULL);
		} else {
			e_source_set_name (source, new_name);
			done = TRUE;
		}
	}
	g_free (new_name);

}

static void
rename_folder_cb (BonoboUIComponent *uih, gpointer user_data, const gchar *path)
{
	AddressbookView *view = (AddressbookView *) user_data;
	if (view)
		rename_addressbook_folder (view);
}

static gboolean
folder_can_delete (AddressbookView *view)
{
	AddressbookViewPrivate *priv = view->priv;
	ESource *source;
	const gchar *source_uri;
	const gchar *delete = NULL;

	source = e_source_selector_peek_primary_selection (E_SOURCE_SELECTOR (priv->selector));
	if (source) {
		source_uri = e_source_peek_relative_uri (source);
		/*check for delete_status property here*/
		delete = e_source_get_property (source, "delete");

		if (source_uri && !strcmp("system", source_uri))
			return 0;
		else if (delete && strcmp (delete,"no") == 0)
			return 0;
		else
			return 1;
	}
	else
		return 0;
}

static void
set_status_message (EABView *eav, const gchar *message, AddressbookView *view)
{
	AddressbookViewPrivate *priv = view->priv;
	EActivityHandler *activity_handler = priv->activity_handler;

	if (!message || !*message) {
		if (priv->activity_id != 0) {
			e_activity_handler_operation_finished (activity_handler, priv->activity_id);
			priv->activity_id = 0;
		}
	} else if (priv->activity_id == 0) {
		gchar *clientid = g_strdup_printf ("%p", (gpointer) view);

		priv->activity_id = e_activity_handler_operation_started (
			activity_handler, clientid, message, TRUE);

		g_free (clientid);
	} else {
		e_activity_handler_operation_progressing (activity_handler, priv->activity_id, message, -1.0);
	}

}

static void
set_folder_bar_message (EABView *eav, const gchar *message, AddressbookView *view)
{
	AddressbookViewPrivate *priv = view->priv;
	EABView *current_view = get_current_view (view);

	if (eav == current_view) {
		ESource *source = eav->source;

		if (source) {
			const gchar *name = e_source_peek_name (source);

			e_info_label_set_info((EInfoLabel*)priv->info_widget, name, message);
		}
	}
}

static void
search_result (EABView *eav, EBookViewStatus status, AddressbookView *view)
{
	eab_search_result_dialog (NULL /* XXX */, status);
}

static void
update_command_state (EABView *eav, AddressbookView *view)
{
	AddressbookViewPrivate *priv = view->priv;
	BonoboUIComponent *uic;
	EABMenuTargetSelect *target;

	if (eav != get_current_view (view))
		return;

	g_object_ref (view);

	target = eab_view_get_menu_target(eav, priv->menu);
	e_menu_update_target((EMenu *)priv->menu, target);

	uic = bonobo_control_get_ui_component (priv->folder_view_control);

	/* TODO: this stuff can mostly be made to use the target bits instead */

	if (bonobo_ui_component_get_container (uic) != CORBA_OBJECT_NIL) {
#define SET_SENSITIVE(verb,f) \
	bonobo_ui_component_set_prop (uic, (verb), "sensitive", (f)(eav) ? "1" : "0", NULL)

		SET_SENSITIVE ("/commands/ContactsSaveAsVCard", eab_view_can_save_as);
		SET_SENSITIVE ("/commands/ContactsView", eab_view_can_view);

		/* Print Contact */
		SET_SENSITIVE ("/commands/ContactsPrint", eab_view_can_print);
		SET_SENSITIVE ("/commands/ContactsPrintPreview", eab_view_can_print);

		/* Delete Contact */
		SET_SENSITIVE ("/commands/ContactDelete", eab_view_can_delete);
		SET_SENSITIVE ("/commands/ContactsCut", eab_view_can_cut);

		SET_SENSITIVE ("/commands/ContactsCopy", eab_view_can_copy);
		SET_SENSITIVE ("/commands/ContactsPaste", eab_view_can_paste);
		SET_SENSITIVE ("/commands/ContactsSelectAll", eab_view_can_select_all);
		SET_SENSITIVE ("/commands/ContactsSendContactToOther", eab_view_can_send);
		SET_SENSITIVE ("/commands/ContactsSendMessageToContact", eab_view_can_send_to);
		SET_SENSITIVE ("/commands/ContactsMoveToFolder", eab_view_can_move_to_folder);
		SET_SENSITIVE ("/commands/ContactsCopyToFolder", eab_view_can_copy_to_folder);

		bonobo_ui_component_set_prop (uic, ("/commands/FolderDelete"), "sensitive", folder_can_delete(view) ? "1" : "0", NULL);

		/* Stop */
		SET_SENSITIVE ("/commands/ContactStop", eab_view_can_stop);
#undef SET_SENSITIVE
	}

	g_object_unref (view);
}

static BonoboUIVerb verbs [] = {
	BONOBO_UI_UNSAFE_VERB ("ContactsPrint", print_cb),
	BONOBO_UI_UNSAFE_VERB ("ContactsPrintPreview", print_preview_cb),
	BONOBO_UI_UNSAFE_VERB ("ContactsSaveAsVCard", save_contact_cb),
	BONOBO_UI_UNSAFE_VERB ("ContactsView", view_contact_cb),

	BONOBO_UI_UNSAFE_VERB ("ContactDelete", delete_contact_cb),
	BONOBO_UI_UNSAFE_VERB ("ContactStop", stop_loading_cb),

	BONOBO_UI_UNSAFE_VERB ("ContactsCut", cut_contacts_cb),
	BONOBO_UI_UNSAFE_VERB ("ContactsCopy", copy_contacts_cb),
	BONOBO_UI_UNSAFE_VERB ("ContactsPaste", paste_contacts_cb),
	BONOBO_UI_UNSAFE_VERB ("ContactsSelectAll", select_all_contacts_cb),

	BONOBO_UI_UNSAFE_VERB ("ContactsSendContactToOther", send_contact_cb),
	BONOBO_UI_UNSAFE_VERB ("ContactsSendMessageToContact", send_contact_to_cb),
	BONOBO_UI_UNSAFE_VERB ("ContactsMoveToFolder", move_contact_to_cb),
	BONOBO_UI_UNSAFE_VERB ("ContactsCopyToFolder", copy_contact_to_cb),
	BONOBO_UI_UNSAFE_VERB ("ContactsForgetPasswords", forget_passwords_cb),
	/* ContactsViewPreview is a toggle */

	BONOBO_UI_UNSAFE_VERB ("FolderCreate", new_folder_cb),
	BONOBO_UI_UNSAFE_VERB ("FolderCopy", copy_all_contacts_to_cb),
	BONOBO_UI_UNSAFE_VERB ("FolderMove", move_all_contacts_to_cb),
	BONOBO_UI_UNSAFE_VERB ("FolderSave", save_all_contacts_cb),
	BONOBO_UI_UNSAFE_VERB ("FolderDelete", delete_folder_cb),
	BONOBO_UI_UNSAFE_VERB ("FolderRename", rename_folder_cb),
	BONOBO_UI_UNSAFE_VERB ("ChangeFolderProperties", edit_folder_cb),

	BONOBO_UI_VERB_END
};

static EPixmap pixmaps [] = {
	E_PIXMAP ("/commands/ChangeFolderProperties", "document-properties", GTK_ICON_SIZE_MENU),
	E_PIXMAP ("/commands/ContactDelete", "edit-delete", GTK_ICON_SIZE_MENU),
	E_PIXMAP ("/commands/ContactsCopy", "edit-copy", GTK_ICON_SIZE_MENU),
	E_PIXMAP ("/commands/ContactsCut", "edit-cut", GTK_ICON_SIZE_MENU),
	E_PIXMAP ("/commands/ContactsPaste", "edit-paste", GTK_ICON_SIZE_MENU),
	E_PIXMAP ("/commands/ContactsPrint", "document-print", GTK_ICON_SIZE_MENU),
	E_PIXMAP ("/commands/ContactsPrintPreview", "document-print-preview", GTK_ICON_SIZE_MENU),
	E_PIXMAP ("/commands/ContactsSaveAsVCard", "document-save-as", GTK_ICON_SIZE_MENU),
	E_PIXMAP ("/commands/ContactsSendContactToOther", "mail-forward", GTK_ICON_SIZE_MENU),
	E_PIXMAP ("/commands/ContactsSendMessageToContact", "mail-message-new", GTK_ICON_SIZE_MENU),
	E_PIXMAP ("/commands/FolderCopy", "edit-copy", GTK_ICON_SIZE_MENU),
	E_PIXMAP ("/commands/FolderDelete", "edit-delete", GTK_ICON_SIZE_MENU),
	E_PIXMAP ("/commands/FolderMove", "folder-move", GTK_ICON_SIZE_MENU),
	E_PIXMAP ("/commands/FolderSave", "document-save-as", GTK_ICON_SIZE_MENU),

	E_PIXMAP ("/Toolbar/ContactsPrint", "document-print", GTK_ICON_SIZE_LARGE_TOOLBAR),
	E_PIXMAP ("/Toolbar/ContactDelete", "edit-delete", GTK_ICON_SIZE_LARGE_TOOLBAR),

	E_PIXMAP_END
};

static void
control_activate (BonoboControl     *control,
		  BonoboUIComponent *uic,
		  AddressbookView   *view)
{
	AddressbookViewPrivate *priv = view->priv;
	Bonobo_UIContainer remote_ui_container;
	EABView *v = get_current_view (view);
	gchar *xmlfile;

	remote_ui_container = bonobo_control_get_remote_ui_container (control, NULL);
	bonobo_ui_component_set_container (uic, remote_ui_container, NULL);
	bonobo_object_release_unref (remote_ui_container, NULL);

	bonobo_ui_component_add_verb_list_with_data (
		uic, verbs, view);

	bonobo_ui_component_freeze (uic, NULL);

	xmlfile = g_build_filename (EVOLUTION_UIDIR,
				    "evolution-addressbook.xml",
				    NULL);
	bonobo_ui_util_set_ui (uic, PREFIX,
			       xmlfile,
			       "evolution-addressbook", NULL);
	g_free (xmlfile);

	if (v)
		eab_view_setup_menus (v, uic);

	e_pixmaps_update (uic, pixmaps);

	e_user_creatable_items_handler_activate (priv->creatable_items_handler, uic);

	bonobo_ui_component_thaw (uic, NULL);

	if (v)
		update_command_state (v, view);
}

static void
control_activate_cb (BonoboControl *control,
		     gboolean activate,
		     AddressbookView *view)
{
	BonoboUIComponent *uic;
	EABView *v = get_current_view (view);

	uic = bonobo_control_get_ui_component (control);
	g_return_if_fail (uic != NULL);

	if (activate) {
		control_activate (control, uic, view);
		e_menu_activate((EMenu *)view->priv->menu, uic, activate);
		if (activate && v && v->model)
			eab_model_force_folder_bar_message (v->model);
	} else {
		e_menu_activate((EMenu *)view->priv->menu, uic, activate);
		bonobo_ui_component_unset_container (uic, NULL);
		eab_view_discard_menus (v);
	}
}

static void
gather_uids_foreach (gchar *key,
		     gpointer value,
		     GList **list)
{
	(*list) = g_list_prepend (*list, key);
}

static void
source_list_changed_cb (ESourceList *source_list, AddressbookView *view)
{
	AddressbookViewPrivate *priv = view->priv;
	GList *uids, *l;
	EABView *v;

	uids = NULL;
	g_hash_table_foreach (priv->uid_to_view, (GHFunc)gather_uids_foreach, &uids);
	for (l = uids; l; l = l->next) {
		gchar *uid = l->data;
		if (e_source_list_peek_source_by_uid (source_list, uid)) {
			/* the source still exists, do nothing */
		}
		else {
			/* the source no longer exists, remove its
			   view remove it from our hash table. */
			v = g_hash_table_lookup (priv->uid_to_view,
						 uid);
			gtk_notebook_remove_page (GTK_NOTEBOOK (priv->notebook),
						  gtk_notebook_page_num (GTK_NOTEBOOK (priv->notebook),
									 GTK_WIDGET (v)));
			g_hash_table_remove (priv->uid_to_view, uid);
		}
	}
	g_list_free (uids);

	uids = NULL;
	g_hash_table_foreach (priv->uid_to_editor, (GHFunc)gather_uids_foreach, &uids);
	for (l = uids; l; l = l->next) {
		gchar *uid = l->data;
		if (e_source_list_peek_source_by_uid (source_list, uid)) {
			/* the source still exists, do nothing */
		}
		else {
			/* the source no longer exists, remove its
			   editor remove it from our hash table. */
			EditorUidClosure *closure = g_hash_table_lookup (priv->uid_to_editor,
									 uid);
			g_object_weak_unref (G_OBJECT (closure->editor),
					     editor_weak_notify, closure);
			gtk_widget_destroy (closure->editor);
			g_hash_table_remove (priv->uid_to_editor, uid);
		}
	}
	g_list_free (uids);

	/* make sure we've got the current view selected and updated
	   properly */
	v = get_current_view (view);
	if (v) {
		eab_view_setup_menus (v, bonobo_control_get_ui_component (priv->folder_view_control));
		update_command_state (v, view);
	}
}

static void
load_uri_for_selection (ESourceSelector *selector,
			AddressbookView *view,
			gboolean force)
{
	ESource *selected_source = e_source_selector_peek_primary_selection (E_SOURCE_SELECTOR (selector));
	ESource *primary = get_primary_source (view);

	if (selected_source != NULL &&
	    ((primary && (!g_str_equal (e_source_peek_uid (primary),e_source_peek_uid (selected_source) )))||force))
		activate_source (view, selected_source);
}

static ESource *
find_first_source (ESourceList *source_list)
{
	GSList *groups, *sources, *l, *m;

	groups = e_source_list_peek_groups (source_list);
	for (l = groups; l; l = l->next) {
		ESourceGroup *group = l->data;

		sources = e_source_group_peek_sources (group);
		for (m = sources; m; m = m->next) {
			ESource *source = m->data;

			return source;
		}
	}

	return NULL;
}

static void
save_primary_selection (AddressbookView *view)
{
	AddressbookViewPrivate *priv = view->priv;
	ESource *source;

	source = e_source_selector_peek_primary_selection (E_SOURCE_SELECTOR (priv->selector));
	if (!source)
		return;

	/* Save the selection for next time we start up */
	gconf_client_set_string (priv->gconf_client,
				 "/apps/evolution/addressbook/display/primary_addressbook",
				 e_source_peek_uid (source), NULL);
}

static ESource *
get_primary_source (AddressbookView *view)
{
	AddressbookViewPrivate *priv = view->priv;
	ESource *source;
	gchar *uid;

	uid = gconf_client_get_string (priv->gconf_client,
				       "/apps/evolution/addressbook/display/primary_addressbook",
				       NULL);
	if (uid) {
		source = e_source_list_peek_source_by_uid (priv->source_list, uid);
		g_free (uid);
	} else {
		/* Try to create a default if there isn't one */
		source = find_first_source (priv->source_list);
	}

	return source;
}

static void
load_primary_selection (AddressbookView *view)
{
	AddressbookViewPrivate *priv = view->priv;
	ESource *source;

	source = get_primary_source (view);
	if (source)
		e_source_selector_set_primary_selection (E_SOURCE_SELECTOR (priv->selector), source);
}

/* Folder popup menu callbacks */
typedef struct {
	AddressbookView *view;
	ESource *selected_source;
	GtkWidget *toplevel;
} BookRemovedClosure;

static void
book_removed (EBook *book, EBookStatus status, gpointer data)
{
	BookRemovedClosure *closure = data;
	AddressbookView *view = closure->view;
	AddressbookViewPrivate *priv = view->priv;
	ESource *source = closure->selected_source;
	GtkWidget *toplevel = closure->toplevel;

	g_free (closure);

	g_object_unref (book);

	if (E_BOOK_ERROR_OK == status) {
		/* Remove source */
		if (e_source_selector_source_is_selected (E_SOURCE_SELECTOR (priv->selector),
							  source))
			e_source_selector_unselect_source (E_SOURCE_SELECTOR (priv->selector),
							   source);

		e_source_group_remove_source (e_source_peek_group (source), source);

		e_source_list_sync (priv->source_list, NULL);
	}
	else {
		e_error_run (GTK_WINDOW (toplevel),
			     "addressbook:remove-addressbook",
			     NULL);
	}
}

static void
delete_addressbook_cb(EPopup *ep, EPopupItem *pitem, gpointer data)
{
	AddressbookView *view = data;
	AddressbookViewPrivate *priv = view->priv;
	ESource *selected_source;
	EBook  *book;
	GError *error = NULL;
	GtkWindow *toplevel;

	selected_source = e_source_selector_peek_primary_selection (E_SOURCE_SELECTOR (priv->selector));
	if (!selected_source)
		return;

	toplevel = (GtkWindow *)gtk_widget_get_toplevel(ep->target->widget);

	if (e_error_run (toplevel, "addressbook:ask-delete-addressbook", e_source_peek_name(selected_source), NULL) != GTK_RESPONSE_YES)
		return;

	/* Remove local data */
	book = e_book_new (selected_source, &error);
	if (book) {
		BookRemovedClosure *closure = g_new (BookRemovedClosure, 1);

		closure->toplevel = (GtkWidget *)toplevel;
		closure->view = view;
		closure->selected_source = selected_source;

		if (e_book_async_remove (book, book_removed, closure)) {
			e_error_run (toplevel, "addressbook:remove-addressbook", NULL);
			g_free (closure);
			g_object_unref (book);
		}
	}
}

static void
new_addressbook_cb(EPopup *ep, EPopupItem *pitem, gpointer data)
{
	addressbook_config_create_new_source (gtk_widget_get_toplevel(ep->target->widget));
}

static void
rename_addressbook_cb (EPopup *ep, EPopupItem *pitem, gpointer data)
{
	AddressbookView *view = data;
	ESourceSelector *selector;

	selector = E_SOURCE_SELECTOR (view->priv->selector);
	e_source_selector_edit_primary_selection (selector);
}

static void
save_addressbook_cb(EPopup *ep, EPopupItem *pitem, gpointer data)
{
	AddressbookView *view = data;
	EABView *v = get_current_view (view);
	if (v)
		eab_view_save_as (v, TRUE);
}

static void
edit_addressbook_cb(EPopup *ep, EPopupItem *pitem, gpointer data)
{
	AddressbookView *view = data;
	if (view)
		edit_addressbook_folder (view);
}

/* Callbacks.  */

static void
primary_source_selection_changed_callback (ESourceSelector *selector,
					   AddressbookView *view)
{
	load_uri_for_selection (selector, view, FALSE);
	save_primary_selection (view);
}

static EPopupItem abv_source_popups[] = {
	{ E_POPUP_ITEM, (gchar *) "10.new", (gchar *) N_("_New Address Book"), new_addressbook_cb, NULL, (gchar *) "address-book-new", 0, 0 },
	{ E_POPUP_ITEM, (gchar *) "20.saveasvcard", (gchar *) N_("Save As vCard..."), save_addressbook_cb, NULL, (gchar *) "document-save-as", 0, EAB_POPUP_SOURCE_PRIMARY },
	{ E_POPUP_ITEM, (gchar *) "25.rename", (gchar *) N_("_Rename..."), rename_addressbook_cb, NULL, NULL, 0, EAB_POPUP_SOURCE_PRIMARY|EAB_POPUP_SOURCE_DELETE },

	{ E_POPUP_BAR,  (gchar *) "30.bar" },
	{ E_POPUP_ITEM, (gchar *) "30.delete", (gchar *) N_("_Delete"), delete_addressbook_cb, NULL, (gchar *) "edit-delete", 0, EAB_POPUP_SOURCE_USER|EAB_POPUP_SOURCE_PRIMARY|EAB_POPUP_SOURCE_DELETE },

	{ E_POPUP_BAR,  (gchar *) "99.bar" },
	{ E_POPUP_ITEM, (gchar *) "99.properties", (gchar *) N_("_Properties"), edit_addressbook_cb, NULL, (gchar *) "document-properties", 0, EAB_POPUP_SOURCE_PRIMARY },
};

static void
abv_source_popup_free(EPopup *ep, GSList *list, gpointer data)
{
	g_slist_free(list);
}

static gboolean
popup_event_callback(ESourceSelector *selector, ESource *source, GdkEventButton *event, AddressbookView *view)
{
	EABPopup *ep;
	EABPopupTargetSource *t;
	GSList *menus = NULL;
	gint i;
	GtkMenu *menu;

	/** @HookPoint-EABPopup:Addressbook Source Selector Context Menu
	 * @Id: org.gnome.evolution.addressbook.source.popup
	 * @Class: org.gnome.evolution.addresbook.popup:1.0
	 * @Target: EABPopupTargetSource
	 *
	 * The context menu on the source selector in the contacts window.
	 */

	ep = eab_popup_new("org.gnome.evolution.addressbook.source.popup");
	t = eab_popup_target_new_source(ep, selector);
	t->target.widget = (GtkWidget *)view->priv->notebook;

	for (i=0;i<sizeof(abv_source_popups)/sizeof(abv_source_popups[0]);i++)
		menus = g_slist_prepend(menus, &abv_source_popups[i]);

	e_popup_add_items((EPopup *)ep, menus, NULL, abv_source_popup_free, view);

	menu = e_popup_create_menu_once((EPopup *)ep, (EPopupTarget *)t, 0);
	gtk_menu_popup(menu, NULL, NULL, NULL, NULL, event?event->button:0, event?event->time:gtk_get_current_event_time());

	return TRUE;
}

typedef struct
{
	guint     remove_from_source : 1;
	guint     copy_done          : 1;
	gint      pending_removals;

	EContact *current_contact;
	GList    *remaining_contacts;

	EBook    *source_book;
	EBook    *target_book;
}
MergeContext;

static void
destroy_merge_context (MergeContext *merge_context)
{
	if (merge_context->source_book)
		g_object_unref (merge_context->source_book);
	if (merge_context->target_book)
		g_object_unref (merge_context->target_book);

	g_free (merge_context);
}

static void
removed_contact_cb (EBook *book, EBookStatus status, gpointer closure)
{
	MergeContext *merge_context = closure;

	merge_context->pending_removals--;

	if (merge_context->copy_done && merge_context->pending_removals == 0) {
		/* Finished */

		destroy_merge_context (merge_context);
	}
}

static void
merged_contact_cb (EBook *book, EBookStatus status, const gchar *id, gpointer closure)
{
	MergeContext *merge_context = closure;

	if (merge_context->remove_from_source && status == E_BOOK_ERROR_OK) {
		/* Remove previous contact from source */

		e_book_async_remove_contact (merge_context->source_book, merge_context->current_contact,
					     removed_contact_cb, merge_context);
		merge_context->pending_removals++;
	}

	g_object_unref (merge_context->current_contact);

	if (merge_context->remaining_contacts) {
		/* Copy next contact */

		merge_context->current_contact = merge_context->remaining_contacts->data;
		merge_context->remaining_contacts = g_list_delete_link (merge_context->remaining_contacts,
									merge_context->remaining_contacts);
		eab_merging_book_add_contact (merge_context->target_book, merge_context->current_contact,
					      merged_contact_cb, merge_context);
	} else if (merge_context->pending_removals == 0) {
		/* Finished */

		destroy_merge_context (merge_context);
	} else {
		/* Finished, but have pending removals */

		merge_context->copy_done = TRUE;
	}
}

static gboolean
selector_tree_data_dropped (ESourceSelector *selector,
                            GtkSelectionData *data,
                            ESource *destination,
                            GdkDragAction action,
                            guint info,
                            AddressbookView *view)
{
	EBook *source_book, *target_book;
	MergeContext *merge_context = NULL;
	GList *contactlist;
	EABView *v;

	target_book = e_book_new (destination, NULL);
	if (!target_book) {
		g_message (G_STRLOC ":Couldn't create EBook.");
		return FALSE;
	}
	e_book_open (target_book, FALSE, NULL);

	eab_book_and_contact_list_from_string ((gchar *)data->data, &source_book, &contactlist);

	v = get_current_view (view);
	g_object_get (v->model, "book",&source_book, NULL);

	/* Set up merge context */

	merge_context = g_new0 (MergeContext, 1);

	merge_context->source_book = source_book;
	merge_context->target_book = target_book;

	merge_context->current_contact = contactlist->data;
	merge_context->remaining_contacts = g_list_delete_link (contactlist, contactlist);

	merge_context->remove_from_source = action == GDK_ACTION_MOVE ? TRUE : FALSE;

	/* Start merge */

	eab_merging_book_add_contact (target_book, merge_context->current_contact,
				      merged_contact_cb, merge_context);

	return TRUE;
}

static void
destroy_callback(gpointer data, GObject *where_object_was)
{
	AddressbookView *view = data;
	g_object_unref (view);
}

GType
addressbook_view_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info =  {
			sizeof (AddressbookViewClass),
			NULL,           /* base_init */
			NULL,           /* base_finalize */
			(GClassInitFunc) addressbook_view_class_init,
			NULL,           /* class_finalize */
			NULL,           /* class_data */
			sizeof (EABView),
			0,             /* n_preallocs */
			(GInstanceInitFunc) addressbook_view_init,
		};

		type = g_type_register_static (PARENT_TYPE, "AddressbookView", &info, 0);
	}

	return type;
}

static void
addressbook_view_class_init (AddressbookViewClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = addressbook_view_dispose;

	parent_class = g_type_class_peek_parent (klass);
}

static gboolean
source_selector_key_press_event_callback (GtkWidget *widget, GdkEventKey *event, AddressbookView *view)
{
	if (event->keyval == GDK_Delete) {
		/* suppress delete key press in a source selector */
		return TRUE;
	}
	return FALSE;
}

static void
addressbook_view_init (AddressbookView *view)
{
	AddressbookViewPrivate *priv;
	GtkWidget *selector_scrolled_window;
	AtkObject *a11y;

	view->priv =
		priv = g_new0 (AddressbookViewPrivate, 1);

	priv->gconf_client = addressbook_component_peek_gconf_client (addressbook_component_peek ());

	priv->uid_to_view = g_hash_table_new_full (g_str_hash, g_str_equal, (GDestroyNotify)g_free, (GDestroyNotify)g_object_unref);
	priv->uid_to_editor = g_hash_table_new_full (g_str_hash, g_str_equal, (GDestroyNotify)g_free, (GDestroyNotify)g_free);

	priv->notebook = gtk_notebook_new ();
	gtk_notebook_set_show_tabs (GTK_NOTEBOOK (priv->notebook), FALSE);
	gtk_notebook_set_show_border (GTK_NOTEBOOK (priv->notebook), FALSE);

	g_object_weak_ref (G_OBJECT (priv->notebook), destroy_callback, view);

	/* Create the control. */
	priv->folder_view_control = bonobo_control_new (priv->notebook);

	gtk_widget_show (priv->notebook);

	e_book_get_addressbooks (&priv->source_list, NULL);
	g_signal_connect (priv->source_list,
			  "changed",
			  G_CALLBACK (source_list_changed_cb), view);

	priv->creatable_items_handler = e_user_creatable_items_handler_new ("contacts", NULL, NULL);
	priv->menu = eab_menu_new("org.gnome.evolution.addressbook.view");

	g_signal_connect (priv->folder_view_control, "activate",
			  G_CALLBACK (control_activate_cb), view);

	priv->activity_handler = e_activity_handler_new ();

	priv->statusbar_widget = e_task_bar_new ();
	gtk_widget_show (priv->statusbar_widget);

	e_activity_handler_attach_task_bar (priv->activity_handler,
					    E_TASK_BAR (priv->statusbar_widget));

	priv->info_widget = e_info_label_new("x-office-address-book");
	e_info_label_set_info((EInfoLabel*)priv->info_widget, _("Contacts"), "");
	gtk_widget_show (priv->info_widget);

	priv->selector = e_source_selector_new (priv->source_list);

	g_signal_connect (
		priv->selector, "data-dropped",
		G_CALLBACK (selector_tree_data_dropped), view);
	gtk_drag_dest_set (priv->selector, GTK_DEST_DEFAULT_ALL, drag_types, num_drag_types, GDK_ACTION_COPY | GDK_ACTION_MOVE);
	a11y = gtk_widget_get_accessible (GTK_WIDGET (priv->selector));
	atk_object_set_name (a11y, _("Contact Source Selector"));

	e_source_selector_show_selection (E_SOURCE_SELECTOR (priv->selector), FALSE);
	gtk_widget_show (priv->selector);

	selector_scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (selector_scrolled_window), GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (selector_scrolled_window),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_container_add (GTK_CONTAINER (selector_scrolled_window), priv->selector);
	gtk_widget_show (selector_scrolled_window);

	priv->sidebar_widget = gtk_vbox_new(FALSE, 0);
	gtk_box_pack_start(GTK_BOX (priv->sidebar_widget), priv->info_widget, FALSE, TRUE, 0);
	gtk_box_pack_start(GTK_BOX (priv->sidebar_widget), selector_scrolled_window, TRUE, TRUE, 0);
	gtk_widget_show (priv->sidebar_widget);

	g_signal_connect_object (priv->selector, "primary_selection_changed",
				 G_CALLBACK (primary_source_selection_changed_callback),
				 G_OBJECT (view), 0);
	g_signal_connect_after (priv->selector, "key_press_event",
				G_CALLBACK (source_selector_key_press_event_callback),
				G_OBJECT (view));
	g_signal_connect_object (priv->selector, "popup_event",
				 G_CALLBACK (popup_event_callback),
				 G_OBJECT (view), 0);

	load_primary_selection (view);
	load_uri_for_selection (E_SOURCE_SELECTOR (priv->selector), view, TRUE);
}

static void
destroy_editor (gchar *key,
		gpointer value,
		gpointer nada)
{
	EditorUidClosure *closure = value;

	g_object_weak_unref (G_OBJECT (closure->editor),
			     editor_weak_notify, closure);

	gtk_widget_destroy (GTK_WIDGET (closure->editor));
}

static void
addressbook_view_dispose (GObject *object)
{
	AddressbookView *view = ADDRESSBOOK_VIEW (object);
	AddressbookViewPrivate *priv = view->priv;

	if (view->priv) {
		if (priv->book)
			g_object_unref (priv->book);

		g_free(priv->passwd);

		if (priv->source_list)
			g_object_unref (priv->source_list);

		if (priv->uid_to_view)
			g_hash_table_destroy (priv->uid_to_view);

		if (priv->uid_to_editor) {
			g_hash_table_foreach (priv->uid_to_editor, (GHFunc)destroy_editor, NULL);
			g_hash_table_destroy (priv->uid_to_editor);
		}

		if (priv->creatable_items_handler)
			g_object_unref (priv->creatable_items_handler);

		if (priv->menu)
			g_object_unref (priv->menu);

		g_free (view->priv);
		view->priv = NULL;
	}

	if (G_OBJECT_CLASS (parent_class)->dispose)
		(* G_OBJECT_CLASS (parent_class)->dispose) (object);
}

typedef struct {
	EABView *view;
	ESource *source;
} BookOpenData;

static void
book_open_cb (EBook *book, EBookStatus status, gpointer closure)
{
	BookOpenData *data = closure;
	EABView *view = data->view;
	ESource *source = data->source;

	g_free (data);

	/* we always set the "source" property on the EABView, since
	   we use it to reload a previously failed book. */
	g_object_set(view,
		     "source", source,
		     NULL);

	if (status == E_BOOK_ERROR_OK) {
		g_object_set(view,
			     "book", book,
			     NULL);

		if (view->model)
			eab_model_force_folder_bar_message (view->model);
	}
	else if (status != E_BOOK_ERROR_CANCELLED) {
		eab_load_error_dialog (NULL /* XXX */, source, status);
	}

	g_object_unref (source);
}

static void
activate_source (AddressbookView *view,
		 ESource *source)
{
	AddressbookViewPrivate *priv = view->priv;
	const gchar *uid;
	GtkWidget *uid_view;
	EBook *book;
	BookOpenData *data;

	uid = e_source_peek_uid (source);
	uid_view = g_hash_table_lookup (priv->uid_to_view, uid);

	if (uid_view) {
		/* there is a view for this uid.  make
		   sure that the view actually
		   contains an EBook (if it doesn't
		   contain an EBook a previous load
		   failed.  try to load it again */
		g_object_get (uid_view,
			      "book", &book,
			      NULL);

		if (book) {
			g_object_unref (book);
		}
		else {
			g_object_get (uid_view,
				      "source", &source,
				      NULL);

			/* source can be NULL here, if
			   a previous load hasn't
			   actually made it to
			   book_open_cb yet. */
			if (source) {
				book = e_book_new (source, NULL);

				if (!book) {
					g_object_unref (source);
				}
				else {
					data = g_new (BookOpenData, 1);
					data->view = g_object_ref (uid_view);
					data->source = source; /* transfer the ref we get back from g_object_get */

					addressbook_load (book, book_open_cb, data);
				}
			}
		}
	}
	else {
		/* we don't have a view for this uid already
		   set up. */
		GtkWidget *label = gtk_label_new (uid);
		GError *error = NULL;

		uid_view = eab_view_new ();

		gtk_widget_show (uid_view);
		gtk_widget_show (label);

		g_object_set (uid_view, "type", EAB_VIEW_TABLE, NULL);

		gtk_notebook_append_page (GTK_NOTEBOOK (priv->notebook),
					  uid_view,
					  label);

		g_hash_table_insert (priv->uid_to_view, g_strdup (uid), uid_view);

		g_signal_connect (uid_view, "status_message",
				  G_CALLBACK(set_status_message), view);

		g_signal_connect (uid_view, "search_result",
				  G_CALLBACK(search_result), view);

		g_signal_connect (uid_view, "folder_bar_message",
				  G_CALLBACK(set_folder_bar_message), view);

		g_signal_connect (uid_view, "command_state_change",
				  G_CALLBACK(update_command_state), view);

		book = e_book_new (source, &error);

		if (book) {
			data = g_new (BookOpenData, 1);
			data->view = g_object_ref (uid_view);
			data->source = g_object_ref (source);

			addressbook_load (book, book_open_cb, data);
		}
		else {
			g_warning ("error loading addressbook : %s", error->message);
			g_error_free (error);
		}
	}

	gtk_notebook_set_current_page (GTK_NOTEBOOK (priv->notebook),
				       gtk_notebook_page_num (GTK_NOTEBOOK (priv->notebook),
							      uid_view));

	if (EAB_VIEW (uid_view)->model)
		eab_model_force_folder_bar_message (EAB_VIEW (uid_view)->model);

	/* change menus/toolbars to reflect the new view, assuming we are already displayed */
	if (bonobo_ui_component_get_container (bonobo_control_get_ui_component (priv->folder_view_control)) != CORBA_OBJECT_NIL) {
		eab_view_setup_menus (EAB_VIEW (uid_view), bonobo_control_get_ui_component (priv->folder_view_control));
		update_command_state (EAB_VIEW (uid_view), view);
	}
}

AddressbookView *
addressbook_view_new (void)
{
	return g_object_new (ADDRESSBOOK_TYPE_VIEW, NULL);
}

EActivityHandler*
addressbook_view_peek_activity_handler (AddressbookView *view)
{
	g_return_val_if_fail (ADDRESSBOOK_IS_VIEW (view), NULL);

	return view->priv->activity_handler;
}

GtkWidget*
addressbook_view_peek_info_label (AddressbookView *view)
{
	g_return_val_if_fail (ADDRESSBOOK_IS_VIEW (view), NULL);

	return view->priv->info_widget;
}

GtkWidget*
addressbook_view_peek_sidebar (AddressbookView *view)
{
	g_return_val_if_fail (ADDRESSBOOK_IS_VIEW (view), NULL);

	return view->priv->sidebar_widget;
}

GtkWidget*
addressbook_view_peek_statusbar (AddressbookView *view)
{
	g_return_val_if_fail (ADDRESSBOOK_IS_VIEW (view), NULL);

	return view->priv->statusbar_widget;
}

BonoboControl*
addressbook_view_peek_folder_view (AddressbookView *view)
{
	g_return_val_if_fail (ADDRESSBOOK_IS_VIEW (view), NULL);

	return view->priv->folder_view_control;
}

void
addressbook_view_edit_contact (AddressbookView* view,
			       const gchar * source_uid,
			       const gchar * contact_uid)
{
	AddressbookViewPrivate *priv = view->priv;

	ESource* source = NULL;
	EContact* contact = NULL;
	EBook* book = NULL;

	if (!source_uid || !contact_uid)
		return;

	source = e_source_list_peek_source_by_uid (priv->source_list, source_uid);
	if (!source)
		return;

	/* FIXME: Can I unref this book? */
	book = e_book_new (source, NULL);
	if (!book)
		return;

	if (!e_book_open (book, TRUE, NULL)) {
		g_object_unref (book);
		return;
	}

	e_book_get_contact (book, contact_uid, &contact, NULL);

	if (!contact) {
		g_object_unref (book);
		return;
	}
	eab_show_contact_editor (book, contact, FALSE, FALSE);
	g_object_unref (contact);
	g_object_unref (book);
}
