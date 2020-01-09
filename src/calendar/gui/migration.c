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
 *		Rodrigo Moya <rodrigo@ximian.com>
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

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>

#include <libecal/e-cal.h>

#include <libebackend/e-dbhash.h>
#include <libedataserver/e-xml-hash-utils.h>
#include <libedataserver/e-xml-utils.h>
#include <libedataserver/e-account-list.h>
#include <camel/camel-url.h>

#include "e-util/e-bconf-map.h"
#include "e-util/e-folder-map.h"
#include "e-util/e-util-private.h"

#include "calendar-config-keys.h"
#include "calendar-config.h"
#include "e-cal-event.h"
#include "migration.h"

#ifndef G_OS_WIN32

/* No previous versions have been available on Win32, so don't
 * bother with upgrade support from 1.x on Win32.
 */

static e_gconf_map_t calendar_display_map[] = {
	/* /Calendar/Display */
	{ "Timezone", "calendar/display/timezone", E_GCONF_MAP_STRING },
	{ "Use24HourFormat", "calendar/display/use_24hour_format", E_GCONF_MAP_BOOL },
	{ "WeekStartDay", "calendar/display/week_start_day", E_GCONF_MAP_INT },
	{ "DayStartHour", "calendar/display/day_start_hour", E_GCONF_MAP_INT },
	{ "DayStartMinute", "calendar/display/day_start_minute", E_GCONF_MAP_INT },
	{ "DayEndHour", "calendar/display/day_end_hour", E_GCONF_MAP_INT },
	{ "DayEndMinute", "calendar/display/day_end_minute", E_GCONF_MAP_INT },
	{ "TimeDivisions", "calendar/display/time_divisions", E_GCONF_MAP_INT },
	{ "View", "calendar/display/default_view", E_GCONF_MAP_INT },
	{ "HPanePosition", "calendar/display/hpane_position", E_GCONF_MAP_FLOAT },
	{ "VPanePosition", "calendar/display/vpane_position", E_GCONF_MAP_FLOAT },
	{ "MonthHPanePosition", "calendar/display/month_hpane_position", E_GCONF_MAP_FLOAT },
	{ "MonthVPanePosition", "calendar/display/month_vpane_position", E_GCONF_MAP_FLOAT },
	{ "CompressWeekend", "calendar/display/compress_weekend", E_GCONF_MAP_BOOL },
	{ "ShowEventEndTime", "calendar/display/show_event_end", E_GCONF_MAP_BOOL },
	{ "WorkingDays", "calendar/display/working_days", E_GCONF_MAP_INT },
	{ NULL },
};

static e_gconf_map_t calendar_tasks_map[] = {
	/* /Calendar/Tasks */
	{ "HideCompletedTasks", "calendar/tasks/hide_completed", E_GCONF_MAP_BOOL },
	{ "HideCompletedTasksUnits", "calendar/tasks/hide_completed_units", E_GCONF_MAP_STRING },
	{ "HideCompletedTasksValue", "calendar/tasks/hide_completed_value", E_GCONF_MAP_INT },
	{ NULL },
};

static e_gconf_map_t calendar_tasks_colours_map[] = {
	/* /Calendar/Tasks/Colors */
	{ "TasksDueToday", "calendar/tasks/colors/due_today", E_GCONF_MAP_STRING },
	{ "TasksOverDue", "calendar/tasks/colors/overdue", E_GCONF_MAP_STRING },
	{ "TasksDueToday", "calendar/tasks/colors/due_today", E_GCONF_MAP_STRING },
	{ NULL },
};

static e_gconf_map_t calendar_other_map[] = {
	/* /Calendar/Other */
	{ "ConfirmDelete", "calendar/prompts/confirm_delete", E_GCONF_MAP_BOOL },
	{ "ConfirmExpunge", "calendar/prompts/confirm_purge", E_GCONF_MAP_BOOL },
	{ "UseDefaultReminder", "calendar/other/use_default_reminder", E_GCONF_MAP_BOOL },
	{ "DefaultReminderInterval", "calendar/other/default_reminder_interval", E_GCONF_MAP_INT },
	{ "DefaultReminderUnits", "calendar/other/default_reminder_units", E_GCONF_MAP_STRING },
	{ NULL },
};

static e_gconf_map_t calendar_datenavigator_map[] = {
	/* /Calendar/DateNavigator */
	{ "ShowWeekNumbers", "calendar/date_navigator/show_week_numbers", E_GCONF_MAP_BOOL },
	{ NULL },
};

static e_gconf_map_t calendar_alarmnotify_map[] = {
	/* /Calendar/AlarmNotify */
	{ "LastNotificationTime", "calendar/notify/last_notification_time", E_GCONF_MAP_INT },
	{ "CalendarToLoad%i", "calendar/notify/calendars", E_GCONF_MAP_STRING|E_GCONF_MAP_LIST },
	{ "BlessedProgram%i", "calendar/notify/programs", E_GCONF_MAP_STRING|E_GCONF_MAP_LIST },
	{ NULL },
};

static e_gconf_map_list_t calendar_remap_list[] = {

	{ "/Calendar/Display", calendar_display_map },
	{ "/Calendar/Other/Map", calendar_other_map },
	{ "/Calendar/DateNavigator", calendar_datenavigator_map },
	{ "/Calendar/AlarmNotify", calendar_alarmnotify_map },

	{ NULL },
};

static e_gconf_map_list_t task_remap_list[] = {

	{ "/Calendar/Tasks", calendar_tasks_map },
	{ "/Calendar/Tasks/Colors", calendar_tasks_colours_map },

	{ NULL },
};

static GtkWidget *window;
static GtkLabel *label;
static GtkProgressBar *progress;

static void
setup_progress_dialog (gboolean tasks)
{
	GtkWidget *vbox, *hbox, *w;

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title ((GtkWindow *) window, _("Migrating..."));
	gtk_window_set_modal ((GtkWindow *) window, TRUE);
	gtk_container_set_border_width ((GtkContainer *) window, 6);

	vbox = gtk_vbox_new (FALSE, 6);
	gtk_widget_show (vbox);
	gtk_container_add ((GtkContainer *) window, vbox);

	if (tasks)
		w = gtk_label_new (_("The location and hierarchy of the Evolution task "
				     "folders has changed since Evolution 1.x.\n\nPlease be "
				     "patient while Evolution migrates your folders..."));
	else
		w = gtk_label_new (_("The location and hierarchy of the Evolution calendar "
				     "folders has changed since Evolution 1.x.\n\nPlease be "
				     "patient while Evolution migrates your folders..."));

	gtk_label_set_line_wrap ((GtkLabel *) w, TRUE);
	gtk_widget_show (w);
	gtk_box_pack_start ((GtkBox *) vbox, w, TRUE, TRUE, 0);

	hbox = gtk_hbox_new (FALSE, 6);
	gtk_widget_show (hbox);
	gtk_box_pack_start ((GtkBox *) vbox, hbox, TRUE, TRUE, 0);

	label = (GtkLabel *) gtk_label_new ("");
	gtk_widget_show ((GtkWidget *) label);
	gtk_box_pack_start ((GtkBox *) hbox, (GtkWidget *) label, TRUE, TRUE, 0);

	progress = (GtkProgressBar *) gtk_progress_bar_new ();
	gtk_widget_show ((GtkWidget *) progress);
	gtk_box_pack_start ((GtkBox *) hbox, (GtkWidget *) progress, TRUE, TRUE, 0);

	gtk_widget_show (window);
}

static void
dialog_close (void)
{
	gtk_widget_destroy ((GtkWidget *) window);
}

static void
dialog_set_folder_name (const gchar *folder_name)
{
	gchar *text;

	text = g_strdup_printf (_("Migrating '%s':"), folder_name);
	gtk_label_set_text (label, text);
	g_free (text);

	gtk_progress_bar_set_fraction (progress, 0.0);

	while (gtk_events_pending ())
		gtk_main_iteration ();
}

static void
dialog_set_progress (double percent)
{
	gchar text[5];

	snprintf (text, sizeof (text), "%d%%", (gint) (percent * 100.0f));

	gtk_progress_bar_set_fraction (progress, percent);
	gtk_progress_bar_set_text (progress, text);

	while (gtk_events_pending ())
		gtk_main_iteration ();
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
	gchar **p = g_strsplit (path, "/", 0);
	gint i, j, starting_index;
	gint num_elements;
	gboolean conflict;
	GString *s = g_string_new (NULL);

	for (i = 0; p[i]; i ++);

	num_elements = i;
	i--;

	/* p[i] is now the last path element */

	/* check if it conflicts */
	starting_index = i;
	do {
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

static gboolean
migrate_ical (ECal *old_ecal, ECal *new_ecal)
{
	GList *l, *objects;
	gint num_added = 0;
	gint num_objects;
	gboolean retval = TRUE;

	/* both ecals are loaded, start the actual migration */
	if (!e_cal_get_object_list (old_ecal, "#t", &objects, NULL))
		return FALSE;

	num_objects = g_list_length (objects);
	for (l = objects; l; l = l->next) {
		icalcomponent *ical_comp = l->data;
		GError *error = NULL;

		if (!e_cal_create_object (new_ecal, ical_comp, NULL, &error)) {
			g_warning ("Migration of object failed: %s", error->message);
			retval = FALSE;
		}

		g_clear_error (&error);

		num_added ++;
		dialog_set_progress ((double)num_added / num_objects);
	}

	g_list_foreach (objects, (GFunc) icalcomponent_free, NULL);
	g_list_free (objects);

	return retval;
}

static gboolean
migrate_ical_folder_to_source (gchar *old_path, ESource *new_source, ECalSourceType type)
{
	ECal *old_ecal = NULL, *new_ecal = NULL;
	ESource *old_source;
	ESourceGroup *group;
	gchar *old_uri = g_filename_to_uri (old_path, NULL, NULL);
	GError *error = NULL;
	gboolean retval = FALSE;

	group = e_source_group_new ("", old_uri);
	old_source = e_source_new ("", "");
	e_source_group_add_source (group, old_source, -1);

	dialog_set_folder_name (e_source_peek_name (new_source));

	if (!(old_ecal = e_cal_new (old_source, type))) {
		g_warning ("could not find a backend for '%s'", e_source_get_uri (old_source));
		goto finish;
	}
	if (!e_cal_open (old_ecal, FALSE, &error)) {
		g_warning ("failed to load source ecal for migration: '%s' (%s)", error->message,
			   e_source_get_uri (old_source));
		goto finish;
	}

	if (!(new_ecal = e_cal_new (new_source, type))) {
		g_warning ("could not find a backend for '%s'", e_source_get_uri (new_source));
		goto finish;
	}
	if (!e_cal_open (new_ecal, FALSE, &error)) {
		g_warning ("failed to load destination ecal for migration: '%s' (%s)", error->message,
			   e_source_get_uri (new_source));
		goto finish;
	}

	retval = migrate_ical (old_ecal, new_ecal);

finish:
	g_clear_error (&error);
	if (old_ecal)
		g_object_unref (old_ecal);
	g_object_unref (group);
	if (new_ecal)
		g_object_unref (new_ecal);
	g_free (old_uri);

	return retval;
}

static gboolean
migrate_ical_folder (gchar *old_path, ESourceGroup *dest_group, gchar *source_name, ECalSourceType type)
{
	ESource *new_source;
	gboolean retval;

	new_source = e_source_new (source_name, source_name);
	e_source_set_relative_uri (new_source, e_source_peek_uid (new_source));
	e_source_group_add_source (dest_group, new_source, -1);

	retval = migrate_ical_folder_to_source (old_path, new_source, type);

	g_object_unref (new_source);

	return retval;
}

#endif	/* !G_OS_WIN32 */

#define WEBCAL_BASE_URI "webcal://"
#define CONTACTS_BASE_URI "contacts://"
#define BAD_CONTACTS_BASE_URI "contact://"
#define PERSONAL_RELATIVE_URI "system"
#define GROUPWISE_BASE_URI "groupwise://"

static ESourceGroup *
create_calendar_contact_source (ESourceList *source_list)
{
	ESourceGroup *group;
	ESource *source;

	/* Create the contacts group */
	group = e_source_group_new (_("Contacts"), CONTACTS_BASE_URI);
	e_source_list_add_group (source_list, group, -1);

	source = e_source_new (_("Birthdays & Anniversaries"), "/");
	e_source_group_add_source (group, source, -1);
	g_object_unref (source);

	e_source_set_color_spec (source, "#FED4D3");
	e_source_group_set_readonly (group, TRUE);

	return group;
}

static void
create_calendar_sources (CalendarComponent *component,
			 ESourceList   *source_list,
			 ESourceGroup **on_this_computer,
			 ESource **personal_source,
			 ESourceGroup **on_the_web,
			 ESourceGroup **contacts)
{
	GSList *groups;
	ESourceGroup *group;
	gchar *base_uri, *base_uri_proto;
	const gchar *base_dir;

	*on_this_computer = NULL;
	*on_the_web = NULL;
	*contacts = NULL;
	*personal_source = NULL;

	base_dir = calendar_component_peek_base_directory (component);
	base_uri = g_build_filename (base_dir, "local", NULL);

	base_uri_proto = g_filename_to_uri (base_uri, NULL, NULL);

	groups = e_source_list_peek_groups (source_list);
	if (groups) {
		/* groups are already there, we need to search for things... */
		GSList *g;

		for (g = groups; g; g = g->next) {

			group = E_SOURCE_GROUP (g->data);

			if (!strcmp (BAD_CONTACTS_BASE_URI, e_source_group_peek_base_uri (group)))
				e_source_group_set_base_uri (group, CONTACTS_BASE_URI);

			if (!strcmp (base_uri, e_source_group_peek_base_uri (group)))
				e_source_group_set_base_uri (group, base_uri_proto);

			if (!*on_this_computer && !strcmp (base_uri_proto, e_source_group_peek_base_uri (group)))
				*on_this_computer = g_object_ref (group);
			else if (!*on_the_web && !strcmp (WEBCAL_BASE_URI, e_source_group_peek_base_uri (group)))
				*on_the_web = g_object_ref (group);
			else if (!*contacts && !strcmp (CONTACTS_BASE_URI, e_source_group_peek_base_uri (group)))
				*contacts = g_object_ref (group);
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
	} else {
		/* create the local source group */
		group = e_source_group_new (_("On This Computer"), base_uri_proto);
		e_source_list_add_group (source_list, group, -1);

		*on_this_computer = group;
	}

	if (!*personal_source) {
		gchar *primary_calendar = calendar_config_get_primary_calendar ();

		/* Create the default Person calendar */
		ESource *source = e_source_new (_("Personal"), PERSONAL_RELATIVE_URI);
		e_source_group_add_source (*on_this_computer, source, -1);

		if (!primary_calendar && !calendar_config_get_calendars_selected ()) {
			GSList selected;

			calendar_config_set_primary_calendar (e_source_peek_uid (source));

			selected.data = (gpointer)e_source_peek_uid (source);
			selected.next = NULL;
			calendar_config_set_calendars_selected (&selected);
		}

		g_free (primary_calendar);
		e_source_set_color_spec (source, "#BECEDD");
		*personal_source = source;
	}

	if (!*on_the_web) {
		/* Create the Webcal source group */
		group = e_source_group_new (_("On The Web"), WEBCAL_BASE_URI);
		e_source_list_add_group (source_list, group, -1);

		*on_the_web = group;
	}

	if (!*contacts) {
		group = create_calendar_contact_source (source_list);

		*contacts = group;
	}

	g_free (base_uri_proto);
	g_free (base_uri);
}

static void
create_task_sources (TasksComponent *component,
		     ESourceList   *source_list,
		     ESourceGroup **on_this_computer,
		     ESourceGroup **on_the_web,
		     ESource **personal_source)
{
	GSList *groups;
	ESourceGroup *group;
	gchar *base_uri, *base_uri_proto;
	const gchar *base_dir;

	*on_this_computer = NULL;
	*on_the_web = NULL;
	*personal_source = NULL;

	base_dir = tasks_component_peek_base_directory (component);
	base_uri = g_build_filename (base_dir, "local", NULL);

	base_uri_proto = g_filename_to_uri (base_uri, NULL, NULL);

	groups = e_source_list_peek_groups (source_list);
	if (groups) {
		/* groups are already there, we need to search for things... */
		GSList *g;

		for (g = groups; g; g = g->next) {

			group = E_SOURCE_GROUP (g->data);

			if (!*on_this_computer && !strcmp (base_uri_proto, e_source_group_peek_base_uri (group)))
				*on_this_computer = g_object_ref (group);
			else if (!*on_the_web && !strcmp (WEBCAL_BASE_URI, e_source_group_peek_base_uri (group)))
				*on_the_web = g_object_ref (group);
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
	} else {
		/* create the local source group */
		group = e_source_group_new (_("On This Computer"), base_uri_proto);
		e_source_list_add_group (source_list, group, -1);

		*on_this_computer = group;
	}

	if (!*personal_source) {
		/* Create the default Person task list */
		ESource *source = e_source_new (_("Personal"), PERSONAL_RELATIVE_URI);
		e_source_group_add_source (*on_this_computer, source, -1);

		if (!calendar_config_get_primary_tasks () && !calendar_config_get_tasks_selected ()) {
			GSList selected;

			calendar_config_set_primary_tasks (e_source_peek_uid (source));

			selected.data = (gpointer)e_source_peek_uid (source);
			selected.next = NULL;
			calendar_config_set_tasks_selected (&selected);
		}

		e_source_set_color_spec (source, "#BECEDD");
		*personal_source = source;
	}

	if (!*on_the_web) {
		/* Create the Webcal source group */
		group = e_source_group_new (_("On The Web"), WEBCAL_BASE_URI);
		e_source_list_add_group (source_list, group, -1);

		*on_the_web = group;
	}

	g_free (base_uri_proto);
	g_free (base_uri);
}

#ifndef G_OS_WIN32

static void
migrate_pilot_db_key (const gchar *key, gpointer user_data)
{
	EXmlHash *xmlhash = user_data;

	e_xmlhash_add (xmlhash, key, "");
}

static void
migrate_pilot_data (const gchar *component, const gchar *conduit, const gchar *old_path, const gchar *new_path)
{
	gchar *changelog, *map;
	const gchar *dent;
	const gchar *ext;
	gchar *filename;
	GDir *dir;

	if (!(dir = g_dir_open (old_path, 0, NULL)))
		return;

	map = g_alloca (12 + strlen (conduit));
	sprintf (map, "pilot-map-%s-", conduit);

	changelog = g_alloca (24 + strlen (conduit));
	sprintf (changelog, "pilot-sync-evolution-%s-", conduit);

	while ((dent = g_dir_read_name (dir))) {
		if (!strncmp (dent, map, strlen (map)) &&
		    ((ext = strrchr (dent, '.')) && !strcmp (ext, ".xml"))) {
			/* pilot map file - src and dest file formats are identical */
			guchar inbuf[4096];
			gsize nread, nwritten;
			gint fd0, fd1;
			gssize n;

			filename = g_build_filename (old_path, dent, NULL);
			if ((fd0 = g_open (filename, O_RDONLY|O_BINARY, 0)) == -1) {
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
		} else if (!strncmp (dent, changelog, strlen (changelog)) &&
			   ((ext = strrchr (dent, '.')) && !strcmp (ext, ".db"))) {
			/* src and dest formats differ, src format is db3 while dest format is xml */
			EXmlHash *xmlhash;
			EDbHash *dbhash;
			struct stat st;

			filename = g_build_filename (old_path, dent, NULL);
			if (g_stat (filename, &st) == -1) {
				g_free (filename);
				continue;
			}

			dbhash = e_dbhash_new (filename);
			g_free (filename);

			filename = g_strdup_printf ("%s/%s.ics-%s", new_path, component, dent);
			if (g_stat (filename, &st) != -1)
				g_unlink (filename);
			xmlhash = e_xmlhash_new (filename);
			g_free (filename);

			e_dbhash_foreach_key (dbhash, migrate_pilot_db_key, xmlhash);

			e_dbhash_destroy (dbhash);

			e_xmlhash_write (xmlhash);
			e_xmlhash_destroy (xmlhash);
		}
	}

	g_dir_close (dir);
}

#endif

gboolean
migrate_calendars (CalendarComponent *component, gint major, gint minor, gint revision, GError **err)
{
	ESourceGroup *on_this_computer = NULL, *on_the_web = NULL, *contacts = NULL;
	ESource *personal_source = NULL;
	ECalEvent *ece;
	ECalEventTargetComponent *target;
	gboolean retval = FALSE;

	/* we call this unconditionally now - create_groups either
	   creates the groups/sources or it finds the necessary
	   groups/sources. */
	create_calendar_sources (component, calendar_component_peek_source_list (component), &on_this_computer, &personal_source, &on_the_web, &contacts);

#ifndef G_OS_WIN32
	if (major == 1) {
		xmlDocPtr config_doc = NULL;
		gchar *conf_file;
		struct stat st;

		conf_file = g_build_filename (g_get_home_dir (), "evolution", "config.xmldb", NULL);
		if (lstat (conf_file, &st) == 0 && S_ISREG (st.st_mode))
			config_doc = xmlParseFile (conf_file);
		g_free (conf_file);

		if (config_doc && minor <= 2) {
			GConfClient *gconf;
			gint res = 0;

			/* move bonobo config to gconf */
			gconf = gconf_client_get_default ();

			res = e_bconf_import (gconf, config_doc, calendar_remap_list);

			g_object_unref (gconf);

			xmlFreeDoc(config_doc);

			if (res != 0) {
				/* FIXME: set proper domain/code */
				g_set_error(err, 0, 0, _("Unable to migrate old settings from evolution/config.xmldb"));
				goto fail;
			}
		}

		if (minor <= 4) {
			GSList *migration_dirs, *l;
			gchar *path, *local_cal_folder;

			setup_progress_dialog (FALSE);

			path = g_build_filename (g_get_home_dir (), "evolution", "local", NULL);
			migration_dirs = e_folder_map_local_folders (path, "calendar");
			local_cal_folder = g_build_filename (path, "Calendar", NULL);
			g_free (path);

			if (personal_source)
				migrate_ical_folder_to_source (local_cal_folder, personal_source, E_CAL_SOURCE_TYPE_EVENT);

			for (l = migration_dirs; l; l = l->next) {
				gchar *source_name;

				if (personal_source && !strcmp ((gchar *)l->data, local_cal_folder))
					continue;

				source_name = get_source_name (on_this_computer, (gchar *)l->data);

				if (!migrate_ical_folder (l->data, on_this_computer, source_name, E_CAL_SOURCE_TYPE_EVENT)) {
					/* FIXME: domain/code */
					g_set_error(err, 0, 0, _("Unable to migrate calendar `%s'"), source_name);
					g_free(source_name);
					goto fail;
				}

				g_free (source_name);
			}

			g_free (local_cal_folder);

			dialog_close ();
		}

		if (minor <= 4 || (minor == 5 && revision < 5)) {
			GConfClient *gconf;
			GConfValue *gconf_val;
			gint i;
			const gchar *keys[] = {
				CALENDAR_CONFIG_HPANE_POS,
				CALENDAR_CONFIG_VPANE_POS,
				CALENDAR_CONFIG_MONTH_HPANE_POS,
				CALENDAR_CONFIG_MONTH_VPANE_POS,
				NULL
			};

			gconf = gconf_client_get_default ();

			for (i = 0; keys[i]; i++) {
				gconf_val = gconf_client_get (gconf, keys[i], NULL);
				if (gconf_val) {
					if (gconf_val->type != GCONF_VALUE_INT)
						gconf_client_unset (gconf, keys[i], NULL);
					gconf_value_free (gconf_val);
				}
			}

			g_object_unref (gconf);
		}

		if (minor < 5 || (minor == 5 && revision <= 10)) {
			gchar *old_path, *new_path;

			old_path = g_build_filename (g_get_home_dir (), "evolution", "local", "Calendar", NULL);
			new_path = g_build_filename (calendar_component_peek_base_directory (component),
						     "local", "system", NULL);
			migrate_pilot_data ("calendar", "calendar", old_path, new_path);
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
			for (g = e_source_list_peek_groups (calendar_component_peek_source_list (component)); g; g = g->next) {
				ESourceGroup *group = g->data;
				GSList *s;

				for (s = e_source_group_peek_sources (group); s; s = s->next) {
					ESource *source = s->data;
					e_source_set_absolute_uri (source, NULL);
				}
			}
		}

	}
#endif	/* !G_OS_WIN32 */

	e_source_list_sync (calendar_component_peek_source_list (component), NULL);

	/** @Event: component.migration
	 * @Title: Migration step in component initialization
	 * @Target: ECalEventTargetComponent
	 *
	 * component.migration is emitted during the calendar component
	 * initialization process. This allows new calendar backend types
	 * to be distributed as an e-d-s backend and a plugin without
	 * reaching their grubby little fingers into migration.c
	 */
	/* Fire off migration event */
	ece = e_cal_event_peek ();
	target = e_cal_event_target_new_component (ece, calendar_component_peek (), 0);
	e_event_emit ((EEvent *) ece, "component.migration", (EEventTarget *) target);

	retval = TRUE;
fail:
	if (on_this_computer)
		g_object_unref (on_this_computer);
	if (on_the_web)
		g_object_unref (on_the_web);
	if (contacts)
		g_object_unref (contacts);
	if (personal_source)
		g_object_unref (personal_source);

	return retval;
}

gboolean
migrate_tasks (TasksComponent *component, gint major, gint minor, gint revision, GError **err)
{
	ESourceGroup *on_this_computer = NULL;
	ESourceGroup *on_the_web = NULL;
	ESource *personal_source = NULL;
	gboolean retval = FALSE;

	/* we call this unconditionally now - create_groups either
	   creates the groups/sources or it finds the necessary
	   groups/sources. */
	create_task_sources (component, tasks_component_peek_source_list (component), &on_this_computer, &on_the_web, &personal_source);

#ifndef G_OS_WIN32
	if (major == 1) {
		xmlDocPtr config_doc = NULL;
		gchar *conf_file;

		conf_file = g_build_filename (g_get_home_dir (), "evolution", "config.xmldb", NULL);
		if (g_file_test (conf_file, G_FILE_TEST_IS_REGULAR))
			config_doc = e_xml_parse_file (conf_file);
		g_free (conf_file);

		if (config_doc && minor <= 2) {
			GConfClient *gconf;
			gint res = 0;

			/* move bonobo config to gconf */
			gconf = gconf_client_get_default ();

			res = e_bconf_import (gconf, config_doc, task_remap_list);

			g_object_unref (gconf);

			xmlFreeDoc(config_doc);

			if (res != 0) {
				g_set_error(err, 0, 0, _("Unable to migrate old settings from evolution/config.xmldb"));
				goto fail;
			}
		}

		if (minor <= 4) {
			GSList *migration_dirs, *l;
			gchar *path, *local_task_folder;

			setup_progress_dialog (TRUE);

			path = g_build_filename (g_get_home_dir (), "evolution", "local", NULL);
			migration_dirs = e_folder_map_local_folders (path, "tasks");
			local_task_folder = g_build_filename (path, "Tasks", NULL);
			g_free (path);

			if (personal_source)
				migrate_ical_folder_to_source (local_task_folder, personal_source, E_CAL_SOURCE_TYPE_TODO);

			for (l = migration_dirs; l; l = l->next) {
				gchar *source_name;

				if (personal_source && !strcmp ((gchar *)l->data, local_task_folder))
					continue;

				source_name = get_source_name (on_this_computer, (gchar *)l->data);

				if (!migrate_ical_folder (l->data, on_this_computer, source_name, E_CAL_SOURCE_TYPE_TODO)) {
					/* FIXME: domain/code */
					g_set_error(err, 0, 0, _("Unable to migrate tasks `%s'"), source_name);
					g_free(source_name);
					goto fail;
				}

				g_free (source_name);
			}

			g_free (local_task_folder);

			dialog_close ();
		}

		if (minor < 5 || (minor == 5 && revision <= 10)) {
			gchar *old_path, *new_path;

			old_path = g_build_filename (g_get_home_dir (), "evolution", "local", "Tasks", NULL);
			new_path = g_build_filename (tasks_component_peek_base_directory (component),
						     "local", "system", NULL);
			migrate_pilot_data ("tasks", "todo", old_path, new_path);
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
			for (g = e_source_list_peek_groups (tasks_component_peek_source_list (component)); g; g = g->next) {
				ESourceGroup *group = g->data;
				GSList *s;

				for (s = e_source_group_peek_sources (group); s; s = s->next) {
					ESource *source = s->data;
					e_source_set_absolute_uri (source, NULL);
				}
			}
		}
	}
#endif	/* !G_OS_WIN32 */
	e_source_list_sync (tasks_component_peek_source_list (component), NULL);
	retval = TRUE;
fail:
	if (on_this_computer)
		g_object_unref (on_this_computer);
	if (on_the_web)
		g_object_unref (on_the_web);
	if (personal_source)
		g_object_unref (personal_source);

        return retval;
}

/********************************************************************************************************
 *
 *		MEMOS
 *
 ********************************************************************************************************/

static void
create_memo_sources (MemosComponent *component,
		     ESourceList   *source_list,
		     ESourceGroup **on_this_computer,
		     ESourceGroup **on_the_web,
		     ESource **personal_source)
{
	GSList *groups;
	ESourceGroup *group;
	gchar *base_uri, *base_uri_proto;
	const gchar *base_dir;

	*on_this_computer = NULL;
	*on_the_web = NULL;
	*personal_source = NULL;

	base_dir = memos_component_peek_base_directory (component);
	base_uri = g_build_filename (base_dir, "local", NULL);

	base_uri_proto = g_filename_to_uri (base_uri, NULL, NULL);

	groups = e_source_list_peek_groups (source_list);
	if (groups) {
		/* groups are already there, we need to search for things... */
		GSList *g;

		for (g = groups; g; g = g->next) {

			group = E_SOURCE_GROUP (g->data);

			if (!*on_this_computer && !strcmp (base_uri_proto, e_source_group_peek_base_uri (group)))
				*on_this_computer = g_object_ref (group);
			else if (!*on_the_web && !strcmp (WEBCAL_BASE_URI, e_source_group_peek_base_uri (group)))
				*on_the_web = g_object_ref (group);
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
	} else {
		/* create the local source group */
		group = e_source_group_new (_("On This Computer"), base_uri_proto);
		e_source_list_add_group (source_list, group, -1);

		*on_this_computer = group;
	}

	if (!*personal_source) {
		/* Create the default Person task list */
		ESource *source = e_source_new (_("Personal"), PERSONAL_RELATIVE_URI);
		e_source_group_add_source (*on_this_computer, source, -1);

		if (!calendar_config_get_primary_memos () && !calendar_config_get_memos_selected ()) {
			GSList selected;

			calendar_config_set_primary_memos (e_source_peek_uid (source));

			selected.data = (gpointer)e_source_peek_uid (source);
			selected.next = NULL;
			calendar_config_set_memos_selected (&selected);
		}

		e_source_set_color_spec (source, "#BECEDD");
		*personal_source = source;
	}

	if (!*on_the_web) {
		/* Create the Webcal source group */
		group = e_source_group_new (_("On The Web"), WEBCAL_BASE_URI);
		e_source_list_add_group (source_list, group, -1);

		*on_the_web = group;
	}

	g_free (base_uri_proto);
	g_free (base_uri);
}

static gboolean
is_groupwise_account (EAccount *account)
{
	if (account->source->url != NULL) {
		return g_str_has_prefix (account->source->url, GROUPWISE_BASE_URI);
	} else {
		return FALSE;
	}
}

static void
add_gw_esource (ESourceList *source_list, const gchar *group_name,  const gchar *source_name, CamelURL *url, GConfClient *client)
{
	ESourceGroup *group;
	ESource *source;
	GSList *ids, *temp;
	GError *error = NULL;
	gchar *relative_uri;
	const gchar *soap_port;
	const gchar * use_ssl;
	const gchar *poa_address;
	const gchar *offline_sync;

	poa_address = url->host;
	if (!poa_address || strlen (poa_address) ==0)
		return;
	soap_port = camel_url_get_param (url, "soap_port");

	if (!soap_port || strlen (soap_port) == 0)
		soap_port = "7191";

	use_ssl = camel_url_get_param (url, "use_ssl");
	offline_sync = camel_url_get_param (url, "offline_sync");

	group = e_source_group_new (group_name,  GROUPWISE_BASE_URI);
	if (!e_source_list_add_group (source_list, group, -1))
		return;
	relative_uri = g_strdup_printf ("%s@%s/", url->user, poa_address);

	source = e_source_new (source_name, relative_uri);
	e_source_set_property (source, "auth", "1");
	e_source_set_property (source, "username", url->user);
	e_source_set_property (source, "port", camel_url_get_param (url, "soap_port"));
	e_source_set_property (source, "auth-domain", "Groupwise");
	e_source_set_property (source, "use_ssl", use_ssl);
	e_source_set_property (source, "offline_sync", offline_sync ? "1" : "0" );

	e_source_set_color_spec (source, "#EEBC60");
	e_source_group_add_source (group, source, -1);

	ids = gconf_client_get_list (client, CALENDAR_CONFIG_MEMOS_SELECTED_MEMOS, GCONF_VALUE_STRING, &error);
	if ( error != NULL ) {
		g_warning("%s (%s) %s\n", G_STRLOC, G_STRFUNC, error->message);
		g_error_free(error);
	}
	ids = g_slist_append (ids, g_strdup (e_source_peek_uid (source)));
	gconf_client_set_list (client, CALENDAR_CONFIG_MEMOS_SELECTED_MEMOS, GCONF_VALUE_STRING, ids, NULL);
	temp  = ids;
	for (; temp != NULL; temp = g_slist_next (temp))
		g_free (temp->data);

	g_slist_free (ids);
	g_object_unref (source);
	g_object_unref (group);
	g_free (relative_uri);
}

gboolean
migrate_memos (MemosComponent *component, gint major, gint minor, gint revision, struct _GError **err)
{
	ESourceGroup *on_this_computer = NULL;
	ESourceGroup *on_the_web = NULL;
	ESource *personal_source = NULL;
	ESourceList *source_list = NULL;
	gboolean retval = FALSE;

	source_list = memos_component_peek_source_list (component);

	/* we call this unconditionally now - create_groups either
	   creates the groups/sources or it finds the necessary
	   groups/sources. */
	create_memo_sources (component, source_list, &on_this_computer, &on_the_web, &personal_source);

	/* Migration for Gw accounts between versions < 2.8 */
	if (major == 2 && minor < 8) {
		EAccountList *al;
		EAccount *a;
		CamelURL *url;
		EIterator *it;
		GConfClient *gconf_client = gconf_client_get_default ();
		al = e_account_list_new (gconf_client);
		for (it = e_list_get_iterator((EList *)al);
				e_iterator_is_valid(it);
				e_iterator_next(it)) {
			a = (EAccount *) e_iterator_get(it);
			if (!a->enabled || !is_groupwise_account (a))
				continue;
			url = camel_url_new (a->source->url, NULL);
			add_gw_esource (source_list, a->name, _("Notes"), url, gconf_client);
			camel_url_free (url);
		}
		g_object_unref (al);
		g_object_unref (gconf_client);
	}

	e_source_list_sync (source_list, NULL);
	retval = TRUE;

	if (on_this_computer)
		g_object_unref (on_this_computer);
	if (on_the_web)
		g_object_unref (on_the_web);
	if (personal_source)
		g_object_unref (personal_source);

        return retval;
}
