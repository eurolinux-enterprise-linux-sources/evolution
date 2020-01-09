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

#include "calendar-config.h"
#include "e-day-view-config.h"

struct _EDayViewConfigPrivate {
	EDayView *view;

	GList *notifications;
};

/* Property IDs */
enum props {
	PROP_0,
	PROP_VIEW
};

G_DEFINE_TYPE (EDayViewConfig, e_day_view_config, G_TYPE_OBJECT)

static void
e_day_view_config_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	EDayViewConfig *view_config;

	view_config = E_DAY_VIEW_CONFIG (object);

	switch (property_id) {
	case PROP_VIEW:
		e_day_view_config_set_view (view_config, g_value_get_object (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
e_day_view_config_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	EDayViewConfig *view_config;

	view_config = E_DAY_VIEW_CONFIG (object);

	switch (property_id) {
	case PROP_VIEW:
		g_value_set_object (value, e_day_view_config_get_view (view_config));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
e_day_view_config_dispose (GObject *object)
{
	EDayViewConfig *view_config = E_DAY_VIEW_CONFIG (object);

	e_day_view_config_set_view (view_config, NULL);

	if (G_OBJECT_CLASS (e_day_view_config_parent_class)->dispose)
		G_OBJECT_CLASS (e_day_view_config_parent_class)->dispose (object);
}

static void
e_day_view_config_finalize (GObject *object)
{
	EDayViewConfig *view_config = E_DAY_VIEW_CONFIG (object);
	EDayViewConfigPrivate *priv;

	priv = view_config->priv;

	g_free (priv);

	if (G_OBJECT_CLASS (e_day_view_config_parent_class)->finalize)
		G_OBJECT_CLASS (e_day_view_config_parent_class)->finalize (object);
}

static void
e_day_view_config_class_init (EDayViewConfigClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GParamSpec *spec;

	/* Method override */
	gobject_class->set_property = e_day_view_config_set_property;
	gobject_class->get_property = e_day_view_config_get_property;
	gobject_class->dispose = e_day_view_config_dispose;
	gobject_class->finalize = e_day_view_config_finalize;

	spec = g_param_spec_object ("view", NULL, NULL, e_day_view_get_type (),
				    G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT);
	g_object_class_install_property (gobject_class, PROP_VIEW, spec);
}

static void
e_day_view_config_init (EDayViewConfig *view_config)
{
	view_config->priv = g_new0 (EDayViewConfigPrivate, 1);

}

EDayViewConfig *
e_day_view_config_new (EDayView *day_view)
{
	EDayViewConfig *view_config;

	view_config = g_object_new (e_day_view_config_get_type (), "view", day_view, NULL);

	return view_config;
}

EDayView *
e_day_view_config_get_view (EDayViewConfig *view_config)
{
	EDayViewConfigPrivate *priv;

	g_return_val_if_fail (view_config != NULL, NULL);
	g_return_val_if_fail (E_IS_DAY_VIEW_CONFIG (view_config), NULL);

	priv = view_config->priv;

	return priv->view;
}

static void
set_week_start (EDayView *day_view)
{
	gint week_start_day;

	week_start_day = calendar_config_get_week_start_day ();

	/* Convert it to 0 (Mon) to 6 (Sun), which is what we use. */
	week_start_day = (week_start_day + 6) % 7;

	e_day_view_set_week_start_day (day_view, week_start_day);
}

static void
week_start_changed_cb (GConfClient *client, guint id, GConfEntry *entry, gpointer data)
{
	EDayViewConfig *view_config = data;
	EDayViewConfigPrivate *priv;

	priv = view_config->priv;

	set_week_start (priv->view);
}

static void
set_twentyfour_hour (EDayView *day_view)
{
	gboolean use_24_hour;

	use_24_hour = calendar_config_get_24_hour_format ();

	e_calendar_view_set_use_24_hour_format (E_CALENDAR_VIEW (day_view), use_24_hour);

	/* To redraw the times */
	gtk_widget_queue_draw (day_view->time_canvas);
}

static void
twentyfour_hour_changed_cb (GConfClient *client, guint id, GConfEntry *entry, gpointer data)
{
	EDayViewConfig *view_config = data;
	EDayViewConfigPrivate *priv;

	priv = view_config->priv;

	set_twentyfour_hour (priv->view);
}

static void
set_working_days (EDayView *day_view)
{
	CalWeekdays working_days;

	working_days = calendar_config_get_working_days ();

	e_day_view_set_working_days (day_view, working_days);
}

static void
working_days_changed_cb (GConfClient *client, guint id, GConfEntry *entry, gpointer data)
{
	EDayViewConfig *view_config = data;
	EDayViewConfigPrivate *priv;

	priv = view_config->priv;

	set_working_days (priv->view);
}

static void
set_day_start_hour (EDayView *day_view)
{
	gint start_hour, start_minute, end_hour, end_minute;

	e_day_view_get_working_day (day_view, &start_hour, &start_minute, &end_hour, &end_minute);

	start_hour = calendar_config_get_day_start_hour ();

	e_day_view_set_working_day (day_view, start_hour, start_minute, end_hour, end_minute);
}

static void
day_start_hour_changed_cb (GConfClient *client, guint id, GConfEntry *entry, gpointer data)
{
	EDayViewConfig *view_config = data;
	EDayViewConfigPrivate *priv;

	priv = view_config->priv;

	set_day_start_hour (priv->view);
}

static void
set_day_start_minute (EDayView *day_view)
{
	gint start_hour, start_minute, end_hour, end_minute;

	e_day_view_get_working_day (day_view, &start_hour, &start_minute, &end_hour, &end_minute);

	start_minute = calendar_config_get_day_start_minute ();

	e_day_view_set_working_day (day_view, start_hour, start_minute, end_hour, end_minute);
}

static void
day_start_minute_changed_cb (GConfClient *client, guint id, GConfEntry *entry, gpointer data)
{
	EDayViewConfig *view_config = data;
	EDayViewConfigPrivate *priv;

	priv = view_config->priv;

	set_day_start_minute (priv->view);
}

static void
set_day_end_hour (EDayView *day_view)
{
	gint start_hour, start_minute, end_hour, end_minute;

	e_day_view_get_working_day (day_view, &start_hour, &start_minute, &end_hour, &end_minute);

	end_hour = calendar_config_get_day_end_hour ();

	e_day_view_set_working_day (day_view, start_hour, start_minute, end_hour, end_minute);
}

static void
day_end_hour_changed_cb (GConfClient *client, guint id, GConfEntry *entry, gpointer data)
{
	EDayViewConfig *view_config = data;
	EDayViewConfigPrivate *priv;

	priv = view_config->priv;

	set_day_end_hour (priv->view);
}

static void
set_day_end_minute (EDayView *day_view)
{
	gint start_hour, start_minute, end_hour, end_minute;

	e_day_view_get_working_day (day_view, &start_hour, &start_minute, &end_hour, &end_minute);

	end_minute = calendar_config_get_day_end_minute ();

	e_day_view_set_working_day (day_view, start_hour, start_minute, end_hour, end_minute);
}

static void
day_end_minute_changed_cb (GConfClient *client, guint id, GConfEntry *entry, gpointer data)
{
	EDayViewConfig *view_config = data;
	EDayViewConfigPrivate *priv;

	priv = view_config->priv;

	set_day_end_minute (priv->view);
}

static void
set_time_divisions (EDayView *day_view)
{
	gint time_divisions;

	time_divisions = calendar_config_get_time_divisions ();

	e_day_view_set_mins_per_row (day_view, time_divisions);
}

static void
time_divisions_changed_cb (GConfClient *client, guint id, GConfEntry *entry, gpointer data)
{
	EDayViewConfig *view_config = data;
	EDayViewConfigPrivate *priv;

	priv = view_config->priv;

	set_time_divisions (priv->view);
}

static void
set_marcus_bains (EDayView *day_view)
{
	gboolean show_marcus_bains_line;
	const gchar *dayview_color, *timebar_color;

	calendar_config_get_marcus_bains (&show_marcus_bains_line, &dayview_color, &timebar_color);

	e_day_view_set_marcus_bains (day_view, show_marcus_bains_line, dayview_color, timebar_color);
}

static void
marcus_bains_changed_cb (GConfClient *client, guint id, GConfEntry *entry, gpointer data)
{
	EDayViewConfig *view_config = data;
	EDayViewConfigPrivate *priv;

	priv = view_config->priv;

	set_marcus_bains (priv->view);
}

static void
set_show_event_end (EDayView *day_view)
{
	gboolean show_event_end;

	show_event_end = calendar_config_get_show_event_end ();

	e_day_view_set_show_event_end_times (day_view, show_event_end);
}

static void
show_event_end_changed_cb (GConfClient *client, guint id, GConfEntry *entry, gpointer data)
{
	EDayViewConfig *view_config = data;
	EDayViewConfigPrivate *priv;

	priv = view_config->priv;

	set_show_event_end (priv->view);
}

void
e_day_view_config_set_view (EDayViewConfig *view_config, EDayView *day_view)
{
	EDayViewConfigPrivate *priv;
	gint not_1, not_2, mbnot;
	guint not;
	GList *l;

	g_return_if_fail (view_config != NULL);
	g_return_if_fail (E_IS_DAY_VIEW_CONFIG (view_config));

	priv = view_config->priv;

	if (priv->view) {
		g_object_unref (priv->view);
		priv->view = NULL;
	}

	for (l = priv->notifications; l; l = l->next)
		calendar_config_remove_notification (GPOINTER_TO_UINT (l->data));

	g_list_free (priv->notifications);
	priv->notifications = NULL;

	/* If the new view is NULL, return right now */
	if (!day_view)
		return;

	priv->view = g_object_ref (day_view);

	/* Week start */
	set_week_start (day_view);

	not = calendar_config_add_notification_week_start_day (week_start_changed_cb, view_config);
	priv->notifications = g_list_prepend (priv->notifications, GUINT_TO_POINTER (not));

	/* 24 Hour format */
	set_twentyfour_hour (day_view);

	not = calendar_config_add_notification_24_hour_format (twentyfour_hour_changed_cb, view_config);
	priv->notifications = g_list_prepend (priv->notifications, GUINT_TO_POINTER (not));

	/* Working days */
	set_working_days (day_view);

	not = calendar_config_add_notification_working_days (working_days_changed_cb, view_config);
	priv->notifications = g_list_prepend (priv->notifications, GUINT_TO_POINTER (not));

	/* Day start hour */
	set_day_start_hour (day_view);

	not = calendar_config_add_notification_day_start_hour (day_start_hour_changed_cb, view_config);
	priv->notifications = g_list_prepend (priv->notifications, GUINT_TO_POINTER (not));

	/* Day start minute */
	set_day_start_minute (day_view);

	not = calendar_config_add_notification_day_start_minute (day_start_minute_changed_cb, view_config);
	priv->notifications = g_list_prepend (priv->notifications, GUINT_TO_POINTER (not));

	/* Day end hour */
	set_day_end_hour (day_view);

	not = calendar_config_add_notification_day_end_hour (day_end_hour_changed_cb, view_config);
	priv->notifications = g_list_prepend (priv->notifications, GUINT_TO_POINTER (not));

	/* Day start minute */
	set_day_end_minute (day_view);

	not = calendar_config_add_notification_day_end_minute (day_end_minute_changed_cb, view_config);
	priv->notifications = g_list_prepend (priv->notifications, GUINT_TO_POINTER (not));

	/* Time divisions */
	set_time_divisions (day_view);

	not = calendar_config_add_notification_time_divisions (time_divisions_changed_cb, view_config);
	priv->notifications = g_list_prepend (priv->notifications, GUINT_TO_POINTER (not));

	/* Showing Marcus Bains Line */
	set_marcus_bains (day_view);

	calendar_config_add_notification_marcus_bains (marcus_bains_changed_cb, view_config, &mbnot, &not_1, &not_2);
	priv->notifications = g_list_prepend (priv->notifications, GUINT_TO_POINTER (mbnot));
	priv->notifications = g_list_prepend (priv->notifications, GUINT_TO_POINTER (not_1));
	priv->notifications = g_list_prepend (priv->notifications, GUINT_TO_POINTER (not_2));

	/* Showing event end */
	set_show_event_end (day_view);

	not = calendar_config_add_notification_show_event_end (show_event_end_changed_cb, view_config);
	priv->notifications = g_list_prepend (priv->notifications, GUINT_TO_POINTER (not));
}
