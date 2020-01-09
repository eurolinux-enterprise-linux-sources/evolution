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

#include "e-user-creatable-items-handler.h"
#include <e-util/e-icon-factory.h>
#include "Evolution.h"

#include "e-util/e-corba-utils.h"
#include "misc/e-combo-button.h"

#include <bonobo/bonobo-ui-util.h>
#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-control.h>

#include <gtk/gtk.h>
#include <glib/gi18n-lib.h>

#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#include <gconf/gconf-client.h>

struct _Component {
	gchar *id, *alias;
	GNOME_Evolution_Component component;
	GNOME_Evolution_CreatableItemTypeList *type_list;
};
typedef struct _Component Component;

/* Representation of a single menu item.  */
struct _MenuItem {
	const gchar *label;
	gchar shortcut;
	gchar *verb;
	gchar *tooltip;
	gchar *component;
	GdkPixbuf *icon;
	GdkPixbuf *icon_toolbar;
};
typedef struct _MenuItem MenuItem;

struct _EUserCreatableItemsHandlerPrivate {
	/* This component's alias */
	gchar *this_component;

	/* For creating items on the view */
	EUserCreatableItemsHandlerCreate create_local;
	gpointer create_data;

	/* The components that register user creatable items.  */
	GSList *components;	/* Component */

	/* The "New ..." menu items.  */
	GSList *objects;     /* MenuItem */
	GSList *folders;     /* MenuItem */

	/* The default item (the mailer's "message" item).  To be used when the
	   component in the view we are in doesn't provide a default user
	   creatable type.  This pointer always points to one of the menu items
	   in ->objects.  */
	const MenuItem *fallback_menu_item;
	const MenuItem *default_menu_item;

	gchar *menu_xml;
	GtkWidget *new_button, *new_menu;
	BonoboControl *new_control;
	GtkAccelGroup *accel_group;
};

enum {
	PROP_THIS_COMPONENT = 1,
	LAST_PROP
};

G_DEFINE_TYPE (EUserCreatableItemsHandler, e_user_creatable_items_handler, G_TYPE_OBJECT)

/* Component struct handling.  */

static Component *
component_new (const gchar *id,
	       const gchar *component_alias,
	       GNOME_Evolution_Component component)
{
	CORBA_Environment ev;
	Component *new;

	CORBA_exception_init (&ev);

	new = g_new (Component, 1);
	new->id = g_strdup (id);
	new->alias = g_strdup (component_alias);

	new->type_list = GNOME_Evolution_Component__get_userCreatableItems (component, &ev);
	if (BONOBO_EX (&ev))
		new->type_list = NULL;

	new->component = component;
	Bonobo_Unknown_ref (new->component, &ev);

	if (ev._major != CORBA_NO_EXCEPTION)
		new->type_list = NULL;

	CORBA_exception_free (&ev);

	return new;
}

static void
component_free (Component *component)
{
	CORBA_Environment ev;

	CORBA_exception_init (&ev);

	Bonobo_Unknown_unref (component->component, &ev);

	g_free (component->id);
	g_free (component->alias);

	if (component->type_list != NULL)
		CORBA_free (component->type_list);

	CORBA_exception_free (&ev);

	g_free (component);
}

static const gchar *component_query =
	"repo_ids.has ('IDL:GNOME/Evolution/Component:" BASE_VERSION "')";

static void
get_components_from_bonobo (EUserCreatableItemsHandler *handler)
{
	Bonobo_ServerInfoList *info_list;
	Bonobo_ActivationProperty *property;
	CORBA_Environment ev;
	const gchar *alias;
	gchar *iid;
	GNOME_Evolution_Component corba_component;
	Component *component;
	gint i;

	CORBA_exception_init (&ev);
	info_list = bonobo_activation_query (component_query, NULL, &ev);
	if (BONOBO_EX (&ev)) {
		gchar *ex_text = bonobo_exception_get_text (&ev);
		g_warning ("Cannot query for components: %s\n", ex_text);
		g_free (ex_text);
		CORBA_exception_free (&ev);
		return;
	}

	for (i = 0; i < info_list->_length; i++) {
		iid = info_list->_buffer[i].iid;
		corba_component = bonobo_activation_activate_from_id (iid, Bonobo_ACTIVATION_FLAG_EXISTING_ONLY, NULL, &ev);
		if (BONOBO_EX (&ev)) {
			CORBA_exception_free (&ev);
			continue;
		}

		property = bonobo_server_info_prop_find (&info_list->_buffer[i],
							 "evolution:component_alias");
		alias = property ? property->v._u.value_string : "unknown";

		component = component_new (iid, alias, corba_component);
		handler->priv->components = g_slist_prepend (handler->priv->components, component);
	}

	CORBA_free (info_list);
}

/* Helper functions.  */

static gboolean
item_is_default (const MenuItem *item,
		 const gchar *component)
{
	if (component == NULL)
		return FALSE;

	if (strcmp (item->component, component) == 0)
		return TRUE;
	else
		return FALSE;
}

static gchar *
create_verb (EUserCreatableItemsHandler *handler, gint component_num, const gchar *comp, const gchar *type_id)
{
	return g_strdup_printf ("EUserCreatableItemsHandler-%s:%d:%s", comp, component_num, type_id);
}

/* Setting up menu items for the "File -> New" submenu and the "New" toolbar
   button.  */

static void
ensure_menu_items (EUserCreatableItemsHandler *handler)
{
	EUserCreatableItemsHandlerPrivate *priv;
	GSList *objects, *folders;
	GSList *p;
	gint component_num;
	const gchar *default_verb;

	priv = handler->priv;
	if (priv->objects != NULL)
		return;

	objects = folders = NULL;
	component_num = 0;
	default_verb = NULL;
	for (p = priv->components; p != NULL; p = p->next) {
		const Component *component;
		gint i;

		component = (const Component *) p->data;
		if (component->type_list != NULL) {
			for (i = 0; i < component->type_list->_length; i ++) {
				const GNOME_Evolution_CreatableItemType *corba_item;
				MenuItem *item;

				corba_item = (const GNOME_Evolution_CreatableItemType *) component->type_list->_buffer + i;

				item = g_new (MenuItem, 1);
				item->label        = corba_item->menuDescription;
				item->shortcut     = corba_item->menuShortcut;
				item->verb         = create_verb (handler, component_num, component->alias, corba_item->id);
				item->tooltip      = corba_item->tooltip;
				item->component    = g_strdup (component->alias);

				if (strcmp (item->component, "mail") == 0
				    && strcmp (corba_item->id, "message") == 0)
					default_verb = item->verb;

				if (corba_item->iconName == NULL || *corba_item->iconName == '\0') {
					item->icon = NULL;
					item->icon_toolbar = NULL;
				} else {
					item->icon = e_icon_factory_get_icon (corba_item->iconName, GTK_ICON_SIZE_MENU);
					if (item_is_default (item, component->alias))
						item->icon_toolbar = e_icon_factory_get_icon (corba_item->iconName, GTK_ICON_SIZE_LARGE_TOOLBAR);
					else
						item->icon_toolbar = NULL;
				}

				if (corba_item->type == GNOME_Evolution_CREATABLE_OBJECT)
					objects = g_slist_prepend (objects, item);
				else
					folders = g_slist_prepend (folders, item);
			}
		}

		component_num ++;
	}

	priv->objects = g_slist_reverse (objects);
	priv->folders = g_slist_reverse (folders);

	priv->fallback_menu_item = NULL;
	if (default_verb != NULL) {
		for (p = priv->objects; p != NULL; p = p->next) {
			const MenuItem *item;

			item = (const MenuItem *) p->data;
			if (strcmp (item->verb, default_verb) == 0)
				priv->fallback_menu_item = item;
		}
	}
}

static void
free_menu_items (GSList *menu_items)
{
	GSList *p;

	if (menu_items == NULL)
		return;

	for (p = menu_items; p != NULL; p = p->next) {
		MenuItem *item;

		item = (MenuItem *) p->data;
		g_free (item->verb);

		if (item->icon != NULL)
			g_object_unref (item->icon);

		if (item->icon_toolbar != NULL)
			g_object_unref (item->icon_toolbar);

		g_free (item->component);
		g_free (item);
	}

	g_slist_free (menu_items);
}

static const MenuItem *
get_default_action_for_view (EUserCreatableItemsHandler *handler)
{
	EUserCreatableItemsHandlerPrivate *priv;
	const GSList *p;

	priv = handler->priv;

	for (p = priv->objects; p != NULL; p = p->next) {
		const MenuItem *item;

		item = (const MenuItem *) p->data;
		if (item_is_default (item, priv->this_component))
			return item;
	}

	return priv->fallback_menu_item;
}

/* Verb handling.  */

static void
execute_verb (EUserCreatableItemsHandler *handler,
	      const gchar *verb_name)
{
	EUserCreatableItemsHandlerPrivate *priv;
	const Component *component;
	gint component_number;
	const gchar *p;
	const gchar *id;
	GSList *component_list_item;
	gint i;

	priv = handler->priv;

	p = strchr (verb_name, ':');
	g_return_if_fail (p != NULL);
	component_number = atoi (p + 1);

	p = strchr (p + 1, ':');
	g_return_if_fail (p != NULL);
	id = p + 1;

	component_list_item = g_slist_nth (priv->components, component_number);
	g_return_if_fail (component_list_item != NULL);

	component = (const Component *) component_list_item->data;

	if (component->type_list == NULL)
		return;

	/* TODO: why do we actually iterate this?  Is it just to check we have it in the menu?  The
	   search isn't used otherwise */
	for (i = 0; i < component->type_list->_length; i ++) {
		if (strcmp (component->type_list->_buffer[i].id, id) == 0) {
			if (priv->create_local && priv->this_component && strcmp(priv->this_component, component->alias) == 0) {
				priv->create_local(handler, id, priv->create_data);
			} else {
				CORBA_Environment ev;

				CORBA_exception_init (&ev);

				GNOME_Evolution_Component_requestCreateItem (component->component, id, &ev);

				if (ev._major != CORBA_NO_EXCEPTION)
					g_warning ("Error in requestCreateItem -- %s", BONOBO_EX_REPOID (&ev));

				CORBA_exception_free (&ev);
			}
			return;
		}
	}
}

static void
verb_fn (BonoboUIComponent *ui_component,
	 gpointer data,
	 const gchar *verb_name)
{
	EUserCreatableItemsHandler *handler=
		E_USER_CREATABLE_ITEMS_HANDLER (data);

	execute_verb (handler, verb_name);
}

static void
add_verbs (EUserCreatableItemsHandler *handler,
	   BonoboUIComponent *ui_component)
{
	EUserCreatableItemsHandlerPrivate *priv;
	gint component_num;
	GSList *p;

	priv = handler->priv;

	component_num = 0;
	for (p = priv->components; p != NULL; p = p->next) {
		const Component *component;
		gint i;

		component = (const Component *) p->data;

		if (component->type_list != NULL) {
			for (i = 0; i < component->type_list->_length; i ++) {
				gchar *verb_name;

				verb_name = create_verb (handler,
							 component_num,
							 component->alias,
							 component->type_list->_buffer[i].id);

				bonobo_ui_component_add_verb (ui_component, verb_name, verb_fn, handler);

				g_free (verb_name);
			}
		}

		component_num ++;
	}
}

/* Generic menu construction code */

static gint
item_types_sort_func (gconstpointer a,
		      gconstpointer b)
{
	const MenuItem *item_a;
	const MenuItem *item_b;
	const gchar *p1, *p2;

	item_a = (const MenuItem *) a;
	item_b = (const MenuItem *) b;

	p1 = item_a->label;
	p2 = item_b->label;

	while (*p1 != '\0' && *p2 != '\0') {
		if (*p1 == '_') {
			p1 ++;
			continue;
		}

		if (*p2 == '_') {
			p2 ++;
			continue;
		}

		if (toupper ((gint) *p1) < toupper ((gint) *p2))
			return -1;
		else if (toupper ((gint) *p1) > toupper ((gint) *p2))
			return +1;

		p1 ++, p2 ++;
	}

	if (*p1 == '\0') {
		if (*p2 == '\0')
			return 0;
		else
			return -1;
	} else {
		return +1;
	}
}

typedef void (*EUserCreatableItemsHandlerMenuItemFunc) (EUserCreatableItemsHandler *, gpointer, MenuItem *, gboolean);
typedef void (*EUserCreatableItemsHandlerSeparatorFunc) (EUserCreatableItemsHandler *, gpointer, gint);

static void
construct_menu (EUserCreatableItemsHandler *handler, gpointer menu,
		EUserCreatableItemsHandlerMenuItemFunc menu_item_func,
		EUserCreatableItemsHandlerSeparatorFunc separator_func)
{
	EUserCreatableItemsHandlerPrivate *priv;
	MenuItem *item;
	GSList *p, *items;
	gboolean first = TRUE;

	priv = handler->priv;

	/* First add the current component's creatable objects */
	for (p = priv->objects; p != NULL; p = p->next) {
		item = p->data;
		if (item_is_default (item, priv->this_component)) {
			menu_item_func (handler, menu, item, first);
			first = FALSE;
		}
	}

	/* Then its creatable folders */
	for (p = priv->folders; p != NULL; p = p->next) {
		item = p->data;
		if (item_is_default (item, priv->this_component))
			menu_item_func (handler, menu, item, FALSE);
	}

	/* Then a separator */
	separator_func (handler, menu, 1);

	/* Then the objects from other components. */
	items = NULL;
	for (p = priv->objects; p != NULL; p = p->next) {
		item = p->data;
		if (! item_is_default (item, priv->this_component))
			items = g_slist_prepend (items, item);
	}

	items = g_slist_sort (items, item_types_sort_func);
	for (p = items; p != NULL; p = p->next)
		menu_item_func (handler, menu, p->data, FALSE);
	g_slist_free (items);

	/* Another separator */
	separator_func (handler, menu, 2);

	/* And finally the folders from other components */
	items = NULL;
	for (p = priv->folders; p != NULL; p = p->next) {
		item = p->data;
		if (! item_is_default (item, priv->this_component))
			items = g_slist_prepend (items, item);
	}

	items = g_slist_sort (items, item_types_sort_func);
	for (p = items; p != NULL; p = p->next)
		menu_item_func (handler, menu, p->data, FALSE);
	g_slist_free (items);
}

/* The XML description for "File -> New".  */

static void
xml_menu_item_func (EUserCreatableItemsHandler *handler, gpointer menu,
		    MenuItem *item, gboolean first)
{
	GString *xml = menu;
	gchar *encoded_label;
	gchar *encoded_tooltip;

	encoded_label = bonobo_ui_util_encode_str (item->label);
	g_string_append_printf (xml, "<menuitem name=\"New:%s\" verb=\"%s\" label=\"%s\"",
				item->verb, item->verb, encoded_label);

	if (first)
		g_string_append_printf (xml, " accel=\"*Control*N\"");
	else if (item->shortcut != '\0')
		g_string_append_printf (xml, " accel=\"*Control**Shift*%c\"", item->shortcut);

	if (item->icon != NULL) {
		gchar *icon_xml;

		icon_xml = bonobo_ui_util_pixbuf_to_xml (item->icon);
		g_string_append_printf (xml, " pixtype=\"pixbuf\" pixname=\"%s\"", icon_xml);
		g_free (icon_xml);
	}

	encoded_tooltip = bonobo_ui_util_encode_str (item->tooltip);
	g_string_append_printf (xml, " tip=\"%s\"", encoded_tooltip);

	g_string_append (xml, "/> ");

	g_free (encoded_label);
	g_free (encoded_tooltip);
}

static void
xml_separator_func (EUserCreatableItemsHandler *handler, gpointer menu, gint nth)
{
	GString *xml = menu;

	g_string_append_printf (xml, "<separator f=\"\" name=\"EUserCreatableItemsHandlerSeparator%d\"/>", nth);
}

static void
create_menu_xml (EUserCreatableItemsHandler *handler)
{
	GString *xml;

	xml = g_string_new ("<placeholder name=\"NewMenu\">");
	construct_menu (handler, xml, xml_menu_item_func, xml_separator_func);
	g_string_append (xml, "</placeholder>");

	handler->priv->menu_xml = xml->str;
	g_string_free (xml, FALSE);
}

/* The GtkMenu for the toolbar button.  */

static void
menuitem_activate (GtkMenuItem *item, gpointer data)
{
	EUserCreatableItemsHandler *handler = data;
	const gchar *verb;

	verb = g_object_get_data (G_OBJECT (item), "EUserCreatableItemsHandler:verb");
	execute_verb (handler, verb);
}

static void
default_activate (EComboButton *combo_button, gpointer data)
{
	EUserCreatableItemsHandler *handler = data;

	execute_verb (handler, handler->priv->default_menu_item->verb);
}

static void
gtk_menu_item_func (EUserCreatableItemsHandler *handler, gpointer menu,
		    MenuItem *item, gboolean first)
{
	GtkWidget *menuitem, *icon;

	menuitem = gtk_image_menu_item_new_with_mnemonic (item->label);

	if (item->icon) {
		icon = gtk_image_new_from_pixbuf (item->icon);
		gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (menuitem),
					       icon);
	}

	if (first) {
		gtk_widget_add_accelerator (menuitem, "activate",
					    handler->priv->accel_group,
					    'n', GDK_CONTROL_MASK,
					    GTK_ACCEL_VISIBLE);
	} else if (item->shortcut != '\0') {
		gtk_widget_add_accelerator (menuitem, "activate",
					    handler->priv->accel_group,
					    item->shortcut,
					    GDK_CONTROL_MASK | GDK_SHIFT_MASK,
					    GTK_ACCEL_VISIBLE);
	}

	g_object_set_data (G_OBJECT (menuitem), "EUserCreatableItemsHandler:verb", item->verb);
	g_signal_connect (menuitem, "activate",
			  G_CALLBACK (menuitem_activate), handler);

	gtk_menu_shell_append (menu, menuitem);
}

static void
gtk_separator_func (EUserCreatableItemsHandler *handler, gpointer menu, gint nth)
{
	gtk_menu_shell_append (menu, gtk_separator_menu_item_new ());
}

static void
set_combo_button_style (EComboButton *button, const gchar *style, GdkPixbuf *icon)
{
	if (!g_ascii_strcasecmp (style,"both-horiz")) {
		e_combo_button_pack_hbox (button);
		e_combo_button_set_label (button, _("New"));
		e_combo_button_set_icon (button, icon);
	}
	else if (!g_ascii_strcasecmp (style,"icons")) {
		e_combo_button_pack_hbox (button);
		e_combo_button_set_icon (button, icon);
		e_combo_button_set_label (button, "");
	}
	else if (!g_ascii_strcasecmp(style,"text")) {
		e_combo_button_pack_hbox (button);
		e_combo_button_set_label (button, _("New"));
		e_combo_button_set_icon (button, NULL);
	} else { /* Default to both */
		e_combo_button_pack_vbox (button);
		e_combo_button_set_icon (button, icon);
		e_combo_button_set_label (button, _("New"));
	}
}

static void
new_button_change (GConfClient *gconf,
		   guint connection_id,
		   GConfEntry *entry,
		   EUserCreatableItemsHandler *handler)
{
	EUserCreatableItemsHandlerPrivate *priv;
	gchar *val;

	priv = handler->priv;
	val = gconf_client_get_string (gconf, "/desktop/gnome/interface/toolbar_style", NULL);

	set_combo_button_style (E_COMBO_BUTTON (priv->new_button),
				val, priv->default_menu_item->icon_toolbar ? priv->default_menu_item->icon_toolbar : priv->default_menu_item->icon);

	g_free (val);
	gtk_widget_show (priv->new_button);
}

static void
setup_toolbar_button (EUserCreatableItemsHandler *handler)
{
	EUserCreatableItemsHandlerPrivate *priv;
	GConfClient *gconf = gconf_client_get_default ();
	gchar *val;

	priv = handler->priv;
	val = gconf_client_get_string (gconf, "/desktop/gnome/interface/toolbar_style", NULL);

	priv->new_button = e_combo_button_new ();
	priv->new_menu = gtk_menu_new ();
	priv->accel_group = gtk_accel_group_new ();
	construct_menu (handler, priv->new_menu,
			gtk_menu_item_func, gtk_separator_func);
	gtk_widget_show_all (priv->new_menu);
	e_combo_button_set_menu (E_COMBO_BUTTON (priv->new_button),
				 GTK_MENU (priv->new_menu));

	g_signal_connect (priv->new_button, "activate_default",
			  G_CALLBACK (default_activate), handler);

	priv->new_control = bonobo_control_new (priv->new_button);

	priv->default_menu_item = get_default_action_for_view (handler);
	if (!priv->default_menu_item) {
		gtk_widget_set_sensitive (priv->new_button, FALSE);
		g_object_unref (gconf);
		return;
	}

	gtk_widget_set_sensitive (priv->new_button, TRUE);

	set_combo_button_style (E_COMBO_BUTTON (priv->new_button),
				val, priv->default_menu_item->icon_toolbar ? priv->default_menu_item->icon_toolbar : priv->default_menu_item->icon);

	gconf_client_notify_add(gconf,"/desktop/gnome/interface/toolbar_style",
		(GConfClientNotifyFunc)new_button_change, handler, NULL, NULL);

	gtk_widget_set_tooltip_text (priv->new_button,
				     priv->default_menu_item->tooltip);
	gtk_widget_show (priv->new_button);

	g_free (val);
	g_object_unref (gconf);
}

/* GObject methods.  */

static void
impl_set_property (GObject *object, guint prop_id,
		   const GValue *value, GParamSpec *pspec)
{
	EUserCreatableItemsHandler *handler =
		E_USER_CREATABLE_ITEMS_HANDLER (object);

	switch (prop_id) {
	case PROP_THIS_COMPONENT:
		handler->priv->this_component = g_value_dup_string (value);

		get_components_from_bonobo (handler);
		ensure_menu_items (handler);
		break;
	default:
		break;
	}
}

static void
impl_dispose (GObject *object)
{
	EUserCreatableItemsHandler *handler;
	EUserCreatableItemsHandlerPrivate *priv;
	GSList *p;

	handler = E_USER_CREATABLE_ITEMS_HANDLER (object);
	priv = handler->priv;

	for (p = priv->components; p != NULL; p = p->next)
		component_free ((Component *) p->data);

	g_slist_free (priv->components);
	priv->components = NULL;

	if (priv->new_control) {
		bonobo_object_unref (priv->new_control);
		priv->new_control = NULL;
	}

	if (priv->accel_group) {
		g_object_unref (priv->accel_group);
		priv->accel_group = NULL;
	}

	(* G_OBJECT_CLASS (e_user_creatable_items_handler_parent_class)->dispose) (object);
}

static void
impl_finalize (GObject *object)
{
	EUserCreatableItemsHandler *handler;
	EUserCreatableItemsHandlerPrivate *priv;

	handler = E_USER_CREATABLE_ITEMS_HANDLER (object);
	priv = handler->priv;

	g_free (priv->this_component);

	free_menu_items (priv->objects);
	free_menu_items (priv->folders);

	g_free (priv->menu_xml);

	g_free (priv);

	(* G_OBJECT_CLASS (e_user_creatable_items_handler_parent_class)->finalize) (object);
}

static void
e_user_creatable_items_handler_class_init (EUserCreatableItemsHandlerClass *klass)
{
	GObjectClass *object_class;

	bindtextdomain (GETTEXT_PACKAGE, EVOLUTION_LOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

	object_class = G_OBJECT_CLASS (klass);
	object_class->dispose      = impl_dispose;
	object_class->finalize     = impl_finalize;
	object_class->set_property = impl_set_property;

	g_object_class_install_property (
		object_class, PROP_THIS_COMPONENT,
		g_param_spec_string ("this_component", "Component alias",
				     "The component_alias of this component",
				     NULL,
				     G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void
e_user_creatable_items_handler_init (EUserCreatableItemsHandler *handler)
{
	EUserCreatableItemsHandlerPrivate *priv;

	priv = g_new0 (EUserCreatableItemsHandlerPrivate, 1);

	handler->priv = priv;
}

EUserCreatableItemsHandler *
e_user_creatable_items_handler_new (const gchar *component_alias,
				    EUserCreatableItemsHandlerCreate create_local, gpointer data)
{
	EUserCreatableItemsHandler *handler;

	handler = g_object_new (e_user_creatable_items_handler_get_type (),
				"this_component", component_alias,
				NULL);
	handler->priv->create_local = create_local;
	handler->priv->create_data = data;

	return handler;
}

/**
 * e_user_creatable_items_handler_activate:
 * @handler: the #EUserCreatableItemsHandler
 * @ui_component: the #BonoboUIComponent to attach to
 *
 * Set up the menus and toolbar items for @ui_component.
 **/
void
e_user_creatable_items_handler_activate (EUserCreatableItemsHandler *handler,
					 BonoboUIComponent *ui_component)
{
	EUserCreatableItemsHandlerPrivate *priv;

	g_return_if_fail (E_IS_USER_CREATABLE_ITEMS_HANDLER (handler));
	g_return_if_fail (BONOBO_IS_UI_COMPONENT (ui_component));

	priv = handler->priv;

	if (!priv->menu_xml) {
		create_menu_xml (handler);
		setup_toolbar_button (handler);
		add_verbs (handler, ui_component);
	}

	bonobo_ui_component_set (ui_component, "/menu/File/New",
				 priv->menu_xml, NULL);

	bonobo_ui_component_object_set (ui_component,
					"/Toolbar/NewComboButton",
					BONOBO_OBJREF (priv->new_control),
					NULL);
}
