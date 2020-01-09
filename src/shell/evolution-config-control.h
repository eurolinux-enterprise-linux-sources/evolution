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

#ifndef EVOLUTION_CONFIG_CONTROL_H
#define EVOLUTION_CONFIG_CONTROL_H

#include "Evolution.h"

#include <bonobo/bonobo-object.h>
#include <gtk/gtk.h>

#ifdef cplusplus
extern "C" {
#pragma }
#endif /* cplusplus */

#define EVOLUTION_TYPE_CONFIG_CONTROL            (evolution_config_control_get_type ())
#define EVOLUTION_CONFIG_CONTROL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), EVOLUTION_TYPE_CONFIG_CONTROL, EvolutionConfigControl))
#define EVOLUTION_CONFIG_CONTROL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), EVOLUTION_TYPE_CONFIG_CONTROL, EvolutionConfigControlClass))
#define EVOLUTION_IS_CONFIG_CONTROL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), EVOLUTION_TYPE_CONFIG_CONTROL))
#define EVOLUTION_IS_CONFIG_CONTROL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), EVOLUTION_TYPE_CONFIG_CONTROL))

typedef struct _EvolutionConfigControl        EvolutionConfigControl;
typedef struct _EvolutionConfigControlPrivate EvolutionConfigControlPrivate;
typedef struct _EvolutionConfigControlClass   EvolutionConfigControlClass;

struct _EvolutionConfigControl {
	BonoboObject parent;

	EvolutionConfigControlPrivate *priv;
};

struct _EvolutionConfigControlClass {
	BonoboObjectClass parent_class;

	POA_GNOME_Evolution_ConfigControl__epv epv;
};


GType                   evolution_config_control_get_type   (void);
EvolutionConfigControl *evolution_config_control_new        (GtkWidget              *widget);
void                    evolution_config_control_construct  (EvolutionConfigControl *control,
							     GtkWidget              *widget);

#endif /* EVOLUTION_CONFIG_CONTROL_H */
