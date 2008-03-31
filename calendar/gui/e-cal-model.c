/* Evolution calendar - Data model for ETable
 *
 * Copyright (C) 2004 Novell, Inc.
 *
 * Authors: Rodrigo Moya <rodrigo@ximian.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <glib/garray.h>
#include <glib/gi18n.h>
#include <libedataserver/e-time-utils.h>
#include <libecal/e-cal-time-util.h>
#include "comp-util.h"
#include "e-cal-model.h"
#include "itip-utils.h"
#include "misc.h"
#include "e-calendar-marshal.h"
#include "calendar-config.h"

typedef struct {
	ECal *client;
	ECalView *query;

	gboolean do_query;
} ECalModelClient;

struct _ECalModelPrivate {
	/* The list of clients we are managing. Each element is of type ECalModelClient */
	GList *clients;

	/* The default client in the list */
	ECal *default_client;

	/* Array for storing the objects. Each element is of type ECalModelComponent */
	GPtrArray *objects;

	icalcomponent_kind kind;
	ECalModelFlags flags;
	icaltimezone *zone;

	/* The time range to display */
	time_t start;
	time_t end;

	/* The search regular expression */
	gchar *search_sexp;

	/* The full regular expression, including time range */
	gchar *full_sexp;

	/* The default category */
	gchar *default_category;

	/* Addresses for determining icons */
	EAccountList *accounts;

	/* Whether we display dates in 24-hour format. */
        gboolean use_24_hour_format;
};

static void e_cal_model_dispose (GObject *object);
static void e_cal_model_finalize (GObject *object);

static int ecm_column_count (ETableModel *etm);
static int ecm_row_count (ETableModel *etm);
static void *ecm_value_at (ETableModel *etm, int col, int row);
static void ecm_set_value_at (ETableModel *etm, int col, int row, const void *value);
static gboolean ecm_is_cell_editable (ETableModel *etm, int col, int row);
static void ecm_append_row (ETableModel *etm, ETableModel *source, int row);
static void *ecm_duplicate_value (ETableModel *etm, int col, const void *value);
static void ecm_free_value (ETableModel *etm, int col, void *value);
static void *ecm_initialize_value (ETableModel *etm, int col);
static gboolean ecm_value_is_empty (ETableModel *etm, int col, const void *value);
static char *ecm_value_to_string (ETableModel *etm, int col, const void *value);

static const char *ecm_get_color_for_component (ECalModel *model, ECalModelComponent *comp_data);

static ECalModelClient *add_new_client (ECalModel *model, ECal *client, gboolean do_query);
static ECalModelClient *find_client_data (ECalModel *model, ECal *client);
static void remove_client_objects (ECalModel *model, ECalModelClient *client_data);
static void remove_client (ECalModel *model, ECalModelClient *client_data);

/* Signal IDs */
enum {
	TIME_RANGE_CHANGED,
	ROW_APPENDED,
	CAL_VIEW_PROGRESS,
	CAL_VIEW_DONE,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (ECalModel, e_cal_model, E_TABLE_MODEL_TYPE)

static void
e_cal_model_class_init (ECalModelClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	ETableModelClass *etm_class = E_TABLE_MODEL_CLASS (klass);

	object_class->dispose = e_cal_model_dispose;
	object_class->finalize = e_cal_model_finalize;

	etm_class->column_count = ecm_column_count;
	etm_class->row_count = ecm_row_count;
	etm_class->value_at = ecm_value_at;
	etm_class->set_value_at = ecm_set_value_at;
	etm_class->is_cell_editable = ecm_is_cell_editable;
	etm_class->append_row = ecm_append_row;
	etm_class->duplicate_value = ecm_duplicate_value;
	etm_class->free_value = ecm_free_value;
	etm_class->initialize_value = ecm_initialize_value;
	etm_class->value_is_empty = ecm_value_is_empty;
	etm_class->value_to_string = ecm_value_to_string;

	klass->get_color_for_component = ecm_get_color_for_component;
	klass->fill_component_from_model = NULL;

	signals[TIME_RANGE_CHANGED] =
		g_signal_new ("time_range_changed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ECalModelClass, time_range_changed),
			      NULL, NULL,
			      e_calendar_marshal_VOID__LONG_LONG,
			      G_TYPE_NONE, 2, G_TYPE_LONG, G_TYPE_LONG);

	signals[ROW_APPENDED] =
		g_signal_new ("row_appended",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ECalModelClass, row_appended),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	signals[CAL_VIEW_PROGRESS] =
		g_signal_new ("cal_view_progress",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ECalModelClass, cal_view_progress),
			      NULL, NULL,
	                      e_calendar_marshal_VOID__STRING_INT_INT,
			      G_TYPE_NONE, 3, G_TYPE_STRING, G_TYPE_INT, G_TYPE_INT);
	signals[CAL_VIEW_DONE] =
		g_signal_new ("cal_view_done",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ECalModelClass, cal_view_done),
			      NULL, NULL,
			      e_calendar_marshal_VOID__INT_INT,
			      G_TYPE_NONE, 2, G_TYPE_INT, G_TYPE_INT);

}

static void
e_cal_model_init (ECalModel *model)
{
	ECalModelPrivate *priv;

	priv = g_new0 (ECalModelPrivate, 1);
	model->priv = priv;

	/* match none by default */
	priv->start = -1;
	priv->end = -1;
	priv->search_sexp = NULL;
	priv->full_sexp = g_strdup ("#f");

	priv->objects = g_ptr_array_new ();
	priv->kind = ICAL_NO_COMPONENT;
	priv->flags = 0;

	priv->accounts = itip_addresses_get ();

	priv->use_24_hour_format = TRUE;
}

static void
clear_objects_array (ECalModelPrivate *priv)
{
	gint i;

	for (i = 0; i < priv->objects->len; i++) {
		ECalModelComponent *comp_data;

		comp_data = g_ptr_array_index (priv->objects, i);
		if (comp_data == NULL) {
			g_warning ("comp_data is null\n");
			continue;
		}
		e_cal_model_free_component_data (comp_data);
	}

	g_ptr_array_set_size (priv->objects, 0);
}

static void
e_cal_model_dispose (GObject *object)
{
	ECalModelPrivate *priv;
	ECalModel *model = (ECalModel *) object;

	g_return_if_fail (E_IS_CAL_MODEL (model));

	priv = model->priv;

	if (priv->clients) {
		while (priv->clients != NULL) {
			ECalModelClient *client_data = (ECalModelClient *) priv->clients->data;

			g_signal_handlers_disconnect_matched (client_data->client, G_SIGNAL_MATCH_DATA,
							      0, 0, NULL, NULL, model);
			if (client_data->query)
				g_signal_handlers_disconnect_matched (client_data->query, G_SIGNAL_MATCH_DATA,
								      0, 0, NULL, NULL, model);

			priv->clients = g_list_remove (priv->clients, client_data);


			g_object_unref (client_data->client);
			if (client_data->query)
				g_object_unref (client_data->query);
			g_free (client_data);
		}

		priv->clients = NULL;
		priv->default_client = NULL;
	}

	if (G_OBJECT_CLASS (e_cal_model_parent_class)->dispose)
		G_OBJECT_CLASS (e_cal_model_parent_class)->dispose (object);
}

static void
e_cal_model_finalize (GObject *object)
{
	ECalModelPrivate *priv;
	ECalModel *model = (ECalModel *) object;

	g_return_if_fail (E_IS_CAL_MODEL (model));

	priv = model->priv;

	g_free (priv->search_sexp);
	g_free (priv->full_sexp);

	g_free (priv->default_category);

	clear_objects_array (priv);
	g_ptr_array_free (priv->objects, FALSE);

	g_free (priv);

	if (G_OBJECT_CLASS (e_cal_model_parent_class)->finalize)
		G_OBJECT_CLASS (e_cal_model_parent_class)->finalize (object);
}

/* ETableModel methods */

static int
ecm_column_count (ETableModel *etm)
{
	return E_CAL_MODEL_FIELD_LAST;
}

static int
ecm_row_count (ETableModel *etm)
{
	ECalModelPrivate *priv;
	ECalModel *model = (ECalModel *) etm;

	g_return_val_if_fail (E_IS_CAL_MODEL (model), -1);

	priv = model->priv;

	return priv->objects->len;
}

static char *
get_categories (ECalModelComponent *comp_data)
{
	icalproperty *prop;

	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_CATEGORIES_PROPERTY);
	if (prop)
		return (char *) icalproperty_get_categories (prop);

	return "";
}

static char *
get_classification (ECalModelComponent *comp_data)
{
	icalproperty *prop;
	icalproperty_class class;

	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_CLASS_PROPERTY);

	if (!prop)
		return _("Public");

	class = icalproperty_get_class (prop);

	switch (class)
	{
	case ICAL_CLASS_PUBLIC:
		return _("Public");
	case ICAL_CLASS_PRIVATE:
		return _("Private");
	case ICAL_CLASS_CONFIDENTIAL:
		return _("Confidential");
	default:
		return _("Unknown");
	}
}

static const char *
get_color (ECalModel *model, ECalModelComponent *comp_data)
{
	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);

	return e_cal_model_get_color_for_component (model, comp_data);
}

static char *
get_description (ECalModelComponent *comp_data)
{
	icalproperty *prop;
	static GString *str = NULL;

	if (str) {
		g_string_free (str, TRUE);
		str = NULL;
	}

	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_DESCRIPTION_PROPERTY);
	if (prop) {
		str = g_string_new (NULL);
		do {
			str = g_string_append (str, icalproperty_get_description (prop));
		} while ((prop = icalcomponent_get_next_property (comp_data->icalcomp, ICAL_DESCRIPTION_PROPERTY)));

		return str->str;
	}

	return "";
}

static ECellDateEditValue*
get_dtstart (ECalModel *model, ECalModelComponent *comp_data)
{
	ECalModelPrivate *priv;
        struct icaltimetype tt_start;

	priv = model->priv;

	if (!comp_data->dtstart) {
		icalproperty *prop;
		icaltimezone *zone;
		gboolean got_zone = FALSE;

		prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_DTSTART_PROPERTY);
		if (!prop)
			return NULL;

		tt_start = icalproperty_get_dtstart (prop);

		if (icaltime_get_tzid (tt_start)
		    && e_cal_get_timezone (comp_data->client, icaltime_get_tzid (tt_start), &zone, NULL))
			got_zone = TRUE;

		if (e_cal_model_get_flags (model) & E_CAL_MODEL_FLAGS_EXPAND_RECURRENCES) {
			if (got_zone) {
				tt_start = icaltime_from_timet_with_zone (comp_data->instance_start, tt_start.is_date, zone);
				if (priv->zone)
					icaltimezone_convert_time (&tt_start, zone, priv->zone);
			} else
				if (priv->zone)
					tt_start = icaltime_from_timet_with_zone (comp_data->instance_start, tt_start.is_date, priv->zone);
		}

		if (!icaltime_is_valid_time (tt_start) || icaltime_is_null_time (tt_start))
			return NULL;

		comp_data->dtstart = g_new0 (ECellDateEditValue, 1);
		comp_data->dtstart->tt = tt_start;

		if (got_zone)
			comp_data->dtstart->zone = zone;
		else
			comp_data->dtstart->zone = NULL;
	}

	return comp_data->dtstart;
}

static char *
get_summary (ECalModelComponent *comp_data)
{
	icalproperty *prop;

	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_SUMMARY_PROPERTY);
	if (prop)
		return (char *) icalproperty_get_summary (prop);

	return "";
}

static char *
get_uid (ECalModelComponent *comp_data)
{
	return (char *) icalcomponent_get_uid (comp_data->icalcomp);
}

static void *
ecm_value_at (ETableModel *etm, int col, int row)
{
	ECalModelPrivate *priv;
	ECalModelComponent *comp_data;
	ECalModel *model = (ECalModel *) etm;

	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);

	priv = model->priv;

	g_return_val_if_fail (col >= 0 && col < E_CAL_MODEL_FIELD_LAST, NULL);
	g_return_val_if_fail (row >= 0 && row < priv->objects->len, NULL);

	comp_data = g_ptr_array_index (priv->objects, row);
	g_return_val_if_fail (comp_data != NULL, NULL);

	switch (col) {
	case E_CAL_MODEL_FIELD_CATEGORIES :
		return get_categories (comp_data);
	case E_CAL_MODEL_FIELD_CLASSIFICATION :
		return get_classification (comp_data);
	case E_CAL_MODEL_FIELD_COLOR :
		return (void *) get_color (model, comp_data);
	case E_CAL_MODEL_FIELD_COMPONENT :
		return comp_data->icalcomp;
	case E_CAL_MODEL_FIELD_DESCRIPTION :
		return get_description (comp_data);
	case E_CAL_MODEL_FIELD_DTSTART :
		return (void *) get_dtstart (model, comp_data);
	case E_CAL_MODEL_FIELD_HAS_ALARMS :
		return GINT_TO_POINTER ((icalcomponent_get_first_component (comp_data->icalcomp,
									    ICAL_VALARM_COMPONENT) != NULL));
	case E_CAL_MODEL_FIELD_ICON :
	{
		ECalComponent *comp;
		icalcomponent *icalcomp;
		gint retval = 0;

		comp = e_cal_component_new ();
		icalcomp = icalcomponent_new_clone (comp_data->icalcomp);
		if (e_cal_component_set_icalcomponent (comp, icalcomp)) {
			if (e_cal_component_get_vtype (comp) == E_CAL_COMPONENT_JOURNAL) {
				g_object_unref (comp);
				return GINT_TO_POINTER (retval);
			}

			if (e_cal_component_has_recurrences (comp))
				retval = 1;
			else if (itip_organizer_is_user (comp, comp_data->client))
				retval = 3;
			else {
				GSList *attendees = NULL, *sl;

				e_cal_component_get_attendee_list (comp, &attendees);
				for (sl = attendees; sl != NULL; sl = sl->next) {
					ECalComponentAttendee *ca = sl->data;
					const char *text;

					text = itip_strip_mailto (ca->value);
					if (e_account_list_find (priv->accounts, E_ACCOUNT_FIND_ID_ADDRESS, text) != NULL) {
						if (ca->delto != NULL)
							retval = 3;
						else
							retval = 2;
						break;
					}
				}

				e_cal_component_free_attendee_list (attendees);
			}
		} else
			icalcomponent_free (icalcomp);

		g_object_unref (comp);

		return GINT_TO_POINTER (retval);
	}
	case E_CAL_MODEL_FIELD_SUMMARY :
		return get_summary (comp_data);
	case E_CAL_MODEL_FIELD_UID :
		return get_uid (comp_data);
	}

	return "";
}

static void
set_categories (ECalModelComponent *comp_data, const char *value)
{
	icalproperty *prop;

	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_CATEGORIES_PROPERTY);
	if (!value || !(*value)) {
		if (prop) {
			icalcomponent_remove_property (comp_data->icalcomp, prop);
			icalproperty_free (prop);
		}
	} else {
		if (!prop) {
			prop = icalproperty_new_categories (value);
			icalcomponent_add_property (comp_data->icalcomp, prop);
		} else
			icalproperty_set_categories (prop, value);
	}
}

static void
set_classification (ECalModelComponent *comp_data, const char *value)
{
	icalproperty *prop;

	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_CLASS_PROPERTY);
	if (!value || !(*value)) {
		if (prop) {
			icalcomponent_remove_property (comp_data->icalcomp, prop);
			icalproperty_free (prop);
		}
	} else {
	  icalproperty_class ical_class;

	  if (!g_ascii_strcasecmp (value, "PUBLIC"))
	    ical_class = ICAL_CLASS_PUBLIC;
	  else if (!g_ascii_strcasecmp (value, "PRIVATE"))
	    ical_class = ICAL_CLASS_PRIVATE;
	  else if (!g_ascii_strcasecmp (value, "CONFIDENTIAL"))
	    ical_class = ICAL_CLASS_CONFIDENTIAL;
	  else
	    ical_class = ICAL_CLASS_NONE;

		if (!prop) {
			prop = icalproperty_new_class (ical_class);
			icalcomponent_add_property (comp_data->icalcomp, prop);
		} else
			icalproperty_set_class (prop, ical_class);
	}
}

static void
set_description (ECalModelComponent *comp_data, const char *value)
{
	icalproperty *prop;

	/* remove old description(s) */
	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_DESCRIPTION_PROPERTY);
	while (prop) {
		icalproperty *next;

		next = icalcomponent_get_next_property (comp_data->icalcomp, ICAL_DESCRIPTION_PROPERTY);

		icalcomponent_remove_property (comp_data->icalcomp, prop);
		icalproperty_free (prop);

		prop = next;
	}

	/* now add the new description */
	if (!value || !(*value))
		return;

	prop = icalproperty_new_description (value);
	icalcomponent_add_property (comp_data->icalcomp, prop);
}

static void
set_dtstart (ECalModel *model, ECalModelComponent *comp_data, const void *value)
{
	ECellDateEditValue *dv = (ECellDateEditValue *) value;
	icalproperty *prop;
	icalparameter *param;
	const char *tzid;

	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_DTSTART_PROPERTY);
	if (prop)
		param = icalproperty_get_first_parameter (prop, ICAL_TZID_PARAMETER);
	else
		param = NULL;

	/* If we are setting the property to NULL (i.e. removing it), then
	   we remove it if it exists. */
	if (!dv) {
		if (prop) {
			icalcomponent_remove_property (comp_data->icalcomp, prop);
			icalproperty_free (prop);
		}

		return;
	}

	/* If the TZID is set to "UTC", we set the is_utc flag. */
	tzid = dv->zone ? icaltimezone_get_tzid (dv->zone) : "UTC";
	if (tzid && !strcmp (tzid, "UTC"))
		dv->tt.is_utc = 1;
	else
		dv->tt.is_utc = 0;

	if (prop) {
		icalproperty_set_dtstart (prop, dv->tt);
	} else {
		prop = icalproperty_new_dtstart (dv->tt);
		icalcomponent_add_property (comp_data->icalcomp, prop);
	}

	/* If the TZID is set to "UTC", we don't want to save the TZID. */
	if (tzid && strcmp (tzid, "UTC")) {
		if (param) {
			icalparameter_set_tzid (param, (char *) tzid);
		} else {
			param = icalparameter_new_tzid ((char *) tzid);
			icalproperty_add_parameter (prop, param);
		}
	} else if (param) {
		icalproperty_remove_parameter (prop, ICAL_TZID_PARAMETER);
	}
}

static void
set_summary (ECalModelComponent *comp_data, const char *value)
{
	icalproperty *prop;

	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_SUMMARY_PROPERTY);

	if (string_is_empty (value)) {
		if (prop) {
			icalcomponent_remove_property (comp_data->icalcomp, prop);
			icalproperty_free (prop);
		}
	} else {
		if (prop)
			icalproperty_set_summary (prop, value);
		else {
			prop = icalproperty_new_summary (value);
			icalcomponent_add_property (comp_data->icalcomp, prop);
		}
	}
}

static void
ecm_set_value_at (ETableModel *etm, int col, int row, const void *value)
{
	ECalModelPrivate *priv;
	ECalModelComponent *comp_data;
	ECalModel *model = (ECalModel *) etm;

	g_return_if_fail (E_IS_CAL_MODEL (model));

	priv = model->priv;

	g_return_if_fail (col >= 0 && col < E_CAL_MODEL_FIELD_LAST);
	g_return_if_fail (row >= 0 && row < priv->objects->len);

	comp_data = g_ptr_array_index (priv->objects, row);
	g_return_if_fail (comp_data != NULL);

	switch (col) {
	case E_CAL_MODEL_FIELD_CATEGORIES :
		set_categories (comp_data, value);
		break;
	case E_CAL_MODEL_FIELD_CLASSIFICATION :
		set_classification (comp_data, value);
		break;
	case E_CAL_MODEL_FIELD_DESCRIPTION :
		set_description (comp_data, value);
		break;
	case E_CAL_MODEL_FIELD_DTSTART :
		set_dtstart (model, comp_data, value);
		break;
	case E_CAL_MODEL_FIELD_SUMMARY :
		set_summary (comp_data, value);
		break;
	}

	/* FIXME ask about mod type */
	if (!e_cal_modify_object (comp_data->client, comp_data->icalcomp, CALOBJ_MOD_ALL, NULL)) {
		g_warning (G_STRLOC ": Could not modify the object!");

		/* FIXME Show error dialog */
	}
}

/**
 * e_cal_model_test_row_editable
 * Checks if component at row 'row' is editable or not. It doesn't check bounds for 'row'.
 *
 * @param model Calendar model.
 * @param row Row of our interest. -1 is editable only when default client is editable.
 * @return Whether row is editable or not.
 **/
gboolean
e_cal_model_test_row_editable (ECalModel *model, int row)
{
	gboolean readonly;
	ECal *cal = NULL;

	if (row != -1) {
		ECalModelComponent *comp_data;

		comp_data = e_cal_model_get_component_at (model, row);

		if (comp_data)
			cal = comp_data->client;

	} else {
		cal = e_cal_model_get_default_client (model);
	}

	readonly = cal == NULL;

	if (!readonly)
		if (!e_cal_is_read_only (cal, &readonly, NULL))
			readonly = TRUE;

	return !readonly;
}

static gboolean
ecm_is_cell_editable (ETableModel *etm, int col, int row)
{
	ECalModelPrivate *priv;
	ECalModel *model = (ECalModel *) etm;

	g_return_val_if_fail (E_IS_CAL_MODEL (model), FALSE);

	priv = model->priv;

	g_return_val_if_fail (col >= 0 && col <= E_CAL_MODEL_FIELD_LAST, FALSE);
	g_return_val_if_fail (row >= -1 || (row >= 0 && row < priv->objects->len), FALSE);

	if (!e_cal_model_test_row_editable (E_CAL_MODEL (etm), row))
		return FALSE;

	switch (col) {
	case E_CAL_MODEL_FIELD_CATEGORIES :
	case E_CAL_MODEL_FIELD_CLASSIFICATION :
	case E_CAL_MODEL_FIELD_DESCRIPTION :
	case E_CAL_MODEL_FIELD_DTSTART :
	case E_CAL_MODEL_FIELD_SUMMARY :
		return TRUE;
	}

	return FALSE;
}

static void
ecm_append_row (ETableModel *etm, ETableModel *source, int row)
{
	ECalModelClass *model_class;
	ECalModelComponent comp_data;
	ECalModel *model = (ECalModel *) etm;

	g_return_if_fail (E_IS_CAL_MODEL (model));
	g_return_if_fail (E_IS_TABLE_MODEL (source));

	memset (&comp_data, 0, sizeof (ECalModelComponent));
	comp_data.client = e_cal_model_get_default_client (model);

	/* guard against saving before the calendar is open */
	if (!(comp_data.client && e_cal_get_load_state (comp_data.client) == E_CAL_LOAD_LOADED))
		return;

	comp_data.icalcomp = e_cal_model_create_component_with_defaults (model);

	/* set values for our fields */
	set_categories (&comp_data, e_table_model_value_at (source, E_CAL_MODEL_FIELD_CATEGORIES, row));
	set_classification (&comp_data, e_table_model_value_at (source, E_CAL_MODEL_FIELD_CLASSIFICATION, row));
	set_description (&comp_data, e_table_model_value_at (source, E_CAL_MODEL_FIELD_DESCRIPTION, row));
	set_dtstart (model, &comp_data, e_table_model_value_at (source, E_CAL_MODEL_FIELD_DTSTART, row));
	set_summary (&comp_data, e_table_model_value_at (source, E_CAL_MODEL_FIELD_SUMMARY, row));

	/* call the class' method for filling the component */
	model_class = (ECalModelClass *) G_OBJECT_GET_CLASS (model);
	if (model_class->fill_component_from_model != NULL) {
		model_class->fill_component_from_model (model, &comp_data, source, row);
	}

	if (!e_cal_create_object (comp_data.client, comp_data.icalcomp, NULL, NULL)) {
		g_warning (G_STRLOC ": Could not create the object!");

		/* FIXME: show error dialog */
		icalcomponent_free (comp_data.icalcomp);
		return;
	}

	icalcomponent_free (comp_data.icalcomp);

	g_signal_emit (G_OBJECT (model), signals[ROW_APPENDED], 0);
}

static void *
ecm_duplicate_value (ETableModel *etm, int col, const void *value)
{
	g_return_val_if_fail (col >= 0 && col < E_CAL_MODEL_FIELD_LAST, NULL);

	switch (col) {
	case E_CAL_MODEL_FIELD_CATEGORIES :
	case E_CAL_MODEL_FIELD_CLASSIFICATION :
	case E_CAL_MODEL_FIELD_DESCRIPTION :
	case E_CAL_MODEL_FIELD_SUMMARY :
		return g_strdup (value);
	case E_CAL_MODEL_FIELD_HAS_ALARMS :
	case E_CAL_MODEL_FIELD_ICON :
	case E_CAL_MODEL_FIELD_COLOR :
		return (void *) value;
	case E_CAL_MODEL_FIELD_COMPONENT :
		return icalcomponent_new_clone ((icalcomponent *) value);
	case E_CAL_MODEL_FIELD_DTSTART :
		if (value) {
			ECellDateEditValue *dv, *orig_dv;

			orig_dv = (ECellDateEditValue *) value;
			dv = g_new0 (ECellDateEditValue, 1);
			*dv = *orig_dv;

			return dv;
		}
		break;
	}

	return NULL;
}

static void
ecm_free_value (ETableModel *etm, int col, void *value)
{
	g_return_if_fail (col >= 0 && col < E_CAL_MODEL_FIELD_LAST);

	switch (col) {
	case E_CAL_MODEL_FIELD_CATEGORIES :
	case E_CAL_MODEL_FIELD_DESCRIPTION :
	case E_CAL_MODEL_FIELD_SUMMARY :
		if (value)
			g_free (value);
		break;
	case E_CAL_MODEL_FIELD_CLASSIFICATION :
	case E_CAL_MODEL_FIELD_HAS_ALARMS :
	case E_CAL_MODEL_FIELD_ICON :
	case E_CAL_MODEL_FIELD_COLOR :
		break;
	case E_CAL_MODEL_FIELD_DTSTART :
		if (value)
			g_free (value);
		break;
	case E_CAL_MODEL_FIELD_COMPONENT :
		if (value)
			icalcomponent_free ((icalcomponent *) value);
		break;
	}
}

static void *
ecm_initialize_value (ETableModel *etm, int col)
{
	ECalModelPrivate *priv;
	ECalModel *model = (ECalModel *) etm;

	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);
	g_return_val_if_fail (col >= 0 && col < E_CAL_MODEL_FIELD_LAST, NULL);

	priv = model->priv;

	switch (col) {
	case E_CAL_MODEL_FIELD_CATEGORIES :
		return g_strdup (priv->default_category?priv->default_category:"");
	case E_CAL_MODEL_FIELD_CLASSIFICATION :
	case E_CAL_MODEL_FIELD_DESCRIPTION :
	case E_CAL_MODEL_FIELD_SUMMARY :
		return g_strdup ("");
	case E_CAL_MODEL_FIELD_DTSTART :
	case E_CAL_MODEL_FIELD_HAS_ALARMS :
	case E_CAL_MODEL_FIELD_ICON :
	case E_CAL_MODEL_FIELD_COLOR :
	case E_CAL_MODEL_FIELD_COMPONENT :
		return NULL;
	}

	return NULL;
}

static gboolean
ecm_value_is_empty (ETableModel *etm, int col, const void *value)
{
	ECalModelPrivate *priv;
	ECalModel *model = (ECalModel *) etm;

	g_return_val_if_fail (E_IS_CAL_MODEL (model), TRUE);
	g_return_val_if_fail (col >= 0 && col < E_CAL_MODEL_FIELD_LAST, TRUE);

	priv = model->priv;

	switch (col) {
	case E_CAL_MODEL_FIELD_CATEGORIES :
		/* This could be a hack or not.  If the categories field only
		 * contains the default category, then it possibly means that
		 * the user has not entered anything at all in the click-to-add;
		 * the category is in the value because we put it there in
		 * ecm_initialize_value().
		 */
		if (priv->default_category && value && strcmp (priv->default_category, value) == 0)
			return TRUE;
		else
			return string_is_empty (value);
	case E_CAL_MODEL_FIELD_CLASSIFICATION :
	case E_CAL_MODEL_FIELD_DESCRIPTION :
	case E_CAL_MODEL_FIELD_SUMMARY :
		return string_is_empty (value);
	case E_CAL_MODEL_FIELD_DTSTART :
		return value ? FALSE : TRUE;
	case E_CAL_MODEL_FIELD_HAS_ALARMS :
	case E_CAL_MODEL_FIELD_ICON :
	case E_CAL_MODEL_FIELD_COLOR :
	case E_CAL_MODEL_FIELD_COMPONENT :
		return TRUE;
	}

	return TRUE;
}

static char *
ecm_value_to_string (ETableModel *etm, int col, const void *value)
{
	g_return_val_if_fail (col >= 0 && col < E_CAL_MODEL_FIELD_LAST, g_strdup (""));

	switch (col) {
	case E_CAL_MODEL_FIELD_CATEGORIES :
	case E_CAL_MODEL_FIELD_CLASSIFICATION :
	case E_CAL_MODEL_FIELD_DESCRIPTION :
	case E_CAL_MODEL_FIELD_SUMMARY :
		return g_strdup (value);
	case E_CAL_MODEL_FIELD_DTSTART :
		return e_cal_model_date_value_to_string (E_CAL_MODEL (etm), value);
	case E_CAL_MODEL_FIELD_ICON :
		if (GPOINTER_TO_INT (value) == 0)
			return g_strdup (_("Normal"));
		else if (GPOINTER_TO_INT (value) == 1)
			return g_strdup (_("Recurring"));
		else
			return g_strdup (_("Assigned"));
	case E_CAL_MODEL_FIELD_HAS_ALARMS :
		return g_strdup (value ? _("Yes") : _("No"));
	case E_CAL_MODEL_FIELD_COLOR :
	case E_CAL_MODEL_FIELD_COMPONENT :
		return g_strdup ("");
	}

	return g_strdup ("");
}

/* ECalModel class methods */

typedef struct {
	const gchar *color;
	GList *uris;
} AssignedColorData;

static const char *
ecm_get_color_for_component (ECalModel *model, ECalModelComponent *comp_data)
{
	ESource *source;
	const gchar *color_spec;
	gint i, first_empty = 0;
	static AssignedColorData assigned_colors[] = {
		{ "#BECEDD", NULL }, /* 190 206 221     Blue */
		{ "#E2F0EF", NULL }, /* 226 240 239     Light Blue */
		{ "#C6E2B7", NULL }, /* 198 226 183     Green */
		{ "#E2F0D3", NULL }, /* 226 240 211     Light Green */
		{ "#E2D4B7", NULL }, /* 226 212 183     Khaki */
		{ "#EAEAC1", NULL }, /* 234 234 193     Light Khaki */
		{ "#F0B8B7", NULL }, /* 240 184 183     Pink */
		{ "#FED4D3", NULL }, /* 254 212 211     Light Pink */
		{ "#E2C6E1", NULL }, /* 226 198 225     Purple */
		{ "#F0E2EF", NULL }  /* 240 226 239     Light Purple */
	};

	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);

	source = e_cal_get_source (comp_data->client);
	color_spec = e_source_peek_color_spec (source);
	if (color_spec != NULL) {
		g_free (comp_data->color);
		comp_data->color = g_strdup (color_spec);
		return comp_data->color;
	}

	for (i = 0; i < G_N_ELEMENTS (assigned_colors); i++) {
		GList *l;

		if (assigned_colors[i].uris == NULL) {
			first_empty = i;
			continue;
		}

		for (l = assigned_colors[i].uris; l != NULL; l = l->next) {
			if (!strcmp ((const char *) l->data,
				     e_cal_get_uri (comp_data->client)))
			{
				return assigned_colors[i].color;
			}
		}
	}

	/* return the first unused color */
	assigned_colors[first_empty].uris = g_list_append (assigned_colors[first_empty].uris,
							   g_strdup (e_cal_get_uri (comp_data->client)));

	return assigned_colors[first_empty].color;
}

/**
 * e_cal_model_get_component_kind
 */
icalcomponent_kind
e_cal_model_get_component_kind (ECalModel *model)
{
	ECalModelPrivate *priv;

	g_return_val_if_fail (E_IS_CAL_MODEL (model), ICAL_NO_COMPONENT);

	priv = model->priv;
	return priv->kind;
}

/**
 * e_cal_model_set_component_kind
 */
void
e_cal_model_set_component_kind (ECalModel *model, icalcomponent_kind kind)
{
	ECalModelPrivate *priv;

	g_return_if_fail (E_IS_CAL_MODEL (model));

	priv = model->priv;
	priv->kind = kind;
}

/**
 * e_cal_model_get_flags
 */
ECalModelFlags
e_cal_model_get_flags (ECalModel *model)
{
	ECalModelPrivate *priv;

	g_return_val_if_fail (E_IS_CAL_MODEL (model), E_CAL_MODEL_FLAGS_INVALID);

	priv = model->priv;
	return priv->flags;
}

/**
 * e_cal_model_set_flags
 */
void
e_cal_model_set_flags (ECalModel *model, ECalModelFlags flags)
{
	ECalModelPrivate *priv;

	g_return_if_fail (E_IS_CAL_MODEL (model));

	priv = model->priv;
	priv->flags = flags;
}

/**
 * e_cal_model_get_timezone
 */
icaltimezone *
e_cal_model_get_timezone (ECalModel *model)
{
	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);
	return model->priv->zone;
}

/**
 * e_cal_model_set_timezone
 */
void
e_cal_model_set_timezone (ECalModel *model, icaltimezone *zone)
{
	ECalModelPrivate *priv;
	GList *l;

	g_return_if_fail (E_IS_CAL_MODEL (model));

	priv = model->priv;
	if (priv->zone != zone) {
		e_table_model_pre_change (E_TABLE_MODEL (model));
		priv->zone = zone;

		for (l = priv->clients; l; l = l->next)
			e_cal_set_default_timezone (((ECalModelClient *)l->data)->client, priv->zone, NULL);

		/* the timezone affects the times shown for date fields,
		   so we need to redisplay everything */
		e_table_model_changed (E_TABLE_MODEL (model));
	}
}

/**
 * e_cal_model_set_default_category
 */
void
e_cal_model_set_default_category (ECalModel *model, const gchar *default_cat)
{
	g_return_if_fail (E_IS_CAL_MODEL (model));

	if (model->priv->default_category)
		g_free (model->priv->default_category);

	model->priv->default_category = g_strdup (default_cat);
}

/**
 * e_cal_model_get_use_24_hour_format
 */
gboolean
e_cal_model_get_use_24_hour_format (ECalModel *model)
{
	g_return_val_if_fail (E_IS_CAL_MODEL (model), FALSE);

	return model->priv->use_24_hour_format;
}

/**
 * e_cal_model_set_use_24_hour_format
 */
void
e_cal_model_set_use_24_hour_format (ECalModel *model, gboolean use24)
{
	g_return_if_fail (E_IS_CAL_MODEL (model));

	if (model->priv->use_24_hour_format != use24) {
                e_table_model_pre_change (E_TABLE_MODEL (model));
                model->priv->use_24_hour_format = use24;
                /* Get the views to redraw themselves. */
                e_table_model_changed (E_TABLE_MODEL (model));
        }
}

/**
 * e_cal_model_get_default_client
 */
ECal *
e_cal_model_get_default_client (ECalModel *model)
{
	ECalModelPrivate *priv;
	ECalModelClient *client_data;

	g_return_val_if_fail (model != NULL, NULL);
	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);

	priv = model->priv;

	/* FIXME Should we force the client to be open? */

	/* we always return a valid ECal, since we rely on it in many places */
	if (priv->default_client)
		return priv->default_client;

	if (!priv->clients)
		return NULL;

	client_data = (ECalModelClient *) priv->clients->data;

	return client_data ? client_data->client : NULL;
}

void
e_cal_model_set_default_client (ECalModel *model, ECal *client)
{
	ECalModelPrivate *priv;
	ECalModelClient *client_data;

	g_return_if_fail (model != NULL);
	g_return_if_fail (E_IS_CAL_MODEL (model));
	g_return_if_fail (client != NULL);
	g_return_if_fail (E_IS_CAL (client));

	priv = model->priv;

	if (priv->default_client) {
		client_data = find_client_data (model, priv->default_client);
		if (!client_data) {
			g_warning ("client_data is NULL\n");
		} else {
			if (!client_data->do_query)
				remove_client (model, client_data);
		}
	}

	/* Make sure its in the model */
	client_data = add_new_client (model, client, FALSE);

	/* Store the default client */
	priv->default_client = client_data->client;
}

/**
 * e_cal_model_get_client_list
 */
GList *
e_cal_model_get_client_list (ECalModel *model)
{
	GList *list = NULL, *l;

	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);

	for (l = model->priv->clients; l != NULL; l = l->next) {
		ECalModelClient *client_data = (ECalModelClient *) l->data;

		list = g_list_append (list, client_data->client);
	}

	return list;
}

/**
 * e_cal_model_get_client_for_uri
 * @model: A calendar model.
 * @uri: Uri for the client to get.
 */
ECal *
e_cal_model_get_client_for_uri (ECalModel *model, const char *uri)
{
	GList *l;

	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);
	g_return_val_if_fail (uri != NULL, NULL);

	for (l = model->priv->clients; l != NULL; l = l->next) {
		ECalModelClient *client_data = (ECalModelClient *) l->data;

		if (!strcmp (uri, e_cal_get_uri (client_data->client)))
			return client_data->client;
	}

	return NULL;
}

static ECalModelClient *
find_client_data (ECalModel *model, ECal *client)
{
	ECalModelPrivate *priv;
	GList *l;

	priv = model->priv;

	for (l = priv->clients; l != NULL; l = l->next) {
		ECalModelClient *client_data = (ECalModelClient *) l->data;

		if (client_data->client == client)
			return client_data;
	}

	return NULL;
}

static ECalModelComponent *
search_by_id_and_client (ECalModelPrivate *priv, ECal *client, const ECalComponentId *id)
{
	gint i;

	for (i = 0; i < priv->objects->len; i++) {
		ECalModelComponent *comp_data = g_ptr_array_index (priv->objects, i);

		if (comp_data) {
			const char *uid;
			char *rid = NULL;
			gboolean has_rid = (id->rid && *id->rid);

			uid = icalcomponent_get_uid (comp_data->icalcomp);
			rid = icaltime_as_ical_string (icalcomponent_get_recurrenceid (comp_data->icalcomp));

			if (uid && *uid) {
				if ((!client || comp_data->client == client) && !strcmp (id->uid, uid)) {
					if (has_rid) {
						if (!(rid && *rid && !strcmp (rid, id->rid))) {
							g_free (rid);
							continue;
						}
					}
					g_free (rid);
					return comp_data;
				}
			g_free (rid);
			}
		}
	}

	return NULL;
}

typedef struct {
	ECal *client;
	ECalView *query;
	ECalModel *model;
	icalcomponent *icalcomp;
} RecurrenceExpansionData;

static gboolean
add_instance_cb (ECalComponent *comp, time_t instance_start, time_t instance_end, gpointer user_data)
{
	ECalModelComponent *comp_data;
	ECalModelPrivate *priv;
	RecurrenceExpansionData *rdata = user_data;
	icaltimetype time;
	ECalComponentDateTime datetime, to_set;
	icaltimezone *zone = NULL;

	g_return_val_if_fail (E_IS_CAL_COMPONENT (comp), TRUE);

	priv = rdata->model->priv;

	e_table_model_pre_change (E_TABLE_MODEL (rdata->model));

	/* set the right instance start date to component */
	e_cal_component_get_dtstart (comp, &datetime);
	e_cal_get_timezone (rdata->client, datetime.tzid, &zone, NULL);
	time = icaltime_from_timet_with_zone (instance_start, FALSE, zone ? zone : priv->zone);
	to_set.value = &time;
	to_set.tzid = datetime.tzid;
	e_cal_component_set_dtstart (comp, &to_set);
	e_cal_component_free_datetime (&datetime);

	/* set the right instance end date to component*/
	e_cal_component_get_dtend (comp, &datetime);
	e_cal_get_timezone (rdata->client, datetime.tzid, &zone, NULL);
	time = icaltime_from_timet_with_zone (instance_end, FALSE, zone ? zone : priv->zone);
	to_set.value = &time;
	to_set.tzid = datetime.tzid;
	e_cal_component_set_dtend (comp, &to_set);
	e_cal_component_free_datetime (&datetime);

	comp_data = g_new0 (ECalModelComponent, 1);
	comp_data->client = g_object_ref (rdata->client);
	comp_data->icalcomp = icalcomponent_new_clone (e_cal_component_get_icalcomponent (comp));
	comp_data->instance_start = instance_start;
	comp_data->instance_end = instance_end;
	comp_data->dtstart = comp_data->dtend = comp_data->due = comp_data->completed = NULL;
	comp_data->color = NULL;

	g_ptr_array_add (priv->objects, comp_data);
	e_table_model_row_inserted (E_TABLE_MODEL (rdata->model), priv->objects->len - 1);

	return TRUE;
}

/* We do this check since the calendar items are downloaded from the server in the open_method,
   since the default timezone might not be set there */
static void
ensure_dates_are_in_default_zone (icalcomponent *icalcomp)
{
	icaltimetype dt;
	icaltimezone *zone = calendar_config_get_icaltimezone ();

	if (!zone)
		return;

	dt = icalcomponent_get_dtstart (icalcomp);
	if (dt.is_utc) {
		dt = icaltime_convert_to_zone (dt, zone);
		icalcomponent_set_dtstart (icalcomp, dt);
	}

	dt = icalcomponent_get_dtend (icalcomp);
	if (dt.is_utc) {
		dt = icaltime_convert_to_zone (dt, zone);
		icalcomponent_set_dtend (icalcomp, dt);
	}
}

static void
e_cal_view_objects_added_cb (ECalView *query, GList *objects, gpointer user_data)
{
	ECalModel *model = (ECalModel *) user_data;
	ECalModelPrivate *priv;
	GList *l;

	priv = model->priv;

	for (l = objects; l; l = l->next) {
		ECalModelComponent *comp_data;
		ECalComponentId *id;
		ECalComponent *comp = e_cal_component_new ();
		ECal *client = e_cal_view_get_client (query);

		/* This will fail for alarm or VCalendar component */
		if (!e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (l->data))) {
			g_object_unref (comp);
			continue;
		}

		id = e_cal_component_get_id (comp);

		/* remove the components if they are already present and re-add them */
		while ((comp_data = search_by_id_and_client (priv, client,
							      id))) {
			int pos;

			pos = get_position_in_array (priv->objects, comp_data);
			e_table_model_pre_change (E_TABLE_MODEL (model));
			e_table_model_row_deleted (E_TABLE_MODEL (model), pos);

 			if (g_ptr_array_remove (priv->objects, comp_data))
	 			e_cal_model_free_component_data (comp_data);
 		}

		e_cal_component_free_id (id);
		g_object_unref (comp);
		ensure_dates_are_in_default_zone (l->data);

		if (e_cal_util_component_has_recurrences (l->data) && (priv->flags & E_CAL_MODEL_FLAGS_EXPAND_RECURRENCES)) {
			RecurrenceExpansionData rdata;

			rdata.client = e_cal_view_get_client (query);
			rdata.query = query;
			rdata.model = model;
			rdata.icalcomp = l->data;
			e_cal_generate_instances_for_object (rdata.client, l->data,
							     priv->start, priv->end,
							     (ECalRecurInstanceFn) add_instance_cb,
							     &rdata);
		} else {
			e_table_model_pre_change (E_TABLE_MODEL (model));

			comp_data = g_new0 (ECalModelComponent, 1);
			comp_data->client = g_object_ref (e_cal_view_get_client (query));
			comp_data->icalcomp = icalcomponent_new_clone (l->data);
			e_cal_model_set_instance_times (comp_data, priv->zone);
			comp_data->dtstart = comp_data->dtend = comp_data->due = comp_data->completed = NULL;
			comp_data->color = NULL;

			g_ptr_array_add (priv->objects, comp_data);
			e_table_model_row_inserted (E_TABLE_MODEL (model), priv->objects->len - 1);
		}
	}
}

static void
e_cal_view_objects_modified_cb (ECalView *query, GList *objects, gpointer user_data)
{
	ECalModel *model = (ECalModel *) user_data;

	/* now re-add all objects */
	e_cal_view_objects_added_cb (query, objects, model);
}

static void
e_cal_view_objects_removed_cb (ECalView *query, GList *ids, gpointer user_data)
{
	ECalModelPrivate *priv;
	ECalModel *model = (ECalModel *) user_data;
	GList *l;

	priv = model->priv;

	for (l = ids; l; l = l->next) {
		ECalModelComponent *comp_data = NULL;
		ECalComponentId *id = l->data;
		int pos;

		/* make sure we remove all objects with this UID */
		while ((comp_data = search_by_id_and_client (priv, e_cal_view_get_client (query), id))) {
			pos = get_position_in_array (priv->objects, comp_data);

			e_table_model_pre_change (E_TABLE_MODEL (model));
			e_table_model_row_deleted (E_TABLE_MODEL (model), pos);

			if (g_ptr_array_remove (priv->objects, comp_data))
				e_cal_model_free_component_data (comp_data);
		}
	}

	/* to notify about changes, because in call of row_deleted there are still all events */
	e_table_model_changed (E_TABLE_MODEL (model));
}

static void
e_cal_view_progress_cb (ECalView *query, const char *message, int percent, gpointer user_data)
{
	ECalModel *model = (ECalModel *) user_data;
	ECal *client = e_cal_view_get_client (query);

	g_return_if_fail (E_IS_CAL_MODEL (model));

	g_signal_emit (G_OBJECT (model), signals[CAL_VIEW_PROGRESS], 0, message,
			percent, e_cal_get_source_type (client));
}

static void
e_cal_view_done_cb (ECalView *query, ECalendarStatus status, gpointer user_data)
{
 	ECalModel *model = (ECalModel *) user_data;
	ECal *client = e_cal_view_get_client (query);

	g_return_if_fail (E_IS_CAL_MODEL (model));

	/* emit the signal on the model and let the view catch it to display */
	g_signal_emit (G_OBJECT (model), signals[CAL_VIEW_DONE], 0, status,
			e_cal_get_source_type (client));
}

static void
update_e_cal_view_for_client (ECalModel *model, ECalModelClient *client_data)
{
	ECalModelPrivate *priv;

	priv = model->priv;

	/* Skip if this client has not finished loading yet */
	if (e_cal_get_load_state (client_data->client) != E_CAL_LOAD_LOADED)
		return;

	/* free the previous query, if any */
	if (client_data->query) {
		g_signal_handlers_disconnect_matched (client_data->query, G_SIGNAL_MATCH_DATA,
						      0, 0, NULL, NULL, model);
		g_object_unref (client_data->query);
		client_data->query = NULL;
	}

	/* prepare the query */
	g_return_if_fail (priv->full_sexp != NULL);

	/* Don't create the new query if we won't use it */
	if (!client_data->do_query)
		return;

	if (!e_cal_get_query (client_data->client, priv->full_sexp, &client_data->query, NULL)) {
		g_warning (G_STRLOC ": Unable to get query");

		return;
	}

	g_signal_connect (client_data->query, "objects_added", G_CALLBACK (e_cal_view_objects_added_cb), model);
	g_signal_connect (client_data->query, "objects_modified", G_CALLBACK (e_cal_view_objects_modified_cb), model);
	g_signal_connect (client_data->query, "objects_removed", G_CALLBACK (e_cal_view_objects_removed_cb), model);
	g_signal_connect (client_data->query, "view_progress", G_CALLBACK (e_cal_view_progress_cb), model);
	g_signal_connect (client_data->query, "view_done", G_CALLBACK (e_cal_view_done_cb), model);

	e_cal_view_start (client_data->query);
}

static void
backend_died_cb (ECal *client, gpointer user_data)
{
	ECalModel *model;

	model = E_CAL_MODEL (user_data);

	e_cal_model_remove_client (model, client);
}

static void
cal_opened_cb (ECal *client, ECalendarStatus status, gpointer user_data)
{
	ECalModel *model = (ECalModel *) user_data;
	ECalModelClient *client_data;

	if (status == E_CALENDAR_STATUS_BUSY) {
		e_cal_open_async (client, FALSE);
		return;
	}

	if (status != E_CALENDAR_STATUS_OK) {
		e_cal_model_remove_client (model, client);

		return;
	}

	/* Stop listening for this calendar to be opened */
	g_signal_handlers_disconnect_matched (client, G_SIGNAL_MATCH_FUNC | G_SIGNAL_MATCH_DATA, 0, 0, NULL, cal_opened_cb, model);

	client_data = find_client_data (model, client);
	g_return_if_fail (client_data);

	update_e_cal_view_for_client (model, client_data);
}


static ECalModelClient *
add_new_client (ECalModel *model, ECal *client, gboolean do_query)
{
	ECalModelPrivate *priv;
	ECalModelClient *client_data;

	priv = model->priv;

	/* Look to see if we already have this client */
	client_data = find_client_data (model, client);
	if (client_data) {
		if (client_data->do_query)
			return client_data;
		else
			client_data->do_query = do_query;

		goto load;
	}

	client_data = g_new0 (ECalModelClient, 1);
	client_data->client = g_object_ref (client);
	client_data->query = NULL;
	client_data->do_query = do_query;

	priv->clients = g_list_append (priv->clients, client_data);

	g_signal_connect (G_OBJECT (client_data->client), "backend_died",
			  G_CALLBACK (backend_died_cb), model);

 load:
	if (e_cal_get_load_state (client) == E_CAL_LOAD_LOADED) {
		update_e_cal_view_for_client (model, client_data);
	} else {
		g_signal_connect (client, "cal_opened", G_CALLBACK (cal_opened_cb), model);
		e_cal_open_async (client, TRUE);
	}

	return client_data;
}

/**
 * e_cal_model_add_client
 */
void
e_cal_model_add_client (ECalModel *model, ECal *client)
{
	ECalModelClient *client_data;

	g_return_if_fail (E_IS_CAL_MODEL (model));
	g_return_if_fail (E_IS_CAL (client));
	/* Check this return value or drop the assignment? */
	client_data = add_new_client (model, client, TRUE);
}

static void
remove_client_objects (ECalModel *model, ECalModelClient *client_data)
{
	int i;

	/* remove all objects belonging to this client */
	for (i = model->priv->objects->len; i > 0; i--) {
		ECalModelComponent *comp_data = (ECalModelComponent *) g_ptr_array_index (model->priv->objects, i - 1);

		g_return_if_fail (comp_data != NULL);

		if (comp_data->client == client_data->client) {
			e_table_model_pre_change (E_TABLE_MODEL (model));
			e_table_model_row_deleted (E_TABLE_MODEL (model), i - 1);

			g_ptr_array_remove (model->priv->objects, comp_data);
			e_cal_model_free_component_data (comp_data);
		}
	}

	/* to notify about changes, because in call of row_deleted there are still all events */
	e_table_model_changed (E_TABLE_MODEL (model));
}

static void
remove_client (ECalModel *model, ECalModelClient *client_data)
{
	/* FIXME We might not want to disconnect the open signal for the default client */
	g_signal_handlers_disconnect_matched (client_data->client, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, model);
	if (client_data->query)
		g_signal_handlers_disconnect_matched (client_data->query, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, model);

	remove_client_objects (model, client_data);

	/* If this is the default client and we were querying (so it
	 * was also a source), keep it around but don't query it */
	if (model->priv->default_client == client_data->client && client_data->do_query) {
		client_data->do_query = FALSE;

		return;
	}

	if (model->priv->default_client == client_data->client)
		model->priv->default_client = NULL;

	/* Remove the client from the list */
	model->priv->clients = g_list_remove (model->priv->clients, client_data);

	/* free all remaining memory */
	g_object_unref (client_data->client);
	if (client_data->query)
		g_object_unref (client_data->query);
	g_free (client_data);
}

/**
 * e_cal_model_remove_client
 */
void
e_cal_model_remove_client (ECalModel *model, ECal *client)
{
	ECalModelClient *client_data;

	g_return_if_fail (E_IS_CAL_MODEL (model));
	g_return_if_fail (E_IS_CAL (client));

	client_data = find_client_data (model, client);
	if (client_data)
		remove_client (model, client_data);
}

/**
 * e_cal_model_remove_all_clients
 */
void
e_cal_model_remove_all_clients (ECalModel *model)
{
	ECalModelPrivate *priv;

	g_return_if_fail (E_IS_CAL_MODEL (model));

	priv = model->priv;
	while (priv->clients != NULL) {
		ECalModelClient *client_data = (ECalModelClient *) priv->clients->data;
		remove_client (model, client_data);
	}
}

static void
redo_queries (ECalModel *model)
{
	ECalModelPrivate *priv;
	GList *l;
	int len;

	priv = model->priv;

	if (priv->full_sexp)
		g_free (priv->full_sexp);

	if (priv->start != -1 && priv->end != -1) {
		char *iso_start, *iso_end;

		iso_start = isodate_from_time_t (priv->start);
		iso_end = isodate_from_time_t (priv->end);

		priv->full_sexp = g_strdup_printf ("(and (occur-in-time-range? (make-time \"%s\")"
						   "                           (make-time \"%s\"))"
						   "     %s)",
						   iso_start, iso_end,
						   priv->search_sexp ? priv->search_sexp : "");
		g_free (iso_start);
		g_free (iso_end);
	} else if (priv->search_sexp) {
		priv->full_sexp = g_strdup (priv->search_sexp);
	} else {
		priv->full_sexp = g_strdup ("#f");
	}

	/* clean up the current contents */
	e_table_model_pre_change (E_TABLE_MODEL (model));
	len = priv->objects->len;
	e_table_model_rows_deleted (E_TABLE_MODEL (model), 0, len);
	clear_objects_array (priv);

	/* update the query for all clients */
	for (l = priv->clients; l != NULL; l = l->next) {
		ECalModelClient *client_data;

		client_data = (ECalModelClient *) l->data;
		update_e_cal_view_for_client (model, client_data);
	}
}

void
e_cal_model_get_time_range (ECalModel *model, time_t *start, time_t *end)
{
	ECalModelPrivate *priv;

	g_return_if_fail (model != NULL);
	g_return_if_fail (E_IS_CAL_MODEL (model));

	priv = model->priv;

	if (start)
		*start = priv->start;

	if (end)
		*end = priv->end;
}

void
e_cal_model_set_time_range (ECalModel *model, time_t start, time_t end)
{
	ECalModelPrivate *priv;

	g_return_if_fail (model != NULL);
	g_return_if_fail (E_IS_CAL_MODEL (model));
	g_return_if_fail (start >= 0 && end >= 0);
	g_return_if_fail (start <= end);

	priv = model->priv;

	if (priv->start == start && priv->end == end)
		return;

	priv->start = start;
	priv->end = end;

	g_signal_emit (G_OBJECT (model), signals[TIME_RANGE_CHANGED], 0, start, end);
	redo_queries (model);
}

const char *
e_cal_model_get_search_query (ECalModel *model)
{
	ECalModelPrivate *priv;

	g_return_val_if_fail (model != NULL, NULL);
	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);

	priv = model->priv;

	return priv->search_sexp;
}

/**
 * e_cal_model_set_query
 */
void
e_cal_model_set_search_query (ECalModel *model, const char *sexp)
{
	ECalModelPrivate *priv;

	g_return_if_fail (E_IS_CAL_MODEL (model));

	priv = model->priv;

	if (!strcmp (sexp ? sexp : "", priv->search_sexp ? priv->search_sexp : ""))
		return;

	if (priv->search_sexp)
		g_free (priv->search_sexp);

	priv->search_sexp = g_strdup (sexp);

	redo_queries (model);
}

/**
 * e_cal_model_set_query
 */
void
e_cal_model_set_search_query_with_time_range (ECalModel *model, const char *sexp, time_t start, time_t end)
{
	ECalModelPrivate *priv;
	gboolean do_query = FALSE;

	g_return_if_fail (E_IS_CAL_MODEL (model));

	priv = model->priv;

	if (strcmp (sexp ? sexp : "", priv->search_sexp ? priv->search_sexp : "")) {
		if (priv->search_sexp)
			g_free (priv->search_sexp);

		priv->search_sexp = g_strdup (sexp);
		do_query = TRUE;
	}

	if (!(priv->start == start && priv->end == end)) {
		priv->start = start;
		priv->end = end;
		do_query = TRUE;
	}

	if (do_query)
		redo_queries (model);
}

/**
 * e_cal_model_create_component_with_defaults
 */
icalcomponent *
e_cal_model_create_component_with_defaults (ECalModel *model)
{
	ECalModelPrivate *priv;
	ECalComponent *comp;
	icalcomponent *icalcomp;
	ECal *client;

	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);

	priv = model->priv;

	g_return_val_if_fail (priv->clients != NULL, NULL);

	client = e_cal_model_get_default_client (model);
	if (!client)
		return icalcomponent_new (priv->kind);

	switch (priv->kind) {
	case ICAL_VEVENT_COMPONENT :
		comp = cal_comp_event_new_with_defaults (client);
		break;
	case ICAL_VTODO_COMPONENT :
		comp = cal_comp_task_new_with_defaults (client);
		break;
	case ICAL_VJOURNAL_COMPONENT :
	        comp = cal_comp_memo_new_with_defaults (client);
		break;
	default:
		return NULL;
	}

	if (!comp)
		return icalcomponent_new (priv->kind);

	icalcomp = icalcomponent_new_clone (e_cal_component_get_icalcomponent (comp));
	g_object_unref (comp);

	/* make sure the component has an UID */
	if (!icalcomponent_get_uid (icalcomp)) {
		char *uid;

		uid = e_cal_component_gen_uid ();
		icalcomponent_set_uid (icalcomp, uid);

		g_free (uid);
	}

	return icalcomp;
}

/**
 * e_cal_model_get_color_for_component
 */
const gchar *
e_cal_model_get_color_for_component (ECalModel *model, ECalModelComponent *comp_data)
{
	ECalModelClass *model_class;
	const gchar *color = NULL;

	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);
	g_return_val_if_fail (comp_data != NULL, NULL);

	model_class = (ECalModelClass *) G_OBJECT_GET_CLASS (model);
	if (model_class->get_color_for_component != NULL)
		color = model_class->get_color_for_component (model, comp_data);

	if (!color)
		color = ecm_get_color_for_component (model, comp_data);

	return color;
}

/**
 * e_cal_model_get_rgb_color_for_component
 */
gboolean
e_cal_model_get_rgb_color_for_component (ECalModel *model, ECalModelComponent *comp_data, double *red, double *green, double *blue)
{
	GdkColor gdk_color;
	const gchar *color;

	color = e_cal_model_get_color_for_component (model, comp_data);
	if (color && gdk_color_parse (color, &gdk_color)) {

		if (red)
			*red = ((double) gdk_color.red)/0xffff;
		if (green)
			*green = ((double) gdk_color.green)/0xffff;
		if (blue)
			*blue = ((double) gdk_color.blue)/0xffff;

		return TRUE;
	}

	return FALSE;
}

/**
 * e_cal_model_get_component_at
 */
ECalModelComponent *
e_cal_model_get_component_at (ECalModel *model, gint row)
{
	ECalModelPrivate *priv;

	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);

	priv = model->priv;

	g_return_val_if_fail (row >= 0 && row < priv->objects->len, NULL);

	return g_ptr_array_index (priv->objects, row);
}

ECalModelComponent *
e_cal_model_get_component_for_uid  (ECalModel *model, const ECalComponentId *id)
{
	ECalModelPrivate *priv;

	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);

	priv = model->priv;

	return search_by_id_and_client (priv, NULL, id);
}

/**
 * e_cal_model_date_value_to_string
 */
gchar*
e_cal_model_date_value_to_string (ECalModel *model, const void *value)
{
	ECalModelPrivate *priv;
	ECellDateEditValue *dv = (ECellDateEditValue *) value;
	struct icaltimetype tt;
	struct tm tmp_tm;
	char buffer[64];

	g_return_val_if_fail (E_IS_CAL_MODEL (model), g_strdup (""));

	priv = model->priv;

	if (!dv)
		return g_strdup ("");

	/* We currently convert all the dates to the current timezone. */
	tt = dv->tt;
	icaltimezone_convert_time (&tt, dv->zone, priv->zone);

	tmp_tm.tm_year = tt.year - 1900;
	tmp_tm.tm_mon = tt.month - 1;
	tmp_tm.tm_mday = tt.day;
	tmp_tm.tm_hour = tt.hour;
	tmp_tm.tm_min = tt.minute;
	tmp_tm.tm_sec = tt.second;
	tmp_tm.tm_isdst = -1;

	tmp_tm.tm_wday = time_day_of_week (tt.day, tt.month - 1, tt.year);

	memset (buffer, 0, sizeof (buffer));
	e_time_format_date_and_time (&tmp_tm, priv->use_24_hour_format,
				     TRUE, FALSE,
				     buffer, sizeof (buffer));
	return g_strdup (buffer);
}

static ECellDateEditValue *
copy_ecdv (ECellDateEditValue *ecdv)
{
	ECellDateEditValue *new_ecdv;

	new_ecdv = g_new0 (ECellDateEditValue, 1);
	new_ecdv->tt = ecdv ? ecdv->tt : icaltime_null_time ();
	new_ecdv->zone = ecdv ? ecdv->zone : NULL;

	return new_ecdv;
}

/**
 * e_cal_model_copy_component_data
 */
ECalModelComponent *
e_cal_model_copy_component_data (ECalModelComponent *comp_data)
{
	ECalModelComponent *new_data;

	g_return_val_if_fail (comp_data != NULL, NULL);

	new_data = g_new0 (ECalModelComponent, 1);

	new_data->instance_start = comp_data->instance_start;
	new_data->instance_end = comp_data->instance_end;
	if (comp_data->icalcomp)
		new_data->icalcomp = icalcomponent_new_clone (comp_data->icalcomp);
	if (comp_data->client)
		new_data->client = g_object_ref (comp_data->client);
	if (comp_data->dtstart)
		new_data->dtstart = copy_ecdv (comp_data->dtstart);
	if (comp_data->dtend)
		new_data->dtend = copy_ecdv (comp_data->dtend);
	if (comp_data->due)
		new_data->due = copy_ecdv (comp_data->due);
	if (comp_data->completed)
		new_data->completed = copy_ecdv (comp_data->completed);
	if (comp_data->color)
		new_data->color = g_strdup (comp_data->color);

	return new_data;
}

/**
 * e_cal_model_free_component_data
 */
void
e_cal_model_free_component_data (ECalModelComponent *comp_data)
{
	g_return_if_fail (comp_data != NULL);

	if (comp_data->client) {
		g_object_unref (comp_data->client);
		comp_data->client = NULL;
	}
	if (comp_data->icalcomp) {
		icalcomponent_free (comp_data->icalcomp);
		comp_data->icalcomp = NULL;
	}
	if (comp_data->dtstart) {
		g_free (comp_data->dtstart);
		comp_data->dtstart = NULL;
	}
	if (comp_data->dtend) {
		g_free (comp_data->dtend);
		comp_data->dtend = NULL;
	}
	if (comp_data->due) {
		g_free (comp_data->due);
		comp_data->due = NULL;
	}
	if (comp_data->completed) {
		g_free (comp_data->completed);
		comp_data->completed = NULL;
	}
	if (comp_data->color) {
		g_free (comp_data->color);
		comp_data->color = NULL;
	}

	g_free (comp_data);
}

/**
 * e_cal_model_generate_instances
 *
 * cb function is not called with cb_data, but with ECalModelGenerateInstancesData which contains cb_data
 */
void
e_cal_model_generate_instances (ECalModel *model, time_t start, time_t end,
				ECalRecurInstanceFn cb, gpointer cb_data)
{
	ECalModelGenerateInstancesData mdata;
	gint i, n;

	n = e_table_model_row_count (E_TABLE_MODEL (model));
	for (i = 0; i < n; i ++) {
		ECalModelComponent *comp_data = e_cal_model_get_component_at (model, i);

		mdata.comp_data = comp_data;
		mdata.cb_data = cb_data;
		e_cal_generate_instances_for_object (comp_data->client, comp_data->icalcomp, start, end, cb, &mdata);
	}
}

/**
 * e_cal_model_get_object_array
 */
GPtrArray *
e_cal_model_get_object_array (ECalModel *model)
{
	g_return_val_if_fail (E_IS_CAL_MODEL (model), NULL);

	return model->priv->objects;
}

void
e_cal_model_set_instance_times (ECalModelComponent *comp_data, const icaltimezone *zone)
{
	struct icaltimetype start_time, end_time;
	icalcomponent_kind kind;

	kind = icalcomponent_isa (comp_data->icalcomp);
	start_time = icalcomponent_get_dtstart (comp_data->icalcomp);
	end_time = icalcomponent_get_dtend (comp_data->icalcomp);

	if (kind == ICAL_VEVENT_COMPONENT) {
		if (start_time.is_date && icaltime_is_null_time (end_time)) {
			/* If end_time is null and it's an all day event,
			 * just make start_time = end_time so that end_time
			 * will be a valid date
			 */
			end_time = start_time;
			icaltime_adjust (&end_time, 1, 0, 0, 0);
			icalcomponent_set_dtend (comp_data->icalcomp, end_time);
		} else if (start_time.is_date && end_time.is_date &&
			   (icaltime_compare_date_only (start_time, end_time) == 0)) {
			/* If both DTSTART and DTEND are DATE values, and they are the
			   same day, we add 1 day to DTEND. This means that most
			   events created with the old Evolution behavior will still
			   work OK. */
			icaltime_adjust (&end_time, 1, 0, 0, 0);
			icalcomponent_set_dtend (comp_data->icalcomp, end_time);
		}
	}

	if (start_time.zone)
		zone = start_time.zone;
	else {
		icalparameter *param = NULL;
		icalproperty *prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_DTSTART_PROPERTY);

	       if (prop)	{
			param = icalproperty_get_first_parameter (prop, ICAL_TZID_PARAMETER);

			if (param) {
				const char *tzid = NULL;
				icaltimezone *st_zone = NULL;

				tzid = icalparameter_get_tzid (param);
				e_cal_get_timezone (comp_data->client, tzid, &st_zone, NULL);

				if (st_zone)
					zone = st_zone;
			}
	       }
	}

	comp_data->instance_start = icaltime_as_timet_with_zone (start_time, zone);

	if (end_time.zone)
		zone = end_time.zone;
	else {
		icalparameter *param = NULL;
		icalproperty *prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_DTSTART_PROPERTY);

	       if (prop)	{
			param = icalproperty_get_first_parameter (prop, ICAL_TZID_PARAMETER);

			if (param) {
				const char *tzid = NULL;
				icaltimezone *end_zone = NULL;

				tzid = icalparameter_get_tzid (param);
				e_cal_get_timezone (comp_data->client, tzid, &end_zone, NULL);

				if (end_zone)
					zone = end_zone;
			}
	       }

	}
	comp_data->instance_end = icaltime_as_timet_with_zone (end_time, zone);
}
