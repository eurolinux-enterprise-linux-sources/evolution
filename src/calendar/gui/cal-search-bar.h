/*
 *
 * Evolution calendar - Search bar widget for calendar views
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
 *		Federico Mena-Quintero <federico@ximian.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#ifndef CAL_SEARCH_BAR_H
#define CAL_SEARCH_BAR_H

#include "misc/e-search-bar.h"
#include "misc/e-filter-bar.h"

G_BEGIN_DECLS



#define TYPE_CAL_SEARCH_BAR            (cal_search_bar_get_type ())
#define CAL_SEARCH_BAR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_CAL_SEARCH_BAR, CalSearchBar))
#define CAL_SEARCH_BAR_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), TYPE_CAL_SEARCH_BAR,	\
					CalSearchBarClass))
#define IS_CAL_SEARCH_BAR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_CAL_SEARCH_BAR))
#define IS_CAL_SEARCH_BAR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), TYPE_CAL_SEARCH_BAR))

typedef struct CalSearchBarPrivate CalSearchBarPrivate;

enum {
	CAL_SEARCH_SUMMARY_CONTAINS      = (1 << 0),
	CAL_SEARCH_DESCRIPTION_CONTAINS  = (1 << 1),
	CAL_SEARCH_CATEGORY_IS           = (1 << 2),
	CAL_SEARCH_COMMENT_CONTAINS      = (1 << 3),
	CAL_SEARCH_LOCATION_CONTAINS     = (1 << 4),
	CAL_SEARCH_ANY_FIELD_CONTAINS    = (1 << 5)
};

#define CAL_SEARCH_ALL (0xff)
#define CAL_SEARCH_CALENDAR_DEFAULT (0x33)
#define CAL_SEARCH_TASKS_DEFAULT    (0xE3)
#define CAL_SEARCH_MEMOS_DEFAULT    (0x23)

typedef struct {
	EFilterBar  search_bar;

	/* Private data */
	CalSearchBarPrivate *priv;
} CalSearchBar;

typedef struct {
	EFilterBarClass parent_class;

	/* Notification signals */

	void (* sexp_changed) (CalSearchBar *cal_search, const gchar *sexp);
	void (* category_changed) (CalSearchBar *cal_search, const gchar *category);
} CalSearchBarClass;

GType cal_search_bar_get_type (void);

CalSearchBar *cal_search_bar_construct (CalSearchBar *cal_search, guint32 flags);

GtkWidget *cal_search_bar_new (guint32 flags);

void cal_search_bar_set_categories (CalSearchBar *cal_search, GPtrArray *categories);

const gchar *cal_search_bar_get_category (CalSearchBar *cal_search);

void cal_search_bar_get_time_range (CalSearchBar *cal_search, time_t *start, time_t *end);



G_END_DECLS

#endif
