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
 *      Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-cert.h"
#include "e-cert-trust.h"
#include "e-cert-db.h"
#include "cert-trust-dialog.h"
#include "ca-trust-dialog.h"

#include <gtk/gtk.h>

#include <glib/gi18n.h>
#include <glade/glade.h>

#include "e-util/e-util-private.h"

#define GLADE_FILE_NAME "smime-ui.glade"

typedef struct {
	GladeXML *gui;
	GtkWidget *dialog;
	GtkWidget *trust_button;
	GtkWidget *notrust_button;
	GtkWidget *label;

	ECert *cert, *cacert;
} CertTrustDialogData;

static void
free_data (gpointer data)
{
	CertTrustDialogData *ctd = data;

	g_object_unref (ctd->cert);
	g_object_unref (ctd->cacert);
	g_object_unref (ctd->gui);
	g_free (ctd);
}

static void
ctd_response(GtkWidget *w, guint id, CertTrustDialogData *data)
{
	CERTCertTrust trust;
	CERTCertificate *icert;

	switch (id) {
	case GTK_RESPONSE_OK:
		icert = e_cert_get_internal_cert(data->cert);
		e_cert_trust_init(&trust);
		e_cert_trust_set_valid_peer(&trust);
		e_cert_trust_add_peer_trust (&trust, FALSE,
					     gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON (data->trust_button)),
					     FALSE);
		CERT_ChangeCertTrust (CERT_GetDefaultCertDB(), icert, &trust);
		break;
	case GTK_RESPONSE_ACCEPT: {
		/* just *what on earth* was chris thinking here!?!?!  copied from certificate-manager.c */
		GtkWidget *dialog = ca_trust_dialog_show (data->cacert, FALSE);
		/* *icert is already declared earlier in this function? */
		CERTCertificate *icert = e_cert_get_internal_cert (data->cacert);

		g_signal_stop_emission_by_name(w, "response");

		ca_trust_dialog_set_trust (dialog,
					   e_cert_trust_has_trusted_ca (icert->trust, TRUE,  FALSE, FALSE),
					   e_cert_trust_has_trusted_ca (icert->trust, FALSE, TRUE,  FALSE),
					   e_cert_trust_has_trusted_ca (icert->trust, FALSE, FALSE, TRUE));

		if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_OK) {
			gboolean trust_ssl, trust_email, trust_objsign;

			ca_trust_dialog_get_trust (dialog,
						   &trust_ssl, &trust_email, &trust_objsign);

			e_cert_trust_init (&trust);
			e_cert_trust_set_valid_ca (&trust);
			e_cert_trust_add_ca_trust (&trust,
						   trust_ssl,
						   trust_email,
						   trust_objsign);

			CERT_ChangeCertTrust(CERT_GetDefaultCertDB(), icert, &trust);
		}

		gtk_widget_destroy (dialog);
		break; }
	}
}

GtkWidget*
cert_trust_dialog_show (ECert *cert)
{
	CertTrustDialogData *ctd_data;
	CERTCertificate *icert;
	gchar *gladefile;

	ctd_data = g_new0 (CertTrustDialogData, 1);

	gladefile = g_build_filename (EVOLUTION_GLADEDIR,
				      GLADE_FILE_NAME,
				      NULL);
	ctd_data->gui = glade_xml_new (gladefile, NULL, NULL);
	g_free (gladefile);

	ctd_data->dialog = glade_xml_get_widget (ctd_data->gui, "cert-trust-dialog");
	ctd_data->cert = g_object_ref (cert);
	ctd_data->cacert = e_cert_get_ca_cert(cert);
	ctd_data->trust_button = glade_xml_get_widget(ctd_data->gui, "cert-trust");
	ctd_data->notrust_button = glade_xml_get_widget(ctd_data->gui, "cert-notrust");

	ctd_data->label = glade_xml_get_widget(ctd_data->gui, "trust-label");

	g_signal_connect(ctd_data->dialog, "response", G_CALLBACK(ctd_response), ctd_data);

	g_object_set_data_full (G_OBJECT (ctd_data->dialog), "CertTrustDialogData", ctd_data, free_data);

	icert = e_cert_get_internal_cert(cert);
	if (e_cert_trust_has_trusted_peer(icert->trust, FALSE, TRUE, FALSE))
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (ctd_data->trust_button), TRUE);
	else
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON (ctd_data->notrust_button), TRUE);

	icert = e_cert_get_internal_cert(ctd_data->cacert);
	if (e_cert_trust_has_trusted_ca(icert->trust, FALSE, TRUE, FALSE))
		gtk_label_set_text((GtkLabel *)ctd_data->label,
				   _("Because you trust the certificate authority that issued this certificate, "
				     "then you trust the authenticity of this certificate unless otherwise indicated here"));
	else
		gtk_label_set_text((GtkLabel *)ctd_data->label,
				   _("Because you do not trust the certificate authority that issued this certificate, "
				     "then you do not trust the authenticity of this certificate unless otherwise indicated here"));

	return ctd_data->dialog;
}
