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
 *		Shakti Sen <shprasad@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glade/glade-xml.h>
#include <gtk/gtk.h>
#include <e-util/e-error.h>
#include <e-folder.h>
#include <exchange-account.h>
#include <exchange-hierarchy.h>
#include "exchange-hierarchy-foreign.h"
#include <e2k-types.h>
#include <exchange-types.h>
#include <e2k-propnames.h>
#include <libedataserverui/e-name-selector.h>
#include "exchange-config-listener.h"
#include "exchange-folder-subscription.h"
#include "exchange-operations.h"

static void
user_response (ENameSelectorDialog *name_selector_dialog, gint response, gpointer data)
{
	gtk_widget_hide (GTK_WIDGET (name_selector_dialog));
}

static void
user_clicked (GtkWidget *button, ENameSelector *name_selector)
{
	ENameSelectorDialog *name_selector_dialog;

	name_selector_dialog = e_name_selector_peek_dialog (name_selector);
	gtk_window_set_modal (GTK_WINDOW (name_selector_dialog), TRUE);
	gtk_widget_show (GTK_WIDGET (name_selector_dialog));
}

static GtkWidget *
setup_name_selector (GladeXML *glade_xml, ENameSelector **name_selector_ret)
{
	ENameSelector *name_selector;
	ENameSelectorModel *name_selector_model;
	ENameSelectorDialog *name_selector_dialog;
	GtkWidget *placeholder;
	GtkWidget *widget;
	GtkWidget *button;

	placeholder = glade_xml_get_widget (glade_xml, "user-picker-placeholder");
	g_assert (GTK_IS_CONTAINER (placeholder));

	name_selector = e_name_selector_new ();

	name_selector_model = e_name_selector_peek_model (name_selector);
	/* FIXME Limit to one user */
	e_name_selector_model_add_section (name_selector_model, "User", _("User"), NULL);

	/* Listen for responses whenever the dialog is shown */
	name_selector_dialog = e_name_selector_peek_dialog (name_selector);
	g_signal_connect (name_selector_dialog, "response",
			  G_CALLBACK (user_response), name_selector);

	widget = GTK_WIDGET (e_name_selector_peek_section_entry (name_selector, "User"));
	gtk_widget_show (widget);

	button = glade_xml_get_widget (glade_xml, "button-user");
	g_signal_connect (button, "clicked", G_CALLBACK (user_clicked), name_selector);
	gtk_box_pack_start (GTK_BOX (placeholder), widget, TRUE, TRUE, 6);
	*name_selector_ret = name_selector;

	return widget;
}

static void
setup_folder_name_combo (GladeXML *glade_xml, const gchar *fname)
{
	GtkComboBox *combo;
	const gchar *strings[] = {
		"Calendar",
		"Inbox",
		"Contacts",
		"Tasks",
		NULL
		/* FIXME: Should these be translated?  */
	};
	gint i;

	combo = GTK_COMBO_BOX (glade_xml_get_widget (glade_xml, "folder-name-combo"));
	g_assert (GTK_IS_COMBO_BOX_ENTRY (combo));

	gtk_list_store_clear (GTK_LIST_STORE (gtk_combo_box_get_model (combo)));

	for (i = 0; strings[i] != NULL; i ++)
		gtk_combo_box_append_text (combo, strings[i]);

	gtk_entry_set_text (GTK_ENTRY (gtk_bin_get_child (GTK_BIN (combo))), fname);
}

static void
folder_name_entry_changed_callback (GtkEditable *editable,
                                    gpointer data)
{
	GtkDialog *dialog = GTK_DIALOG (data);
	const gchar *folder_name_text = gtk_entry_get_text (GTK_ENTRY (editable));

	if (*folder_name_text == '\0')
		gtk_dialog_set_response_sensitive (dialog, GTK_RESPONSE_OK, FALSE);
	else
		gtk_dialog_set_response_sensitive (dialog, GTK_RESPONSE_OK, TRUE);
}

static void
user_name_entry_changed_callback (GtkEditable *editable, gpointer data)
{
	GtkDialog *dialog = GTK_DIALOG (data);
	const gchar *user_name_text = gtk_entry_get_text (GTK_ENTRY (editable));

	if (*user_name_text == '\0')
		gtk_dialog_set_response_sensitive (dialog, GTK_RESPONSE_OK, FALSE);
	else
		gtk_dialog_set_response_sensitive (dialog, GTK_RESPONSE_OK, TRUE);
}

static void
setup_server_combobox (GladeXML *glade_xml, gchar *mail_account)
{
	GtkWidget *widget;

	widget = glade_xml_get_widget (glade_xml, "server-combobox");
	g_return_if_fail (GTK_IS_COMBO_BOX (widget));

	gtk_list_store_clear (GTK_LIST_STORE (gtk_combo_box_get_model (GTK_COMBO_BOX (widget))));

	gtk_combo_box_append_text (GTK_COMBO_BOX (widget), mail_account);
	gtk_combo_box_set_active (GTK_COMBO_BOX (widget), 0);

	/* FIXME: Default to the current storage in the shell view.  */
}

typedef struct {
	ExchangeAccount *account;
	ENameSelector *name_selector;
	GtkWidget *name_selector_widget;
	GtkWidget *folder_name_entry;
}SubscriptionInfo;

static void
destroy_subscription_info (SubscriptionInfo *subscription_info)
{
	if (subscription_info->name_selector) {
		g_object_unref (subscription_info->name_selector);
		subscription_info->name_selector = NULL;
	}
	g_free (subscription_info);
}

static void
subscribe_to_folder (GtkWidget *dialog, gint response, gpointer data)
{
	SubscriptionInfo *subscription_info = data;
	gchar *user_email_address = NULL, *folder_name = NULL, *path = NULL;
	gchar *subscriber_email;
	EFolder *folder = NULL;
	EDestinationStore *destination_store;
	GList *destinations;
	EDestination *destination;
	ExchangeAccountFolderResult result;

	if (response == GTK_RESPONSE_CANCEL) {
		gtk_widget_destroy (dialog);
		destroy_subscription_info (subscription_info);
	}
	else if (response == GTK_RESPONSE_OK) {
		while (TRUE) {
			destination_store = e_name_selector_entry_peek_destination_store (
						E_NAME_SELECTOR_ENTRY (GTK_ENTRY (subscription_info->name_selector_widget)));
			destinations = e_destination_store_list_destinations (destination_store);
			if (!destinations)
				break;
			destination = destinations->data;
			user_email_address = g_strdup (e_destination_get_email (destination));
			g_list_free (destinations);

			if (user_email_address != NULL && *user_email_address != '\0')
				break;

			/* check if user is trying to subscribe to his own folder */
			subscriber_email = exchange_account_get_email_id (subscription_info->account);
			if (subscriber_email != NULL && *subscriber_email != '\0') {
				if (g_str_equal (subscriber_email, user_email_address)) {
					e_error_run (NULL, ERROR_DOMAIN ":folder-exists-error", NULL);
					g_free (user_email_address);
					gtk_widget_destroy (dialog);
					destroy_subscription_info (subscription_info);
					return;
				}
			}

			/* It would be nice to insensitivize the OK button appropriately
			instead of doing this, but unfortunately we can't do this for the
			Bonobo control.  */
			e_error_run (GTK_WINDOW (dialog), ERROR_DOMAIN ":select-user", NULL);
		}

		folder_name = g_strdup (gtk_entry_get_text (GTK_ENTRY (subscription_info->folder_name_entry)));
		if (user_email_address && folder_name) {
			result = exchange_account_discover_shared_folder (subscription_info->account,
									  user_email_address,
									  folder_name, &folder);
			g_free (folder_name);
			gtk_widget_hide (dialog);
			switch (result) {
				case EXCHANGE_ACCOUNT_FOLDER_OK:
					exchange_account_rescan_tree (subscription_info->account);
					if (!g_ascii_strcasecmp (e_folder_get_type_string (folder), "mail"))
						e_error_run (NULL, ERROR_DOMAIN ":folder-restart-evo", NULL);
					break;
				case EXCHANGE_ACCOUNT_FOLDER_ALREADY_EXISTS:
					e_error_run (NULL, ERROR_DOMAIN ":folder-exists-error", NULL);
					break;
				case EXCHANGE_ACCOUNT_FOLDER_DOES_NOT_EXIST:
					e_error_run (NULL, ERROR_DOMAIN ":folder-doesnt-exist-error", NULL);
					break;
				case EXCHANGE_ACCOUNT_FOLDER_UNKNOWN_TYPE:
					e_error_run (NULL, ERROR_DOMAIN ":folder-unknown-type", NULL);
					break;
				case EXCHANGE_ACCOUNT_FOLDER_PERMISSION_DENIED:
					e_error_run (NULL, ERROR_DOMAIN ":folder-perm-error", NULL);
					break;
				case EXCHANGE_ACCOUNT_FOLDER_OFFLINE:
					e_error_run (NULL, ERROR_DOMAIN ":folder-offline-error", NULL);
					break;
				case EXCHANGE_ACCOUNT_FOLDER_UNSUPPORTED_OPERATION:
					e_error_run (NULL, ERROR_DOMAIN ":folder-unsupported-error", NULL);
					break;
				case EXCHANGE_ACCOUNT_FOLDER_GC_NOTREACHABLE:
					e_error_run (NULL, ERROR_DOMAIN ":folder-no-gc-error", NULL);
					break;
				case EXCHANGE_ACCOUNT_FOLDER_NO_SUCH_USER:
					e_error_run (NULL, ERROR_DOMAIN ":no-user-error", user_email_address, NULL);
					break;
				case EXCHANGE_ACCOUNT_FOLDER_GENERIC_ERROR:
					e_error_run (NULL, ERROR_DOMAIN ":folder-generic-error", NULL);
					break;
				default:
					break;
			}
		}

		if (!folder) {
			g_free (user_email_address);
			gtk_widget_destroy (dialog);
			return;
		}

		g_object_unref (folder);
		path = g_strdup_printf ("/%s", user_email_address);
		exchange_account_open_folder (subscription_info->account, path);
		g_free (path);
		g_free (user_email_address);
		gtk_widget_destroy (dialog);
		destroy_subscription_info (subscription_info);
	}
}

gboolean
create_folder_subscription_dialog (ExchangeAccount *account, const gchar *fname)
{
	ENameSelector *name_selector;
	GladeXML *glade_xml;
	GtkWidget *dialog, *ok_button;
	SubscriptionInfo *subscription_info;
	gint mode;

	exchange_account_is_offline (account, &mode);
	if (mode == OFFLINE_MODE)
		return FALSE;

	subscription_info = g_new0 (SubscriptionInfo, 1);
	subscription_info->account = account;

	glade_xml = glade_xml_new (CONNECTOR_GLADEDIR "/e-foreign-folder-dialog.glade",
				   NULL, NULL);
	g_return_val_if_fail (glade_xml != NULL, FALSE);

	dialog = glade_xml_get_widget (glade_xml, "dialog");
	g_return_val_if_fail (dialog != NULL, FALSE);
	gtk_window_set_modal (GTK_WINDOW (dialog), FALSE);
	gtk_window_set_title (GTK_WINDOW (dialog), _("Subscribe to Other User's Folder"));

	subscription_info->name_selector_widget = setup_name_selector (glade_xml, &name_selector);
	subscription_info->name_selector = name_selector;
	gtk_widget_grab_focus (subscription_info->name_selector_widget);

	ok_button = glade_xml_get_widget (glade_xml, "button1");
	gtk_widget_set_sensitive (ok_button, FALSE);
	g_signal_connect (subscription_info->name_selector_widget, "changed",
			  G_CALLBACK (user_name_entry_changed_callback), dialog);

	setup_server_combobox (glade_xml, account->account_name);
	setup_folder_name_combo (glade_xml, fname);
	subscription_info->folder_name_entry = gtk_bin_get_child (GTK_BIN (glade_xml_get_widget (glade_xml, "folder-name-combo")));
	g_signal_connect (dialog, "response", G_CALLBACK (subscribe_to_folder), subscription_info);
	gtk_widget_show (dialog);

	/* Connect the callback to set the OK button insensitive when there is
	   no text in the folder_name_entry.  Notice that we put a value there
	   by default so the OK button is sensitive by default.  */
	g_signal_connect (subscription_info->folder_name_entry, "changed",
			  G_CALLBACK (folder_name_entry_changed_callback), dialog);

	return TRUE;
}

