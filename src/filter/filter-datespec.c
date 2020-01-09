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
 *		Not Zed <notzed@lostzed.mmc.com.au>
 *      Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <glade/glade.h>

#include "filter-datespec.h"
#include "libedataserver/e-sexp.h"
#include "e-util/e-error.h"
#include "e-util/e-util-private.h"

#ifdef G_OS_WIN32
#define localtime_r(tp,tmp) memcpy(tmp,localtime(tp),sizeof(struct tm))
#endif

#define d(x)

static gboolean validate (FilterElement *fe);
static gint date_eq (FilterElement *fe, FilterElement *cm);
static void xml_create (FilterElement *fe, xmlNodePtr node);
static xmlNodePtr xml_encode (FilterElement *fe);
static gint xml_decode (FilterElement *fe, xmlNodePtr node);
static GtkWidget *get_widget (FilterElement *fe);
static void build_code (FilterElement *fe, GString *out, struct _FilterPart *fds);
static void format_sexp (FilterElement *, GString *);

static void filter_datespec_class_init (FilterDatespecClass *klass);
static void filter_datespec_init (FilterDatespec *fd);
static void filter_datespec_finalise (GObject *obj);

#define PRIV(x) (((FilterDatespec *)(x))->priv)

typedef struct _timespan {
	guint32 seconds;
	const gchar *past_singular;
	const gchar *past_plural;
	const gchar *future_singular;
	const gchar *future_plural;
	gfloat max;
} timespan;

#if 0
ngettext("1 second ago", "%d seconds ago")
ngettext("1 second in the future", "%d seconds in the future")
ngettext("1 minute ago", "%d minutes ago")
ngettext("1 minute in the future", "%d minutes in the future")
ngettext("1 hour ago", "%d hours ago")
ngettext("1 hour in the future", "%d hours in the future")
ngettext("1 day ago", "%d days ago")
ngettext("1 day in the future", "%d days in the future")
ngettext("1 week ago", "%d weeks ago")
ngettext("1 week in the future", "%d weeks in the future")
ngettext("1 month ago", "%d months ago")
ngettext("1 month in the future", "%d months in the future")
ngettext("1 year ago", "%d years ago")
ngettext("1 year in the future", "%d years in the future")
#endif

static const timespan timespans[] = {
	{ 1, "1 second ago", "%d seconds ago", "1 second in the future", "%d seconds in the future", 59.0 },
	{ 60, "1 minute ago", "%d minutes ago", "1 minute in the future", "%d minutes in the future", 59.0 },
	{ 3600, "1 hour ago", "%d hours ago", "1 hour in the future", "%d hours in the future", 23.0 },
	{ 86400, "1 day ago", "%d days ago", "1 day in the future", "%d days in the future", 31.0 },
	{ 604800, "1 week ago", "%d weeks ago", "1 week in the future", "%d weeks in the future", 52.0 },
	{ 2419200, "1 month ago", "%d months ago", "1 month in the future", "%d months in the future", 12.0 },
	{ 31557600, "1 year ago", "%d years ago", "1 year in the future", "%d years in the future", 1000.0 },
};

#define DAY_INDEX 3
#define N_TIMESPANS (sizeof (timespans) / sizeof (timespans[0]))

struct _FilterDatespecPrivate {
	GtkWidget *label_button;
	GtkWidget *notebook_type, *combobox_type, *calendar_specify, *spin_relative, *combobox_relative, *combobox_past_future;
	FilterDatespec_type type;
	gint span;
};

static FilterElementClass *parent_class;

GType
filter_datespec_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (FilterDatespecClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) filter_datespec_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (FilterDatespec),
			0,    /* n_preallocs */
			(GInstanceInitFunc) filter_datespec_init,
		};

		type = g_type_register_static (FILTER_TYPE_ELEMENT, "FilterDatespec", &info, 0);
	}

	return type;
}

static void
filter_datespec_class_init (FilterDatespecClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	FilterElementClass *fe_class = FILTER_ELEMENT_CLASS (klass);

	parent_class = g_type_class_ref (FILTER_TYPE_ELEMENT);

	object_class->finalize = filter_datespec_finalise;

	/* override methods */
	fe_class->validate = validate;
	fe_class->eq = date_eq;
	fe_class->xml_create = xml_create;
	fe_class->xml_encode = xml_encode;
	fe_class->xml_decode = xml_decode;
	fe_class->get_widget = get_widget;
	fe_class->build_code = build_code;
	fe_class->format_sexp = format_sexp;
}

static void
filter_datespec_init (FilterDatespec *fd)
{
	fd->priv = g_malloc0 (sizeof (*fd->priv));
	fd->type = FDST_UNKNOWN;
}

static void
filter_datespec_finalise (GObject *obj)
{
	FilterDatespec *fd = (FilterDatespec *) obj;

	g_free (fd->priv);

        G_OBJECT_CLASS (parent_class)->finalize (obj);
}

/**
 * filter_datespec_new:
 *
 * Create a new FilterDatespec object.
 *
 * Return value: A new #FilterDatespec object.
 **/
FilterDatespec *
filter_datespec_new (void)
{
	return (FilterDatespec *) g_object_new (FILTER_TYPE_DATESPEC, NULL, NULL);
}

static gboolean
validate (FilterElement *fe)
{
	FilterDatespec *fds = (FilterDatespec *) fe;
	gboolean valid;

	valid = fds->type != FDST_UNKNOWN;
	if (!valid) {
		/* FIXME: FilterElement should probably have a
                   GtkWidget member pointing to the value gotten with
                   ::get_widget() so that we can get the parent window
                   here. */
		e_error_run(NULL, "filter:no-date", NULL);
	}

	return valid;
}

static gint
date_eq (FilterElement *fe, FilterElement *cm)
{
	FilterDatespec *fd = (FilterDatespec *)fe, *cd = (FilterDatespec *)cm;

        return FILTER_ELEMENT_CLASS (parent_class)->eq(fe, cm)
		&& (fd->type == cd->type)
		&& (fd->value == cd->value);
}

static void
xml_create (FilterElement *fe, xmlNodePtr node)
{
	/* parent implementation */
        FILTER_ELEMENT_CLASS (parent_class)->xml_create (fe, node);
}

static xmlNodePtr
xml_encode (FilterElement *fe)
{
	xmlNodePtr value, work;
	FilterDatespec *fds = (FilterDatespec *)fe;
	gchar str[32];

	d(printf ("Encoding datespec as xml\n"));

	value = xmlNewNode (NULL, (const guchar *)"value");
	xmlSetProp (value, (const guchar *)"name", (guchar *)fe->name);
	xmlSetProp (value, (const guchar *)"type", (const guchar *)"datespec");

	work = xmlNewChild (value, NULL, (const guchar *)"datespec", NULL);
	sprintf (str, "%d", fds->type);
	xmlSetProp (work, (const guchar *)"type", (guchar *)str);
	sprintf (str, "%d", (gint)fds->value);
	xmlSetProp (work, (const guchar *)"value", (guchar *)str);

	return value;
}

static gint
xml_decode (FilterElement *fe, xmlNodePtr node)
{
	FilterDatespec *fds = (FilterDatespec *)fe;
	xmlNodePtr n;
	xmlChar *val;

	d(printf ("Decoding datespec from xml %p\n", fe));

	xmlFree (fe->name);
	fe->name = (gchar *)xmlGetProp (node, (const guchar *)"name");

	n = node->children;
	while (n) {
		if (!strcmp ((gchar *)n->name, "datespec")) {
			val = xmlGetProp (n, (const guchar *)"type");
			fds->type = atoi ((gchar *)val);
			xmlFree (val);
			val = xmlGetProp (n, (const guchar *)"value");
			fds->value = atoi ((gchar *)val);
			xmlFree (val);
			break;
		}
		n = n->next;
	}

	return 0;
}

static gint
get_best_span (time_t val)
{
	gint i;

	for (i=N_TIMESPANS-1;i>=0;i--) {
		if (val % timespans[i].seconds == 0)
			return i;
	}

	return 0;
}

/* sets button label */
static void
set_button (FilterDatespec *fds)
{
	gchar buf[128];
	gchar *label = buf;

	switch (fds->type) {
	case FDST_UNKNOWN:
		label = _("<click here to select a date>");
		break;
	case FDST_NOW:
		label = _("now");
		break;
	case FDST_SPECIFIED: {
		struct tm tm;

		localtime_r(&fds->value, &tm);
		/* strftime for date filter display, only needs to show a day date (i.e. no time) */
		strftime(buf, sizeof(buf), _("%d-%b-%Y"), &tm);
		break; }
	case FDST_X_AGO:
		if (fds->value == 0)
			label = _("now");
		else {
			gint span, count;

			span = get_best_span(fds->value);
			count = fds->value / timespans[span].seconds;
			sprintf(buf, ngettext(timespans[span].past_singular, timespans[span].past_plural, count), count);
		}
		break;
	case FDST_X_FUTURE:
		if (fds->value == 0)
			label = _("now");
		else {
			gint span, count;

			span = get_best_span(fds->value);
			count = fds->value / timespans[span].seconds;
			sprintf(buf, ngettext(timespans[span].future_singular, timespans[span].future_plural, count), count);
		}
		break;
	}

	gtk_label_set_text((GtkLabel *)fds->priv->label_button, label);
}

static void
get_values (FilterDatespec *fds)
{
	struct _FilterDatespecPrivate *p = PRIV(fds);

	switch (fds->priv->type) {
	case FDST_SPECIFIED: {
		guint year, month, day;
		struct tm tm;

		gtk_calendar_get_date((GtkCalendar *)p->calendar_specify, &year, &month, &day);
		memset(&tm, 0, sizeof(tm));
		tm.tm_mday = day;
		tm.tm_year = year - 1900;
		tm.tm_mon = month;
		fds->value = mktime(&tm);
		/* what about timezone? */
		break; }
	case FDST_X_FUTURE:
	case FDST_X_AGO: {
		gint val;

		val = gtk_spin_button_get_value_as_int((GtkSpinButton *)p->spin_relative);
		fds->value = timespans[p->span].seconds * val;
		break; }
	case FDST_NOW:
	default:
		break;
	}

	fds->type = p->type;
}

static void
set_values (FilterDatespec *fds)
{
	gint note_type;

	struct _FilterDatespecPrivate *p = PRIV(fds);

	p->type = fds->type==FDST_UNKNOWN ? FDST_NOW : fds->type;

	note_type = fds->type==FDST_X_FUTURE ? FDST_X_AGO : fds->type; /* FUTURE and AGO use the same notebook pages/etc. */

	switch (p->type) {
	case FDST_NOW:
	case FDST_UNKNOWN:
		/* noop */
		break;
	case FDST_SPECIFIED:
	{
		struct tm tm;

		localtime_r(&fds->value, &tm);
		gtk_calendar_select_month((GtkCalendar*)p->calendar_specify, tm.tm_mon, tm.tm_year + 1900);
		gtk_calendar_select_day((GtkCalendar*)p->calendar_specify, tm.tm_mday);
		break;
	}
	case FDST_X_AGO:
		p->span = get_best_span(fds->value);
		gtk_spin_button_set_value((GtkSpinButton*)p->spin_relative, fds->value/timespans[p->span].seconds);
		gtk_combo_box_set_active (GTK_COMBO_BOX (p->combobox_relative), p->span);
		gtk_combo_box_set_active (GTK_COMBO_BOX (p->combobox_past_future), 0);
		break;
	case FDST_X_FUTURE:
		p->span = get_best_span(fds->value);
		gtk_spin_button_set_value((GtkSpinButton*)p->spin_relative, fds->value/timespans[p->span].seconds);
		gtk_combo_box_set_active (GTK_COMBO_BOX (p->combobox_relative), p->span);
		gtk_combo_box_set_active (GTK_COMBO_BOX (p->combobox_past_future), 1);
		break;
	}

	gtk_notebook_set_current_page ((GtkNotebook*) p->notebook_type, note_type);
	gtk_combo_box_set_active (GTK_COMBO_BOX (p->combobox_type), note_type);
}

static void
set_combobox_type (GtkComboBox *combobox, FilterDatespec *fds)
{
	fds->priv->type = gtk_combo_box_get_active (combobox);
	gtk_notebook_set_current_page ((GtkNotebook*) fds->priv->notebook_type, fds->priv->type);
}

static void
set_combobox_relative (GtkComboBox *combobox, FilterDatespec *fds)
{
	fds->priv->span = gtk_combo_box_get_active (combobox);
}

static void
set_combobox_past_future (GtkComboBox *combobox, FilterDatespec *fds)
{
	if (gtk_combo_box_get_active (combobox) == 0)
		fds->type = fds->priv->type = FDST_X_AGO;
	else
		fds->type = fds->priv->type = FDST_X_FUTURE;
}

static void
button_clicked (GtkButton *button, FilterDatespec *fds)
{
	struct _FilterDatespecPrivate *p = PRIV(fds);
	GtkWidget *toplevel;
	GtkDialog *dialog;
	GladeXML *gui;
	gchar *filter_glade = g_build_filename (EVOLUTION_GLADEDIR,
					       "filter.glade",
					       NULL);

	gui = glade_xml_new (filter_glade, "filter_datespec", NULL);
	g_free (filter_glade);
	toplevel = glade_xml_get_widget (gui, "filter_datespec");

	dialog = (GtkDialog *) gtk_dialog_new ();
	gtk_window_set_title ((GtkWindow *) dialog, _("Select a time to compare against"));
	gtk_dialog_add_buttons (dialog,
				GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				GTK_STOCK_OK, GTK_RESPONSE_OK,
				NULL);
	gtk_dialog_set_has_separator (dialog, FALSE);

	p->notebook_type = glade_xml_get_widget (gui, "notebook_type");
	p->combobox_type = glade_xml_get_widget (gui, "combobox_type");
	p->calendar_specify = glade_xml_get_widget (gui, "calendar_specify");
	p->spin_relative = glade_xml_get_widget (gui, "spin_relative");
	p->combobox_relative = glade_xml_get_widget (gui, "combobox_relative");
	p->combobox_past_future = glade_xml_get_widget (gui, "combobox_past_future");

	set_values (fds);

	g_signal_connect (p->combobox_type, "changed", G_CALLBACK (set_combobox_type), fds);
	g_signal_connect (p->combobox_relative, "changed", G_CALLBACK (set_combobox_relative), fds);
	g_signal_connect (p->combobox_past_future, "changed", G_CALLBACK (set_combobox_past_future), fds);

	gtk_box_pack_start ((GtkBox *) dialog->vbox, toplevel, TRUE, TRUE, 3);

	if (gtk_dialog_run (dialog) == GTK_RESPONSE_OK) {
		get_values (fds);
		set_button (fds);
	}

	gtk_widget_destroy ((GtkWidget *)dialog);
}

static GtkWidget *
get_widget (FilterElement *fe)
{
	FilterDatespec *fds = (FilterDatespec *)fe;
	GtkWidget *button;

	fds->priv->label_button = gtk_label_new ("");
	gtk_misc_set_alignment (GTK_MISC (fds->priv->label_button), 0.5, 0.5);
	set_button(fds);

	button = gtk_button_new();
	gtk_container_add (GTK_CONTAINER (button), fds->priv->label_button);
	g_signal_connect (button, "clicked", G_CALLBACK (button_clicked), fds);

	gtk_widget_show (button);
	gtk_widget_show (fds->priv->label_button);

	return button;
}

static void
build_code (FilterElement *fe, GString *out, struct _FilterPart *fp)
{
	return;
}

static void
format_sexp (FilterElement *fe, GString *out)
{
	FilterDatespec *fds = (FilterDatespec *)fe;

	switch (fds->type) {
	case FDST_UNKNOWN:
		g_warning ("user hasn't selected a datespec yet!");
		/* fall through */
	case FDST_NOW:
		g_string_append (out, "(get-current-date)");
		break;
	case FDST_SPECIFIED:
		g_string_append_printf (out, "%d", (gint) fds->value);
		break;
	case FDST_X_AGO:
		g_string_append_printf (out, "(- (get-current-date) %d)", (gint) fds->value);
		break;
	case FDST_X_FUTURE:
		g_string_append_printf (out, "(+ (get-current-date) %d)", (gint) fds->value);
		break;
	}
}
