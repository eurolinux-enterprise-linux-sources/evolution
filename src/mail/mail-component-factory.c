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
 *		Ettore Perazzoli <ettore@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "mail-component.h"
#include "em-account-prefs.h"
#include "em-mailer-prefs.h"
#include "em-composer-prefs.h"
#include "em-network-prefs.h"

#include "mail-config-factory.h"
#include "mail-config.h"
#include "mail-mt.h"

#include "em-popup.h"
#include "em-menu.h"
#include "em-event.h"
#include "em-config.h"
#include "em-format-hook.h"
#include "em-junk-hook.h"
#include "em-format-html-display.h"

#include "importers/mail-importer.h"
#include "e-util/e-import.h"

#include <bonobo-activation/bonobo-activation.h>
#include <bonobo/bonobo-shlib-factory.h>

#include <string.h>

/* TODO: clean up these definitions */

#define FACTORY_ID	"OAFIID:GNOME_Evolution_Mail_Factory:" BASE_VERSION
#define COMPONENT_ID	"OAFIID:GNOME_Evolution_Mail_Component:" BASE_VERSION
#define FOLDER_INFO_ID	"OAFIID:GNOME_Evolution_FolderInfo:" BASE_VERSION

static BonoboObject *
factory(BonoboGenericFactory *factory, const gchar *component_id, gpointer closure)
{
	BonoboObject *o;

	if (strcmp (component_id, COMPONENT_ID) == 0) {
		MailComponent *component = mail_component_peek ();

		bonobo_object_ref (BONOBO_OBJECT (component));
		return BONOBO_OBJECT (component);
	} else if (strcmp (component_id, EM_ACCOUNT_PREFS_CONTROL_ID) == 0
		   || strcmp (component_id, EM_MAILER_PREFS_CONTROL_ID) == 0
		   || strcmp (component_id, EM_COMPOSER_PREFS_CONTROL_ID) == 0
		   || strcmp (component_id, EM_NETWORK_PREFS_CONTROL_ID) == 0) {
		return mail_config_control_factory_cb (factory, component_id, CORBA_OBJECT_NIL);
	}

	o = mail_importer_factory_cb(factory, component_id, NULL);
	if (o == NULL)
		g_warning (FACTORY_ID ": Don't know what to do with %s", component_id);

	return o;
}

static Bonobo_Unknown
make_factory (PortableServer_POA poa, const gchar *iid, gpointer impl_ptr, CORBA_Environment *ev)
{
	static gint init = 0;

	if (!init) {
		EImportClass *klass;

		init = 1;

		mail_config_init();
		mail_msg_init();

		e_plugin_hook_register_type(em_popup_hook_get_type());
		e_plugin_hook_register_type(em_menu_hook_get_type());
		e_plugin_hook_register_type(em_config_hook_get_type());

		em_format_hook_register_type(em_format_get_type());
		em_format_hook_register_type(em_format_html_get_type());
		em_format_hook_register_type(em_format_html_display_get_type());
		em_junk_hook_register_type(emj_get_type());

		e_plugin_hook_register_type(em_format_hook_get_type());
		e_plugin_hook_register_type(em_event_hook_get_type());
		e_plugin_hook_register_type(em_junk_hook_get_type());

		klass = g_type_class_ref(e_import_get_type());
		e_import_class_add_importer(klass, mbox_importer_peek(), NULL, NULL);
		e_import_class_add_importer(klass, elm_importer_peek(), NULL, NULL);
		e_import_class_add_importer(klass, pine_importer_peek(), NULL, NULL);
	}

	return bonobo_shlib_factory_std (FACTORY_ID, poa, impl_ptr, factory, NULL, ev);
}

static BonoboActivationPluginObject plugin_list[] = {
	{ FACTORY_ID, make_factory},
	{ NULL }
};

const  BonoboActivationPlugin Bonobo_Plugin_info = {
	plugin_list, "Evolution Mail component factory"
};
