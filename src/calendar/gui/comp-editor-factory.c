/*
 * Evolution calendar - Component editor factory object
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
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <bonobo/bonobo-exception.h>
#include <evolution-calendar.h>
#include <libedataserver/e-url.h>
#include <libecal/e-cal.h>
#include "calendar-config.h"
#include "e-comp-editor-registry.h"
#include "comp-editor-factory.h"
#include "comp-util.h"
#include "common/authentication.h"
#include "dialogs/event-editor.h"
#include "dialogs/task-editor.h"

#ifdef G_OS_WIN32
const ECompEditorRegistry * const comp_editor_get_registry();
#define comp_editor_registry comp_editor_get_registry()
#else
extern ECompEditorRegistry *comp_editor_registry;
#endif



/* A pending request */

typedef enum {
	REQUEST_EXISTING,
	REQUEST_NEW
} RequestType;

typedef struct {
	RequestType type;

	union {
		struct {
			gchar *uid;
		} existing;

		struct {
			 GNOME_Evolution_Calendar_CompEditorFactory_CompEditorMode type;
		} new;
	} u;
} Request;

/* A client we have open */
typedef struct {
	/* Our parent CompEditorFactory */
	CompEditorFactory *factory;

	/* Uri of the calendar, used as key in the clients hash table */
	gchar *uri;

	/* Client of the calendar */
	ECal *client;

	/* Count editors using this client */
	gint editor_count;

	/* Pending requests; they are pending if the client is still being opened */
	GSList *pending;

	/* Whether this is open or still waiting */
	guint open : 1;
} OpenClient;

/* Private part of the CompEditorFactory structure */
struct CompEditorFactoryPrivate {
	/* Hash table of URI->OpenClient */
	GHashTable *uri_client_hash;
};



static void comp_editor_factory_class_init (CompEditorFactoryClass *class);
static void comp_editor_factory_init (CompEditorFactory *factory);
static void comp_editor_factory_finalize (GObject *object);

static void impl_editExisting (PortableServer_Servant servant,
			       const CORBA_char *str_uri,
			       const CORBA_char *uid,
			       const GNOME_Evolution_Calendar_CompEditorFactory_CompEditorMode corba_type,
			       CORBA_Environment *ev);
static void impl_editNew (PortableServer_Servant servant,
			  const CORBA_char *str_uri,
			  const GNOME_Evolution_Calendar_CalObjType type,
			  CORBA_Environment *ev);

static BonoboObjectClass *parent_class = NULL;



BONOBO_TYPE_FUNC_FULL (CompEditorFactory,
		       GNOME_Evolution_Calendar_CompEditorFactory,
		       BONOBO_OBJECT_TYPE,
		       comp_editor_factory)

/* Class initialization function for the component editor factory */
static void
comp_editor_factory_class_init (CompEditorFactoryClass *class)
{
	GObjectClass *object_class;

	object_class = (GObjectClass *) class;

	parent_class = g_type_class_peek_parent (class);

	class->epv.editExisting = impl_editExisting;
	class->epv.editNew = impl_editNew;

	object_class->finalize = comp_editor_factory_finalize;
}

/* Frees a Request structure */
static void
free_request (Request *r)
{
	if (r->type == REQUEST_EXISTING)
		g_free (r->u.existing.uid);

	g_free (r);
}

/* Frees an OpenClient structure */
static void
free_client (OpenClient *oc)
{
	GSList *l;

	g_free (oc->uri);
	oc->uri = NULL;

	g_object_unref (oc->client);
	oc->client = NULL;

	for (l = oc->pending; l; l = l->next) {
		Request *r;

		r = l->data;
		free_request (r);
	}
	g_slist_free (oc->pending);
	oc->pending = NULL;

	g_free (oc);
}

/* Object initialization function for the component editor factory */
static void
comp_editor_factory_init (CompEditorFactory *factory)
{
	CompEditorFactoryPrivate *priv;

	priv = g_new (CompEditorFactoryPrivate, 1);
	factory->priv = priv;

	priv->uri_client_hash = g_hash_table_new_full (
		g_str_hash, g_str_equal,
		(GDestroyNotify) NULL,
		(GDestroyNotify) free_client);
}

/* Destroy handler for the component editor factory */
static void
comp_editor_factory_finalize (GObject *object)
{
	CompEditorFactory *factory;
	CompEditorFactoryPrivate *priv;

	g_return_if_fail (object != NULL);
	g_return_if_fail (IS_COMP_EDITOR_FACTORY (object));

	factory = COMP_EDITOR_FACTORY (object);
	priv = factory->priv;

	g_hash_table_destroy (priv->uri_client_hash);
	priv->uri_client_hash = NULL;

	g_free (priv);
	factory->priv = NULL;

	if (G_OBJECT_CLASS (parent_class)->finalize)
		(* G_OBJECT_CLASS (parent_class)->finalize) (object);
}



/* Callback used when a component editor gets destroyed */
static void
editor_destroy_cb (GtkObject *object, gpointer data)
{
	OpenClient *oc;
	CompEditorFactory *factory;
	CompEditorFactoryPrivate *priv;

	oc = data;
	factory = oc->factory;
	priv = factory->priv;

	oc->editor_count--;

	/* See if we need to free the client */
	g_return_if_fail (oc->pending == NULL);

	if (oc->editor_count != 0)
		return;

	g_hash_table_remove (priv->uri_client_hash, oc->uri);
}

/* Starts editing an existing component on a client that is already open */
static void
edit_existing (OpenClient *oc, const gchar *uid)
{
	ECalComponent *comp;
	icalcomponent *icalcomp;
	CompEditor *editor;
	ECalComponentVType vtype;
	CompEditorFlags flags = { 0, };

	g_return_if_fail (oc->open);

	/* Get the object */
	if (!e_cal_get_object (oc->client, uid, NULL, &icalcomp, NULL)) {
		/* FIXME Better error handling */
		g_warning (G_STRLOC ": Syntax error while getting component `%s'", uid);

		return;
	}

	comp = e_cal_component_new ();
	if (!e_cal_component_set_icalcomponent (comp, icalcomp)) {
		g_object_unref (comp);
		icalcomponent_free (icalcomp);
		return;
	}

	/* Create the appropriate type of editor */

	vtype = e_cal_component_get_vtype (comp);
	if (itip_organizer_is_user (comp, oc->client))
		flags |= COMP_EDITOR_USER_ORG;

	switch (vtype) {
	case E_CAL_COMPONENT_EVENT:
		if (e_cal_component_has_attendees (comp))
			flags |= COMP_EDITOR_MEETING;

		editor = event_editor_new (oc->client, flags);
		break;

	case E_CAL_COMPONENT_TODO:
		editor = COMP_EDITOR (task_editor_new (oc->client, flags));
		break;

	default:
		g_message ("edit_exiting(): Unsupported object type %d", (gint) vtype);
		g_object_unref (comp);
		return;
	}

	/* Set the object on the editor */
	comp_editor_edit_comp (editor, comp);
	gtk_window_present (GTK_WINDOW (editor));
	g_object_unref (comp);

	oc->editor_count++;
	g_signal_connect (editor, "destroy", G_CALLBACK (editor_destroy_cb), oc);

	e_comp_editor_registry_add (comp_editor_registry, editor, TRUE);
}

static ECalComponent *
get_default_task (ECal *client)
{
	ECalComponent *comp;

	comp = cal_comp_task_new_with_defaults (client);

	return comp;
}

/* Edits a new object in the context of a client */
static void
edit_new (OpenClient *oc, const GNOME_Evolution_Calendar_CompEditorFactory_CompEditorMode type)
{
	ECalComponent *comp;
	CompEditor *editor;

	switch (type) {
	case GNOME_Evolution_Calendar_CompEditorFactory_EDITOR_MODE_EVENT:
		editor = event_editor_new (oc->client, FALSE);
		comp = cal_comp_event_new_with_current_time (oc->client, FALSE);
		break;
	case GNOME_Evolution_Calendar_CompEditorFactory_EDITOR_MODE_MEETING:
		editor = event_editor_new (oc->client, TRUE);
		comp = cal_comp_event_new_with_current_time (oc->client, FALSE);
		break;
	case GNOME_Evolution_Calendar_CompEditorFactory_EDITOR_MODE_ALLDAY_EVENT:
		editor = event_editor_new (oc->client, FALSE);
		comp = cal_comp_event_new_with_current_time (oc->client, TRUE);
		break;
	case GNOME_Evolution_Calendar_CompEditorFactory_EDITOR_MODE_TODO:
		editor = COMP_EDITOR (task_editor_new (oc->client, FALSE));
		comp = get_default_task (oc->client);
		break;
	default:
		g_return_if_reached ();
		return;
	}

	comp_editor_edit_comp (editor, comp);
	if (type == GNOME_Evolution_Calendar_CompEditorFactory_EDITOR_MODE_MEETING)
		event_editor_show_meeting (EVENT_EDITOR (editor));
	gtk_window_present (GTK_WINDOW (editor));

	oc->editor_count++;
	g_signal_connect (editor, "destroy", G_CALLBACK (editor_destroy_cb), oc);

	e_comp_editor_registry_add (comp_editor_registry, editor, TRUE);
}

/* Resolves all the pending requests for a client */
static void
resolve_pending_requests (OpenClient *oc)
{
	GSList *l;
	icaltimezone *zone;

	if (!oc->pending)
		return;

	/* Set the default timezone in the backend. */
	zone = calendar_config_get_icaltimezone ();

	/* FIXME Error handling? */
	e_cal_set_default_timezone (oc->client, zone, NULL);

	for (l = oc->pending; l; l = l->next) {
		Request *request;

		request = l->data;

		switch (request->type) {
		case REQUEST_EXISTING:
			edit_existing (oc, request->u.existing.uid);
			break;

		case REQUEST_NEW:
			edit_new (oc, request->u.new.type);
			break;
		}

		free_request (request);
	}

	g_slist_free (oc->pending);
	oc->pending = NULL;
}

/* Callback used when a client is finished opening.  We resolve all the pending
 * requests.
 */
static void
cal_opened_cb (ECal *client, ECalendarStatus status, gpointer data)
{
	OpenClient *oc;
	CompEditorFactory *factory;
	CompEditorFactoryPrivate *priv;
	GtkWidget *dialog = NULL;

	oc = data;
	factory = oc->factory;
	priv = factory->priv;

	switch (status) {
	case E_CALENDAR_STATUS_OK:
		oc->open = TRUE;
		resolve_pending_requests (oc);
		return;

	case E_CALENDAR_STATUS_OTHER_ERROR:
	case E_CALENDAR_STATUS_NO_SUCH_CALENDAR:
		dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
						 GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
						 "%s", _("Error while opening the calendar"));
		break;

	case E_CALENDAR_STATUS_PROTOCOL_NOT_SUPPORTED:
		dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
						 GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
						 "%s", _("Method not supported when opening the calendar"));
		break;

	case E_CALENDAR_STATUS_PERMISSION_DENIED :
		dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
						 GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
						 "%s", _("Permission denied to open the calendar"));
		break;

	case E_CALENDAR_STATUS_AUTHENTICATION_REQUIRED:
		dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
						 GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
						 "%s", _("Authentication Required"));
		break;

	case E_CALENDAR_STATUS_AUTHENTICATION_FAILED :
		dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
						 GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
						 "%s", _("Authentication Failed"));
		break;

	default:
		dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL,
						 GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE,
						 "%s", _("Unknown error"));
		return;
	}

	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);

	g_hash_table_remove (priv->uri_client_hash, oc->uri);
}

/* Creates a new OpenClient structure and queues the component editing/creation
 * process until the client is open.  Returns NULL if it could not issue the
 * open request.
 */
static OpenClient *
open_client (CompEditorFactory *factory, ECalSourceType source_type, const gchar *uristr)
{
	CompEditorFactoryPrivate *priv;
	ECal *client;
	OpenClient *oc;
	GError *error = NULL;

	priv = factory->priv;

	client = auth_new_cal_from_uri (uristr, source_type);
	if (!client)
		return NULL;

	oc = g_new (OpenClient, 1);
	oc->factory = factory;

	oc->uri = g_strdup (uristr);

	oc->client = client;
	oc->editor_count = 0;
	oc->pending = NULL;
	oc->open = FALSE;

	g_signal_connect (oc->client, "cal_opened", G_CALLBACK (cal_opened_cb), oc);

	g_hash_table_insert (priv->uri_client_hash, oc->uri, oc);

	if (!e_cal_open (oc->client, FALSE, &error)) {
		g_warning (G_STRLOC ": %s", error->message);
		g_free (oc->uri);
		g_object_unref (oc->client);
		g_free (oc);
		g_error_free (error);

		return NULL;
	}

	return oc;
}

/* Looks up an open client or queues it for being opened.  Returns the client or
 * NULL on failure; in the latter case it sets the ev exception.
 */
static OpenClient *
lookup_open_client (CompEditorFactory *factory, ECalSourceType source_type, const gchar *str_uri, CORBA_Environment *ev)
{
	CompEditorFactoryPrivate *priv;
	OpenClient *oc;
	EUri *uri;

	priv = factory->priv;

	/* Look up the client */

	uri = e_uri_new (str_uri);
	if (!uri) {
		bonobo_exception_set (ev, ex_GNOME_Evolution_Calendar_CompEditorFactory_InvalidURI);
		return NULL;
	}
	e_uri_free (uri);

	oc = g_hash_table_lookup (priv->uri_client_hash, str_uri);
	if (!oc) {
		oc = open_client (factory, source_type, str_uri);
		if (!oc) {
			bonobo_exception_set (ev, ex_GNOME_Evolution_Calendar_CompEditorFactory_BackendContactError);
			return NULL;
		}
	}

	return oc;
}

/* Queues a request for editing an existing object */
static void
queue_edit_existing (OpenClient *oc, const gchar *uid)
{
	Request *request;

	g_return_if_fail (!oc->open);

	request = g_new (Request, 1);
	request->type = REQUEST_EXISTING;
	request->u.existing.uid = g_strdup (uid);

	oc->pending = g_slist_append (oc->pending, request);
}

/* ::editExisting() method implementation */
static void
impl_editExisting (PortableServer_Servant servant,
		   const CORBA_char *str_uri,
		   const CORBA_char *uid,
		   const GNOME_Evolution_Calendar_CompEditorFactory_CompEditorMode corba_type,
		   CORBA_Environment *ev)
{
	CompEditorFactory *factory;
	OpenClient *oc;
	CompEditor *editor;
	ECalSourceType source_type;

	factory = COMP_EDITOR_FACTORY (bonobo_object_from_servant (servant));

	switch (corba_type) {
	case GNOME_Evolution_Calendar_CompEditorFactory_EDITOR_MODE_TODO:
		source_type = E_CAL_SOURCE_TYPE_TODO;
		break;
	default:
		source_type = E_CAL_SOURCE_TYPE_EVENT;
	}

	oc = lookup_open_client (factory, source_type, str_uri, ev);
	if (!oc)
		return;

	if (!oc->open) {
		queue_edit_existing (oc, uid);
		return;
	}

	/* Look up the component */
	editor = e_comp_editor_registry_find (comp_editor_registry, uid);
	if (editor == NULL) {
		edit_existing (oc, uid);
	} else {
		gtk_window_present (GTK_WINDOW (editor));
	}
}

/* Queues a request for creating a new object */
static void
queue_edit_new (OpenClient *oc, const GNOME_Evolution_Calendar_CompEditorFactory_CompEditorMode type)
{
	Request *request;

	g_return_if_fail (!oc->open);

	request = g_new (Request, 1);
	request->type = REQUEST_NEW;
	request->u.new.type = type;

	oc->pending = g_slist_append (oc->pending, request);
}

/* ::editNew() method implementation */
static void
impl_editNew (PortableServer_Servant servant,
	      const CORBA_char *str_uri,
	      const GNOME_Evolution_Calendar_CompEditorFactory_CompEditorMode corba_type,
	      CORBA_Environment *ev)
{
	CompEditorFactory *factory;
	OpenClient *oc;
	ECalSourceType source_type;

	factory = COMP_EDITOR_FACTORY (bonobo_object_from_servant (servant));

	switch (corba_type) {
	case GNOME_Evolution_Calendar_CompEditorFactory_EDITOR_MODE_TODO:
		source_type = E_CAL_SOURCE_TYPE_TODO;
		break;
	default:
		source_type = E_CAL_SOURCE_TYPE_EVENT;
	}

	oc = lookup_open_client (factory, source_type, str_uri, ev);
	if (!oc)
		return;

	if (!oc->open)
		queue_edit_new (oc, corba_type);
	else
		edit_new (oc, corba_type);
}



/**
 * comp_editor_factory_new:
 *
 * Creates a new calendar component editor factory.
 *
 * Return value: A newly-created component editor factory.
 **/
CompEditorFactory *
comp_editor_factory_new (void)
{
	return g_object_new (TYPE_COMP_EDITOR_FACTORY, NULL);
}

