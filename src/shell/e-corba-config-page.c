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

#include "e-corba-config-page.h"

#include "Evolution.h"

#include <string.h>

#include <bonobo/bonobo-widget.h>
#include <bonobo/bonobo-exception.h>
#include <bonobo/bonobo-object.h>
#include <bonobo/bonobo-listener.h>

struct _ECorbaConfigPagePrivate {
	GNOME_Evolution_ConfigControl config_control_interface;
};

G_DEFINE_TYPE (ECorbaConfigPage, e_corba_config_page, E_TYPE_CONFIG_PAGE)

/* GObject methods.  */

static void
impl_dispose (GObject *object)
{
	ECorbaConfigPage *corba_config_page;
	ECorbaConfigPagePrivate *priv;
	CORBA_Environment ev;

	corba_config_page = E_CORBA_CONFIG_PAGE (object);
	priv = corba_config_page->priv;

	CORBA_exception_init (&ev);

	if (priv->config_control_interface != CORBA_OBJECT_NIL) {
		bonobo_object_release_unref (priv->config_control_interface, &ev);
		priv->config_control_interface = CORBA_OBJECT_NIL;
	}

	CORBA_exception_free (&ev);

	(* G_OBJECT_CLASS (e_corba_config_page_parent_class)->dispose) (object);
}

static void
impl_finalize (GObject *object)
{
	ECorbaConfigPage *corba_config_page;
	ECorbaConfigPagePrivate *priv;

	corba_config_page = E_CORBA_CONFIG_PAGE (object);
	priv = corba_config_page->priv;

	g_free (priv);

	(* G_OBJECT_CLASS (e_corba_config_page_parent_class)->finalize) (object);
}

/* GTK+ ctors.  */

static void
e_corba_config_page_class_init (ECorbaConfigPageClass *class)
{
	GObjectClass *object_class;

	object_class = G_OBJECT_CLASS (class);
	object_class->dispose  = impl_dispose;
	object_class->finalize = impl_finalize;
}

static void
e_corba_config_page_init (ECorbaConfigPage *corba_config_page)
{
	ECorbaConfigPagePrivate *priv;

	priv = g_new (ECorbaConfigPagePrivate, 1);
	priv->config_control_interface = CORBA_OBJECT_NIL;

	corba_config_page->priv = priv;
}


gboolean
e_corba_config_page_construct (ECorbaConfigPage *corba_config_page,
			       GNOME_Evolution_ConfigControl corba_object)
{
	Bonobo_Control control;
	GtkWidget *control_widget;
	CORBA_Environment ev;

	g_return_val_if_fail (E_IS_CORBA_CONFIG_PAGE (corba_config_page), FALSE);
	g_return_val_if_fail (corba_object != CORBA_OBJECT_NIL, FALSE);

	CORBA_exception_init (&ev);

	control = GNOME_Evolution_ConfigControl__get_control (corba_object, &ev);
	if (BONOBO_EX (&ev)) {
		g_warning ("Can't get control from ::ConfigControl -- %s", BONOBO_EX_REPOID (&ev));
		CORBA_exception_init (&ev);
		return FALSE;
	}

	control_widget = bonobo_widget_new_control_from_objref (control, CORBA_OBJECT_NIL);
	gtk_widget_show (control_widget);
	gtk_container_add (GTK_CONTAINER (corba_config_page), control_widget);

	/* Notice we *don't* unref the corba_object here as
	   bonobo_widget_new_control_from_objref() effectively takes ownership
	   for the object that we get from ::__get_control.  */

	CORBA_exception_free (&ev);

	return TRUE;
}

GtkWidget *
e_corba_config_page_new_from_objref (GNOME_Evolution_ConfigControl corba_object)
{
	ECorbaConfigPage *corba_config_page;

	g_return_val_if_fail (corba_object != CORBA_OBJECT_NIL, NULL);
	g_return_val_if_fail (corba_object != CORBA_OBJECT_NIL, NULL);

	corba_config_page = g_object_new (e_corba_config_page_get_type (), NULL);
	if (! e_corba_config_page_construct (corba_config_page, corba_object)) {
		gtk_widget_destroy (GTK_WIDGET (corba_config_page));
		return NULL;
	}

	return GTK_WIDGET (corba_config_page);
}
