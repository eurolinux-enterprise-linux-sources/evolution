/*
 * Helper class for evolution components to setup a view
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
 *		Michael Zucchi <notzed@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>

#include "e-component-view.h"

#include "bonobo/bonobo-control.h"

static BonoboObjectClass *parent_class = NULL;

static void
impl_ComponentView_getControls(PortableServer_Servant servant,
		 Bonobo_Control *side_control,
		 Bonobo_Control *view_control,
		 Bonobo_Control *statusbar_control,
		 CORBA_Environment *ev)
{
	EComponentView *ecv = (EComponentView *)bonobo_object_from_servant(servant);

	*side_control = CORBA_Object_duplicate (BONOBO_OBJREF (ecv->side_control), ev);
	*view_control = CORBA_Object_duplicate (BONOBO_OBJREF (ecv->view_control), ev);
	*statusbar_control = CORBA_Object_duplicate (BONOBO_OBJREF (ecv->statusbar_control), ev);
}

/* GObject methods.  */

static void
impl_dispose (GObject *object)
{
	EComponentView *ecv = (EComponentView *)object;

	ecv->side_control = NULL;
	ecv->view_control = NULL;
	ecv->statusbar_control = NULL;

	((GObjectClass *)parent_class)->dispose(object);
}

static void
impl_finalise (GObject *object)
{
	EComponentView *ecv = (EComponentView *)object;

	g_free(ecv->id);

	((GObjectClass *)parent_class)->finalize(object);
}

static void
e_component_view_class_init (EComponentViewClass *klass)
{
	GObjectClass *object_class;
	POA_GNOME_Evolution_ComponentView__epv *epv;

	parent_class = g_type_class_ref(bonobo_object_get_type());

	object_class = G_OBJECT_CLASS (klass);
	object_class->dispose  = impl_dispose;
	object_class->finalize = impl_finalise;

	epv = & klass->epv;
	epv->getControls = impl_ComponentView_getControls;
}

static void
e_component_view_init (EComponentView *shell)
{
}

EComponentView *e_component_view_new(GNOME_Evolution_ShellView parent, const gchar *id, GtkWidget *side, GtkWidget *view, GtkWidget *statusbar)
{
	EComponentView *new = g_object_new (e_component_view_get_type (), NULL);
	CORBA_Environment ev = { NULL };

	new->id = g_strdup(id);
	new->shell_view = CORBA_Object_duplicate(parent, &ev);
	CORBA_exception_free(&ev);

	/* FIXME: hook onto destroys */
	new->side_control = bonobo_control_new(side);
	new->view_control = bonobo_control_new(view);
	new->statusbar_control = bonobo_control_new(statusbar);

	return new;
}

EComponentView *e_component_view_new_controls(GNOME_Evolution_ShellView parent, const gchar *id, BonoboControl *side, BonoboControl *view, BonoboControl *statusbar)
{
	EComponentView *new = g_object_new (e_component_view_get_type (), NULL);
	CORBA_Environment ev = { NULL };

	new->id = g_strdup(id);
	new->shell_view = CORBA_Object_duplicate(parent, &ev);
	CORBA_exception_free(&ev);

	/* FIXME: hook onto destroys */
	new->side_control = side;
	new->view_control = view;
	new->statusbar_control = statusbar;

	return new;
}

void
e_component_view_set_title(EComponentView *ecv, const gchar *title)
{
	CORBA_Environment ev = { NULL };

	/* save roundtrips, check title is the same */
	GNOME_Evolution_ShellView_setTitle(ecv->shell_view, ecv->id, title, &ev);
	CORBA_exception_free(&ev);
}

void
e_component_view_set_button_icon (EComponentView *ecv, const gchar *iconName)
{
	CORBA_Environment ev = { NULL };

	/* save roundtrips, check title is the same */
	GNOME_Evolution_ShellView_setButtonIcon(ecv->shell_view, ecv->id, iconName, &ev);
	CORBA_exception_free(&ev);
}

BONOBO_TYPE_FUNC_FULL (EComponentView, GNOME_Evolution_ComponentView, bonobo_object_get_type(), e_component_view)

