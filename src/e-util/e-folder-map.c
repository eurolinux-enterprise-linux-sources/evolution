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
 *		Jeffrey Stedfast <fejj@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include <config.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <libxml/tree.h>
#include <libxml/parser.h>
#include <libxml/xmlmemory.h>

#include <libedataserver/e-xml-utils.h>

#include "e-folder-map.h"

#define d(x)

static gboolean
is_type_folder (const gchar *metadata, const gchar *search_type)
{
	xmlNodePtr node;
	xmlDocPtr doc;
	gchar *type;

	doc = e_xml_parse_file (metadata);
	if (!doc) {
		g_warning ("Cannot parse `%s'", metadata);
		return FALSE;
	}

	if (!(node = xmlDocGetRootElement (doc))) {
		g_warning ("`%s' corrupt: document contains no root node", metadata);
		xmlFreeDoc (doc);
		return FALSE;
	}

	if (!node->name || strcmp ((gchar *)node->name, "efolder") != 0) {
		g_warning ("`%s' corrupt: root node is not 'efolder'", metadata);
		xmlFreeDoc (doc);
		return FALSE;
	}

	node = node->children;
	while (node != NULL) {
		if (node->name && !strcmp ((gchar *)node->name, "type")) {
			type = (gchar *)xmlNodeGetContent (node);
			if (!strcmp (type, search_type)) {
				xmlFreeDoc (doc);
				xmlFree (type);

				return TRUE;
			}

			xmlFree (type);

			break;
		}

		node = node->next;
	}

	xmlFreeDoc (doc);

	return FALSE;
}

static void
e_folder_map_dir (const gchar *dirname, const gchar *type, GSList **dir_list)
{
	gchar *path;
	const gchar *name;
	GDir *dir;
	GError *error = NULL;

	path = g_build_filename (dirname, "folder-metadata.xml", NULL);
	if (!g_file_test (path, G_FILE_TEST_IS_REGULAR)) {
		g_free (path);
		return;
	}

	if (!is_type_folder (path, type)) {
		g_free (path);
		goto try_subdirs;
	}

	d(g_message ("Found '%s'", dirname));
	*dir_list = g_slist_prepend (*dir_list, g_strdup (dirname));

	g_free (path);

 try_subdirs:

	path = g_build_filename (dirname, "subfolders", NULL);
	if (!g_file_test (path, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
		g_free (path);
		return;
	}

	if (!(dir = g_dir_open (path, 0, &error))) {
		g_warning ("cannot open `%s': %s", path, error->message);
		g_error_free (error);
		g_free (path);
		return;
	}

	while ((name = g_dir_read_name (dir))) {
		gchar *full_path;

		if (*name == '.')
			continue;

		full_path = g_build_filename (path, name, NULL);
		if (!g_file_test (full_path, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
			g_free (full_path);
			continue;
		}

		e_folder_map_dir (full_path, type, dir_list);
		g_free (full_path);
	}

	g_dir_close (dir);

	g_free (path);
}

GSList *
e_folder_map_local_folders (const gchar *local_dir, const gchar *type)
{
	const gchar *name;
	GDir *dir;
	GSList *dir_list = NULL;
	GError *error = NULL;

	if (!(dir = g_dir_open (local_dir, 0, &error))) {
		g_warning ("cannot open `%s': %s", local_dir, error->message);
		g_error_free (error);
		return NULL;
	}

	while ((name = g_dir_read_name (dir))) {
		gchar *full_path;

		if (*name == '.')
			continue;

		full_path = g_build_filename (local_dir, name, NULL);
		d(g_message ("Looking in %s", full_path));
		if (!g_file_test (full_path, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR)) {
			g_free (full_path);
			continue;
		}

		e_folder_map_dir (full_path, type, &dir_list);

		g_free (full_path);
	}

	g_dir_close (dir);

	return dir_list;
}
