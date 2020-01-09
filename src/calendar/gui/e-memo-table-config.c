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
 *		JP Rosevear <jpr@ximian.com>
 *		Nathan Owens <pianocomp81@yahoo.com>
 *
 * Copyright (C) 1999-2008 Novell, Inc. (www.novell.com)
 *
 */

#include "calendar-config.h"
#include "e-cell-date-edit-config.h"
#include "e-memo-table-config.h"

struct _EMemoTableConfigPrivate {
	EMemoTable *table;

	ECellDateEditConfig *cell_config;

	GList *notifications;
};

G_DEFINE_TYPE (EMemoTableConfig, e_memo_table_config, G_TYPE_OBJECT)

/* Property IDs */
enum props {
	PROP_0,
	PROP_TABLE
};

static void
e_memo_table_config_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	EMemoTableConfig *table_config;

	table_config = E_MEMO_TABLE_CONFIG (object);

	switch (property_id) {
	case PROP_TABLE:
		e_memo_table_config_set_table (table_config, g_value_get_object (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
e_memo_table_config_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	EMemoTableConfig *table_config;

	table_config = E_MEMO_TABLE_CONFIG (object);

	switch (property_id) {
	case PROP_TABLE:
		g_value_set_object (value, e_memo_table_config_get_table (table_config));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
e_memo_table_config_dispose (GObject *object)
{
	EMemoTableConfig *table_config = E_MEMO_TABLE_CONFIG (object);

	e_memo_table_config_set_table (table_config, NULL);

	if (G_OBJECT_CLASS (e_memo_table_config_parent_class)->dispose)
		G_OBJECT_CLASS (e_memo_table_config_parent_class)->dispose (object);
}

static void
e_memo_table_config_finalize (GObject *object)
{
	EMemoTableConfig *table_config = E_MEMO_TABLE_CONFIG (object);
	EMemoTableConfigPrivate *priv;

	priv = table_config->priv;

	g_free (priv);

	if (G_OBJECT_CLASS (e_memo_table_config_parent_class)->finalize)
		G_OBJECT_CLASS (e_memo_table_config_parent_class)->finalize (object);
}

static void
e_memo_table_config_class_init (EMemoTableConfigClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GParamSpec *spec;

	/* Method override */
	gobject_class->set_property = e_memo_table_config_set_property;
	gobject_class->get_property = e_memo_table_config_get_property;
	gobject_class->dispose = e_memo_table_config_dispose;
	gobject_class->finalize = e_memo_table_config_finalize;

	spec = g_param_spec_object ("table", NULL, NULL, e_memo_table_get_type (),
				    G_PARAM_READABLE | G_PARAM_WRITABLE | G_PARAM_CONSTRUCT);
	g_object_class_install_property (gobject_class, PROP_TABLE, spec);
}

static void
e_memo_table_config_init (EMemoTableConfig *table_config)
{
	table_config->priv = g_new0 (EMemoTableConfigPrivate, 1);

}

EMemoTableConfig *
e_memo_table_config_new (EMemoTable *table)
{
	EMemoTableConfig *table_config;

	table_config = g_object_new (e_memo_table_config_get_type (), "table", table, NULL);

	return table_config;
}

EMemoTable *
e_memo_table_config_get_table (EMemoTableConfig *table_config)
{
	EMemoTableConfigPrivate *priv;

	g_return_val_if_fail (table_config != NULL, NULL);
	g_return_val_if_fail (E_IS_MEMO_TABLE_CONFIG (table_config), NULL);

	priv = table_config->priv;

	return priv->table;
}

static void
set_timezone (EMemoTable *table)
{
	ECalModel *model;
	icaltimezone *zone;

	zone = calendar_config_get_icaltimezone ();
	model = e_memo_table_get_model (table);
	if (model)
		e_cal_model_set_timezone (model, zone);
}

static void
timezone_changed_cb (GConfClient *client, guint id, GConfEntry *entry, gpointer data)
{
	EMemoTableConfig *table_config = data;
	EMemoTableConfigPrivate *priv;

	priv = table_config->priv;

	set_timezone (priv->table);
}

static void
set_twentyfour_hour (EMemoTable *table)
{
	ECalModel *model;
	gboolean use_24_hour;

	use_24_hour = calendar_config_get_24_hour_format ();

	model = e_memo_table_get_model (table);
	if (model)
		e_cal_model_set_use_24_hour_format (model, use_24_hour);
}

static void
twentyfour_hour_changed_cb (GConfClient *client, guint id, GConfEntry *entry, gpointer data)
{
	EMemoTableConfig *table_config = data;
	EMemoTableConfigPrivate *priv;

	priv = table_config->priv;

	set_twentyfour_hour (priv->table);
}

void
e_memo_table_config_set_table (EMemoTableConfig *table_config, EMemoTable *table)
{
	EMemoTableConfigPrivate *priv;
	guint not;
	GList *l;

	g_return_if_fail (table_config != NULL);
	g_return_if_fail (E_IS_MEMO_TABLE_CONFIG (table_config));

	priv = table_config->priv;

	if (priv->table) {
		g_object_unref (priv->table);
		priv->table = NULL;
	}

	if (priv->cell_config) {
		g_object_unref (priv->cell_config);
		priv->cell_config = NULL;
	}

	for (l = priv->notifications; l; l = l->next)
		calendar_config_remove_notification (GPOINTER_TO_UINT (l->data));

	g_list_free (priv->notifications);
	priv->notifications = NULL;

	/* If the new view is NULL, return right now */
	if (!table)
		return;

	priv->table = g_object_ref (table);

	/* Time zone */
	set_timezone (table);

	not = calendar_config_add_notification_timezone (timezone_changed_cb, table_config);
	priv->notifications = g_list_prepend (priv->notifications, GUINT_TO_POINTER (not));

	/* 24 Hour format */
	set_twentyfour_hour (table);

	not = calendar_config_add_notification_24_hour_format (twentyfour_hour_changed_cb, table_config);
	priv->notifications = g_list_prepend (priv->notifications, GUINT_TO_POINTER (not));

	/* Date cell */
	priv->cell_config = e_cell_date_edit_config_new (table->dates_cell);
}
