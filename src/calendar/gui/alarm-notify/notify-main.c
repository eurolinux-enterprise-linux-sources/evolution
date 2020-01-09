/*
 * Evolution calendar - Alarm notification service main file
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
 *		Federico Mena-Quintero <federico@ximian.com>
 *      Rodrigo Moya <rodrigo@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <libgnome/gnome-init.h>
#include <libgnome/gnome-sound.h>
#include <libgnomeui/gnome-client.h>
#include <libgnomeui/gnome-ui-init.h>
#include <glade/glade.h>
#include <bonobo/bonobo-main.h>
#include <bonobo/bonobo-generic-factory.h>
#include <bonobo-activation/bonobo-activation.h>
#include <libedataserver/e-source.h>
#include <libedataserverui/e-passwords.h>
#include "e-util/e-icon-factory.h"
#include "e-util/e-util-private.h"
#include "alarm.h"
#include "alarm-queue.h"
#include "alarm-notify.h"
#include "config-data.h"
#include <camel/camel-object.h>



static BonoboGenericFactory *factory;

static AlarmNotify *alarm_notify_service = NULL;

/* Callback for the master client's "die" signal.  We must terminate the daemon
 * since the session is ending.
 */
static void
client_die_cb (GnomeClient *client)
{
	bonobo_main_quit ();
}

static gint
save_session_cb (GnomeClient *client, GnomeSaveStyle save_style, gint shutdown,
		 GnomeInteractStyle interact_style, gint fast, gpointer user_data)
{
	gchar *args[2];

	args[0] = g_build_filename (EVOLUTION_LIBEXECDIR,
				    "evolution-alarm-notify"
#ifdef G_OS_WIN32
				    ".exe"
#endif
				    ,
				    NULL);
	args[1] = NULL;
	gnome_client_set_restart_command (client, 1, args);
	g_free (args[0]);

	return TRUE;
}

/* Sees if a session manager is present.  If so, it tells the SM how to restart
 * the daemon when the session starts.  It also sets the die callback so that
 * the daemon can terminate properly when the session ends.
 */
static void
init_session (void)
{
	GnomeClient *master_client;

	master_client = gnome_master_client ();

	g_signal_connect (G_OBJECT (master_client), "die",
			  G_CALLBACK (client_die_cb), NULL);
	g_signal_connect (G_OBJECT (master_client), "save_yourself",
			  G_CALLBACK (save_session_cb), NULL);

	/* The daemon should always be started up by the session manager when
	 * the session starts.  The daemon will take care of loading whatever
	 * calendars it was told to load.
	 */
	gnome_client_set_restart_style (master_client, GNOME_RESTART_IF_RUNNING);
}

/* Factory function for the alarm notify service; just creates and references a
 * singleton service object.
 */
static BonoboObject *
alarm_notify_factory_fn (BonoboGenericFactory *factory, const gchar *component_id, gpointer data)
{
	g_return_val_if_fail (alarm_notify_service != NULL, NULL);

	bonobo_object_ref (BONOBO_OBJECT (alarm_notify_service));
	return BONOBO_OBJECT (alarm_notify_service);
}

/* Creates the alarm notifier */
static gboolean
init_alarm_service (gpointer user_data)
{
	alarm_notify_service = alarm_notify_new ();
	g_return_val_if_fail  (alarm_notify_service != NULL, FALSE);
	return FALSE;
}

gint
main (gint argc, gchar **argv)
{
	bindtextdomain (GETTEXT_PACKAGE, EVOLUTION_LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	gnome_program_init ("evolution-alarm-notify", VERSION, LIBGNOMEUI_MODULE, argc, argv, NULL);

	if (bonobo_init_full (&argc, argv, bonobo_activation_orb_get (),
			      CORBA_OBJECT_NIL, CORBA_OBJECT_NIL) == FALSE)
		g_error (_("Could not initialize Bonobo"));

	glade_init ();

	gnome_sound_init ("localhost");

	e_icon_factory_init ();

	init_alarm_service (NULL);

	factory = bonobo_generic_factory_new ("OAFIID:GNOME_Evolution_Calendar_AlarmNotify_Factory:" BASE_VERSION,
					      (BonoboFactoryCallback) alarm_notify_factory_fn, NULL);
	if (!factory) {
		g_warning (_("Could not create the alarm notify service factory, maybe it's already running..."));
		return 1;
	}

	init_session ();

	/* FIXME Ideally we should not use camel libraries in calendar, though it is the case
	   currently for attachments. Remove this once that is fixed.
	   Initialise global camel_object_type */
	camel_object_get_type();

	bonobo_main ();

	bonobo_object_unref (BONOBO_OBJECT (factory));
	factory = NULL;

	if (alarm_notify_service)
		bonobo_object_unref (BONOBO_OBJECT (alarm_notify_service));

	alarm_done ();

	e_passwords_shutdown ();
	gnome_sound_shutdown ();

	return 0;
}
