/*
 *
 * Abstract class wrapper for EvolutionComponent
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
 *		Michael Zucchi <notzed@novell.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _EVOLUTION_COMPONENT_H_
#define _EVOLUTION_COMPONENT_H_

#include <bonobo/bonobo-object.h>
#include "shell/Evolution.h"

typedef struct _EvolutionComponent        EvolutionComponent;
typedef struct _EvolutionComponentClass   EvolutionComponentClass;

struct _EvolutionComponent {
	BonoboObject parent;
};

struct _EvolutionComponentClass {
	BonoboObjectClass parent_class;

	POA_GNOME_Evolution_Component__epv epv;
};

GType           evolution_component_get_type(void);

#endif /* _EVOLUTION_COMPONENT_H_ */
