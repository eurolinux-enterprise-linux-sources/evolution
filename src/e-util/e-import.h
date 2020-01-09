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
 *		Michel Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef __E_IMPORT_H__
#define __E_IMPORT_H__

#include <gtk/gtk.h>
#include <libedataserver/e-msgport.h>

G_BEGIN_DECLS

/* This is an importer function */

typedef struct _EImport EImport;
typedef struct _EImportClass EImportClass;

typedef struct _EImportImporter EImportImporter;
typedef struct _EImportFactory EImportFactory;
typedef struct _EImportTarget EImportTarget;

typedef void (*EImportCompleteFunc)(EImport *ei, gpointer data);
typedef void (*EImportStatusFunc)(EImport *ei, const gchar *what, gint pc, gpointer data);

typedef void (*EImportFactoryFunc)(EImport *ei, gpointer data);
typedef void (*EImportImporterFunc)(EImportImporter *importer, gpointer data);
typedef gboolean (*EImportSupportedFunc)(EImport *ei, EImportTarget *, EImportImporter *im);
typedef GtkWidget *(*EImportWidgetFunc)(EImport *ei, EImportTarget *, EImportImporter *im);
typedef void (*EImportImportFunc)(EImport *ei, EImportTarget *, EImportImporter *im);

/* The global target types, implementors may add additional ones */
enum _e_import_target_t {
	E_IMPORT_TARGET_URI,	/* simple file */
	E_IMPORT_TARGET_HOME,	/* a home-directory thing, i.e. old applications */
	E_IMPORT_TARGET_LAST = 256
};

/**
 * struct _EImportImporter -
 *
 * @type: target type
 * @priority: Priority of importer.  Higher values will be processed first.
 * @supported: Callback to see if this target is supported by the importer.
 * @get_widget: A widget factory for this importer, if it needs any extra information in the druid.  It will update the target.
 * @import: Run the import.
 * @user_data: User data for the callbacks;
 *
 * Base importer description.
 **/
struct _EImportImporter {
	enum _e_import_target_t type;

	gint pri;

	EImportSupportedFunc supported;
	EImportWidgetFunc get_widget;
	EImportImportFunc import;
	EImportImportFunc cancel;

	gpointer user_data;

	/* ?? */
	gchar *name;
	gchar *description;
};

/**
 * struct _EImportTarget - importation context.
 *
 * @import: The parent object.
 * @type: The type of target, defined by implementing classes.
 * @data: This can be used to store run-time information
 * about this target.  Any allocated data must be set so
 * as to free it when the target is freed.
 *
 * The base target object is used as the parent and placeholder for
 * import context for a given importer.
 **/
struct _EImportTarget {
	struct _EImport *import;

	guint32 type;

	GData *data;

	/* implementation fields follow, depends on target type */
};

typedef struct _EImportTargetURI EImportTargetURI;
typedef struct _EImportTargetHome EImportTargetHome;

struct _EImportTargetURI {
	struct _EImportTarget target;

	gchar *uri_src;
	gchar *uri_dest;
};

struct _EImportTargetHome {
	struct _EImportTarget target;

	gchar *homedir;
};

/**
 * struct _EImport - An importer management object.
 *
 * @object: Superclass.
 * @id: ID of importer.
 * @status: Status callback of current running import.
 * @done: Completion callback of current running import.
 * @done_data: Callback data for both of above.
 *
 **/
struct _EImport {
	GObject object;

	gchar *id;

	EImportStatusFunc status;
	EImportCompleteFunc done;
	gpointer done_data;
};

/**
 * struct _EImportClass - Importer manager abstract class.
 *
 * @object_class: Superclass.
 * @factories: A list of factories registered on this type of
 * importuration manager.
 * @set_target: A virtual method used to set the target on the
 * importuration manager.  This is used by subclasses so they may hook
 * into changes on the target to propery drive the manager.
 * @target_free: A virtual method used to free the target in an
 * implementation-defined way.
 *
 **/
struct _EImportClass {
	GObjectClass object_class;

	EDList importers;

	void (*target_free)(EImport *ep, EImportTarget *t);
};

GType e_import_get_type(void);

EImport *e_import_new(const gchar *id);

/* Static class methods */
void e_import_class_add_importer(EImportClass *klass, EImportImporter *importer, EImportImporterFunc freefunc, gpointer data);
void e_import_class_remove_importer(EImportClass *klass, EImportImporter *f);

GSList *e_import_get_importers(EImport *emp, EImportTarget *target);

EImport *e_import_construct(EImport *, const gchar *id);

void e_import_import(EImport *ei, EImportTarget *, EImportImporter *, EImportStatusFunc status, EImportCompleteFunc done, gpointer data);
void e_import_cancel(EImport *, EImportTarget *, EImportImporter *);

GtkWidget *e_import_get_widget(EImport *ei, EImportTarget *, EImportImporter *);

void e_import_status(EImport *, EImportTarget *, const gchar *what, gint pc);
void e_import_complete(EImport *, EImportTarget *);

gpointer e_import_target_new(EImport *ep, gint type, gsize size);
void e_import_target_free(EImport *ep, gpointer o);

EImportTargetURI *e_import_target_new_uri(EImport *ei, const gchar *suri, const gchar *duri);
EImportTargetHome *e_import_target_new_home(EImport *ei, const gchar *home);

/* ********************************************************************** */

/* import plugin target, they are closely integrated */

/* To implement a basic import plugin, you just need to subclass
   this and initialise the class target type tables */

#include "e-util/e-plugin.h"

typedef struct _EPluginHookTargetMap EImportHookTargetMap;
typedef struct _EPluginHookTargetKey EImportHookTargetMask;

typedef struct _EImportHook EImportHook;
typedef struct _EImportHookClass EImportHookClass;

typedef struct _EImportHookImporter EImportHookImporter;

struct _EImportHookImporter {
	EImportImporter importer;

	/* user_data == EImportHook */

	gchar *supported;
	gchar *get_widget;
	gchar *import;
	gchar *cancel;
};

/**
 * struct _EImportHook - Plugin hook for importuration windows.
 *
 * @hook: Superclass.
 * @groups: A list of EImportHookGroup's of all importuration windows
 * this plugin hooks into.
 *
 **/
struct _EImportHook {
	EPluginHook hook;

	GSList *importers;
};

/**
 * struct _EImportHookClass - Abstract class for importuration window
 * plugin hooks.
 *
 * @hook_class: Superclass.
 * @target_map: A table of EImportHookTargetMap structures describing
 * the possible target types supported by this class.
 * @import_class: The EImport derived class that this hook
 * implementation drives.
 *
 * This is an abstract class defining the plugin hook point for
 * importuration windows.
 *
 **/
struct _EImportHookClass {
	EPluginHookClass hook_class;

	/* EImportHookTargetMap by .type */
	GHashTable *target_map;
	/* the import class these imports's belong to */
	EImportClass *import_class;
};

GType e_import_hook_get_type(void);

/* for implementors */
void e_import_hook_class_add_target_map(EImportHookClass *klass, const EImportHookTargetMap *);

G_END_DECLS

#endif /* __E_IMPORT_H__ */
