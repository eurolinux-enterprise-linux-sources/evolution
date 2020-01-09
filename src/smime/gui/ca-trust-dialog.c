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
 *		Chris Toshok <toshok@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "ca-trust-dialog.h"
#include "certificate-viewer.h"

#include <glib/gi18n.h>
#include <glade/glade.h>

#include "e-util/e-util-private.h"

#define GLADE_FILE_NAME "smime-ui.glade"

typedef struct {
	GladeXML *gui;
	GtkWidget *dialog;
	GtkWidget *ssl_checkbutton;
	GtkWidget *email_checkbutton;
	GtkWidget *objsign_checkbutton;

	ECert *cert;
} CATrustDialogData;

static void
free_data (gpointer data)
{
	CATrustDialogData *ctd = data;

	g_object_unref (ctd->cert);
	g_object_unref (ctd->gui);
	g_free (ctd);
}

static void
catd_response(GtkWidget *w, guint id, CATrustDialogData *data)
{
	switch (id) {
	case GTK_RESPONSE_ACCEPT: {
		GtkWidget *dialog = certificate_viewer_show (data->cert);

		g_signal_stop_emission_by_name(w, "response");
		gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (data->dialog));
		gtk_dialog_run (GTK_DIALOG (dialog));
		gtk_widget_destroy (dialog);
		break; }
	}
}

GtkWidget*
ca_trust_dialog_show (ECert *cert, gboolean importing)
{
	CATrustDialogData *ctd_data;
	GtkWidget *w;
	gchar *txt;
	gchar *gladefile;

	ctd_data = g_new0 (CATrustDialogData, 1);

	gladefile = g_build_filename (EVOLUTION_GLADEDIR,
				      GLADE_FILE_NAME,
				      NULL);
	ctd_data->gui = glade_xml_new (gladefile, NULL, NULL);
	g_free (gladefile);

	ctd_data->dialog = glade_xml_get_widget (ctd_data->gui, "ca-trust-dialog");

	gtk_widget_ensure_style (ctd_data->dialog);
	gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (ctd_data->dialog)->vbox), 0);
	gtk_container_set_border_width (GTK_CONTAINER (GTK_DIALOG (ctd_data->dialog)->action_area), 12);

	ctd_data->cert = g_object_ref (cert);

	ctd_data->ssl_checkbutton = glade_xml_get_widget (ctd_data->gui, "ssl_trust_checkbutton");
	ctd_data->email_checkbutton = glade_xml_get_widget (ctd_data->gui, "email_trust_checkbutton");
	ctd_data->objsign_checkbutton = glade_xml_get_widget (ctd_data->gui, "objsign_trust_checkbutton");

	w = glade_xml_get_widget(ctd_data->gui, "ca-trust-label");
	txt = g_strdup_printf(_("Certificate '%s' is a CA certificate.\n\nEdit trust settings:"), e_cert_get_cn(cert));
	gtk_label_set_text((GtkLabel *)w, txt);
	g_free(txt);

	g_signal_connect (ctd_data->dialog, "response", G_CALLBACK (catd_response), ctd_data);

	g_object_set_data_full (G_OBJECT (ctd_data->dialog), "CATrustDialogData", ctd_data, free_data);

	return ctd_data->dialog;
}

void
ca_trust_dialog_set_trust (GtkWidget *widget, gboolean ssl, gboolean email, gboolean objsign)
{
	CATrustDialogData *ctd_data;

	ctd_data = g_object_get_data (G_OBJECT (widget), "CATrustDialogData");
	if (!ctd_data)
		return;

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ctd_data->ssl_checkbutton), ssl);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ctd_data->email_checkbutton), email);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (ctd_data->objsign_checkbutton), objsign);
}

void
ca_trust_dialog_get_trust (GtkWidget *widget, gboolean *ssl, gboolean *email, gboolean *objsign)
{
	CATrustDialogData *ctd_data;

	ctd_data = g_object_get_data (G_OBJECT (widget), "CATrustDialogData");
	if (!ctd_data)
		return;

	*ssl = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (ctd_data->ssl_checkbutton));
	*email = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (ctd_data->email_checkbutton));
	*objsign = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (ctd_data->objsign_checkbutton));
}
