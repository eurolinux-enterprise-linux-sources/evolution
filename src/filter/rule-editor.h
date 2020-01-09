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

#ifndef _RULE_EDITOR_H
#define _RULE_EDITOR_H

#include <gtk/gtk.h>
#include <glade/glade.h>

#include "rule-context.h"
#include "filter-rule.h"

#define RULE_TYPE_EDITOR            (rule_editor_get_type ())
#define RULE_EDITOR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), RULE_TYPE_EDITOR, RuleEditor))
#define RULE_EDITOR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), RULE_TYPE_EDITOR, RuleEditorClass))
#define IS_RULE_EDITOR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RULE_TYPE_EDITOR))
#define IS_RULE_EDITOR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), RULE_TYPE_EDITOR))
#define RULE_EDITOR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), RULE_TYPE_EDITOR, RuleEditorClass))

typedef struct _RuleEditor	RuleEditor;
typedef struct _RuleEditorClass	RuleEditorClass;
typedef struct _RuleEditorUndo	RuleEditorUndo;

struct _RuleEditor {
	GtkDialog parent_object;

	GtkListStore *model;
	GtkTreeView *list;

	RuleContext *context;
	FilterRule *current;
	FilterRule *edit;	/* for editing/adding rules, so we only do 1 at a time */

	GtkWidget *dialog;

	gchar *source;

	struct _RuleEditorUndo *undo_log;	/* cancel/undo log */
	guint undo_active:1; /* we're performing undo */

	struct _RuleEditorPrivate *priv;
};

struct _RuleEditorClass {
	GtkDialogClass parent_class;

	/* virtual methods */
	void (*set_sensitive) (RuleEditor *);
	void (*set_source) (RuleEditor *, const gchar *source);

	FilterRule *(*create_rule) (RuleEditor *);

	/* signals */
};

enum {
	RULE_EDITOR_LOG_EDIT,
	RULE_EDITOR_LOG_ADD,
	RULE_EDITOR_LOG_REMOVE,
	RULE_EDITOR_LOG_RANK
};

struct _RuleEditorUndo {
	struct _RuleEditorUndo *next;

	guint type;
	FilterRule *rule;
	gint rank;
	gint newrank;
};

GType rule_editor_get_type(void);
RuleEditor *rule_editor_new(RuleContext *rc, const gchar *source, const gchar *label);

void rule_editor_construct(RuleEditor *re, RuleContext *context, GladeXML *gui, const gchar *source, const gchar *label);

/* methods */
void rule_editor_set_source(RuleEditor *re, const gchar *source);
/* calculates the sensitivity of the editor */
void rule_editor_set_sensitive(RuleEditor *re);
/* used internally to create a new rule appropriate for the editor */
struct _FilterRule *rule_editor_create_rule(RuleEditor *re);

#endif /* ! _RULE_EDITOR_H */
