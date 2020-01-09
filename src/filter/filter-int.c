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
 *		Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdlib.h>
#include <gtk/gtk.h>

#include <libedataserver/e-sexp.h>
#include "filter-int.h"

#define d(x)

static gint int_eq (FilterElement *fe, FilterElement *cm);
static FilterElement *int_clone(FilterElement *fe);
static void xml_create (FilterElement *fe, xmlNodePtr node);
static xmlNodePtr xml_encode (FilterElement *fe);
static gint xml_decode (FilterElement *fe, xmlNodePtr node);
static GtkWidget *get_widget (FilterElement *fe);
static void build_code (FilterElement *fe, GString *out, struct _FilterPart *ff);
static void format_sexp (FilterElement *fe, GString *out);

static void filter_int_class_init (FilterIntClass *klass);
static void filter_int_init (FilterInt *fi);
static void filter_int_finalise (GObject *obj);

static FilterElementClass *parent_class;

GType
filter_int_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof (FilterIntClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc) filter_int_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (FilterInt),
			0,    /* n_preallocs */
			(GInstanceInitFunc) filter_int_init,
		};

		type = g_type_register_static (FILTER_TYPE_ELEMENT, "FilterInt", &info, 0);
	}

	return type;
}

static void
filter_int_class_init (FilterIntClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	FilterElementClass *fe_class = FILTER_ELEMENT_CLASS (klass);

	parent_class = g_type_class_ref (FILTER_TYPE_ELEMENT);

	object_class->finalize = filter_int_finalise;

	/* override methods */
	fe_class->eq = int_eq;
	fe_class->clone = int_clone;
	fe_class->xml_create = xml_create;
	fe_class->xml_encode = xml_encode;
	fe_class->xml_decode = xml_decode;
	fe_class->get_widget = get_widget;
	fe_class->build_code = build_code;
	fe_class->format_sexp = format_sexp;
}

static void
filter_int_init (FilterInt *fi)
{
	fi->min = 0;
	fi->max = G_MAXINT;
}

static void
filter_int_finalise (GObject *obj)
{
	FilterInt *fi = (FilterInt *) obj;

	g_free (fi->type);

        G_OBJECT_CLASS (parent_class)->finalize (obj);
}

/**
 * filter_int_new:
 *
 * Create a new FilterInt object.
 *
 * Return value: A new #FilterInt object.
 **/
FilterInt *
filter_int_new (void)
{
	return (FilterInt *) g_object_new (FILTER_TYPE_INT, NULL, NULL);
}

FilterInt *
filter_int_new_type (const gchar *type, gint min, gint max)
{
	FilterInt *fi;

	fi = filter_int_new ();

	fi->type = g_strdup (type);
	fi->min = min;
	fi->max = max;

	return fi;
}

void
filter_int_set_value (FilterInt *fi, gint val)
{
	fi->val = val;
}

static gint
int_eq (FilterElement *fe, FilterElement *cm)
{
        return FILTER_ELEMENT_CLASS (parent_class)->eq (fe, cm)
		&& ((FilterInt *)fe)->val == ((FilterInt *)cm)->val;
}

static FilterElement *
int_clone(FilterElement *fe)
{
	FilterInt *fi, *fs;

	fs = (FilterInt *)fe;
	fi = filter_int_new_type(fs->type, fs->min, fs->max);
	fi->val = fs->val;
	((FilterElement *)fi)->name = g_strdup(fe->name);

	return (FilterElement *)fi;
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
	xmlNodePtr value;
	FilterInt *fs = (FilterInt *)fe;
	gchar intval[32];
	const gchar *type;

	type = fs->type?fs->type:"integer";

	d(printf("Encoding %s as xml\n", type));

	value = xmlNewNode (NULL, (const guchar *)"value");
	xmlSetProp (value, (const guchar *)"name", (guchar *)fe->name);
	xmlSetProp (value, (const guchar *)"type", (guchar *)type);

	sprintf(intval, "%d", fs->val);
	xmlSetProp (value, (guchar *)type, (guchar *)intval);

	return value;
}

static gint
xml_decode (FilterElement *fe, xmlNodePtr node)
{
	FilterInt *fs = (FilterInt *)fe;
	gchar *name, *type;
	gchar *intval;

	d(printf("Decoding integer from xml %p\n", fe));

	name = (gchar *)xmlGetProp (node, (const guchar *)"name");
	d(printf ("Name = %s\n", name));
	xmlFree (fe->name);
	fe->name = name;

	type = (gchar *)xmlGetProp(node, (const guchar *)"type");
	d(printf ("Type = %s\n", type));
	g_free(fs->type);
	fs->type = g_strdup(type);
	xmlFree(type);

	intval = (gchar *)xmlGetProp (node, (guchar *)(fs->type ? fs->type : "integer"));
	if (intval) {
		d(printf ("Value = %s\n", intval));
		fs->val = atoi (intval);
		xmlFree (intval);
	} else {
		d(printf ("Value = ?unknown?\n"));
		fs->val = 0;
	}

	return 0;
}

static void
spin_changed (GtkWidget *spin, FilterElement *fe)
{
	FilterInt *fs = (FilterInt *)fe;

	fs->val = gtk_spin_button_get_value_as_int (GTK_SPIN_BUTTON (spin));
}

static GtkWidget *
get_widget (FilterElement *fe)
{
	GtkWidget *spin;
	GtkObject *adjustment;
	FilterInt *fs = (FilterInt *)fe;

	adjustment = gtk_adjustment_new (0.0, (gfloat)fs->min, (gfloat)fs->max, 1.0, 1.0, 0);
	spin = gtk_spin_button_new (GTK_ADJUSTMENT (adjustment), fs->max>fs->min+1000?5.0:1.0, 0);
	gtk_spin_button_set_numeric (GTK_SPIN_BUTTON (spin), TRUE);

	if (fs->val)
		gtk_spin_button_set_value (GTK_SPIN_BUTTON (spin), (gfloat) fs->val);

	g_signal_connect (spin, "value-changed", G_CALLBACK (spin_changed), fe);

	return spin;
}

static void
build_code (FilterElement *fe, GString *out, struct _FilterPart *ff)
{
	return;
}

static void
format_sexp (FilterElement *fe, GString *out)
{
	FilterInt *fs = (FilterInt *)fe;

	if (fs->val < 0)
		/* See #364731 #457523 C6*/
		g_string_append_printf (out, "(- %d)", (fs->val * -1));
	else
		g_string_append_printf (out, "%d", fs->val);
}
