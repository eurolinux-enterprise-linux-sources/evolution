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

#include <config.h>

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <glib.h>
#include <glib/gstdio.h>

#include <gtk/gtk.h>

#include <libebook/e-destination.h>
#include <libebook/e-book.h>
#include <glib/gi18n.h>

#include <libedataserver/e-xml-utils.h>

#include "e-util/e-util.h"
#include "e-util/e-util-private.h"
#include "e-util/e-xml-utils.h"
#include "e-util/e-folder-map.h"

#include "addressbook-migrate.h"

/*#define SLOW_MIGRATION*/

typedef struct {
	/* this hash table maps old folder uris to new uids.  It's
	   build in migrate_contact_folder and it's used in
	   migrate_completion_folders. */
	GHashTable *folder_uid_map;

	ESourceList *source_list;

	AddressbookComponent *component;

	GtkWidget *window;
	GtkWidget *label;
	GtkWidget *folder_label;
	GtkWidget *progress;
} MigrationContext;

static void
setup_progress_dialog (MigrationContext *context)
{
	GtkWidget *vbox, *hbox;

	context->window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title (GTK_WINDOW (context->window), _("Migrating..."));
	gtk_window_set_modal (GTK_WINDOW (context->window), TRUE);
	gtk_container_set_border_width (GTK_CONTAINER (context->window), 6);

	vbox = gtk_vbox_new (FALSE, 6);
	gtk_widget_show (vbox);
	gtk_container_add (GTK_CONTAINER (context->window), vbox);

	context->label = gtk_label_new ("");
	gtk_label_set_line_wrap (GTK_LABEL (context->label), TRUE);
	gtk_widget_show (context->label);
	gtk_box_pack_start (GTK_BOX (vbox), context->label, TRUE, TRUE, 0);

	hbox = gtk_hbox_new (FALSE, 6);
	gtk_widget_show (hbox);
	gtk_box_pack_start (GTK_BOX (vbox), hbox, TRUE, TRUE, 0);

	context->folder_label = gtk_label_new ("");
	gtk_widget_show (context->folder_label);
	gtk_box_pack_start (GTK_BOX (hbox), context->folder_label, TRUE, TRUE, 0);

	context->progress = gtk_progress_bar_new ();
	gtk_widget_show (context->progress);
	gtk_box_pack_start (GTK_BOX (hbox), context->progress, TRUE, TRUE, 0);

	gtk_widget_show (context->window);
}

static void
dialog_close (MigrationContext *context)
{
	gtk_widget_destroy (context->window);
}

static void
dialog_set_label (MigrationContext *context, const gchar *str)
{
	gtk_label_set_text (GTK_LABEL (context->label), str);

	while (gtk_events_pending ())
		gtk_main_iteration ();

#ifdef SLOW_MIGRATION
	sleep (1);
#endif
}

static void
dialog_set_folder_name (MigrationContext *context, const gchar *folder_name)
{
	gchar *text;

	text = g_strdup_printf (_("Migrating '%s':"), folder_name);
	gtk_label_set_text (GTK_LABEL (context->folder_label), text);
	g_free (text);

	gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (context->progress), 0.0);

	while (gtk_events_pending ())
		gtk_main_iteration ();

#ifdef SLOW_MIGRATION
	sleep (1);
#endif
}

static void
dialog_set_progress (MigrationContext *context, double percent)
{
	gchar text[5];

	snprintf (text, sizeof (text), "%d%%", (gint) (percent * 100.0f));

	gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (context->progress), percent);
	gtk_progress_bar_set_text (GTK_PROGRESS_BAR (context->progress), text);

	while (gtk_events_pending ())
		gtk_main_iteration ();

#ifdef SLOW_MIGRATION
	sleep (1);
#endif
}

static gboolean
check_for_conflict (ESourceGroup *group, gchar *name)
{
	GSList *sources;
	GSList *s;

	sources = e_source_group_peek_sources (group);

	for (s = sources; s; s = s->next) {
		ESource *source = E_SOURCE (s->data);

		if (!strcmp (e_source_peek_name (source), name))
			return TRUE;
	}

	return FALSE;
}

static gchar *
get_source_name (ESourceGroup *group, const gchar *path)
{
#ifndef G_OS_WIN32
	gchar **p = g_strsplit (path, "/", 0);
#else
	gchar **p = g_strsplit_set (path, "\\/", 0);
#endif
	gint i, j, starting_index;
	gint num_elements;
	gboolean conflict;
	GString *s = g_string_new ("");

	for (i = 0; p[i]; i ++);

	num_elements = i;
	i--;

	/* p[i] is now the last path element */

	/* check if it conflicts */
	starting_index = i;
	do {
		g_string_assign (s, "");
		for (j = starting_index; j < num_elements; j += 2) {
			if (j != starting_index)
				g_string_append_c (s, '_');
			g_string_append (s, p[j]);
		}

		conflict = check_for_conflict (group, s->str);

		/* if there was a conflict back up 2 levels (skipping the /subfolder/ element) */
		if (conflict)
			starting_index -= 2;

		/* we always break out if we can't go any further,
		   regardless of whether or not we conflict. */
		if (starting_index < 0)
			break;

	} while (conflict);

	g_strfreev (p);

	return g_string_free (s, FALSE);
}

static void
migrate_contacts (MigrationContext *context, EBook *old_book, EBook *new_book)
{
	EBookQuery *query = e_book_query_any_field_contains ("");
	GList *l, *contacts;
	gint num_added = 0;
	gint num_contacts;

	/* both books are loaded, start the actual migration */
	e_book_get_contacts (old_book, query, &contacts, NULL);
	e_book_query_unref (query);

	num_contacts = g_list_length (contacts);
	for (l = contacts; l; l = l->next) {
		EContact *contact = l->data;
		GError *e = NULL;
		GList *attrs, *attr;

		/* do some last minute massaging of the contact's attributes */

		attrs = e_vcard_get_attributes (E_VCARD (contact));
		for (attr = attrs; attr;) {
			EVCardAttribute *a = attr->data;

			/* evo 1.4 used the non-standard X-EVOLUTION-OFFICE attribute,
			   evo 1.5 uses the third element in the ORG list attribute. */
			if (!strcmp ("X-EVOLUTION-OFFICE", e_vcard_attribute_get_name (a))) {
				GList *v = e_vcard_attribute_get_values (a);
				GList *next_attr;

				if (v && v->data)
					e_contact_set (contact, E_CONTACT_OFFICE, v->data);

				next_attr = attr->next;
				e_vcard_remove_attribute (E_VCARD (contact), a);
				attr = next_attr;
			}
			/* evo 1.4 didn't put TYPE=VOICE in for phone numbers.
			   evo 1.5 does.

			   so we search through the attribute params for
			   either TYPE=VOICE or TYPE=FAX.  If we find
			   either we do nothing.  If we find neither, we
			   add TYPE=VOICE.
			*/
			else if (!strcmp ("TEL", e_vcard_attribute_get_name (a))) {
				GList *params, *param;
				gboolean found = FALSE;

				params = e_vcard_attribute_get_params (a);
				for (param = params; param; param = param->next) {
					EVCardAttributeParam *p = param->data;
					if (!strcmp (EVC_TYPE, e_vcard_attribute_param_get_name (p))) {
						GList *v = e_vcard_attribute_param_get_values (p);
						while (v && v->data) {
							if (!strcmp ("VOICE", v->data)
							    || !strcmp ("FAX", v->data)) {
								found = TRUE;
								break;
							}
							v = v->next;
						}
					}
				}

				if (!found)
					e_vcard_attribute_add_param_with_value (a,
										e_vcard_attribute_param_new (EVC_TYPE),
										"VOICE");
				attr = attr->next;
			}
			/* Replace "POSTAL" (1.4) addresses with "OTHER" (1.5) */
			else if (!strcmp ("ADR", e_vcard_attribute_get_name (a))) {
				GList *params, *param;
				gboolean found = FALSE;
				EVCardAttributeParam *p;

				params = e_vcard_attribute_get_params (a);
				for (param = params; param; param = param->next) {
					p = param->data;
					if (!strcmp (EVC_TYPE, e_vcard_attribute_param_get_name (p))) {
						GList *v = e_vcard_attribute_param_get_values (p);
						while (v && v->data ) {
							if (!strcmp ("POSTAL", v->data)) {
								found = TRUE;
								break;
							}
							v = v->next;
						}
						if (found)
							break;
					}
				}

				if (found) {
					e_vcard_attribute_param_remove_values (p);
					e_vcard_attribute_param_add_value (p, "OTHER");
				}

				attr = attr->next;
			}
			/* this is kinda gross.  The new vcard parser
			   needs ';'s to be escaped by \'s.  but the
			   1.4 vcard generator would put unescaped xml
			   (including entities like &gt;) in the value
			   of attributes, so we need to go through and
			   escape those ';'s. */
			else if (!strcmp ("EMAIL", e_vcard_attribute_get_name (a))) {
				GList *params;
				GList *v = e_vcard_attribute_get_values (a);

				/* Add TYPE=OTHER if there is no type set */
				params = e_vcard_attribute_get_params (a);
				if (!params)
					e_vcard_attribute_add_param_with_value (a,
							e_vcard_attribute_param_new (EVC_TYPE),
							"OTHER");

				if (v && v->data) {
					if (!strncmp ((gchar *)v->data, "<?xml", 5)) {
						/* k, this is the nasty part.  we glomb all the
						   value strings back together again (if there is
						   more than one), then work our magic */
						GString *str = g_string_new ("");
						while (v) {
							g_string_append (str, v->data);
							if (v->next)
								g_string_append_c (str, ';');
							v = v->next;
						}

						e_vcard_attribute_remove_values (a);
						e_vcard_attribute_add_value (a, str->str);
						g_string_free (str, TRUE);
					}
				}

				attr = attr->next;
			}
			else {
				attr = attr->next;
			}
		}

		if (!e_book_add_contact (new_book,
					 contact,
					 &e))
			g_warning ("contact add failed: `%s'", e->message);

		num_added ++;

		dialog_set_progress (context, (double)num_added / num_contacts);
	}

	g_list_foreach (contacts, (GFunc)g_object_unref, NULL);
	g_list_free (contacts);
}

static void
migrate_contact_folder_to_source (MigrationContext *context, gchar *old_path, ESource *new_source)
{
	gchar *old_uri = g_filename_to_uri (old_path, NULL, NULL);
	GError *e = NULL;

	EBook *old_book = NULL, *new_book = NULL;
	ESource *old_source;
	ESourceGroup *group;

	group = e_source_group_new ("", old_uri);
	old_source = e_source_new ("", "");
	e_source_group_add_source (group, old_source, -1);

	dialog_set_folder_name (context, e_source_peek_name (new_source));

	old_book = e_book_new (old_source, &e);
	if (!old_book
	    || !e_book_open (old_book, TRUE, &e)) {
		g_warning ("failed to load source book for migration: `%s'", e->message);
		goto finish;
	}

	new_book = e_book_new (new_source, &e);
	if (!new_book
	    || !e_book_open (new_book, FALSE, &e)) {
		g_warning ("failed to load destination book for migration: `%s'", e->message);
		goto finish;
	}

	migrate_contacts (context, old_book, new_book);

 finish:
	g_object_unref (old_source);
	g_object_unref (group);
	if (old_book)
		g_object_unref (old_book);
	if (new_book)
		g_object_unref (new_book);
	g_free (old_uri);
}

static void
migrate_contact_folder (MigrationContext *context, gchar *old_path, ESourceGroup *dest_group, gchar *source_name)
{
	ESource *new_source;

	new_source = e_source_new (source_name, source_name);
	e_source_set_relative_uri (new_source, e_source_peek_uid (new_source));
	e_source_group_add_source (dest_group, new_source, -1);

	g_hash_table_insert (context->folder_uid_map, g_strdup (old_path), g_strdup (e_source_peek_uid (new_source)));

	migrate_contact_folder_to_source (context, old_path, new_source);

	g_object_unref (new_source);
}

#define LDAP_BASE_URI "ldap://"
#define PERSONAL_RELATIVE_URI "system"

static void
create_groups (MigrationContext *context,
	       ESourceGroup **on_this_computer,
	       ESourceGroup **on_ldap_servers,
	       ESource      **personal_source)
{
	GSList *groups;
	ESourceGroup *group;
	gchar *base_uri, *base_uri_proto;
	const gchar *base_dir;

	*on_this_computer = NULL;
	*on_ldap_servers = NULL;
	*personal_source = NULL;

	base_dir = addressbook_component_peek_base_directory (context->component);
	base_uri = g_build_filename (base_dir, "local", NULL);

	base_uri_proto = g_filename_to_uri (base_uri, NULL, NULL);

	groups = e_source_list_peek_groups (context->source_list);
	if (groups) {
		/* groups are already there, we need to search for things... */
		GSList *g;

		for (g = groups; g; g = g->next) {

			group = E_SOURCE_GROUP (g->data);

			if (!*on_this_computer && !strcmp (base_uri_proto, e_source_group_peek_base_uri (group)))
				*on_this_computer = g_object_ref (group);
			else if (!*on_ldap_servers && !strcmp (LDAP_BASE_URI, e_source_group_peek_base_uri (group)))
				*on_ldap_servers = g_object_ref (group);
		}
	}

	if (*on_this_computer) {
		/* make sure "Personal" shows up as a source under
		   this group */
		GSList *sources = e_source_group_peek_sources (*on_this_computer);
		GSList *s;
		for (s = sources; s; s = s->next) {
			ESource *source = E_SOURCE (s->data);
			const gchar *relative_uri;

			relative_uri = e_source_peek_relative_uri (source);
			if (relative_uri == NULL)
				continue;
			if (!strcmp (PERSONAL_RELATIVE_URI, relative_uri)) {
				*personal_source = g_object_ref (source);
				break;
			}
		}
	}
	else {
		/* create the local source group */
		group = e_source_group_new (_("On This Computer"), base_uri_proto);
		e_source_list_add_group (context->source_list, group, -1);

		*on_this_computer = group;
	}

	if (!*personal_source) {
		/* Create the default Person addressbook */
		ESource *source = e_source_new (_("Personal"), PERSONAL_RELATIVE_URI);
		e_source_group_add_source (*on_this_computer, source, -1);

		e_source_set_property (source, "completion", "true");

		*personal_source = source;
	}

	if (!*on_ldap_servers) {
		/* Create the LDAP source group */
		group = e_source_group_new (_("On LDAP Servers"), LDAP_BASE_URI);
		e_source_list_add_group (context->source_list, group, -1);

		*on_ldap_servers = group;
	}

	g_free (base_uri_proto);
	g_free (base_uri);
}

static gboolean
migrate_local_folders (MigrationContext *context, ESourceGroup *on_this_computer, ESource *personal_source)
{
	gchar *old_path = NULL;
	GSList *dirs, *l;
	gchar *local_contact_folder = NULL;

	old_path = g_strdup_printf ("%s/evolution/local", g_get_home_dir ());

	dirs = e_folder_map_local_folders (old_path, "contacts");

	/* migrate the local addressbook first, to local/system */
	local_contact_folder = g_build_filename (g_get_home_dir (),
						 "evolution", "local", "Contacts",
						 NULL);

	for (l = dirs; l; l = l->next) {
		gchar *source_name;
		/* we handle the system folder differently */
		if (personal_source && !strcmp ((gchar *)l->data, local_contact_folder)) {
			g_hash_table_insert (context->folder_uid_map, g_strdup (l->data), g_strdup (e_source_peek_uid (personal_source)));
			migrate_contact_folder_to_source (context, local_contact_folder, personal_source);
			continue;
		}

		source_name = get_source_name (on_this_computer, (gchar *)l->data);
		migrate_contact_folder (context, l->data, on_this_computer, source_name);
		g_free (source_name);
	}

	g_slist_foreach (dirs, (GFunc)g_free, NULL);
	g_slist_free (dirs);
	g_free (local_contact_folder);
	g_free (old_path);

	return TRUE;
}

static gchar *
get_string_child (xmlNode *node,
		  const gchar *name)
{
	xmlNode *p;
	xmlChar *xml_string;
	gchar *retval;

	p = e_xml_get_child_by_name (node, (xmlChar *) name);
	if (p == NULL)
		return NULL;

	p = e_xml_get_child_by_name (p, (xmlChar *) "text");
	if (p == NULL) /* there's no text between the tags, return the empty string */
		return g_strdup("");

	xml_string = xmlNodeListGetString (node->doc, p, 1);
	retval = g_strdup ((gchar *) xml_string);
	xmlFree (xml_string);

	return retval;
}

static gint
get_integer_child (xmlNode *node,
		   const gchar *name,
		   gint defval)
{
	xmlNode *p;
	xmlChar *xml_string;
	gint retval;

	p = e_xml_get_child_by_name (node, (xmlChar *) name);
	if (p == NULL)
		return defval;

	p = e_xml_get_child_by_name (p, (xmlChar *) "text");
	if (p == NULL) /* there's no text between the tags, return the default */
		return defval;

	xml_string = xmlNodeListGetString (node->doc, p, 1);
	retval = atoi ((gchar *)xml_string);
	xmlFree (xml_string);

	return retval;
}

static gboolean
migrate_ldap_servers (MigrationContext *context, ESourceGroup *on_ldap_servers)
{
	gchar *sources_xml = g_strdup_printf ("%s/evolution/addressbook-sources.xml",
					     g_get_home_dir ());

	printf ("trying to migrate from %s\n", sources_xml);

	if (g_file_test (sources_xml, G_FILE_TEST_EXISTS)) {
		xmlDoc  *doc = xmlParseFile (sources_xml);
		xmlNode *root;
		xmlNode *child;
		gint num_contactservers;
		gint servernum;

		if (!doc)
			return FALSE;

		root = xmlDocGetRootElement (doc);
		if (root == NULL || strcmp ((const gchar *)root->name, "addressbooks") != 0) {
			xmlFreeDoc (doc);
			return FALSE;
		}

		/* count the number of servers, so we can give progress */
		num_contactservers = 0;
		for (child = root->children; child; child = child->next) {
			if (!strcmp ((const gchar *)child->name, "contactserver")) {
				num_contactservers++;
			}
		}
		printf ("found %d contact servers to migrate\n", num_contactservers);

		dialog_set_folder_name (context, _("LDAP Servers"));

		servernum = 0;
		for (child = root->children; child; child = child->next) {
			if (!strcmp ((const gchar *)child->name, "contactserver")) {
				gchar *port, *host, *rootdn, *scope, *authmethod, *ssl;
				gchar *emailaddr, *binddn, *limitstr;
				gint limit;
				gchar *name, *description;
				GString *uri = g_string_new ("");
				ESource *source;

				name        = get_string_child (child, "name");
				description = get_string_child (child, "description");
				port        = get_string_child (child, "port");
				host        = get_string_child (child, "host");
				rootdn      = get_string_child (child, "rootdn");
				scope       = get_string_child (child, "scope");
				authmethod  = get_string_child (child, "authmethod");
				ssl         = get_string_child (child, "ssl");
				emailaddr   = get_string_child (child, "emailaddr");
				binddn      = get_string_child (child, "binddn");
				limit       = get_integer_child (child, "limit", 100);
				limitstr    = g_strdup_printf ("%d", limit);

				g_string_append_printf (uri,
							"%s:%s/%s?"/*trigraph prevention*/"?%s",
							host, port, rootdn, scope);

				source = e_source_new (name, uri->str);
				e_source_set_property (source, "description", description);
				e_source_set_property (source, "limit", limitstr);
				e_source_set_property (source, "ssl", ssl);
				e_source_set_property (source, "auth", authmethod);
				if (emailaddr)
					e_source_set_property (source, "email_addr", emailaddr);
				if (binddn)
					e_source_set_property (source, "binddn", binddn);

				e_source_group_add_source (on_ldap_servers, source, -1);

				g_string_free (uri, TRUE);
				g_free (port);
				g_free (host);
				g_free (rootdn);
				g_free (scope);
				g_free (authmethod);
				g_free (ssl);
				g_free (emailaddr);
				g_free (binddn);
				g_free (limitstr);
				g_free (name);
				g_free (description);

				servernum++;
				dialog_set_progress (context, (double)servernum/num_contactservers);
			}
		}

		xmlFreeDoc (doc);
	}

	g_free (sources_xml);

	return TRUE;
}

static ESource*
get_source_by_name (ESourceList *source_list, const gchar *name)
{
	GSList *groups;
	GSList *g;

	groups = e_source_list_peek_groups (source_list);
	if (!groups)
		return NULL;

	for (g = groups; g; g = g->next) {
		GSList *sources;
		GSList *s;
		ESourceGroup *group = E_SOURCE_GROUP (g->data);

		sources = e_source_group_peek_sources (group);
		if (!sources)
			continue;

		for (s = sources; s; s = s->next) {
			ESource *source = E_SOURCE (s->data);
			const gchar *source_name = e_source_peek_name (source);

			if (!strcmp (name, source_name))
				return source;
		}
	}

	return NULL;
}

static gboolean
migrate_completion_folders (MigrationContext *context)
{
	gchar *uris_xml = gconf_client_get_string (addressbook_component_peek_gconf_client (context->component),
						  "/apps/evolution/addressbook/completion/uris",
						  NULL);

	printf ("trying to migrate completion folders\n");

	if (uris_xml) {
		xmlDoc  *doc = xmlParseMemory (uris_xml, strlen (uris_xml));
		xmlNode *root;
		xmlNode *child;

		if (!doc)
			return FALSE;

		dialog_set_folder_name (context, _("Autocompletion Settings"));

		root = xmlDocGetRootElement (doc);
		if (root == NULL || strcmp ((const gchar *)root->name, "EvolutionFolderList") != 0) {
			xmlFreeDoc (doc);
			return FALSE;
		}

		for (child = root->children; child; child = child->next) {
			if (!strcmp ((const gchar *)child->name, "folder")) {
				gchar *physical_uri = e_xml_get_string_prop_by_name (child, (const guchar *)"physical-uri");
				ESource *source = NULL;

				/* if the physical uri is file://...
				   we look it up in our folder_uid_map
				   hashtable.  If it's a folder we
				   converted over, we should get back
				   a uid we can search for.

				   if the physical_uri is anything
				   else, we strip off the args
				   (anything after;) before searching
				   for the uri. */

				if (!strncmp (physical_uri, "file://", 7)) {
					gchar *filename = g_filename_from_uri (physical_uri, NULL, NULL);
					gchar *uid = NULL;

					if (filename)
						uid  = g_hash_table_lookup (context->folder_uid_map,
									    filename);
					g_free (filename);
					if (uid)
						source = e_source_list_peek_source_by_uid (context->source_list, uid);
				}
				else {
					gchar *name = e_xml_get_string_prop_by_name (child, (const guchar *)"display-name");

					source = get_source_by_name (context->source_list, name);

					g_free (name);
				}

				if (source) {
					e_source_set_property (source, "completion", "true");
				}
				else {
					g_warning ("found completion folder with uri `%s' that "
						   "doesn't correspond to anything we migrated.", physical_uri);
				}

				g_free (physical_uri);
			}
		}

		g_free (uris_xml);
	}
	else {
		g_message ("no completion folder settings to migrate");
	}

	return TRUE;
}

static void
migrate_contact_lists_for_local_folders (MigrationContext *context, ESourceGroup *on_this_computer)
{
	GSList *sources, *s;

	sources = e_source_group_peek_sources (on_this_computer);
	for (s = sources; s; s = s->next) {
		ESource *source = s->data;
		EBook *book;
		EBookQuery *query;
		GList *l, *contacts;
		gint num_contacts, num_converted;

		dialog_set_folder_name (context, e_source_peek_name (source));

		book = e_book_new (source, NULL);
		if (!book
		    || !e_book_open (book, TRUE, NULL)) {
			gchar *uri = e_source_get_uri (source);
			g_warning ("failed to migrate contact lists for source %s", uri);
			g_free (uri);
			continue;
		}

		query = e_book_query_any_field_contains ("");
		e_book_get_contacts (book, query, &contacts, NULL);
		e_book_query_unref (query);

		num_converted = 0;
		num_contacts = g_list_length (contacts);
		for (l = contacts; l; l = l->next) {
			EContact *contact = l->data;
			GError *e = NULL;
			GList *attrs, *attr;
			gboolean converted = FALSE;

			attrs = e_contact_get_attributes (contact, E_CONTACT_EMAIL);
			for (attr = attrs; attr; attr = attr->next) {
				EVCardAttribute *a = attr->data;
				GList *v = e_vcard_attribute_get_values (a);

				if (v && v->data) {
					if (!strncmp ((gchar *)v->data, "<?xml", 5)) {
						EDestination *dest = e_destination_import ((gchar *)v->data);

						e_destination_export_to_vcard_attribute (dest, a);

						g_object_unref (dest);

						converted = TRUE;
					}
				}
			}

			if (converted) {
				e_contact_set_attributes (contact, E_CONTACT_EMAIL, attrs);

				if (!e_book_commit_contact (book,
							    contact,
							    &e))
					g_warning ("contact commit failed: `%s'", e->message);
			}

			num_converted ++;

			dialog_set_progress (context, (double)num_converted / num_contacts);
		}

		g_list_foreach (contacts, (GFunc)g_object_unref, NULL);
		g_list_free (contacts);

		g_object_unref (book);
	}
}

static void
migrate_company_phone_for_local_folders (MigrationContext *context, ESourceGroup *on_this_computer)
{
	GSList *sources, *s;

	sources = e_source_group_peek_sources (on_this_computer);
	for (s = sources; s; s = s->next) {
		ESource *source = s->data;
		EBook *book;
		EBookQuery *query;
		GList *l, *contacts;
		gint num_contacts, num_converted;

		dialog_set_folder_name (context, e_source_peek_name (source));

		book = e_book_new (source, NULL);
		if (!book
		    || !e_book_open (book, TRUE, NULL)) {
			gchar *uri = e_source_get_uri (source);
			g_warning ("failed to migrate company phone numbers for source %s", uri);
			g_free (uri);
			continue;
		}

		query = e_book_query_any_field_contains ("");
		e_book_get_contacts (book, query, &contacts, NULL);
		e_book_query_unref (query);

		num_converted = 0;
		num_contacts = g_list_length (contacts);
		for (l = contacts; l; l = l->next) {
			EContact *contact = l->data;
			GError *e = NULL;
			GList *attrs, *attr;
			gboolean converted = FALSE;
			gint num_work_voice = 0;

			attrs = e_vcard_get_attributes (E_VCARD (contact));
			for (attr = attrs; attr;) {
				EVCardAttribute *a = attr->data;
				GList *next_attr = attr->next;

				if (!strcmp ("TEL", e_vcard_attribute_get_name (a))) {
					GList *params, *param;
					gboolean found_voice = FALSE;
					gboolean found_work = FALSE;

					params = e_vcard_attribute_get_params (a);
					for (param = params; param; param = param->next) {
						EVCardAttributeParam *p = param->data;
						if (!strcmp (EVC_TYPE, e_vcard_attribute_param_get_name (p))) {
							GList *v = e_vcard_attribute_param_get_values (p);
							while (v && v->data) {
								if (!strcmp ("VOICE", v->data))
									found_voice = TRUE;
								else if (!strcmp ("WORK", v->data))
									found_work = TRUE;
								v = v->next;
							}
						}

						if (found_work && found_voice)
							num_work_voice++;

						if (num_work_voice == 3) {
							GList *v = e_vcard_attribute_get_values (a);

							if (v && v->data)
								e_contact_set (contact, E_CONTACT_PHONE_COMPANY, v->data);

							e_vcard_remove_attribute (E_VCARD (contact), a);

							converted = TRUE;
							break;
						}
					}
				}

				attr = next_attr;

				if (converted)
					break;
			}

			if (converted) {
				if (!e_book_commit_contact (book,
							    contact,
							    &e))
					g_warning ("contact commit failed: `%s'", e->message);
			}

			num_converted ++;

			dialog_set_progress (context, (double)num_converted / num_contacts);
		}

		g_list_foreach (contacts, (GFunc)g_object_unref, NULL);
		g_list_free (contacts);

		g_object_unref (book);
	}
}

static void
migrate_pilot_data (const gchar *old_path, const gchar *new_path)
{
	const gchar *dent;
	const gchar *ext;
	gchar *filename;
	GDir *dir;

	if (!(dir = g_dir_open (old_path, 0, NULL)))
		return;

	while ((dent = g_dir_read_name (dir))) {
		if ((!strncmp (dent, "pilot-map-", 10) &&
		     ((ext = strrchr (dent, '.')) && !strcmp (ext, ".xml"))) ||
		    (!strncmp (dent, "pilot-sync-evolution-addressbook-", 33) &&
		     ((ext = strrchr (dent, '.')) && !strcmp (ext, ".db")))) {
			/* src and dest file formats are identical for both map and changelog files */
			guchar inbuf[4096];
			gsize nread, nwritten;
			gint fd0, fd1;
			gssize n;

			filename = g_build_filename (old_path, dent, NULL);
			if ((fd0 = g_open (filename, O_RDONLY | O_BINARY, 0)) == -1) {
				g_free (filename);
				continue;
			}

			g_free (filename);
			filename = g_build_filename (new_path, dent, NULL);
			if ((fd1 = g_open (filename, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0666)) == -1) {
				g_free (filename);
				close (fd0);
				continue;
			}

			do {
				do {
					n = read (fd0, inbuf, sizeof (inbuf));
				} while (n == -1 && errno == EINTR);

				if (n < 1)
					break;

				nread = n;
				nwritten = 0;
				do {
					do {
						n = write (fd1, inbuf + nwritten, nread - nwritten);
					} while (n == -1 && errno == EINTR);

					if (n > 0)
						nwritten += n;
				} while (nwritten < nread && n != -1);

				if (n == -1)
					break;
			} while (1);

			if (n != -1)
				n = fsync (fd1);

			if (n == -1) {
				g_warning ("Failed to migrate %s: %s", dent, g_strerror (errno));
				g_unlink (filename);
			}

			close (fd0);
			close (fd1);
			g_free (filename);
		}
	}

	g_dir_close (dir);
}

static MigrationContext*
migration_context_new (AddressbookComponent *component)
{
	MigrationContext *context = g_new (MigrationContext, 1);

	/* set up the mapping from old uris to new uids */
	context->folder_uid_map = g_hash_table_new_full (g_str_hash, g_str_equal, (GDestroyNotify)g_free, (GDestroyNotify)g_free);

	e_book_get_addressbooks (&context->source_list, NULL);

	context->component = component;

	return context;
}

static void
migration_context_free (MigrationContext *context)
{
	e_source_list_sync (context->source_list, NULL);

	g_hash_table_destroy (context->folder_uid_map);

	g_object_unref (context->source_list);

	g_free (context);
}

gint
addressbook_migrate (AddressbookComponent *component, gint major, gint minor, gint revision, GError **err)
{
	ESourceGroup *on_this_computer;
	ESourceGroup *on_ldap_servers;
	ESource *personal_source;
	MigrationContext *context = migration_context_new (component);
	gboolean need_dialog = FALSE;

	printf ("addressbook_migrate (%d.%d.%d)\n", major, minor, revision);

	/* we call this unconditionally now - create_groups either
	   creates the groups/sources or it finds the necessary
	   groups/sources. */
	create_groups (context, &on_this_computer, &on_ldap_servers, &personal_source);

	/* figure out if we need the dialog displayed */
	if (major == 1
	    /* we only need the most recent upgrade point here.
	       further decomposition will happen below. */
	    && (minor < 5 || (minor == 5 && revision <= 10)))
		need_dialog = TRUE;

	if (need_dialog)
		setup_progress_dialog (context);

	if (major == 1) {

		if (minor < 5 || (minor == 5 && revision <= 2)) {
			/* initialize our dialog */
			dialog_set_label (context,
					  _("The location and hierarchy of the Evolution contact "
					    "folders has changed since Evolution 1.x.\n\nPlease be "
					    "patient while Evolution migrates your folders..."));

			if (on_this_computer)
				migrate_local_folders (context, on_this_computer, personal_source);
			if (on_ldap_servers)
				migrate_ldap_servers (context, on_ldap_servers);

			migrate_completion_folders (context);
		}

		if (minor < 5 || (minor == 5 && revision <= 7)) {
			dialog_set_label (context,
					  _("The format of mailing list contacts has changed.\n\n"
					    "Please be patient while Evolution migrates your "
					    "folders..."));

			migrate_contact_lists_for_local_folders (context, on_this_computer);
		}

		if (minor < 5 || (minor == 5 && revision <= 8)) {
			dialog_set_label (context,
					  _("The way Evolution stores some phone numbers has changed.\n\n"
					    "Please be patient while Evolution migrates your "
					    "folders..."));

			migrate_company_phone_for_local_folders (context, on_this_computer);
		}

		if (minor < 5 || (minor == 5 && revision <= 10)) {
			gchar *old_path, *new_path;

			dialog_set_label (context, _("Evolution's Palm Sync changelog and map files have changed.\n\n"
						     "Please be patient while Evolution migrates your Pilot Sync data..."));

			old_path = g_build_filename (g_get_home_dir (), "evolution", "local", "Contacts", NULL);
			new_path = g_build_filename (addressbook_component_peek_base_directory (component),
						     "local", "system", NULL);
			migrate_pilot_data (old_path, new_path);
			g_free (new_path);
			g_free (old_path);
		}

		/* we only need to do this next step if people ran
		   older versions of 1.5.  We need to clear out the
		   absolute URI's that were assigned to ESources
		   during one phase of development, as they take
		   precedent over relative uris (but aren't updated
		   when editing an ESource). */
		if (minor == 5 && revision <= 11) {
			GSList *g;
			for (g = e_source_list_peek_groups (context->source_list); g; g = g->next) {
				ESourceGroup *group = g->data;
				GSList *s;

				for (s = e_source_group_peek_sources (group); s; s = s->next) {
					ESource *source = s->data;
					e_source_set_absolute_uri (source, NULL);
				}
			}
		}
	}

	if (need_dialog)
		dialog_close (context);

	if (on_this_computer)
		g_object_unref (on_this_computer);
	if (on_ldap_servers)
		g_object_unref (on_ldap_servers);
	if (personal_source)
		g_object_unref (personal_source);

	migration_context_free (context);

	return TRUE;
}
