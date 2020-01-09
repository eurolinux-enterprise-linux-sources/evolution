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
 *		Michael Zucchi <notzed@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>
#include <string.h>
#include <stdio.h>

#include "camel/camel-multipart.h"
#include "camel/camel-mime-part.h"
#include "mail/em-format-hook.h"
#include "mail/em-format.h"

#include <gconf/gconf-client.h>
#include "mail/em-config.h"

void org_gnome_prefer_plain_multipart_alternative(gpointer ep, EMFormatHookTarget *t);
void org_gnome_prefer_plain_text_html(gpointer ep, EMFormatHookTarget *t);
GtkWidget *org_gnome_prefer_plain_config_mode(struct _EPlugin *epl, struct _EConfigHookItemFactoryData *data);

enum {
	EPP_NORMAL,
	EPP_PREFER,
	EPP_TEXT
};

static GConfClient *epp_gconf = NULL;
static gint epp_mode = -1;

void
org_gnome_prefer_plain_text_html(gpointer ep, EMFormatHookTarget *t)
{
	/* In text-only mode, all html output is suppressed */
	if (epp_mode != EPP_TEXT)
		t->item->handler.old->handler(t->format, t->stream, t->part, t->item->handler.old);
	else
		em_format_part_as(t->format, t->stream, t->part, NULL);
}

static void
export_as_attachments (CamelMultipart *mp, EMFormat *format, CamelStream *stream, CamelMimePart *except)
{
	gint i, nparts, partidlen;
	CamelMimePart *part;

	if (!mp || !CAMEL_IS_MULTIPART (mp))
		return;

	partidlen = format->part_id->len;

	nparts = camel_multipart_get_number(mp);
	for (i = 0; i < nparts; i++) {
		part = camel_multipart_get_part (mp, i);

		if (part != except) {
			CamelMultipart *multipart = (CamelMultipart *)camel_medium_get_content_object((CamelMedium *)part);

			if (CAMEL_IS_MULTIPART (multipart)) {
				export_as_attachments (multipart, format, stream, except);
			} else {
				g_string_append_printf (format->part_id, ".alternative.%d", i);

				if (camel_content_type_is (camel_mime_part_get_content_type (part), "text", "html")) {
					/* always show HTML as attachments and not inline */
					camel_mime_part_set_disposition (part, "attachment");

					if (!camel_mime_part_get_filename (part)) {
						gchar *str = g_strdup_printf ("%s.html", _("attachment"));
						camel_mime_part_set_filename (part, str);
						g_free (str);
					}

					em_format_part_as (format, stream, part, "application/octet-stream");
				} else
					em_format_part (format, stream, part);

				g_string_truncate (format->part_id, partidlen);
			}
		}
	}
}

void
org_gnome_prefer_plain_multipart_alternative(gpointer ep, EMFormatHookTarget *t)
{
	CamelMultipart *mp = (CamelMultipart *)camel_medium_get_content_object((CamelMedium *)t->part);
	CamelMimePart *part, *display_part = NULL;
	gint i, nparts, partidlen, displayid = 0;

	/* FIXME: this part-id stuff is poking private data, needs api */
	partidlen = t->format->part_id->len;

	if (epp_mode == EPP_NORMAL) {
		gboolean have_plain = FALSE;

		/* Try to find text/html part even when not as last and force to show it.
		   Old handler will show the last part of multipart/alternate, but if we
		   can offer HTML, then offer it, regardless of position in multipart.
		   But do this when have only text/plain and text/html parts, not more.
		*/
		nparts = camel_multipart_get_number (mp);
		for (i = 0; i < nparts; i++) {
			CamelContentType *content_type;

			part = camel_multipart_get_part (mp, i);

			if (!part)
				continue;

			content_type = camel_mime_part_get_content_type (part);

			if (camel_content_type_is (content_type, "text", "html")) {
				displayid = i;
				display_part = part;

				if (have_plain)
					break;
			} else if (camel_content_type_is (content_type, "text", "plain")) {
				have_plain = TRUE;

				if (display_part)
					break;
			}
		}

		if (display_part && have_plain && nparts == 2) {
			g_string_append_printf (t->format->part_id, ".alternative.%d", displayid);
			em_format_part_as (t->format, t->stream, display_part, "text/html");
			g_string_truncate (t->format->part_id, partidlen);
		} else {
			t->item->handler.old->handler (t->format, t->stream, t->part, t->item->handler.old);
		}
		return;
	} else if (!CAMEL_IS_MULTIPART(mp)) {
		em_format_format_source(t->format, t->stream, t->part);
		return;
	}

	nparts = camel_multipart_get_number(mp);
	for (i=0; i<nparts; i++) {
		part = camel_multipart_get_part(mp, i);
		if (part && camel_content_type_is(camel_mime_part_get_content_type(part), "text", "plain")) {
			displayid = i;
			display_part = part;
			break;
		}
	}

	/* if we found a text part, show it */
	if (display_part) {
		g_string_append_printf(t->format->part_id, ".alternative.%d", displayid);
		em_format_part_as(t->format, t->stream, display_part, "text/plain");
		g_string_truncate(t->format->part_id, partidlen);
	}

	/* all other parts are attachments */
	export_as_attachments (mp, t->format, t->stream, display_part);

	g_string_truncate(t->format->part_id, partidlen);
}

static struct {
	const gchar *label;
	const gchar *key;
} epp_options[] = {
	{ N_("Show HTML if present"), "normal" },
	{ N_("Prefer PLAIN"), "prefer_plain" },
	{ N_("Only ever show PLAIN"), "only_plain" },
};

static void
epp_mode_changed(GtkComboBox *dropdown, gpointer dummy)
{
	epp_mode = gtk_combo_box_get_active(dropdown);
	if (epp_mode > 2)
		epp_mode = 0;

	gconf_client_set_string(epp_gconf, "/apps/evolution/eplugin/prefer_plain/mode", epp_options[epp_mode].key, NULL);
}

GtkWidget *
org_gnome_prefer_plain_config_mode(struct _EPlugin *epl, struct _EConfigHookItemFactoryData *data)
{
	/*EMConfigTargetPrefs *ep = (EMConfigTargetPrefs *)data->target;*/
	GtkComboBox *dropdown;
	GtkCellRenderer *cell;
	GtkListStore *store;
	GtkWidget *w;
	gint i;
	GtkTreeIter iter;

	if (data->old)
		return data->old;

	dropdown = (GtkComboBox *)gtk_combo_box_new();
	cell = gtk_cell_renderer_text_new();
	store = gtk_list_store_new(1, G_TYPE_STRING);
	for (i=0;i<sizeof(epp_options)/sizeof(epp_options[0]);i++) {
		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter, 0, _(epp_options[i].label), -1);
	}

	gtk_cell_layout_pack_start((GtkCellLayout *)dropdown, cell, TRUE);
	gtk_cell_layout_set_attributes((GtkCellLayout *)dropdown, cell, "text", 0, NULL);
	gtk_combo_box_set_model(dropdown, (GtkTreeModel *)store);
	/*gtk_combo_box_set_active(dropdown, -1);*/
	gtk_combo_box_set_active(dropdown, epp_mode);
	g_signal_connect(dropdown, "changed", G_CALLBACK(epp_mode_changed), NULL);
	gtk_widget_show((GtkWidget *)dropdown);

	w = gtk_label_new_with_mnemonic(_("HTML _Mode"));
	gtk_widget_show(w);
	gtk_label_set_mnemonic_widget(GTK_LABEL(w),(GtkWidget *)dropdown);
	i = ((GtkTable *)data->parent)->nrows;
	gtk_table_attach((GtkTable *)data->parent, w, 0, 1, i, i+1, 0, 0, 0, 0);
	gtk_table_attach((GtkTable *)data->parent, (GtkWidget *)dropdown, 1, 2, i, i+1, GTK_FILL|GTK_EXPAND, 0, 0, 0);

	/* since this isnt dynamic, we don't need to track each item */

	return (GtkWidget *)dropdown;
}

gint e_plugin_lib_enable(EPluginLib *ep, gint enable);

gint
e_plugin_lib_enable(EPluginLib *ep, gint enable)
{
	gchar *key;
	gint i;

	if (epp_gconf || epp_mode != -1)
		return 0;

	if (enable) {
		epp_gconf = gconf_client_get_default();
		key = gconf_client_get_string(epp_gconf, "/apps/evolution/eplugin/prefer_plain/mode", NULL);
		if (key) {
			for (i=0;i<sizeof(epp_options)/sizeof(epp_options[0]);i++) {
				if (!strcmp(epp_options[i].key, key)) {
					epp_mode = i;
					break;
				}
			}
			g_free (key);
		} else {
			epp_mode = 0;
		}
	} else {
		if (epp_gconf) {
			g_object_unref(epp_gconf);
			epp_gconf = NULL;
		}
	}

	return 0;
}
