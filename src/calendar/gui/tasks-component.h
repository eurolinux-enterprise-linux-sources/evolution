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
 *		Rodrigo Moya <rodrigo@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _TASKS_COMPONENT_H_
#define _TASKS_COMPONENT_H_

#include <bonobo/bonobo-object.h>
#include <libedataserver/e-source-list.h>
#include <misc/e-activity-handler.h>
#include "Evolution.h"

#define TASKS_TYPE_COMPONENT			(tasks_component_get_type ())
#define TASKS_COMPONENT(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), TASKS_TYPE_COMPONENT, TasksComponent))
#define TASKS_COMPONENT_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), TASKS_TYPE_COMPONENT, TasksComponentClass))
#define TASKS_IS_COMPONENT(obj)			(G_TYPE_CHECK_INSTANCE_TYPE ((obj), TASKS_TYPE_COMPONENT))
#define TASKS_IS_COMPONENT_CLASS(klass)		(G_TYPE_CHECK_CLASS_TYPE ((obj), TASKS_TYPE_COMPONENT))

typedef struct _TasksComponent        TasksComponent;
typedef struct _TasksComponentPrivate TasksComponentPrivate;
typedef struct _TasksComponentClass   TasksComponentClass;

struct _TasksComponent {
	BonoboObject parent;

	TasksComponentPrivate *priv;
};

struct _TasksComponentClass {
	BonoboObjectClass parent_class;

	POA_GNOME_Evolution_Component__epv epv;
};

GType             tasks_component_get_type  (void);
TasksComponent   *tasks_component_peek  (void);

const gchar       *tasks_component_peek_base_directory (TasksComponent *component);
const gchar       *tasks_component_peek_config_directory (TasksComponent *component);
ESourceList      *tasks_component_peek_source_list (TasksComponent *component);

#endif /* _TASKS_COMPONENT_H_ */
