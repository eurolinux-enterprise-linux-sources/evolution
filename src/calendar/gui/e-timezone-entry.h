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
 *		Damon Chaplin <damon@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

/*
 * ETimezoneEntry - a field for setting a timezone. It shows the timezone in
 * a GtkEntry with a '...' button beside it which shows a dialog for changing
 * the timezone. The dialog contains a map of the world with a point for each
 * timezone, and an option menu as an alternative way of selecting the
 * timezone.
 */

#ifndef __E_TIMEZONE_ENTRY_H_
#define __E_TIMEZONE_ENTRY_H_

#include <gtk/gtk.h>
#include <libecal/e-cal.h>

G_BEGIN_DECLS

#define E_TYPE_TIMEZONE_ENTRY            (e_timezone_entry_get_type ())
#define E_TIMEZONE_ENTRY(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), E_TYPE_TIMEZONE_ENTRY, ETimezoneEntry))
#define E_TIMEZONE_ENTRY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), E_TYPE_TIMEZONE_ENTRY, ETimezoneEntryClass))
#define E_IS_TIMEZONE_ENTRY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), E_TYPE_TIMEZONE_ENTRY))
#define E_IS_TIMEZONE_ENTRY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), E_TYPE_TIMEZONE_ENTRY))

typedef struct _ETimezoneEntry        ETimezoneEntry;
typedef struct _ETimezoneEntryPrivate ETimezoneEntryPrivate;
typedef struct _ETimezoneEntryClass   ETimezoneEntryClass;

struct _ETimezoneEntry {
	GtkHBox hbox;

	/*< private >*/
	ETimezoneEntryPrivate *priv;
};

struct _ETimezoneEntryClass {
	GtkHBoxClass parent_class;

	void (* changed)      (ETimezoneEntry    *tentry);
};

GType      e_timezone_entry_get_type		(void);
GtkWidget* e_timezone_entry_new			(void);

icaltimezone*   e_timezone_entry_get_timezone	(ETimezoneEntry	*tentry);
void		e_timezone_entry_set_timezone	(ETimezoneEntry	*tentry,
						 icaltimezone	*zone);

/* Sets the default timezone. If the current timezone matches this, then the
   entry field is hidden. This is useful since most people do not use timezones
   so it makes the user interface simpler. */
void	   e_timezone_entry_set_default_timezone(ETimezoneEntry	*tentry,
						 icaltimezone	*zone);

G_END_DECLS

#endif /* __E_TIMEZONE_ENTRY_H_ */
