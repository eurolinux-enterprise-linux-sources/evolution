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
 *      Nathan Owens <pianocomp81@yahoo.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef _MEMOS_COMPONENT_H_
#define _MEMOS_COMPONENT_H_

#include <bonobo/bonobo-object.h>
#include <libedataserver/e-source-list.h>
#include <widgets/misc/e-activity-handler.h>
#include "Evolution.h"

#define MEMOS_TYPE_COMPONENT			(memos_component_get_type ())
#define MEMOS_COMPONENT(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), MEMOS_TYPE_COMPONENT, MemosComponent))
#define MEMOS_COMPONENT_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), MEMOS_TYPE_COMPONENT, MemosComponentClass))
#define MEMOS_IS_COMPONENT(obj)			(G_TYPE_CHECK_INSTANCE_TYPE ((obj), MEMOS_TYPE_COMPONENT))
#define MEMOS_IS_COMPONENT_CLASS(klass)		(G_TYPE_CHECK_CLASS_TYPE ((obj), MEMOS_TYPE_COMPONENT))

typedef struct _MemosComponent        MemosComponent;
typedef struct _MemosComponentPrivate MemosComponentPrivate;
typedef struct _MemosComponentClass   MemosComponentClass;

struct _MemosComponent {
	BonoboObject parent;

	MemosComponentPrivate *priv;
};

struct _MemosComponentClass {
	BonoboObjectClass parent_class;

	POA_GNOME_Evolution_Component__epv epv;
};

GType             memos_component_get_type  (void);
MemosComponent   *memos_component_peek  (void);

const gchar       *memos_component_peek_base_directory (MemosComponent *component);
const gchar       *memos_component_peek_config_directory (MemosComponent *component);
ESourceList      *memos_component_peek_source_list (MemosComponent *component);

#endif /* _MEMOS_COMPONENT_H_ */
