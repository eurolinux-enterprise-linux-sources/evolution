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
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _E_PLUGIN_H
#define _E_PLUGIN_H

#include <gtk/gtk.h>
#include <libxml/tree.h>

/* ********************************************************************** */

/* Standard GObject macros */
#define E_TYPE_PLUGIN \
	(e_plugin_get_type ())
#define E_PLUGIN(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_PLUGIN, EPlugin))
#define E_PLUGIN_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_PLUGIN, EPluginClass))
#define E_IS_PLUGIN(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_PLUGIN))
#define E_IS_PLUGIN_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_PLUGIN))
#define E_PLUGIN_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_PLUGIN, EPluginClass))

typedef struct _EPlugin EPlugin;
typedef struct _EPluginClass EPluginClass;

#define E_PLUGIN_CLASSID "org.gnome.evolution.plugin"

/* Structure to define the author(s) names and addresses */
typedef struct _EPluginAuthor EPluginAuthor;
struct _EPluginAuthor {
	gchar *name;
	gchar *email;
};

/**
 * struct _EPlugin - An EPlugin instance.
 *
 * @object: Superclass.
 * @id: Unique identifier for plugin instance.
 * @path: Filename where the xml definition resides.
 * @hooks_pending: A list hooks which can't yet be loaded.  This is
 * the xmlNodePtr to the root node of the hook definition.
 * @description: A description of the plugin's purpose.
 * @name: The name of the plugin.
 * @domain: The translation domain for this plugin.
 * @hooks: A list of the EPluginHooks this plugin requires.
 * @enabled: Whether the plugin is enabled or not.  This is not fully
 * implemented.
 *
 * The base EPlugin object is used to represent each plugin directly.
 * All of the plugin's hooks are loaded and managed through this
 * object.
 **/
struct _EPlugin {
	GObject object;

	gchar *id;
	gchar *path;
	GSList *hooks_pending;

	gchar *description;
	gchar *name;
	gchar *domain;
	GSList *hooks;
	GSList *authors;	/* EPluginAuthor structures */

	guint32 flags;

	guint enabled:1;
};

/**
 * struct _EPluginClass -
 *
 * @class: Superclass.
 * @type: The plugin type.  This is used by the plugin loader to
 * determine which plugin object to instantiate to handle the plugin.
 * This must be overriden by each subclass to provide a unique name.
 * @construct: The construct virtual method scans the XML tree to
 * initialise itself.
 * @invoke: The invoke virtual method loads the plugin code, resolves
 * the function name, and marshals a simple pointer to execute the
 * plugin.
 * @enable: Virtual method to enable/disable the plugin.
 *
 * The EPluginClass represents each plugin type.  The type of each class is
 * registered in a global table and is used to instantiate a
 * container for each plugin.
 *
 * It provides two main functions, to load the plugin definition, and
 * to invoke a function.  Each plugin class is used to handle mappings
 * to different languages.
 **/
struct _EPluginClass {
	GObjectClass parent_class;

	const gchar *type;

	gint (*construct)(EPlugin *, xmlNodePtr root);
	gpointer (*get_symbol)(EPlugin *, const gchar *name);
	gpointer (*invoke)(EPlugin *, const gchar *name, gpointer data);
	void (*enable)(EPlugin *, gint state);
	GtkWidget *(*get_configure_widget)(EPlugin *);
};

GType e_plugin_get_type(void);

gint e_plugin_construct(EPlugin *ep, xmlNodePtr root);
void e_plugin_add_load_path(const gchar *);
gint e_plugin_load_plugins(void);
void e_plugin_load_plugins_with_missing_symbols(void);
GSList * e_plugin_list_plugins(void);

void e_plugin_register_type(GType type);

gpointer e_plugin_get_symbol(EPlugin *ep, const gchar *name);
gpointer e_plugin_invoke(EPlugin *ep, const gchar *name, gpointer data);
void e_plugin_enable(EPlugin *eph, gint state);

GtkWidget *e_plugin_get_configure_widget (EPlugin *ep);

/* static helpers */
/* maps prop or content to 'g memory' */
gchar *e_plugin_xml_prop(xmlNodePtr node, const gchar *id);
gchar *e_plugin_xml_prop_domain(xmlNodePtr node, const gchar *id, const gchar *domain);
gint e_plugin_xml_int(xmlNodePtr node, const gchar *id, gint def);
gchar *e_plugin_xml_content(xmlNodePtr node);
gchar *e_plugin_xml_content_domain(xmlNodePtr node, const gchar *domain);

/* ********************************************************************** */
#include <gmodule.h>

/* Standard GObject macros */
#define E_TYPE_PLUGIN_LIB \
	(e_plugin_lib_get_type ())
#define E_PLUGIN_LIB(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_PLUGIN_LIB, EPluginLib))
#define E_PLUGIN_LIB_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_PLUGIN_LIB, EPluginLibClass))
#define E_IS_PLUGIN_LIB(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_PLUGIN_LIB))
#define E_IS_PLUGIN_LIB_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_PLUGIN_LIB))
#define E_PLUGIN_LIB_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_PLUGIN_LIB, EPluginLibClass))

typedef struct _EPluginLib EPluginLib;
typedef struct _EPluginLibClass EPluginLibClass;

/* The callback signature used for epluginlib methods */
typedef gpointer (*EPluginLibFunc)(EPluginLib *ep, gpointer data);
/* The setup method, this will be called when the plugin is
 * initialised.  In the future it may also be called when the plugin
 * is disabled. */
typedef gint (*EPluginLibEnableFunc)(EPluginLib *ep, gint enable);
typedef gpointer (*EPluginLibGetConfigureWidgetFunc)(EPluginLib *ep);

/**
 * struct _EPluginLib -
 *
 * @plugin: Superclass.
 * @location: The filename of the shared object.
 * @module: The GModule once it is loaded.
 *
 * This is a concrete EPlugin class.  It loads and invokes dynamically
 * loaded libraries using GModule.  The shared object isn't loaded
 * until the first callback is invoked.
 *
 * When the plugin is loaded, and if it exists, "e_plugin_lib_enable"
 * will be invoked to initialise the
 **/
struct _EPluginLib {
	EPlugin plugin;

	gchar *location;
	GModule *module;
};

/**
 * struct _EPluginLibClass -
 *
 * @plugin_class: Superclass.
 *
 * The plugin library needs no additional class data.
 **/
struct _EPluginLibClass {
	EPluginClass plugin_class;
};

GType e_plugin_lib_get_type(void);

/* ********************************************************************** */

/* Standard GObject macros */
#define E_TYPE_PLUGIN_HOOK \
	(e_plugin_hook_get_type ())
#define E_PLUGIN_HOOK(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_PLUGIN_HOOK, EPluginHook))
#define E_PLUGIN_HOOK_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_PLUGIN_HOOK, EPluginHookClass))
#define E_IS_PLUGIN_HOOK(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_PLUGIN_HOOK))
#define E_IS_PLUGIN_HOOK_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_PLUGIN_HOOK))
#define E_PLUGIN_HOOK_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_PLUGIN_HOOK, EPluginHookClass))

typedef struct _EPluginHook EPluginHook;
typedef struct _EPluginHookClass EPluginHookClass;

/* utilities for subclasses to use */
typedef struct _EPluginHookTargetMap EPluginHookTargetMap;
typedef struct _EPluginHookTargetKey EPluginHookTargetKey;

/**
 * struct _EPluginHookTargetKey -
 *
 * @key: Enumeration value as a string.
 * @value: Enumeration value as an integer.
 *
 * A multi-purpose string to id mapping structure used with various
 * helper functions to simplify plugin hook subclassing.
 **/
struct _EPluginHookTargetKey {
	const gchar *key;
	guint32 value;
};

/**
 * struct _EPluginHookTargetMap -
 *
 * @type: The string id of the target.
 * @id: The integer id of the target.  Maps directly to the type field
 * of the various plugin type target id's.
 * @mask_bits: A zero-fill terminated array of EPluginHookTargetKeys.
 *
 * Used by EPluginHook to define mappings of target type enumerations
 * to and from strings.  Also used to define the mask option names
 * when reading the XML plugin hook definitions.
 **/
struct _EPluginHookTargetMap {
	const gchar *type;
	gint id;
	const struct _EPluginHookTargetKey *mask_bits;	/* null terminated array */
};

/**
 * struct _EPluginHook - A plugin hook.
 *
 * @object: Superclass.
 * @plugin: The parent object.
 *
 * An EPluginHook is used as a container for each hook a given plugin
 * is listening to.
 **/
struct _EPluginHook {
	GObject object;

	struct _EPlugin *plugin;
};

/**
 * struct _EPluginHookClass -
 *
 * @class: Superclass.
 * @id: The plugin hook type. This must be overriden by each subclass
 * and is used as a key when loading hook definitions.  This string
 * should contain a globally unique name followed by a : and a version
 * specification.  This is to ensure plugins only hook into hooks with
 * the right API.
 * @construct: Virtual method used to initialise the object when
 * loaded.
 * @enable: Virtual method used to enable or disable the hook.
 *
 * The EPluginHookClass represents each hook type.  The type of the
 * class is registered in a global table and is used to instantiate a
 * container for each hook.
 **/
struct _EPluginHookClass {
	GObjectClass parent_class;

	const gchar *id;

	gint (*construct)(EPluginHook *eph, EPlugin *ep, xmlNodePtr root);
	void (*enable)(EPluginHook *eph, gint state);
};

GType e_plugin_hook_get_type(void);

void e_plugin_hook_register_type(GType type);

EPluginHook * e_plugin_hook_new(EPlugin *ep, xmlNodePtr root);
void e_plugin_hook_enable(EPluginHook *eph, gint state);

/* static methods */
guint32 e_plugin_hook_mask(xmlNodePtr root, const struct _EPluginHookTargetKey *map, const gchar *prop);
guint32 e_plugin_hook_id(xmlNodePtr root, const struct _EPluginHookTargetKey *map, const gchar *prop);

/* ********************************************************************** */

/* EPluginTypeHook lets a plugin register a new plugin type.
  <hook class="org.gnome.evolution.plugin.type:1.0">
   <plugin-type get-type="e_plugin_mono_get_type/>
  </hook>
*/

/* Standard GObject macros */
#define E_TYPE_PLUGIN_TYPE_HOOK \
	(e_plugin_type_hook_get_type ())
#define E_PLUGIN_TYPE_HOOK(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST \
	((obj), E_TYPE_PLUGIN_TYPE_HOOK, EPluginTypeHook))
#define E_PLUGIN_TYPE_HOOK_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_CAST \
	((cls), E_TYPE_PLUGIN_TYPE_HOOK, EPluginTypeHookClass))
#define E_IS_PLUGIN_TYPE_HOOK(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE \
	((obj), E_TYPE_PLUGIN_TYPE_HOOK))
#define E_IS_PLUGIN_TYPE_HOOK_CLASS(cls) \
	(G_TYPE_CHECK_CLASS_TYPE \
	((cls), E_TYPE_PLUGIN_TYPE_HOOK))
#define E_PLUGIN_TYPE_HOOK_GET_CLASS(obj) \
	(G_TYPE_INSTANCE_GET_CLASS \
	((obj), E_TYPE_PLUGIN_TYPE_HOOK, EPluginTypeHookClass))

typedef struct _EPluginTypeHook EPluginTypeHook;
typedef struct _EPluginTypeHookClass EPluginTypeHookClass;

struct _EPluginTypeHook {
	EPluginHook hook;

	gchar *get_type;
	guint idle;
};

struct _EPluginTypeHookClass {
	EPluginHookClass hook_class;
};

GType e_plugin_type_hook_get_type(void);

/* README: Currently there is only one flag.
   But we may need more in the future and hence makes
   sense to keep as an enum */

typedef enum _EPluginFlags {
	E_PLUGIN_FLAGS_SYSTEM_PLUGIN = 1 << 0
} EPluginFlags;

#endif /* ! _E_PLUGIN_H */

