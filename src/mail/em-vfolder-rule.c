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

#include <gtk/gtk.h>
#include <glade/glade.h>
#include <glib/gi18n.h>

#include "camel/camel-url.h"
#include "em-vfolder-context.h"
#include "em-vfolder-rule.h"
#include "mail/em-utils.h"
#include "mail/em-folder-tree.h"
#include "mail/em-folder-selector.h"
#include "mail/mail-component.h"
#include "e-util/e-error.h"
#include "e-util/e-util-private.h"

#define d(x)

static gint validate(FilterRule *);
static gint vfolder_eq(FilterRule *fr, FilterRule *cm);
static xmlNodePtr xml_encode(FilterRule *);
static gint xml_decode(FilterRule *, xmlNodePtr, RuleContext *f);
static void rule_copy(FilterRule *dest, FilterRule *src);
/*static void build_code(FilterRule *, GString *out);*/
static GtkWidget *get_widget(FilterRule *fr, RuleContext *f);

static void em_vfolder_rule_class_init(EMVFolderRuleClass *klass);
static void em_vfolder_rule_init(EMVFolderRule *vr);
static void em_vfolder_rule_finalise(GObject *obj);

/* DO NOT internationalise these strings */
static const gchar *with_names[] = {
	"specific",
	"local_remote_active",
	"remote_active",
	"local"
};

static FilterRuleClass *parent_class = NULL;

GType
em_vfolder_rule_get_type(void)
{
	static GType type = 0;

	if (!type) {
		static const GTypeInfo info = {
			sizeof(EMVFolderRuleClass),
			NULL, /* base_class_init */
			NULL, /* base_class_finalize */
			(GClassInitFunc)em_vfolder_rule_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof(EMVFolderRule),
			0,    /* n_preallocs */
			(GInstanceInitFunc)em_vfolder_rule_init,
		};

		type = g_type_register_static(FILTER_TYPE_RULE, "EMVFolderRule", &info, 0);
	}

	return type;
}

static void
em_vfolder_rule_class_init(EMVFolderRuleClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	FilterRuleClass *fr_class =(FilterRuleClass *)klass;

	parent_class = g_type_class_ref(FILTER_TYPE_RULE);

	object_class->finalize = em_vfolder_rule_finalise;

	/* override methods */
	fr_class->validate   = validate;
	fr_class->eq = vfolder_eq;
	fr_class->xml_encode = xml_encode;
	fr_class->xml_decode = xml_decode;
	fr_class->copy = rule_copy;
	/*fr_class->build_code = build_code;*/
	fr_class->get_widget = get_widget;
}

static void
em_vfolder_rule_init(EMVFolderRule *vr)
{
	vr->with = EM_VFOLDER_RULE_WITH_SPECIFIC;
	vr->rule.source = g_strdup("incoming");
}

static void
em_vfolder_rule_finalise(GObject *obj)
{
	EMVFolderRule *vr =(EMVFolderRule *)obj;

	g_list_foreach(vr->sources, (GFunc)g_free, NULL);
	g_list_free(vr->sources);

        G_OBJECT_CLASS(parent_class)->finalize(obj);
}

/**
 * em_vfolder_rule_new:
 *
 * Create a new EMVFolderRule object.
 *
 * Return value: A new #EMVFolderRule object.
 **/
EMVFolderRule *
em_vfolder_rule_new(void)
{
	return (EMVFolderRule *)g_object_new(em_vfolder_rule_get_type(), NULL, NULL);
}

void
em_vfolder_rule_add_source(EMVFolderRule *vr, const gchar *uri)
{
	g_return_if_fail (EM_IS_VFOLDER_RULE(vr));
	g_return_if_fail (uri);

	vr->sources = g_list_append(vr->sources, g_strdup(uri));

	filter_rule_emit_changed((FilterRule *)vr);
}

const gchar *
em_vfolder_rule_find_source(EMVFolderRule *vr, const gchar *uri)
{
	GList *l;

	g_return_val_if_fail (EM_IS_VFOLDER_RULE(vr), NULL);

	/* only does a simple string or address comparison, should
	   probably do a decoded url comparison */
	l = vr->sources;
	while (l) {
		if (l->data == uri || !strcmp(l->data, uri))
			return l->data;
		l = l->next;
	}

	return NULL;
}

void
em_vfolder_rule_remove_source(EMVFolderRule *vr, const gchar *uri)
{
	gchar *found;

	g_return_if_fail (EM_IS_VFOLDER_RULE(vr));

	found =(gchar *)em_vfolder_rule_find_source(vr, uri);
	if (found) {
		vr->sources = g_list_remove(vr->sources, found);
		g_free(found);
		filter_rule_emit_changed((FilterRule *)vr);
	}
}

const gchar *
em_vfolder_rule_next_source(EMVFolderRule *vr, const gchar *last)
{
	GList *node;

	if (last == NULL) {
		node = vr->sources;
	} else {
		node = g_list_find(vr->sources, (gchar *)last);
		if (node == NULL)
			node = vr->sources;
		else
			node = g_list_next(node);
	}

	if (node)
		return (const gchar *)node->data;

	return NULL;
}

static gint
validate(FilterRule *fr)
{
	g_return_val_if_fail(fr != NULL, 0);

	if (!fr->name || !*fr->name) {
		/* FIXME: set a parent window? */
		e_error_run(NULL, "mail:no-name-vfolder", NULL);
		return 0;
	}

	/* We have to have at least one source set in the "specific" case.
	   Do not translate this string! */
	if (((EMVFolderRule *)fr)->with == EM_VFOLDER_RULE_WITH_SPECIFIC && ((EMVFolderRule *)fr)->sources == NULL) {
		/* FIXME: set a parent window? */
		e_error_run(NULL, "mail:vfolder-no-source", NULL);
		return 0;
	}

	return FILTER_RULE_CLASS(parent_class)->validate(fr);
}

static gint
list_eq(GList *al, GList *bl)
{
	gint truth = TRUE;

	while (truth && al && bl) {
		gchar *a = al->data, *b = bl->data;

		truth = strcmp(a, b)== 0;
		al = al->next;
		bl = bl->next;
	}

	return truth && al == NULL && bl == NULL;
}

static gint
vfolder_eq(FilterRule *fr, FilterRule *cm)
{
        return FILTER_RULE_CLASS(parent_class)->eq(fr, cm)
		&& list_eq(((EMVFolderRule *)fr)->sources, ((EMVFolderRule *)cm)->sources);
}

static xmlNodePtr
xml_encode(FilterRule *fr)
{
	EMVFolderRule *vr =(EMVFolderRule *)fr;
	xmlNodePtr node, set, work;
	GList *l;

        node = FILTER_RULE_CLASS(parent_class)->xml_encode(fr);
	g_return_val_if_fail (node != NULL, NULL);
	g_return_val_if_fail (vr->with < sizeof(with_names)/sizeof(with_names[0]), NULL);

	set = xmlNewNode(NULL, (const guchar *)"sources");
	xmlAddChild(node, set);
	xmlSetProp(set, (const guchar *)"with", (guchar *)with_names[vr->with]);
	l = vr->sources;
	while (l) {
		work = xmlNewNode(NULL, (const guchar *)"folder");
		xmlSetProp(work, (const guchar *)"uri", (guchar *)l->data);
		xmlAddChild(set, work);
		l = l->next;
	}

	return node;
}

static void
set_with(EMVFolderRule *vr, const gchar *name)
{
	gint i;

	for (i=0;i<sizeof(with_names)/sizeof(with_names[0]);i++) {
		if (!strcmp(name, with_names[i])) {
			vr->with = i;
			return;
		}
	}

	vr->with = 0;
}

static gint
xml_decode(FilterRule *fr, xmlNodePtr node, struct _RuleContext *f)
{
	xmlNodePtr set, work;
	gint result;
	EMVFolderRule *vr =(EMVFolderRule *)fr;
	gchar *tmp;

        result = FILTER_RULE_CLASS(parent_class)->xml_decode(fr, node, f);
	if (result != 0)
		return result;

	/* handle old format file, vfolder source is in filterrule */
	if (strcmp(fr->source, "incoming")!= 0) {
		set_with(vr, fr->source);
		g_free(fr->source);
		fr->source = g_strdup("incoming");
	}

	set = node->children;
	while (set) {
		if (!strcmp((gchar *)set->name, "sources")) {
			tmp = (gchar *)xmlGetProp(set, (const guchar *)"with");
			if (tmp) {
				set_with(vr, tmp);
				xmlFree(tmp);
			}
			work = set->children;
			while (work) {
				if (!strcmp((gchar *)work->name, "folder")) {
					tmp = (gchar *)xmlGetProp(work, (const guchar *)"uri");
					if (tmp) {
						vr->sources = g_list_append(vr->sources, g_strdup(tmp));
						xmlFree(tmp);
					}
				}
				work = work->next;
			}
		}
		set = set->next;
	}
	return 0;
}

static void
rule_copy(FilterRule *dest, FilterRule *src)
{
	EMVFolderRule *vdest, *vsrc;
	GList *node;

	vdest =(EMVFolderRule *)dest;
	vsrc =(EMVFolderRule *)src;

	if (vdest->sources) {
		g_list_foreach(vdest->sources, (GFunc)g_free, NULL);
		g_list_free(vdest->sources);
		vdest->sources = NULL;
	}

	node = vsrc->sources;
	while (node) {
		gchar *uri = node->data;

		vdest->sources = g_list_append(vdest->sources, g_strdup(uri));
		node = node->next;
	}

	vdest->with = vsrc->with;

	FILTER_RULE_CLASS(parent_class)->copy(dest, src);
}

enum {
	BUTTON_ADD,
	BUTTON_REMOVE,
	BUTTON_LAST
};

struct _source_data {
	RuleContext *rc;
	EMVFolderRule *vr;
	const gchar *current;
	GtkListStore *model;
	GtkTreeView *list;
	GtkWidget *source_selector;
	GtkButton *buttons[BUTTON_LAST];
};

static void source_add(GtkWidget *widget, struct _source_data *data);
static void source_remove(GtkWidget *widget, struct _source_data *data);

static struct {
	const gchar *name;
	GCallback func;
} edit_buttons[] = {
	{ "source_add",    G_CALLBACK(source_add)   },
	{ "source_remove", G_CALLBACK(source_remove)},
};

static void
set_sensitive(struct _source_data *data)
{
	gtk_widget_set_sensitive((GtkWidget *)data->buttons[BUTTON_ADD], TRUE);
	gtk_widget_set_sensitive((GtkWidget *)data->buttons[BUTTON_REMOVE], data->current != NULL);
}

static void
select_source(GtkWidget *list, struct _source_data *data)
{
	GtkTreeViewColumn *column;
	GtkTreePath *path;
	GtkTreeIter iter;

	gtk_tree_view_get_cursor(data->list, &path, &column);
	gtk_tree_model_get_iter(GTK_TREE_MODEL(data->model), &iter, path);
	gtk_tree_path_free(path);

	gtk_tree_model_get(GTK_TREE_MODEL(data->model), &iter, 0, &data->current, -1);

	set_sensitive(data);
}

static void
select_source_with_changed(GtkWidget *widget, struct _source_data *data)
{
	em_vfolder_rule_with_t with = 0;
	GSList *group = NULL;
	gint i = 0;

	if ( !gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget)) )
		return;

	group = gtk_radio_button_get_group (GTK_RADIO_BUTTON (widget));

	for (i=0; i< g_slist_length(group); i++) {
		if ( g_slist_nth_data (group, with = i) == widget )
			break;
	}

	if (with > EM_VFOLDER_RULE_WITH_LOCAL )
		with = 0;

	gtk_widget_set_sensitive (data->source_selector, !with );

	data->vr->with = with;
}

/* attempt to make a 'nice' folder name out of the raw uri */
static gchar *format_source(const gchar *euri)
{
	CamelURL *url;
	GString *out;
	gchar *res, *uri;

	/* This should really probably base it on the account name? */
	uri = em_uri_to_camel(euri);
	url = camel_url_new(uri, NULL);

	/* bad uri */
	if (url == NULL)
		return uri;

	g_free(uri);

	out = g_string_new(url->protocol);
	g_string_append_c(out, ':');
	if (url->user && url->host) {
		g_string_append_printf(out, "%s@%s", url->user, url->host);
		if (url->port)
			g_string_append_printf(out, ":%d", url->port);
	}
	if (url->fragment)
		g_string_append(out, url->fragment);
	else if (url->path)
		g_string_append(out, url->path);

	res = out->str;
	g_string_free(out, FALSE);

	return res;
}

static void
vfr_folder_response(GtkWidget *dialog, gint button, struct _source_data *data)
{
	const gchar *uri = em_folder_selector_get_selected_uri((EMFolderSelector *)dialog);

	if (button == GTK_RESPONSE_OK && uri != NULL) {
		gchar *urinice, *euri;
		GtkTreeSelection *selection;
		GtkTreeIter iter;

		euri = em_uri_from_camel(uri);

		data->vr->sources = g_list_append(data->vr->sources, euri);

		gtk_list_store_append(data->model, &iter);
		urinice = format_source(euri);
		gtk_list_store_set(data->model, &iter, 0, urinice, 1, euri, -1);
		g_free(urinice);
		selection = gtk_tree_view_get_selection(data->list);
		gtk_tree_selection_select_iter(selection, &iter);
		data->current = euri;

		set_sensitive(data);
	}

	gtk_widget_destroy(dialog);
}

static void
source_add(GtkWidget *widget, struct _source_data *data)
{
	EMFolderTree *emft;
	GtkWidget *dialog;

	emft =(EMFolderTree *)em_folder_tree_new_with_model(mail_component_peek_tree_model(mail_component_peek()));
	em_folder_tree_set_excluded(emft, EMFT_EXCLUDE_NOSELECT);

	dialog = em_folder_selector_new(emft, EM_FOLDER_SELECTOR_CAN_CREATE, _("Select Folder"), NULL, _("_Add"));
	gtk_window_set_transient_for ((GtkWindow *)dialog, (GtkWindow *)gtk_widget_get_toplevel(widget));
	gtk_window_set_modal((GtkWindow *)dialog, TRUE);
	g_signal_connect(dialog, "response", G_CALLBACK(vfr_folder_response), data);
	gtk_widget_show(dialog);
}

static void
source_remove(GtkWidget *widget, struct _source_data *data)
{
	GtkTreeSelection *selection;
	const gchar *source;
	GtkTreePath *path;
	GtkTreeIter iter;
	gint index = 0;
	gint n;

	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(data->list));

	source = NULL;
	while ((source = em_vfolder_rule_next_source(data->vr, source))) {
		path = gtk_tree_path_new();
		gtk_tree_path_append_index(path, index);

		if (gtk_tree_selection_path_is_selected(selection, path)) {
			gtk_tree_model_get_iter(GTK_TREE_MODEL(data->model), &iter, path);

			em_vfolder_rule_remove_source(data->vr, source);
			gtk_list_store_remove(data->model, &iter);
			gtk_tree_path_free(path);

			/* now select the next rule */
			n = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(data->model), NULL);
			index = index >= n ? n - 1 : index;

			if (index >= 0) {
				path = gtk_tree_path_new();
				gtk_tree_path_append_index(path, index);
				gtk_tree_model_get_iter(GTK_TREE_MODEL(data->model), &iter, path);
				gtk_tree_path_free(path);

				gtk_tree_selection_select_iter(selection, &iter);
				gtk_tree_model_get(GTK_TREE_MODEL(data->model), &iter, 0, &data->current, -1);
			} else {
				data->current = NULL;
			}

			break;
		}

		index++;
		gtk_tree_path_free(path);
	}

	set_sensitive(data);
}

GtkWidget *em_vfolder_editor_sourcelist_new(gchar *widget_name, gchar *string1, gchar *string2,
					  gint int1, gint int2);

GtkWidget *
em_vfolder_editor_sourcelist_new(gchar *widget_name, gchar *string1, gchar *string2, gint int1, gint int2)
{
	GtkWidget *table, *scrolled;
	GtkTreeSelection *selection;
	GtkCellRenderer *renderer;
	GtkListStore *model;

	scrolled = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled), GTK_SHADOW_IN);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
					GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	model = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_POINTER);
	table = gtk_tree_view_new_with_model((GtkTreeModel *)model);
	gtk_tree_view_set_headers_visible((GtkTreeView *)table, FALSE);

	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_insert_column_with_attributes((GtkTreeView *)table, -1,
						     _("Search Folder source"), renderer,
						     "text", 0, NULL);

	selection = gtk_tree_view_get_selection((GtkTreeView *)table);
	gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);

	gtk_container_add(GTK_CONTAINER(scrolled), table);

	g_object_set_data((GObject *)scrolled, "table", table);
	g_object_set_data((GObject *)scrolled, "model", model);

	gtk_widget_show(scrolled);
	gtk_widget_show(table);

	g_object_unref (model);

	return scrolled;
}

static GtkWidget *
get_widget(FilterRule *fr, RuleContext *rc)
{
	EMVFolderRule *vr =(EMVFolderRule *)fr;
	GtkWidget *widget, *frame, *list;
	struct _source_data *data;
	GtkRadioButton *rb;
	const gchar *source;
	GtkTreeIter iter;
	GladeXML *gui;
	gint i;
	gchar *gladefile;

        widget = FILTER_RULE_CLASS(parent_class)->get_widget(fr, rc);

	data = g_malloc0(sizeof(*data));
	data->rc = rc;
	data->vr = vr;

	gladefile = g_build_filename (EVOLUTION_GLADEDIR,
				      "mail-dialogs.glade",
				      NULL);
	gui = glade_xml_new(gladefile, "vfolder_source_frame", NULL);
	g_free (gladefile);

        frame = glade_xml_get_widget(gui, "vfolder_source_frame");

	g_object_set_data_full((GObject *)frame, "data", data, g_free);

	for (i = 0; i < BUTTON_LAST; i++) {
		data->buttons[i] =(GtkButton *)glade_xml_get_widget(gui, edit_buttons[i].name);
		g_signal_connect(data->buttons[i], "clicked", edit_buttons[i].func, data);
	}

	list = glade_xml_get_widget(gui, "source_list");
	data->list =(GtkTreeView *)g_object_get_data((GObject *)list, "table");
	data->model =(GtkListStore *)g_object_get_data((GObject *)list, "model");

	source = NULL;
	while ((source = em_vfolder_rule_next_source(vr, source))) {
		gchar *nice = format_source(source);

		gtk_list_store_append(data->model, &iter);
		gtk_list_store_set(data->model, &iter, 0, nice, 1, source, -1);
		g_free(nice);
	}

	g_signal_connect(data->list, "cursor-changed", G_CALLBACK(select_source), data);

	rb = (GtkRadioButton *)glade_xml_get_widget (gui, "local_rb");
	g_signal_connect (GTK_WIDGET(rb), "toggled", G_CALLBACK(select_source_with_changed), data);

	rb = (GtkRadioButton *)glade_xml_get_widget (gui, "remote_rb");
	g_signal_connect (GTK_WIDGET(rb), "toggled", G_CALLBACK(select_source_with_changed), data);

	rb = (GtkRadioButton *)glade_xml_get_widget (gui, "local_and_remote_rb");
	g_signal_connect (GTK_WIDGET(rb), "toggled", G_CALLBACK(select_source_with_changed), data);

	rb = (GtkRadioButton *)glade_xml_get_widget (gui, "specific_rb");
	g_signal_connect (GTK_WIDGET(rb), "toggled", G_CALLBACK(select_source_with_changed), data);

	data->source_selector = (GtkWidget *)glade_xml_get_widget (gui, "source_selector");

	rb = g_slist_nth_data(gtk_radio_button_get_group (rb), vr->with);
	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (rb), TRUE);
	g_signal_emit_by_name (rb, "toggled");

	set_sensitive(data);

	g_object_unref(gui);

	gtk_box_pack_start(GTK_BOX(widget), frame, TRUE, TRUE, 3);

	return widget;
}
