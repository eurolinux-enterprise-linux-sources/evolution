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
 *		Damon Chaplin <damon@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-dropdown-button.h"

#include <stdio.h>

struct _EDropdownButtonPrivate {
	GtkAccelGroup *accel_group;
	GtkWidget *menu;
};

G_DEFINE_TYPE (EDropdownButton, e_dropdown_button, GTK_TYPE_TOGGLE_BUTTON)

/* Callback to position the pop-up menu.  */

static void
menu_position_cb (GtkMenu *menu,
		  gint *x,
		  gint *y,
		  gboolean *push_in,
		  gpointer data)
{
	EDropdownButton *dropdown_button;
	EDropdownButtonPrivate *priv;
	GtkRequisition menu_requisition;
	gint max_x, max_y;

	dropdown_button = E_DROPDOWN_BUTTON (data);
	priv = dropdown_button->priv;

	/* Calculate our preferred position.  */
	gdk_window_get_origin (GTK_WIDGET (dropdown_button)->window, x, y);
	*y += GTK_WIDGET (dropdown_button)->allocation.height;

	/* Now make sure we are on the screen.  */
	gtk_widget_size_request (GTK_WIDGET (priv->menu), &menu_requisition);
	max_x = MAX (0, gdk_screen_width () - menu_requisition.width);
	max_y = MAX (0, gdk_screen_height () - menu_requisition.height);

	*x = CLAMP (*x, 0, max_x);
	*y = CLAMP (*y, 0, max_y);
}

/* Callback for the "deactivate" signal on the pop-up menu.  This is used so
   that we unset the state of the toggle button when the pop-up menu
   disappears.  */

static gint
menu_deactivate_cb (GtkMenuShell *menu_shell,
		    gpointer data)
{
	EDropdownButton *dropdown_button;

	dropdown_button = E_DROPDOWN_BUTTON (data);

	gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (dropdown_button), FALSE);
	return TRUE;
}


/* GtkObject methods.  */

static void
impl_destroy (GtkObject *object)
{
	EDropdownButton *dropdown_button;
	EDropdownButtonPrivate *priv;

	dropdown_button = E_DROPDOWN_BUTTON (object);
	priv = dropdown_button->priv;

	g_object_unref (priv->accel_group);
	gtk_widget_destroy (priv->menu);

	g_free (priv);

	if (GTK_OBJECT_CLASS (e_dropdown_button_parent_class)->destroy)
		(* GTK_OBJECT_CLASS (e_dropdown_button_parent_class)->destroy) (object);
}


/* GtkWidget methods.  */

static void
impl_toggled (GtkToggleButton *toggle_button)
{
	EDropdownButton *dropdown_button;
	EDropdownButtonPrivate *priv;

	if (GTK_TOGGLE_BUTTON_CLASS (e_dropdown_button_parent_class)->toggled)
		GTK_TOGGLE_BUTTON_CLASS (e_dropdown_button_parent_class)->toggled (toggle_button);

	dropdown_button = E_DROPDOWN_BUTTON (toggle_button);
	priv = dropdown_button->priv;

	if (toggle_button->active) {
		gtk_menu_popup (GTK_MENU (priv->menu), NULL, NULL,
				menu_position_cb, dropdown_button,
				1, GDK_CURRENT_TIME);
	} else {
		gtk_menu_popdown (GTK_MENU (priv->menu));
	}
}


static void
e_dropdown_button_class_init (EDropdownButtonClass *klass)
{
	GtkObjectClass *object_class;
	GtkToggleButtonClass *toggle_class;

	object_class = GTK_OBJECT_CLASS (klass);
	toggle_class = GTK_TOGGLE_BUTTON_CLASS (klass);

	object_class->destroy = impl_destroy;
	toggle_class->toggled = impl_toggled;
}

static void
e_dropdown_button_init (EDropdownButton *dropdown_button)
{
	EDropdownButtonPrivate *priv;

	priv = g_new (EDropdownButtonPrivate, 1);
	priv->accel_group = gtk_accel_group_new ();
	priv->menu        = NULL;

	dropdown_button->priv = priv;
}


/**
 * e_dropdown_button_construct:
 * @dropdown_button: A pointer to an %EDropdownButton object
 * @label_text: Text to display in the button
 * @menu: The menu to pop up when the button is pressed
 *
 * Construct the @dropdown_button with the specified @label_text and the
 * associated @menu.
 **/
void
e_dropdown_button_construct (EDropdownButton *dropdown_button,
			     const gchar *label_text,
			     GtkMenu *menu)
{
	EDropdownButtonPrivate *priv;
	GtkWidget *hbox;
	GtkWidget *arrow;
	GtkWidget *label;
	guint accel_key;

	g_return_if_fail (dropdown_button != NULL);
	g_return_if_fail (E_IS_DROPDOWN_BUTTON (dropdown_button));
	g_return_if_fail (label_text != NULL);
	g_return_if_fail (menu != NULL);
	g_return_if_fail (GTK_IS_MENU (menu));

	priv = dropdown_button->priv;

	hbox = gtk_hbox_new (FALSE, 2);
	gtk_container_add (GTK_CONTAINER (dropdown_button), hbox);
	gtk_widget_show (hbox);

	label = gtk_label_new ("");
	gtk_label_set_label (GTK_LABEL (label), label_text);
	gtk_label_set_use_markup (GTK_LABEL (label), FALSE);
	gtk_label_set_use_underline (GTK_LABEL (label), TRUE);

	accel_key = gtk_label_get_mnemonic_keyval (GTK_LABEL (label));
	gtk_box_pack_start (GTK_BOX (hbox), label, TRUE, TRUE, 0);
	gtk_widget_show (label);
	gtk_widget_add_accelerator (GTK_WIDGET (dropdown_button), "clicked",
				    priv->accel_group, accel_key, GDK_MOD1_MASK, 0);

	arrow = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_NONE);
	gtk_box_pack_start (GTK_BOX (hbox), arrow, FALSE, FALSE, 2);
	gtk_widget_show (arrow);

	priv->menu = GTK_WIDGET (menu);

	g_signal_connect_object (priv->menu, "deactivate",
				 G_CALLBACK (menu_deactivate_cb),
				 dropdown_button, 0);
}

/**
 * e_dropdown_button_new:
 * @label_text: Text to display in the button
 * @menu: The menu to pop up when the button is pressed
 *
 * Create a new dropdown button.  When the button is clicked, the specified
 * @menu will be popped up.
 *
 * Return value: A pointer to the newly created %EDropdownButton.
 **/
GtkWidget *
e_dropdown_button_new (const gchar *label_text,
		       GtkMenu *menu)
{
	GtkWidget *widget;

	g_return_val_if_fail (label_text != NULL, NULL);
	g_return_val_if_fail (menu != NULL, NULL);
	g_return_val_if_fail (GTK_IS_MENU (menu), NULL);

	widget = g_object_new (e_dropdown_button_get_type (), NULL);

	e_dropdown_button_construct (E_DROPDOWN_BUTTON (widget), label_text, menu);
	return widget;
}

