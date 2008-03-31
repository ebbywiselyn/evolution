/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * Author :
 *  Rodrigo Moya <rodrigo@ximian.com>
 *
 * Copyright 2003, Ximian, Inc.
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301
 * USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <time.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <gtk/gtkimage.h>
#include <gtk/gtkstock.h>
#include <gdk/gdkkeysyms.h>
#include <gtk/gtkbindings.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkbox.h>
#include <gtk/gtkcontainer.h>
#include <gtk/gtkwindow.h>
#include <glib/gi18n.h>
#include <libedataserver/e-time-utils.h>
#include <e-util/e-error.h>
#include <e-util/e-dialog-utils.h>
#include <e-util/e-icon-factory.h>
#include "e-calendar-marshal.h"
#include <libecal/e-cal-time-util.h>
#include <libecal/e-cal-component.h>

#include "common/authentication.h"
#include "calendar-commands.h"
#include "calendar-component.h"
#include "calendar-config.h"
#include "comp-util.h"
#include "e-cal-model-calendar.h"
#include "e-calendar-view.h"
#include "e-comp-editor-registry.h"
#include "itip-utils.h"
#include "dialogs/delete-comp.h"
#include "dialogs/delete-error.h"
#include "dialogs/event-editor.h"
#include "dialogs/send-comp.h"
#include "dialogs/cancel-comp.h"
#include "dialogs/recur-comp.h"
#include "dialogs/select-source-dialog.h"
#include "print.h"
#include "goto.h"
#include "ea-calendar.h"
#include "e-cal-popup.h"
#include "misc.h"

/* Used for the status bar messages */
#define EVOLUTION_CALENDAR_PROGRESS_IMAGE "stock_calendar"
static GdkPixbuf *progress_icon = NULL;

struct _ECalendarViewPrivate {
	/* The GnomeCalendar we are associated to */
	GnomeCalendar *calendar;

	/* The calendar model we are monitoring */
	ECalModel *model;

	/* Current activity (for the EActivityHandler, i.e. the status bar).  */
	EActivityHandler *activity_handler;
	guint activity_id;

	/* The default category */
	char *default_category;
};

static void e_calendar_view_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void e_calendar_view_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static void e_calendar_view_destroy (GtkObject *object);
static void open_event_with_flags (ECalendarView *cal_view, ECal *client, icalcomponent *icalcomp, guint32 flags);

static GdkAtom clipboard_atom = GDK_NONE;
extern ECompEditorRegistry *comp_editor_registry;

/* Property IDs */
enum props {
	PROP_0,
	PROP_MODEL,
};

/* FIXME Why are we emitting these event signals here? Can't the model just be listened to? */
/* Signal IDs */
enum {
	SELECTION_CHANGED,
	SELECTED_TIME_CHANGED,
	TIMEZONE_CHANGED,
	EVENT_CHANGED,
	EVENT_ADDED,
	USER_CREATED,
	OPEN_EVENT,
	LAST_SIGNAL
};

static guint e_calendar_view_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (ECalendarView, e_calendar_view, GTK_TYPE_TABLE)

static void
e_calendar_view_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
	ECalendarView *cal_view;

	cal_view = E_CALENDAR_VIEW (object);

	switch (property_id) {
	case PROP_MODEL:
		e_calendar_view_set_model (cal_view, E_CAL_MODEL (g_value_get_object (value)));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
e_calendar_view_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
	ECalendarView *cal_view;

	cal_view = E_CALENDAR_VIEW (object);

	switch (property_id) {
	case PROP_MODEL:
		g_value_set_object (value, e_calendar_view_get_model (cal_view));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
		break;
	}
}

static void
e_calendar_view_class_init (ECalendarViewClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
	GtkObjectClass *object_class = GTK_OBJECT_CLASS (klass);

	GtkBindingSet *binding_set;

	/* Method override */
	gobject_class->set_property = e_calendar_view_set_property;
	gobject_class->get_property = e_calendar_view_get_property;
	object_class->destroy = e_calendar_view_destroy;

	klass->selection_changed = NULL;
 	klass->selected_time_changed = NULL;
	klass->event_changed = NULL;
	klass->event_added = NULL;
	klass->user_created = NULL;

	klass->get_selected_events = NULL;
	klass->get_selected_time_range = NULL;
	klass->set_selected_time_range = NULL;
	klass->get_visible_time_range = NULL;
	klass->update_query = NULL;
	klass->open_event = e_calendar_view_open_event;

	g_object_class_install_property (gobject_class, PROP_MODEL,
					 g_param_spec_object ("model", NULL, NULL, E_TYPE_CAL_MODEL,
							      G_PARAM_READABLE | G_PARAM_WRITABLE));

	/* Create class' signals */
	e_calendar_view_signals[SELECTION_CHANGED] =
		g_signal_new ("selection_changed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ECalendarViewClass, selection_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	e_calendar_view_signals[SELECTED_TIME_CHANGED] =
		g_signal_new ("selected_time_changed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ECalendarViewClass, selected_time_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);
	e_calendar_view_signals[TIMEZONE_CHANGED] =
		g_signal_new ("timezone_changed",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ECalendarViewClass, timezone_changed),
			      NULL, NULL,
			      e_calendar_marshal_VOID__POINTER_POINTER,
			      G_TYPE_NONE, 2, G_TYPE_POINTER, G_TYPE_POINTER);

	e_calendar_view_signals[EVENT_CHANGED] =
		g_signal_new ("event_changed",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (ECalendarViewClass, event_changed),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1,
			      G_TYPE_POINTER);

	e_calendar_view_signals[EVENT_ADDED] =
		g_signal_new ("event_added",
			      G_TYPE_FROM_CLASS (object_class),
			      G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (ECalendarViewClass, event_added),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__POINTER,
			      G_TYPE_NONE, 1,
			      G_TYPE_POINTER);

	e_calendar_view_signals[USER_CREATED] =
		g_signal_new ("user_created",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ECalendarViewClass, user_created),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	e_calendar_view_signals[OPEN_EVENT] =
		g_signal_new ("open_event",
			      G_TYPE_FROM_CLASS (klass),
			      G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
			      G_STRUCT_OFFSET (ECalendarViewClass, open_event),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	/* clipboard atom */
	if (!clipboard_atom)
		clipboard_atom = gdk_atom_intern ("CLIPBOARD", FALSE);


        /*
         * Key bindings
         */

	binding_set = gtk_binding_set_by_class (klass);

	gtk_binding_entry_add_signal (binding_set, GDK_o,
                                      GDK_CONTROL_MASK,
                                      "open_event", 0);

	/* init the accessibility support for e_day_view */
 	e_cal_view_a11y_init ();
}


void
e_calendar_view_add_event (ECalendarView *cal_view, ECal *client, time_t dtstart,
		      icaltimezone *default_zone, icalcomponent *icalcomp, gboolean in_top_canvas)
{
	ECalComponent *comp;
	struct icaltimetype itime, old_dtstart, old_dtend;
	time_t tt_start, tt_end, new_dtstart = 0;
	struct icaldurationtype ic_dur, ic_oneday;
	char *uid;
	gint start_offset, end_offset;
	gboolean all_day_event = FALSE;
	GnomeCalendarViewType view_type;
	ECalComponentDateTime dt;

	start_offset = 0;
	end_offset = 0;

	old_dtstart = icalcomponent_get_dtstart (icalcomp);
	tt_start = icaltime_as_timet (old_dtstart);
	old_dtend = icalcomponent_get_dtend (icalcomp);
	tt_end = icaltime_as_timet (old_dtend);
	ic_dur = icaldurationtype_from_int (tt_end - tt_start);

	if (icaldurationtype_as_int (ic_dur) > 60*60*24) {
		/* This is a long event */
		start_offset = old_dtstart.hour * 60 + old_dtstart.minute;
		end_offset = old_dtstart.hour * 60 + old_dtend.minute;
	}

	ic_oneday = icaldurationtype_null_duration ();
	ic_oneday.days = 1;

	view_type = gnome_calendar_get_view (cal_view->priv->calendar);

	switch (view_type) {
	case GNOME_CAL_DAY_VIEW:
	case GNOME_CAL_WORK_WEEK_VIEW:
		if (start_offset == 0 && end_offset == 0 && in_top_canvas)
			all_day_event = TRUE;

		if (all_day_event) {
			ic_dur = ic_oneday;
		} else if (icaldurationtype_as_int (ic_dur) >= 60*60*24
				&& !in_top_canvas) {
			/* copy & paste from top canvas to main canvas */
			int time_divisions;

			time_divisions = calendar_config_get_time_divisions ();
			ic_dur = icaldurationtype_from_int (time_divisions * 60);
		}

		if (in_top_canvas)
			new_dtstart = dtstart + start_offset * 60;
		else
			new_dtstart = dtstart;
		break;
	case GNOME_CAL_WEEK_VIEW:
	case GNOME_CAL_MONTH_VIEW:
	case GNOME_CAL_LIST_VIEW:
		if (old_dtstart.is_date && old_dtend.is_date
			&& memcmp (&ic_dur, &ic_oneday, sizeof(ic_dur)) == 0)
			all_day_event = TRUE;
		else {
			icaltimetype new_time = icaltime_from_timet_with_zone (dtstart, FALSE, default_zone);

			new_time.hour = old_dtstart.hour;
			new_time.minute = old_dtstart.minute;
			new_time.second = old_dtstart.second;

			new_dtstart = icaltime_as_timet_with_zone (new_time, default_zone);
		}
		break;
	default:
		g_return_if_reached ();
	}


	itime = icaltime_from_timet_with_zone (new_dtstart, FALSE, default_zone);
	if (all_day_event)
		itime.is_date = TRUE;
	icalcomponent_set_dtstart (icalcomp, itime);

	itime.is_date = FALSE;
	itime = icaltime_add (itime, ic_dur);
	if (all_day_event)
		itime.is_date = TRUE;
	icalcomponent_set_dtend (icalcomp, itime);

	/* FIXME The new uid stuff can go away once we actually set it in the backend */
	uid = e_cal_component_gen_uid ();
	comp = e_cal_component_new ();
	e_cal_component_set_icalcomponent (
		comp, icalcomponent_new_clone (icalcomp));
	e_cal_component_set_uid (comp, uid);
	g_free (uid);

	/* set the timezone properly */
	e_cal_component_get_dtstart (comp, &dt);
	dt.tzid = icaltimezone_get_tzid (default_zone);
	e_cal_component_set_dtstart (comp, &dt);
	e_cal_component_free_datetime (&dt);

	e_cal_component_get_dtend (comp, &dt);
	dt.tzid = icaltimezone_get_tzid (default_zone);
	e_cal_component_set_dtend (comp, &dt);
	e_cal_component_free_datetime (&dt);

	e_cal_component_commit_sequence (comp);

	/* FIXME Error handling */
	uid = NULL;
	if (e_cal_create_object (client, e_cal_component_get_icalcomponent (comp), &uid, NULL)) {
		if (uid) {
			e_cal_component_set_uid (comp, uid);
			g_free (uid);
		}

		if ((itip_organizer_is_user (comp, client) || itip_sentby_is_user (comp)) &&
		    send_component_dialog ((GtkWindow *) gtk_widget_get_toplevel (GTK_WIDGET (cal_view)),
					   client, comp, TRUE)) {
			itip_send_comp (E_CAL_COMPONENT_METHOD_REQUEST, comp,
				client, NULL, NULL, NULL);
		}
	} else {
		g_message (G_STRLOC ": Could not create the object!");
	}

	g_object_unref (comp);
}

static void
e_calendar_view_init (ECalendarView *cal_view)
{
	cal_view->priv = g_new0 (ECalendarViewPrivate, 1);

	cal_view->priv->model = (ECalModel *) e_cal_model_calendar_new ();
}

static void
e_calendar_view_destroy (GtkObject *object)
{
	ECalendarView *cal_view = (ECalendarView *) object;

	g_return_if_fail (E_IS_CALENDAR_VIEW (cal_view));

	if (cal_view->priv) {
		if (cal_view->priv->model) {
			g_signal_handlers_disconnect_matched (cal_view->priv->model,
							      G_SIGNAL_MATCH_DATA,
							      0, 0, NULL, NULL, cal_view);
			g_object_unref (cal_view->priv->model);
			cal_view->priv->model = NULL;
		}

		if (cal_view->priv->default_category) {
			g_free (cal_view->priv->default_category);
			cal_view->priv->default_category = NULL;
		}

		g_free (cal_view->priv);
		cal_view->priv = NULL;
	}

	if (GTK_OBJECT_CLASS (e_calendar_view_parent_class)->destroy)
		GTK_OBJECT_CLASS (e_calendar_view_parent_class)->destroy (object);
}

GnomeCalendar *
e_calendar_view_get_calendar (ECalendarView *cal_view)
{
	g_return_val_if_fail (E_IS_CALENDAR_VIEW (cal_view), NULL);

	return cal_view->priv->calendar;
}

void
e_calendar_view_set_calendar (ECalendarView *cal_view, GnomeCalendar *calendar)
{
	g_return_if_fail (E_IS_CALENDAR_VIEW (cal_view));

	cal_view->priv->calendar = calendar;
}

ECalModel *
e_calendar_view_get_model (ECalendarView *cal_view)
{
	g_return_val_if_fail (E_IS_CALENDAR_VIEW (cal_view), NULL);

	return cal_view->priv->model;
}

void
e_calendar_view_set_model (ECalendarView *cal_view, ECalModel *model)
{
	g_return_if_fail (E_IS_CALENDAR_VIEW (cal_view));
	g_return_if_fail (E_IS_CAL_MODEL (model));

	if (cal_view->priv->model) {
		g_signal_handlers_disconnect_matched (cal_view->priv->model, G_SIGNAL_MATCH_DATA,
						      0, 0, NULL, NULL, cal_view);
		g_object_unref (cal_view->priv->model);
	}

	cal_view->priv->model = g_object_ref (model);
	e_calendar_view_update_query (cal_view);
}

icaltimezone *
e_calendar_view_get_timezone (ECalendarView *cal_view)
{
	g_return_val_if_fail (E_IS_CALENDAR_VIEW (cal_view), NULL);
	return e_cal_model_get_timezone (cal_view->priv->model);
}

void
e_calendar_view_set_timezone (ECalendarView *cal_view, icaltimezone *zone)
{
	icaltimezone *old_zone;

	g_return_if_fail (E_IS_CALENDAR_VIEW (cal_view));

	old_zone = e_cal_model_get_timezone (cal_view->priv->model);
	if (old_zone == zone)
		return;

	e_cal_model_set_timezone (cal_view->priv->model, zone);
	g_signal_emit (G_OBJECT (cal_view), e_calendar_view_signals[TIMEZONE_CHANGED], 0,
		       old_zone, zone);
}

const char *
e_calendar_view_get_default_category (ECalendarView *cal_view)
{
	g_return_val_if_fail (E_IS_CALENDAR_VIEW (cal_view), NULL);
	return (const char *) cal_view->priv->default_category;
}

/**
 * e_calendar_view_set_default_category
 * @cal_view: A calendar view.
 * @category: Default category name or NULL for no category.
 *
 * Sets the default category that will be used when creating new calendar
 * components from the given calendar view.
 */
void
e_calendar_view_set_default_category (ECalendarView *cal_view, const char *category)
{
	g_return_if_fail (E_IS_CALENDAR_VIEW (cal_view));

	if (cal_view->priv->default_category)
		g_free (cal_view->priv->default_category);

	cal_view->priv->default_category = g_strdup (category);
}

/**
 * e_calendar_view_get_use_24_hour_format:
 * @cal_view: A calendar view.
 *
 * Gets whether the view is using 24 hour times or not.
 *
 * Returns: the 24 hour setting.
 */
gboolean
e_calendar_view_get_use_24_hour_format (ECalendarView *cal_view)
{
	g_return_val_if_fail (E_IS_CALENDAR_VIEW (cal_view), FALSE);

	return e_cal_model_get_use_24_hour_format (cal_view->priv->model);
}

/**
 * e_calendar_view_set_use_24_hour_format
 * @cal_view: A calendar view.
 * @use_24_hour: Whether to use 24 hour times or not.
 *
 * Sets the 12/24 hour times setting for the given view.
 */
void
e_calendar_view_set_use_24_hour_format (ECalendarView *cal_view, gboolean use_24_hour)
{
	g_return_if_fail (E_IS_CALENDAR_VIEW (cal_view));

	e_cal_model_set_use_24_hour_format (cal_view->priv->model, use_24_hour);
}

void
e_calendar_view_set_activity_handler (ECalendarView *cal_view, EActivityHandler *activity_handler)
{
	ECalendarViewPrivate *priv;

	g_return_if_fail (E_IS_CALENDAR_VIEW (cal_view));

	priv = cal_view->priv;

	priv->activity_handler = activity_handler;
}

void
e_calendar_view_set_status_message (ECalendarView *cal_view, const gchar *message, int percent)
{
	ECalendarViewPrivate *priv;

	g_return_if_fail (E_IS_CALENDAR_VIEW (cal_view));

	priv = cal_view->priv;

	if (!priv->activity_handler)
		return;

	if (!message || !*message) {
		if (priv->activity_id != 0) {
			e_activity_handler_operation_finished (priv->activity_handler, priv->activity_id);
			priv->activity_id = 0;
		}
	} else if (priv->activity_id == 0) {
		char *client_id = g_strdup_printf ("%p", cal_view);

		if (progress_icon == NULL)
			progress_icon = e_icon_factory_get_icon (EVOLUTION_CALENDAR_PROGRESS_IMAGE, E_ICON_SIZE_STATUS);

		priv->activity_id = e_activity_handler_operation_started (priv->activity_handler, client_id, progress_icon, message, TRUE);

		g_free (client_id);
	} else {
		double progress;

		if (percent < 0)
			progress = -1.0;
		else {
			progress = ((double) percent / 100);
		}

		e_activity_handler_operation_progressing (priv->activity_handler, priv->activity_id, message, progress);
	}
}

GList *
e_calendar_view_get_selected_events (ECalendarView *cal_view)
{
	g_return_val_if_fail (E_IS_CALENDAR_VIEW (cal_view), NULL);

	if (E_CALENDAR_VIEW_CLASS (G_OBJECT_GET_CLASS (cal_view))->get_selected_events)
		return E_CALENDAR_VIEW_CLASS (G_OBJECT_GET_CLASS (cal_view))->get_selected_events (cal_view);

	return NULL;
}

gboolean
e_calendar_view_get_selected_time_range (ECalendarView *cal_view, time_t *start_time, time_t *end_time)
{
	g_return_val_if_fail (E_IS_CALENDAR_VIEW (cal_view), FALSE);

	if (E_CALENDAR_VIEW_CLASS (G_OBJECT_GET_CLASS (cal_view))->get_selected_time_range) {
		return E_CALENDAR_VIEW_CLASS (G_OBJECT_GET_CLASS (cal_view))->get_selected_time_range (
			cal_view, start_time, end_time);
	}

	return FALSE;
}

void
e_calendar_view_set_selected_time_range (ECalendarView *cal_view, time_t start_time, time_t end_time)
{
	g_return_if_fail (E_IS_CALENDAR_VIEW (cal_view));

	if (E_CALENDAR_VIEW_CLASS (G_OBJECT_GET_CLASS (cal_view))->set_selected_time_range) {
		E_CALENDAR_VIEW_CLASS (G_OBJECT_GET_CLASS (cal_view))->set_selected_time_range (
			cal_view, start_time, end_time);
	}
}

gboolean
e_calendar_view_get_visible_time_range (ECalendarView *cal_view, time_t *start_time, time_t *end_time)
{
	g_return_val_if_fail (E_IS_CALENDAR_VIEW (cal_view), FALSE);

	if (E_CALENDAR_VIEW_CLASS (G_OBJECT_GET_CLASS (cal_view))->get_visible_time_range) {
		return E_CALENDAR_VIEW_CLASS (G_OBJECT_GET_CLASS (cal_view))->get_visible_time_range (
			cal_view, start_time, end_time);
	}

	return FALSE;
}

void
e_calendar_view_update_query (ECalendarView *cal_view)
{
	g_return_if_fail (E_IS_CALENDAR_VIEW (cal_view));

	if (E_CALENDAR_VIEW_CLASS (G_OBJECT_GET_CLASS (cal_view))->update_query) {
		E_CALENDAR_VIEW_CLASS (G_OBJECT_GET_CLASS (cal_view))->update_query (cal_view);
	}
}

void
e_calendar_view_cut_clipboard (ECalendarView *cal_view)
{
	GList *selected, *l;
	const char *uid;

	g_return_if_fail (E_IS_CALENDAR_VIEW (cal_view));

	selected = e_calendar_view_get_selected_events (cal_view);
	if (!selected)
		return;

	e_calendar_view_set_status_message (cal_view, _("Deleting selected objects"), -1);

	e_calendar_view_copy_clipboard (cal_view);
	for (l = selected; l != NULL; l = l->next) {
		ECalComponent *comp;
		ECalendarViewEvent *event = (ECalendarViewEvent *) l->data;
		GError *error = NULL;

		if (!event)
			continue;

		comp = e_cal_component_new ();
		e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (event->comp_data->icalcomp));

		if ((itip_organizer_is_user (comp, event->comp_data->client) || itip_sentby_is_user (comp))
		    && cancel_component_dialog ((GtkWindow *) gtk_widget_get_toplevel (GTK_WIDGET (cal_view)),
						event->comp_data->client, comp, TRUE))
			itip_send_comp (E_CAL_COMPONENT_METHOD_CANCEL, comp,
					event->comp_data->client, NULL, NULL, NULL);

		e_cal_component_get_uid (comp, &uid);
		if (e_cal_component_is_instance (comp)) {
			char *rid = NULL;
			icalcomponent *icalcomp;

			/* when cutting detached instances, only cut that instance */
			rid = e_cal_component_get_recurid_as_string (comp);
			if (e_cal_get_object (event->comp_data->client, uid, rid, &icalcomp, NULL)) {
				e_cal_remove_object_with_mod (event->comp_data->client, uid,
							      rid, CALOBJ_MOD_THIS,
							      &error);
				icalcomponent_free (icalcomp);
			} else
				e_cal_remove_object_with_mod (event->comp_data->client, uid, NULL,
						CALOBJ_MOD_ALL, &error);
			g_free (rid);
		} else
			e_cal_remove_object (event->comp_data->client, uid, &error);
		delete_error_dialog (error, E_CAL_COMPONENT_EVENT);

		g_clear_error (&error);

		g_object_unref (comp);
	}

	e_calendar_view_set_status_message (cal_view, NULL, -1);

	g_list_free (selected);
}

void
e_calendar_view_copy_clipboard (ECalendarView *cal_view)
{
	GList *selected, *l;
	gchar *comp_str;
	icalcomponent *vcal_comp;
	icalcomponent *new_icalcomp;
	ECalendarViewEvent *event;

	g_return_if_fail (E_IS_CALENDAR_VIEW (cal_view));

	selected = e_calendar_view_get_selected_events (cal_view);
	if (!selected)
		return;

	/* create top-level VCALENDAR component and add VTIMEZONE's */
	vcal_comp = e_cal_util_new_top_level ();
	for (l = selected; l != NULL; l = l->next) {
		event = (ECalendarViewEvent *) l->data;

		if (event)
			e_cal_util_add_timezones_from_component (vcal_comp, event->comp_data->icalcomp);
	}

	for (l = selected; l != NULL; l = l->next) {
		event = (ECalendarViewEvent *) l->data;

		new_icalcomp = icalcomponent_new_clone (event->comp_data->icalcomp);

		/* remove RECURRENCE-IDs from copied objects */
		if (e_cal_util_component_is_instance (new_icalcomp)) {
			icalproperty *prop;

			prop = icalcomponent_get_first_property (new_icalcomp, ICAL_RECURRENCEID_PROPERTY);
			if (prop)
				icalcomponent_remove_property (new_icalcomp, prop);
		}
		icalcomponent_add_component (vcal_comp, new_icalcomp);
	}

	/* copy the VCALENDAR to the clipboard */
	comp_str = icalcomponent_as_ical_string (vcal_comp);
	gtk_clipboard_set_text (gtk_widget_get_clipboard (GTK_WIDGET (cal_view), clipboard_atom),
				(const gchar *) comp_str,
				strlen (comp_str));

	/* free memory */
	icalcomponent_free (vcal_comp);
	g_free (comp_str);
	g_list_free (selected);
}

static void
clipboard_get_text_cb (GtkClipboard *clipboard, const gchar *text, ECalendarView *cal_view)
{
	icalcomponent *icalcomp;
	icalcomponent_kind kind;
	time_t selected_time_start, selected_time_end;
	icaltimezone *default_zone;
	ECal *client;
	gboolean in_top_canvas;

	g_return_if_fail (E_IS_CALENDAR_VIEW (cal_view));

	if (!text || !*text)
		return;

	icalcomp = icalparser_parse_string ((const char *) text);
	if (!icalcomp)
		return;

	default_zone = calendar_config_get_icaltimezone ();
	client = e_cal_model_get_default_client (cal_view->priv->model);

	/* check the type of the component */
	/* FIXME An error dialog if we return? */
	kind = icalcomponent_isa (icalcomp);
	if (kind != ICAL_VCALENDAR_COMPONENT && kind != ICAL_VEVENT_COMPONENT)
		return;

	e_calendar_view_set_status_message (cal_view, _("Updating objects"), -1);
	e_calendar_view_get_selected_time_range (cal_view, &selected_time_start, &selected_time_end);

	if ((selected_time_end - selected_time_start) == 60 * 60 * 24)
		in_top_canvas = TRUE;
	else
		in_top_canvas = FALSE;

	/* FIXME Timezone handling */
	if (kind == ICAL_VCALENDAR_COMPONENT) {
		icalcomponent_kind child_kind;
		icalcomponent *subcomp;

		subcomp = icalcomponent_get_first_component (icalcomp, ICAL_ANY_COMPONENT);
		while (subcomp) {
			child_kind = icalcomponent_isa (subcomp);
			if (child_kind == ICAL_VEVENT_COMPONENT) {

				if (e_cal_util_component_has_recurrences (subcomp)) {
					icalproperty *icalprop = icalcomponent_get_first_property (subcomp, ICAL_RRULE_PROPERTY);
					if (icalprop)
						icalproperty_remove_parameter_by_name (icalprop, "X-EVOLUTION-ENDDATE");
				}

				e_calendar_view_add_event (cal_view, client, selected_time_start,
							   default_zone, subcomp, in_top_canvas);
			} else if (child_kind == ICAL_VTIMEZONE_COMPONENT) {
				icaltimezone *zone;

				zone = icaltimezone_new ();
				icaltimezone_set_component (zone, subcomp);
				e_cal_add_timezone (client, zone, NULL);

				icaltimezone_free (zone, 1);
			}

			subcomp = icalcomponent_get_next_component (
				icalcomp, ICAL_ANY_COMPONENT);
		}

		icalcomponent_free (icalcomp);

	} else {
		e_calendar_view_add_event (cal_view, client, selected_time_start, default_zone, icalcomp, in_top_canvas);
	}

	e_calendar_view_set_status_message (cal_view, NULL, -1);
}

void
e_calendar_view_paste_clipboard (ECalendarView *cal_view)
{
	g_return_if_fail (E_IS_CALENDAR_VIEW (cal_view));

	gtk_clipboard_request_text (gtk_widget_get_clipboard (GTK_WIDGET (cal_view), clipboard_atom),
				    (GtkClipboardTextReceivedFunc) clipboard_get_text_cb, cal_view);
}

static void
add_retract_data (ECalComponent *comp, const char *retract_comment, CalObjModType mod)
{
	icalcomponent *icalcomp = NULL;
	icalproperty *icalprop = NULL;

	icalcomp = e_cal_component_get_icalcomponent (comp);
	if (retract_comment && *retract_comment)
		icalprop = icalproperty_new_x (retract_comment);
	else
		icalprop = icalproperty_new_x ("0");
	icalproperty_set_x_name (icalprop, "X-EVOLUTION-RETRACT-COMMENT");
	icalcomponent_add_property (icalcomp, icalprop);

	if (mod == CALOBJ_MOD_ALL)
		icalprop = icalproperty_new_x ("All");
	else
		icalprop = icalproperty_new_x ("This");
	icalproperty_set_x_name (icalprop, "X-EVOLUTION-RECUR-MOD");
	icalcomponent_add_property (icalcomp, icalprop);
}

static gboolean
check_for_retract (ECalComponent *comp, ECal *client)
{
	ECalComponentOrganizer org;
	char *email = NULL;
	const char *strip = NULL;
	gboolean ret_val = FALSE;

	if (!(e_cal_component_has_attendees (comp) &&
				e_cal_get_save_schedules (client)))
		return ret_val;

	e_cal_component_get_organizer (comp, &org);
	strip = itip_strip_mailto (org.value);

	if (e_cal_get_cal_address (client, &email, NULL) && !g_ascii_strcasecmp (email, strip)) {
		ret_val = TRUE;
	}

	if (!ret_val)
		ret_val = e_account_list_find(itip_addresses_get(), E_ACCOUNT_FIND_ID_ADDRESS, strip) != NULL;

	g_free (email);
	return ret_val;
}

static void
delete_event (ECalendarView *cal_view, ECalendarViewEvent *event)
{
	ECalComponent *comp;
	ECalComponentVType vtype;
	gboolean  delete = FALSE;
	GError *error = NULL;

	comp = e_cal_component_new ();
	e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (event->comp_data->icalcomp));
	vtype = e_cal_component_get_vtype (comp);

	/*FIXME remove it once the we dont set the recurrence id for all the generated instances */
	if (!e_cal_get_static_capability (event->comp_data->client, CAL_STATIC_CAPABILITY_RECURRENCES_NO_MASTER))
		e_cal_component_set_recurid (comp, NULL);

	if (check_for_retract (comp, event->comp_data->client)) {
		char *retract_comment = NULL;
		gboolean retract = FALSE;

		retract = prompt_retract_dialog (comp, &retract_comment, GTK_WIDGET (cal_view));
		if (retract) {
			GList *users = NULL;
			icalcomponent *icalcomp = NULL, *mod_comp = NULL;

			add_retract_data (comp, retract_comment, CALOBJ_MOD_ALL);
			icalcomp = e_cal_component_get_icalcomponent (comp);
			icalcomponent_set_method (icalcomp, ICAL_METHOD_CANCEL);
			if (!e_cal_send_objects (event->comp_data->client, icalcomp, &users,
						&mod_comp, &error))	{
				delete_error_dialog (error, E_CAL_COMPONENT_EVENT);
				g_clear_error (&error);
				error = NULL;
			} else {

				if (mod_comp)
					icalcomponent_free (mod_comp);

				if (users) {
					g_list_foreach (users, (GFunc) g_free, NULL);
					g_list_free (users);
				}
			}
		}
	} else
		delete = delete_component_dialog (comp, FALSE, 1, vtype, GTK_WIDGET (cal_view));

	if (delete) {
		const char *uid;
		char *rid = NULL;

		if ((itip_organizer_is_user (comp, event->comp_data->client) || itip_sentby_is_user (comp))
		    && cancel_component_dialog ((GtkWindow *) gtk_widget_get_toplevel (GTK_WIDGET (cal_view)),
						event->comp_data->client,
						comp, TRUE))
			itip_send_comp (E_CAL_COMPONENT_METHOD_CANCEL, comp,
					event->comp_data->client, NULL, NULL, NULL);

		e_cal_component_get_uid (comp, &uid);
		if (!uid || !*uid) {
			g_object_unref (comp);
			return;
		}
		rid = e_cal_component_get_recurid_as_string (comp);
		if (e_cal_util_component_is_instance (event->comp_data->icalcomp) || e_cal_util_component_has_recurrences (event->comp_data->icalcomp))
			e_cal_remove_object_with_mod (event->comp_data->client, uid,
				rid, CALOBJ_MOD_ALL, &error);
		else
			e_cal_remove_object (event->comp_data->client, uid, &error);

		delete_error_dialog (error, E_CAL_COMPONENT_EVENT);
		g_clear_error (&error);
		g_free (rid);
	}

	g_object_unref (comp);
}

void
e_calendar_view_delete_selected_event (ECalendarView *cal_view)
{
	GList *selected;
	ECalendarViewEvent *event;

	selected = e_calendar_view_get_selected_events (cal_view);
	if (!selected)
		return;

	event = (ECalendarViewEvent *) selected->data;
	if (event)
		delete_event (cal_view, event);

	g_list_free (selected);
}

void
e_calendar_view_delete_selected_events (ECalendarView *cal_view)
{
	GList *selected, *l;
	ECalendarViewEvent *event;

	selected = e_calendar_view_get_selected_events (cal_view);
	if (!selected)
		return;

	for (l = selected; l != NULL; l = l->next) {
		event = (ECalendarViewEvent *) l->data;
		if (event)
			delete_event (cal_view, event);
	}

	g_list_free (selected);
}

void
e_calendar_view_delete_selected_occurrence (ECalendarView *cal_view)
{
	GList *selected;
	ECalComponent *comp;
	ECalendarViewEvent *event;
	ECalComponentVType vtype;
	gboolean  delete = FALSE;
	GError *error = NULL;

	selected = e_calendar_view_get_selected_events (cal_view);
	if (!selected)
		return;
	event = (ECalendarViewEvent *) selected->data;
	comp = e_cal_component_new ();
	e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (event->comp_data->icalcomp));
	vtype = e_cal_component_get_vtype (comp);

	if (check_for_retract (comp, event->comp_data->client)) {
		char *retract_comment = NULL;
		gboolean retract = FALSE;

		retract = prompt_retract_dialog (comp, &retract_comment, GTK_WIDGET (cal_view));
		if (retract) {
			GList *users = NULL;
			icalcomponent *icalcomp = NULL, *mod_comp = NULL;

			add_retract_data (comp, retract_comment, CALOBJ_MOD_THIS);
			icalcomp = e_cal_component_get_icalcomponent (comp);
			icalcomponent_set_method (icalcomp, ICAL_METHOD_CANCEL);
			if (!e_cal_send_objects (event->comp_data->client, icalcomp, &users,
						&mod_comp, &error))	{
				delete_error_dialog (error, E_CAL_COMPONENT_EVENT);
				g_clear_error (&error);
				error = NULL;
			} else {
				if (mod_comp)
					icalcomponent_free (mod_comp);
				if (users) {
					g_list_foreach (users, (GFunc) g_free, NULL);
					g_list_free (users);
				}
			}
		}
	} else
		delete = delete_component_dialog (comp, FALSE, 1, vtype, GTK_WIDGET (cal_view));

	if (delete) {
		const char *uid;
		char *rid = NULL;
		ECalComponentDateTime dt;
		icaltimezone *zone = NULL;
		gboolean is_instance = FALSE;

		e_cal_component_get_uid (comp, &uid);
		e_cal_component_get_dtstart (comp, &dt);
		is_instance = e_cal_component_is_instance (comp);

		if (dt.tzid) {
			GError *error = NULL;

			e_cal_get_timezone (event->comp_data->client, dt.tzid, &zone, &error);
			if (error) {
				zone = e_calendar_view_get_timezone (cal_view);
				g_clear_error(&error);
			}
		} else
			zone = e_calendar_view_get_timezone (cal_view);


		if (is_instance)
			rid = e_cal_component_get_recurid_as_string (comp);

		e_cal_component_free_datetime (&dt);


		if ((itip_organizer_is_user (comp, event->comp_data->client) || itip_sentby_is_user (comp))
				&& cancel_component_dialog ((GtkWindow *) gtk_widget_get_toplevel (GTK_WIDGET (cal_view)),
					event->comp_data->client,
					comp, TRUE) && !e_cal_get_save_schedules (event->comp_data->client)) {
			if (!e_cal_component_is_instance (comp)) {
				ECalComponentRange range;

				/* set the recurrence ID of the object we send */
				range.type = E_CAL_COMPONENT_RANGE_SINGLE;
				e_cal_component_get_dtstart (comp, &range.datetime);
				range.datetime.value->is_date = 1;
				e_cal_component_set_recurid (comp, &range);

				e_cal_component_free_datetime (&range.datetime);
			}
			itip_send_comp (E_CAL_COMPONENT_METHOD_CANCEL, comp, event->comp_data->client, NULL, NULL, NULL);
		}

		if (is_instance)
			e_cal_remove_object_with_mod (event->comp_data->client, uid, rid, CALOBJ_MOD_THIS, &error);
		else {
			struct icaltimetype instance_rid;

			instance_rid = icaltime_from_timet_with_zone (event->comp_data->instance_start,
					TRUE, zone ? zone : icaltimezone_get_utc_timezone ());
			e_cal_util_remove_instances (event->comp_data->icalcomp, instance_rid, CALOBJ_MOD_THIS);
			e_cal_modify_object (event->comp_data->client, event->comp_data->icalcomp, CALOBJ_MOD_THIS,
				       	&error);
		}

		delete_error_dialog (error, E_CAL_COMPONENT_EVENT);
		g_clear_error (&error);
		g_free (rid);
	}

	/* free memory */
	g_list_free (selected);
	g_object_unref (comp);
}

static void
on_new_appointment (EPopup *ep, EPopupItem *pitem, void *data)
{
	ECalendarView *cal_view = data;

	e_calendar_view_new_appointment (cal_view);
}

static void
on_new_event (EPopup *ep, EPopupItem *pitem, void *data)
{
	ECalendarView *cal_view = data;

	e_calendar_view_new_appointment_full (cal_view, TRUE, FALSE, FALSE);
}

static void
on_new_meeting (EPopup *ep, EPopupItem *pitem, void *data)
{
	ECalendarView *cal_view = data;

	e_calendar_view_new_appointment_full (cal_view, FALSE, TRUE, FALSE);
}

static void
on_new_task (EPopup *ep, EPopupItem *pitem, void *data)
{
	ECalendarView *cal_view = data;
	time_t dtstart, dtend;

	e_calendar_view_get_selected_time_range (cal_view, &dtstart, &dtend);
	gnome_calendar_new_task (cal_view->priv->calendar, &dtstart, &dtend);
}

static void
on_goto_date (EPopup *ep, EPopupItem *pitem, void *data)
{
	ECalendarView *cal_view = data;

	goto_dialog (cal_view->priv->calendar);
}

static void
on_goto_today (EPopup *ep, EPopupItem *pitem, void *data)
{
	ECalendarView *cal_view = data;

	calendar_goto_today (cal_view->priv->calendar);
}

static void
on_edit_appointment (EPopup *ep, EPopupItem *pitem, void *data)
{
	ECalendarView *cal_view = data;
	GList *selected;

	selected = e_calendar_view_get_selected_events (cal_view);
	if (selected) {
		ECalendarViewEvent *event = (ECalendarViewEvent *) selected->data;

		if (event)
			e_calendar_view_edit_appointment (cal_view, event->comp_data->client,
						     event->comp_data->icalcomp,
						     icalcomponent_get_first_property(event->comp_data->icalcomp, ICAL_ATTENDEE_PROPERTY) != NULL);

		g_list_free (selected);
	}
}

static void
on_print (EPopup *ep, EPopupItem *pitem, void *data)
{
	ECalendarView *cal_view = data;

	calendar_command_print (cal_view->priv->calendar, GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG);
}

static void
on_save_as (EPopup *ep, EPopupItem *pitem, void *data)
{
	ECalendarView *cal_view = data;
	GList *selected;
	char *filename;
	char *ical_string;
	ECalendarViewEvent *event;

	selected = e_calendar_view_get_selected_events (cal_view);
	if (!selected)
		return;

	filename = e_file_dialog_save (_("Save as..."), NULL);
	if (filename == NULL)
		return;

	event = (ECalendarViewEvent *) selected->data;
	ical_string = e_cal_get_component_as_string (event->comp_data->client, event->comp_data->icalcomp);
	if (ical_string == NULL) {
		g_warning ("Couldn't convert item to a string");
		return;
	}

	e_write_file_uri (filename, ical_string);
	g_free (ical_string);

	g_list_free (selected);
}

static void
on_print_event (EPopup *ep, EPopupItem *pitem, void *data)
{
	ECalendarView *cal_view = data;
	GList *selected;
	ECalendarViewEvent *event;
	ECalComponent *comp;

	selected = e_calendar_view_get_selected_events (cal_view);
	if (!selected)
		return;

	event = (ECalendarViewEvent *) selected->data;

	comp = e_cal_component_new ();
	e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (event->comp_data->icalcomp));
	print_comp (comp, event->comp_data->client, GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG);

	g_object_unref (comp);
	g_list_free (selected);
}

static void
transfer_item_to (ECalendarViewEvent *event, ECal *dest_client, gboolean remove_item)
{
	const char *uid;
	char *new_uid;
	icalcomponent *orig_icalcomp;
	icalproperty *icalprop;

	uid = icalcomponent_get_uid (event->comp_data->icalcomp);

	/* put the new object into the destination calendar */
	if (e_cal_get_object (dest_client, uid, NULL, &orig_icalcomp, NULL)) {
		icalcomponent_free (orig_icalcomp);


		if (!e_cal_modify_object (dest_client, event->comp_data->icalcomp, CALOBJ_MOD_ALL, NULL))
			return;
	} else {
		orig_icalcomp = icalcomponent_new_clone (event->comp_data->icalcomp);

		icalprop = icalproperty_new_x ("1");
		icalproperty_set_x_name (icalprop, "X-EVOLUTION-MOVE-CALENDAR");
		icalcomponent_add_property (orig_icalcomp, icalprop);

		if (!remove_item) {
			/* change the UID to avoid problems with duplicated UIDs */
			new_uid = e_cal_component_gen_uid ();
			icalcomponent_set_uid (orig_icalcomp, new_uid);

			g_free (new_uid);
		}

		new_uid = NULL;
		if (!e_cal_create_object (dest_client, orig_icalcomp, &new_uid, NULL)) {
			icalcomponent_free (orig_icalcomp);
			return;
		}

		if (new_uid)
			g_free (new_uid);
		icalcomponent_free (orig_icalcomp);
	}

	/* remove the item from the source calendar */
	if (remove_item) {
		if (e_cal_util_component_is_instance (event->comp_data->icalcomp) || e_cal_util_component_is_instance (event->comp_data->icalcomp))
			e_cal_remove_object_with_mod (event->comp_data->client, uid,
					NULL, CALOBJ_MOD_ALL, NULL);
		else
			e_cal_remove_object (event->comp_data->client, uid, NULL);
	}
}

static void
transfer_selected_items (ECalendarView *cal_view, gboolean remove_item)
{
	GList *selected, *l;
	ESource *destination_source;
	ECal *dest_client;

	selected = e_calendar_view_get_selected_events (cal_view);
	if (!selected)
		return;

	/* prompt the user for destination source */
	destination_source = select_source_dialog ((GtkWindow *) gtk_widget_get_toplevel ((GtkWidget *)cal_view), E_CAL_SOURCE_TYPE_EVENT);
	if (!destination_source)
		return;

	/* open the destination calendar */
	dest_client = auth_new_cal_from_source (destination_source, E_CAL_SOURCE_TYPE_EVENT);
	if (!dest_client || !e_cal_open (dest_client, FALSE, NULL)) {
		if (dest_client)
			g_object_unref (dest_client);
		g_object_unref (destination_source);
		return;
	}

	/* process all selected events */
	if (remove_item)
		e_calendar_view_set_status_message (cal_view, _("Moving items"), -1);
	else
		e_calendar_view_set_status_message (cal_view, _("Copying items"), -1);

	for (l = selected; l != NULL; l = l->next)
		transfer_item_to ((ECalendarViewEvent *) l->data, dest_client, remove_item);

	e_calendar_view_set_status_message (cal_view, NULL, -1);

	/* free memory */
	g_object_unref (destination_source);
	g_object_unref (dest_client);
	g_list_free (selected);
}

static void
on_copy_to (EPopup *ep, EPopupItem *pitem, void *data)
{
	ECalendarView *cal_view = data;

	transfer_selected_items (cal_view, FALSE);
}

static void
on_move_to (EPopup *ep, EPopupItem *pitem, void *data)
{
	ECalendarView *cal_view = data;

	transfer_selected_items (cal_view, TRUE);
}

static void
on_meeting (EPopup *ep, EPopupItem *pitem, void *data)
{
	ECalendarView *cal_view = data;
	GList *selected;

	selected = e_calendar_view_get_selected_events (cal_view);
	if (selected) {
		ECalendarViewEvent *event = (ECalendarViewEvent *) selected->data;
		e_calendar_view_edit_appointment (cal_view, event->comp_data->client, event->comp_data->icalcomp, TRUE);

		g_list_free (selected);
	}
}

static void
set_attendee_status_for_delegate (icalcomponent *icalcomp, ECal *client)
{
	icalproperty *prop;
	icalparameter *param;
	char *address = NULL;
	ECalComponent *comp;
	gboolean found = FALSE;

	comp = e_cal_component_new ();
	e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (icalcomp));

	address = itip_get_comp_attendee (comp, client);


	for (prop = icalcomponent_get_first_property (icalcomp, ICAL_ATTENDEE_PROPERTY);
			prop;
			prop = icalcomponent_get_next_property (icalcomp, ICAL_ATTENDEE_PROPERTY)) {
		const char *attendee = icalproperty_get_attendee (prop);

		if (!g_ascii_strcasecmp (itip_strip_mailto (attendee), address)) {
			param = icalparameter_new_role (ICAL_ROLE_NONPARTICIPANT);
			icalproperty_set_parameter (prop, param);

			param = icalparameter_new_partstat (ICAL_PARTSTAT_DELEGATED);
			icalproperty_set_parameter (prop, param);

			found = TRUE;
			break;
		}

	}

	/* We couldn find the attendee in the component, so add a new attendee */
	if (!found) {
		char *temp = g_strdup_printf ("MAILTO:%s", address);

		prop = icalproperty_new_attendee ((const char *) temp);
		icalcomponent_add_property (icalcomp, prop);

		param = icalparameter_new_partstat (ICAL_PARTSTAT_DELEGATED);
		icalproperty_add_parameter (prop, param);

		param = icalparameter_new_role (ICAL_ROLE_NONPARTICIPANT);
		icalproperty_add_parameter (prop, param);

		param = icalparameter_new_cutype (ICAL_CUTYPE_INDIVIDUAL);
		icalproperty_add_parameter (prop, param);

		param = icalparameter_new_rsvp (ICAL_RSVP_TRUE);
		icalproperty_add_parameter (prop, param);

		g_free (temp);
	}


	g_free (address);
	g_object_unref (comp);
}

static void
on_delegate (EPopup *ep, EPopupItem *pitem, void *data)
{
	ECalendarView *cal_view = data;
	GList *selected;
	guint32 flags = 0;
	icalcomponent *clone;

	selected = e_calendar_view_get_selected_events (cal_view);
	if (selected) {
		ECalendarViewEvent *event = (ECalendarViewEvent *) selected->data;

		clone = icalcomponent_new_clone (event->comp_data->icalcomp);
		set_attendee_status_for_delegate (clone, event->comp_data->client);

		flags |= COMP_EDITOR_MEETING | COMP_EDITOR_DELEGATE;

		open_event_with_flags (cal_view, event->comp_data->client, clone, flags);

		icalcomponent_free (clone);
		g_list_free (selected);
	}
}

static void
on_forward (EPopup *ep, EPopupItem *pitem, void *data)
{
	ECalendarView *cal_view = data;
	GList *selected;

	selected = e_calendar_view_get_selected_events (cal_view);
	if (selected) {
		ECalComponent *comp;
		ECalendarViewEvent *event = (ECalendarViewEvent *) selected->data;

		comp = e_cal_component_new ();
		e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (event->comp_data->icalcomp));
		itip_send_comp (E_CAL_COMPONENT_METHOD_PUBLISH, comp, event->comp_data->client, NULL, NULL, NULL);

		g_list_free (selected);
		g_object_unref (comp);
	}
}

static void
on_reply (EPopup *ep, EPopupItem *pitem, void *data)
{
	ECalendarView *cal_view = data;
	GList *selected;
	gboolean reply_all = FALSE;

	selected = e_calendar_view_get_selected_events (cal_view);
	if (selected) {
		ECalComponent *comp;
		ECalendarViewEvent *event = (ECalendarViewEvent *) selected->data;

		comp = e_cal_component_new ();
		e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (event->comp_data->icalcomp));
		reply_to_calendar_comp (E_CAL_COMPONENT_METHOD_REPLY, comp, event->comp_data->client, reply_all, NULL, NULL);

		g_list_free (selected);
		g_object_unref (comp);
	}
}

static void
on_reply_all (EPopup *ep, EPopupItem *pitem, void *data)
{
	ECalendarView *cal_view = data;
	GList *selected;
	gboolean reply_all = TRUE;

	selected = e_calendar_view_get_selected_events (cal_view);
	if (selected) {
		ECalComponent *comp;
		ECalendarViewEvent *event = (ECalendarViewEvent *) selected->data;

		comp = e_cal_component_new ();
		e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (event->comp_data->icalcomp));
		reply_to_calendar_comp (E_CAL_COMPONENT_METHOD_REPLY, comp, event->comp_data->client, reply_all, NULL, NULL);

		g_list_free (selected);
		g_object_unref (comp);
	}
}

static void
on_delete_appointment (EPopup *ep, EPopupItem *pitem, void *data)
{
	ECalendarView *cal_view = data;

	e_calendar_view_delete_selected_event (cal_view);
}

static void
on_unrecur_appointment (EPopup *ep, EPopupItem *pitem, void *data)
{
	ECalendarView *cal_view = data;
	ECalendarViewEvent *event;
	ECalComponent *comp, *new_comp;
	ECalComponentDateTime date;
	struct icaltimetype itt;
	GList *selected;
	ECal *client;
	char *new_uid;
	ECalComponentId *id = NULL;

	selected = e_calendar_view_get_selected_events (cal_view);
	if (!selected)
		return;

	event = (ECalendarViewEvent *) selected->data;
	client = g_object_ref (event->comp_data->client);

	date.value = &itt;
	date.tzid = NULL;

	/* For the recurring object, we add an exception to get rid of the
	   instance. */

	comp = e_cal_component_new ();
	e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (event->comp_data->icalcomp));
	id = e_cal_component_get_id (comp);


	/* For the unrecurred instance we duplicate the original object,
	   create a new uid for it, get rid of the recurrence rules, and set
	   the start & end times to the instances times. */
	new_comp = e_cal_component_new ();
	e_cal_component_set_icalcomponent (new_comp, icalcomponent_new_clone (event->comp_data->icalcomp));

	new_uid = e_cal_component_gen_uid ();
	e_cal_component_set_uid (new_comp, new_uid);
	g_free (new_uid);
	e_cal_component_set_recurid (new_comp, NULL);
	e_cal_component_set_rdate_list (new_comp, NULL);
	e_cal_component_set_rrule_list (new_comp, NULL);
	e_cal_component_set_exdate_list (new_comp, NULL);
	e_cal_component_set_exrule_list (new_comp, NULL);

	date.value = &itt;
	date.tzid = icaltimezone_get_tzid (e_calendar_view_get_timezone (cal_view));

	*date.value = icaltime_from_timet_with_zone (event->comp_data->instance_start, FALSE,
						     e_calendar_view_get_timezone (cal_view));
	e_cal_component_set_dtstart (new_comp, &date);
	*date.value = icaltime_from_timet_with_zone (event->comp_data->instance_end, FALSE,
						     e_calendar_view_get_timezone (cal_view));
	e_cal_component_set_dtend (new_comp, &date);
	e_cal_component_commit_sequence (new_comp);

	/* Now update both ECalComponents. Note that we do this last since at
	 * present the updates happen synchronously so our event may disappear.
	 */

	if (!e_cal_remove_object_with_mod (client, id->uid, id->rid, CALOBJ_MOD_THIS,
				NULL))
		g_message ("on_unrecur_appointment(): Could not remove the old object!");

	e_cal_component_free_id (id);
	g_object_unref (comp);

	if (!e_cal_create_object (client, e_cal_component_get_icalcomponent (new_comp), &new_uid, NULL))
		g_message ("on_unrecur_appointment(): Could not update the object!");
	else
		g_free (new_uid);

	g_object_unref (new_comp);
	g_object_unref (client);
	g_list_free (selected);
}

static void
on_delete_occurrence (EPopup *ep, EPopupItem *pitem, void *data)
{
	ECalendarView *cal_view = data;

	e_calendar_view_delete_selected_occurrence (cal_view);
}

static void
on_cut (EPopup *ep, EPopupItem *pitem, void *data)
{
	ECalendarView *cal_view = data;

	e_calendar_view_cut_clipboard (cal_view);
}

static void
on_copy (EPopup *ep, EPopupItem *pitem, void *data)
{
	ECalendarView *cal_view = data;

	e_calendar_view_copy_clipboard (cal_view);
}

static void
on_paste (EPopup *ep, EPopupItem *pitem, void *data)
{
	ECalendarView *cal_view = data;

	e_calendar_view_paste_clipboard (cal_view);
}

static EPopupItem ecv_main_items [] = {
	{ E_POPUP_ITEM, "00.new", N_("New _Appointment..."), on_new_appointment, NULL, "appointment-new", 0, 0 },
	{ E_POPUP_ITEM, "10.newallday", N_("New All Day _Event"), on_new_event, NULL, "stock_new-24h-appointment", 0, 0},
	{ E_POPUP_ITEM, "20.meeting", N_("New _Meeting"), on_new_meeting, NULL, "stock_new-meeting", 0, 0},
	{ E_POPUP_ITEM, "30.task", N_("New _Task"), on_new_task, NULL, "stock_task", 0, 0},

	{ E_POPUP_BAR, "40."},
	{ E_POPUP_ITEM, "40.print", N_("P_rint..."), on_print, NULL, GTK_STOCK_PRINT, 0, 0 },

	{ E_POPUP_BAR, "50." },
	{ E_POPUP_ITEM, "50.paste", N_("_Paste"), on_paste, NULL, GTK_STOCK_PASTE, 0, E_CAL_POPUP_SELECT_EDITABLE },

	{ E_POPUP_BAR, "60." },
	/* FIXME: hook in this somehow */
	{ E_POPUP_SUBMENU, "60.view", N_("_Current View") },

	{ E_POPUP_ITEM, "61.today", N_("Select T_oday"), on_goto_today, NULL, "go-today" },
	{ E_POPUP_ITEM, "62.todate", N_("_Select Date..."), on_goto_date, NULL, GTK_STOCK_JUMP_TO },
};

static EPopupItem ecv_child_items [] = {
	{ E_POPUP_ITEM, "00.open", N_("_Open"), on_edit_appointment, NULL, GTK_STOCK_OPEN, 0, E_CAL_POPUP_SELECT_NOTEDITING },
	{ E_POPUP_ITEM, "10.saveas", N_("_Save As..."), on_save_as, NULL, GTK_STOCK_SAVE_AS, 0, E_CAL_POPUP_SELECT_NOTEDITING },
	{ E_POPUP_ITEM, "20.print", N_("Pri_nt..."), on_print_event, NULL, GTK_STOCK_PRINT, 0, E_CAL_POPUP_SELECT_NOTEDITING },

	{ E_POPUP_BAR, "30." },

	{ E_POPUP_ITEM, "31.cut", N_("C_ut"), on_cut, NULL, GTK_STOCK_CUT, 0, E_CAL_POPUP_SELECT_NOTEDITING|E_CAL_POPUP_SELECT_EDITABLE|E_CAL_POPUP_SELECT_ORGANIZER },
	{ E_POPUP_ITEM, "32.copy", N_("_Copy"), on_copy, NULL, GTK_STOCK_COPY, 0, E_CAL_POPUP_SELECT_NOTEDITING|E_CAL_POPUP_SELECT_ORGANIZER },
	{ E_POPUP_ITEM, "33.paste", N_("_Paste"), on_paste, NULL, GTK_STOCK_PASTE, 0, E_CAL_POPUP_SELECT_EDITABLE },

	{ E_POPUP_BAR, "40." },

	{ E_POPUP_ITEM, "43.copyto", N_("Cop_y to Calendar..."), on_copy_to, NULL, NULL, 0, E_CAL_POPUP_SELECT_NOTEDITING },
	{ E_POPUP_ITEM, "44.moveto", N_("Mo_ve to Calendar..."), on_move_to, NULL, NULL, 0, E_CAL_POPUP_SELECT_NOTEDITING | E_CAL_POPUP_SELECT_EDITABLE },
	{ E_POPUP_ITEM, "45.delegate", N_("_Delegate Meeting..."), on_delegate, NULL, NULL, 0, E_CAL_POPUP_SELECT_NOTEDITING | E_CAL_POPUP_SELECT_EDITABLE | E_CAL_POPUP_SELECT_DELEGATABLE | E_CAL_POPUP_SELECT_MEETING},
	{ E_POPUP_ITEM, "46.schedule", N_("_Schedule Meeting..."), on_meeting, NULL, NULL, 0, E_CAL_POPUP_SELECT_NOTEDITING | E_CAL_POPUP_SELECT_EDITABLE | E_CAL_POPUP_SELECT_NOTMEETING },
	{ E_POPUP_ITEM, "47.forward", N_("_Forward as iCalendar..."), on_forward, NULL, "mail-forward", 0, E_CAL_POPUP_SELECT_NOTEDITING },
	{ E_POPUP_ITEM, "48.reply", N_("_Reply"), on_reply, NULL, "mail-reply-sender", E_CAL_POPUP_SELECT_MEETING | E_CAL_POPUP_SELECT_NOSAVESCHEDULES, E_CAL_POPUP_SELECT_NOTEDITING },
	{ E_POPUP_ITEM, "49.reply-all", N_("Reply to _All"), on_reply_all, NULL, "mail-reply-all", E_CAL_POPUP_SELECT_MEETING | E_CAL_POPUP_SELECT_NOSAVESCHEDULES, E_CAL_POPUP_SELECT_NOTEDITING },

	{ E_POPUP_BAR, "50." },

	{ E_POPUP_ITEM, "51.delete", N_("_Delete"), on_delete_appointment, NULL, GTK_STOCK_DELETE, E_CAL_POPUP_SELECT_NONRECURRING, E_CAL_POPUP_SELECT_NOTEDITING | E_CAL_POPUP_SELECT_EDITABLE },
	{ E_POPUP_ITEM, "52.move", N_("Make this Occurrence _Movable"), on_unrecur_appointment, NULL, NULL, E_CAL_POPUP_SELECT_RECURRING | E_CAL_POPUP_SELECT_INSTANCE, E_CAL_POPUP_SELECT_NOTEDITING | E_CAL_POPUP_SELECT_EDITABLE },
	{ E_POPUP_ITEM, "53.delete", N_("Delete this _Occurrence"), on_delete_occurrence, NULL, GTK_STOCK_DELETE, E_CAL_POPUP_SELECT_RECURRING, E_CAL_POPUP_SELECT_NOTEDITING | E_CAL_POPUP_SELECT_EDITABLE },
	{ E_POPUP_ITEM, "54.delete", N_("Delete _All Occurrences"), on_delete_appointment, NULL, GTK_STOCK_DELETE, E_CAL_POPUP_SELECT_RECURRING, E_CAL_POPUP_SELECT_NOTEDITING | E_CAL_POPUP_SELECT_EDITABLE },
};

static void
ecv_popup_free (EPopup *ep, GSList *list, void *data)
{
	g_slist_free(list);
}

GtkMenu *
e_calendar_view_create_popup_menu (ECalendarView *cal_view)
{
	ECalPopup *ep;
	GSList *menus = NULL;
	GList *selected, *l;
	int i;
	ECalPopupTargetSelect *t;
	ECalModel *model;
	GPtrArray *events;

	g_return_val_if_fail (E_IS_CALENDAR_VIEW (cal_view), NULL);

	/* We could do this using a factory on the ECalPopup class,
	 * that way we would get called implicitly whenever a popup
	 * menu was created rather than everyone having to call us.
	 * We could also have a different menu id for each view */

	/** @HookPoint-ECalPopup: Calendar Main View Context Menu
	 * @Id: org.gnome.evolution.calendar.view.popup
	 * @Class: org.gnome.evolution.calendar.popup:1.0
	 * @Target: ECalPopupTargetSelect
	 *
	 * The context menu on the main calendar view.  This menu
	 * applies to all view types.
	 */
	ep = e_cal_popup_new("org.gnome.evolution.calendar.view.popup");

	model = e_calendar_view_get_model(cal_view);
	events = g_ptr_array_new();
	selected = e_calendar_view_get_selected_events(cal_view);
	for (l=selected;l;l=g_list_next(l)) {
		ECalendarViewEvent *event = l->data;

		if (event)
			g_ptr_array_add(events, e_cal_model_copy_component_data(event->comp_data));
	}
	g_list_free(selected);

	t = e_cal_popup_target_new_select(ep, model, events);
	t->target.widget = (GtkWidget *)cal_view;

	if (t->events->len == 0) {
		for (i=0;i<sizeof(ecv_main_items)/sizeof(ecv_main_items[0]);i++)
			menus = g_slist_prepend(menus, &ecv_main_items[i]);

		gnome_calendar_view_popup_factory(cal_view->priv->calendar, (EPopup *)ep, "60.view");
	} else {
		for (i=0;i<sizeof(ecv_child_items)/sizeof(ecv_child_items[0]);i++)
			menus = g_slist_prepend(menus, &ecv_child_items[i]);
	}

	e_popup_add_items((EPopup *)ep, menus, NULL, ecv_popup_free, cal_view);

	return e_popup_create_menu_once((EPopup *)ep, (EPopupTarget *)t, 0);
}

void
e_calendar_view_open_event (ECalendarView *cal_view)
{
	GList *selected;

	selected = e_calendar_view_get_selected_events (cal_view);
	if (selected) {
		ECalendarViewEvent *event = (ECalendarViewEvent *) selected->data;
		if (event)
			e_calendar_view_edit_appointment (cal_view, event->comp_data->client,
					event->comp_data->icalcomp, icalcomponent_get_first_property(event->comp_data->icalcomp, ICAL_ATTENDEE_PROPERTY) != NULL);

		g_list_free (selected);
	}
}

/**
 * e_calendar_view_new_appointment_for
 * @cal_view: A calendar view.
 * @dtstart: A Unix time_t that marks the beginning of the appointment.
 * @dtend: A Unix time_t that marks the end of the appointment.
 * @all_day: If TRUE, the dtstart and dtend are expanded to cover
 * the entire day, and the event is set to TRANSPARENT.
 * @meeting: Whether the appointment is a meeting or not.
 *
 * Opens an event editor dialog for a new appointment.
 */
void
e_calendar_view_new_appointment_for (ECalendarView *cal_view,
				     time_t dtstart, time_t dtend,
				     gboolean all_day,
				     gboolean meeting)
{
	ECalendarViewPrivate *priv;
	struct icaltimetype itt;
	ECalComponentDateTime dt;
	ECalComponent *comp;
	icalcomponent *icalcomp;
	ECalComponentTransparency transparency;
	ECal *default_client = NULL;
	guint32 flags = 0;
	gboolean readonly = FALSE;

	g_return_if_fail (E_IS_CALENDAR_VIEW (cal_view));

	priv = cal_view->priv;

	default_client = e_cal_model_get_default_client (priv->model);

	if (!default_client || e_cal_get_load_state (default_client) != E_CAL_LOAD_LOADED) {
		g_warning ("Default client not loaded \n");
		return;
	}

	if (e_cal_is_read_only (default_client, &readonly, NULL) && readonly) {
		GtkWidget *widget;

		widget = e_error_new (NULL, "calendar:prompt-read-only-cal", e_source_peek_name (e_cal_get_source (default_client)), NULL);

		g_signal_connect ((GtkDialog *)widget, "response", G_CALLBACK (gtk_widget_destroy),
				  widget);
		gtk_widget_show (widget);
		return;
	}

	dt.value = &itt;
	if (all_day)
		dt.tzid = NULL;
	else
		dt.tzid = icaltimezone_get_tzid (e_cal_model_get_timezone (cal_view->priv->model));

	icalcomp = e_cal_model_create_component_with_defaults (priv->model);
	comp = e_cal_component_new ();
	e_cal_component_set_icalcomponent (comp, icalcomp);

	/* DTSTART, DTEND */
	itt = icaltime_from_timet_with_zone (dtstart, FALSE, e_cal_model_get_timezone (cal_view->priv->model));
	if (all_day) {
		itt.hour = itt.minute = itt.second = 0;
		itt.is_date = TRUE;
	}
	e_cal_component_set_dtstart (comp, &dt);

	itt = icaltime_from_timet_with_zone (dtend, FALSE, e_cal_model_get_timezone (cal_view->priv->model));
	if (all_day) {
		/* We round it up to the end of the day, unless it is
		   already set to midnight */
		if (itt.hour != 0 || itt.minute != 0 || itt.second != 0) {
			icaltime_adjust (&itt, 1, 0, 0, 0);
		}
		itt.hour = itt.minute = itt.second = 0;
		itt.is_date = TRUE;
	}
	e_cal_component_set_dtend (comp, &dt);

	/* TRANSPARENCY */
	transparency = all_day ? E_CAL_COMPONENT_TRANSP_TRANSPARENT
		: E_CAL_COMPONENT_TRANSP_OPAQUE;
	e_cal_component_set_transparency (comp, transparency);

	/* CATEGORY */
	e_cal_component_set_categories (comp, priv->default_category);

	/* edit the object */
	e_cal_component_commit_sequence (comp);

	flags |= COMP_EDITOR_NEW_ITEM;
	if (meeting) {
		flags |= COMP_EDITOR_MEETING;
		flags |= COMP_EDITOR_USER_ORG;
	}

	open_event_with_flags (cal_view, default_client,
			icalcomp, flags);

	g_object_unref (comp);
}

/**
 * e_calendar_view_new_appointment_full
 * @param cal_view: A calendar view.
 * @param all_day: Whether create all day event or not.
 * @param meeting: This is a meeting or an appointment.
 * @param no_past_date: Don't create event in past date, use actual date instead (if TRUE).
 *
 * Opens an event editor dialog for a new appointment. The appointment's
 * start and end times are set to the currently selected time range in
 * the calendar view.
 *
 * When the selection is for all day and we don't need @all_day event,
 * then this do a rounding to the actual hour for actual day (today) and
 * to the 'day begins' from preferences in other selected day.
 */
void
e_calendar_view_new_appointment_full (ECalendarView *cal_view, gboolean all_day, gboolean meeting, gboolean no_past_date)
{
	time_t dtstart, dtend, now;
	gboolean do_rounding = FALSE;

	g_return_if_fail (E_IS_CALENDAR_VIEW (cal_view));

	now = time (NULL);

	if (!e_calendar_view_get_selected_time_range (cal_view, &dtstart, &dtend)) {
		dtstart = now;
		dtend = dtstart + 3600;
	}

	if (no_past_date && dtstart < now) {
		dtend = time_day_begin (now) + (dtend - dtstart);
		dtstart = time_day_begin (now);
		do_rounding = TRUE;
	}

	/* We either need rounding or don't want to set all_day for this, we will rather use actual */
	/* time in this cases; dtstart should be a midnight in this case */
	if (do_rounding || (!all_day && (dtend - dtstart) % (60 * 60 * 24) == 0)) {
		struct tm local = *localtime (&now);
		int time_div = calendar_config_get_time_divisions ();
		int hours, mins;

		if (!time_div) /* Possible if your gconf values aren't so nice */
			time_div = 30; 

		if (time_day_begin (now) == time_day_begin (dtstart)) {
			/* same day as today */
			hours = local.tm_hour;
			mins = local.tm_min;

			/* round minutes to nearest time division, up or down */
			if ((mins % time_div) >= time_div / 2)
				mins += time_div;
			mins = (mins - (mins % time_div));
		} else {
			/* other day than today */
			hours = calendar_config_get_day_start_hour ();
			mins = calendar_config_get_day_start_minute ();
		}

		dtstart = dtstart + (60 * 60 * hours) + (mins * 60);
		dtend = dtstart + (time_div * 60);
	}

	e_calendar_view_new_appointment_for (cal_view, dtstart, dtend, all_day, meeting);
}

void
e_calendar_view_new_appointment (ECalendarView *cal_view)
{
	g_return_if_fail (E_IS_CALENDAR_VIEW (cal_view));

	e_calendar_view_new_appointment_full (cal_view, FALSE, FALSE, FALSE);
}

/* Ensures the calendar is selected */
static void
object_created_cb (CompEditor *ce, ECalendarView *cal_view)
{
	gnome_calendar_emit_user_created_signal (cal_view, e_calendar_view_get_calendar (cal_view), comp_editor_get_e_cal (ce));
}

static void
open_event_with_flags (ECalendarView *cal_view, ECal *client, icalcomponent *icalcomp, guint32 flags)
{
	CompEditor *ce;
	const char *uid;
	ECalComponent *comp;


	uid = icalcomponent_get_uid (icalcomp);

	ce = e_comp_editor_registry_find (comp_editor_registry, uid);
	if (!ce) {
		EventEditor *ee;

		ee = event_editor_new (client, flags);
		ce = COMP_EDITOR (ee);

		g_signal_connect (ce, "object_created", G_CALLBACK (object_created_cb), cal_view);

		comp = e_cal_component_new ();
		e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (icalcomp));
		comp_editor_edit_comp (ce, comp);
		if (flags & COMP_EDITOR_MEETING)
			event_editor_show_meeting (ee);

		e_comp_editor_registry_add (comp_editor_registry, ce, FALSE);

		g_object_unref (comp);
	}

	comp_editor_focus (ce);

}

/**
 * e_calendar_view_edit_appointment
 * @cal_view: A calendar view.
 * @client: Calendar client.
 * @icalcomp: The object to be edited.
 * @meeting: Whether the appointment is a meeting or not.
 *
 * Opens an editor window to allow the user to edit the selected
 * object.
 */
void
e_calendar_view_edit_appointment (ECalendarView *cal_view,
			     ECal *client,
			     icalcomponent *icalcomp,
			     gboolean meeting)
{
	guint32 flags = 0;

	g_return_if_fail (E_IS_CALENDAR_VIEW (cal_view));
	g_return_if_fail (E_IS_CAL (client));
	g_return_if_fail (icalcomp != NULL);

	if (meeting) {
		ECalComponent *comp = e_cal_component_new ();
		e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (icalcomp));
		flags |= COMP_EDITOR_MEETING;
		if (itip_organizer_is_user (comp, client) || itip_sentby_is_user (comp) || !e_cal_component_has_attendees (comp))
			flags |= COMP_EDITOR_USER_ORG;
		g_object_unref (comp);
	}


	open_event_with_flags (cal_view, client, icalcomp, flags);
}

void
e_calendar_view_modify_and_send (ECalComponent *comp,
				 ECal *client,
				 CalObjModType mod,
				 GtkWindow *toplevel,
				 gboolean new)
{
	if (e_cal_modify_object (client, e_cal_component_get_icalcomponent (comp), mod, NULL)) {
		if ((itip_organizer_is_user (comp, client) || itip_sentby_is_user (comp)) &&
		    send_component_dialog (toplevel, client, comp, new))
			itip_send_comp (E_CAL_COMPONENT_METHOD_REQUEST, comp, client, NULL, NULL, NULL);
	} else {
		g_message (G_STRLOC ": Could not update the object!");
	}
}

static gboolean
tooltip_grab (GtkWidget *tooltip, GdkEventKey *event, ECalendarView *view)
{
	GtkWidget *widget = (GtkWidget *) g_object_get_data (G_OBJECT (view), "tooltip-window");

	if (!widget)
		return TRUE;

	gdk_keyboard_ungrab(GDK_CURRENT_TIME);
	gtk_widget_destroy (widget);
	g_object_set_data (G_OBJECT (view), "tooltip-window", NULL);

	return FALSE;
}

static char *
get_label (struct icaltimetype *tt, icaltimezone *f_zone, icaltimezone *t_zone)
{
        char buffer[1000];
        struct tm tmp_tm;

	tmp_tm = icaltimetype_to_tm_with_zone (tt, f_zone, t_zone);
        e_time_format_date_and_time (&tmp_tm,
                                     calendar_config_get_24_hour_format (),
                                     FALSE, FALSE,
                                     buffer, 1000);

        return g_strdup (buffer);
}

void
e_calendar_view_move_tip (GtkWidget *widget, int x, int y)
{
  GtkRequisition requisition;
  gint w, h;
  GdkScreen *screen;
  GdkScreen *pointer_screen;
  gint monitor_num, px, py;
  GdkRectangle monitor;

  screen = gtk_widget_get_screen (widget);

  gtk_widget_size_request (widget, &requisition);
  w = requisition.width;
  h = requisition.height;

  gdk_display_get_pointer (gdk_screen_get_display (screen),
                           &pointer_screen, &px, &py, NULL);
  if (pointer_screen != screen)
    {
      px = x;
      py = y;
    }
  monitor_num = gdk_screen_get_monitor_at_point (screen, px, py);
  gdk_screen_get_monitor_geometry (screen, monitor_num, &monitor);

  if ((x + w) > monitor.x + monitor.width)
    x -= (x + w) - (monitor.x + monitor.width);
  else if (x < monitor.x)
    x = monitor.x;

  if ((y + h + widget->allocation.height + 4) > monitor.y + monitor.height)
    y = y - h - 36;

  gtk_window_move (GTK_WINDOW (widget), x, y);
  gtk_widget_show (widget);
}

/*
 * It is expected to show the tooltips in this below format
 *
 * 	<B>SUBJECT OF THE MEETING</B>
 * 	Organiser: NameOfTheUser<email@ofuser.com>
 * 	Location: PlaceOfTheMeeting
 * 	Time : DateAndTime (xx Minutes)
 */

gboolean
e_calendar_view_get_tooltips (ECalendarViewEventData *data)
{
	GtkWidget *label, *box, *hbox, *ebox, *frame;
	const char *str;
	char *tmp, *tmp1, *tmp2;
	ECalComponentOrganizer organiser;
	ECalComponentDateTime dtstart, dtend;
	icalcomponent *clone_comp;
	time_t t_start, t_end;
	ECalendarViewEvent *pevent;
	GtkStyle *style = gtk_widget_get_default_style ();
	GtkWidget *widget = (GtkWidget *) g_object_get_data (G_OBJECT (data->cal_view), "tooltip-window");
	ECalComponent *newcomp = e_cal_component_new ();
	icaltimezone *zone, *default_zone;
	ECal *client = NULL;
	gboolean free_text = FALSE;

	/* Delete any stray tooltip if left */
	if (widget)
		gtk_widget_destroy (widget);

	default_zone = e_calendar_view_get_timezone  (data->cal_view);
	pevent = data->get_view_event (data->cal_view, data->day, data->event_num);

	client = pevent->comp_data->client;

	clone_comp = icalcomponent_new_clone (pevent->comp_data->icalcomp);
	if (!e_cal_component_set_icalcomponent (newcomp, clone_comp))
		g_warning ("couldn't update calendar component with modified data from backend\n");

	box = gtk_vbox_new (FALSE, 0);

	str = e_calendar_view_get_icalcomponent_summary (pevent->comp_data->client, pevent->comp_data->icalcomp, &free_text);

	if (!(str && *str)) {
		g_object_unref (newcomp);
		gtk_widget_destroy (box);
		g_free (data);

		return FALSE;
	}

	tmp = g_markup_printf_escaped ("<b>%s</b>", str);
	label = gtk_label_new (NULL);
	gtk_label_set_line_wrap ((GtkLabel *)label, TRUE);
	gtk_label_set_markup ((GtkLabel *)label, tmp);

	if (free_text) {
		g_free ((char*)str);
		str = NULL;
	}

	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start ((GtkBox *)hbox, label, FALSE, FALSE, 0);
	ebox = gtk_event_box_new ();
	gtk_container_add ((GtkContainer *)ebox, hbox);
	gtk_widget_modify_bg (ebox, GTK_STATE_NORMAL, &(style->bg[GTK_STATE_SELECTED]));
	gtk_widget_modify_fg (label, GTK_STATE_NORMAL, &(style->text[GTK_STATE_SELECTED]));

	gtk_box_pack_start ((GtkBox *)box, ebox, FALSE, FALSE, 0);
	g_free (tmp);

	e_cal_component_get_organizer (newcomp, &organiser);
	if (organiser.cn) {
		char *ptr ;
		ptr = strchr(organiser.value, ':');

		if (ptr) {
			ptr++;
			/* To Translators: It will display "Organiser: NameOfTheUser <email@ofuser.com>" */
			tmp = g_strdup_printf (_("Organizer: %s <%s>"), organiser.cn, ptr);
		}
		else
			/* With SunOne accouts, there may be no ':' in organiser.value*/
			tmp = g_strdup_printf (_("Organizer: %s"), organiser.cn);

		label = gtk_label_new (tmp);
		hbox = gtk_hbox_new (FALSE, 0);
		gtk_box_pack_start ((GtkBox *)hbox, label, FALSE, FALSE, 0);
		ebox = gtk_event_box_new ();
		gtk_container_add ((GtkContainer *)ebox, hbox);
		gtk_box_pack_start ((GtkBox *)box, ebox, FALSE, FALSE, 0);

		g_free (tmp);
	}

	e_cal_component_get_location (newcomp, &str);

	if (str) {
		/* To Translators: It will display "Location: PlaceOfTheMeeting" */
		tmp = g_strdup_printf (_("Location: %s"), str);
		label = gtk_label_new (NULL);
		gtk_label_set_markup ((GtkLabel *)label, tmp);
		hbox = gtk_hbox_new (FALSE, 0);
		gtk_box_pack_start ((GtkBox *)hbox, label, FALSE, FALSE, 0);
		ebox = gtk_event_box_new ();
		gtk_container_add ((GtkContainer *)ebox, hbox);
		gtk_box_pack_start ((GtkBox *)box, ebox, FALSE, FALSE, 0);
		g_free (tmp);
	}
	e_cal_component_get_dtstart (newcomp, &dtstart);
	e_cal_component_get_dtend (newcomp, &dtend);

	if (dtstart.tzid) {
		zone = icalcomponent_get_timezone (e_cal_component_get_icalcomponent (newcomp), dtstart.tzid);
		if (!zone)
			e_cal_get_timezone (client, dtstart.tzid, &zone, NULL);

		if (!zone)
			zone = default_zone;

	} else {
		zone = NULL;
	}
	t_start = icaltime_as_timet_with_zone (*dtstart.value, zone);
	t_end = icaltime_as_timet_with_zone (*dtend.value, zone);

	tmp1 = get_label(dtstart.value, zone, default_zone);
	tmp = calculate_time (t_start, t_end);

	e_cal_component_free_datetime (&dtstart);
	e_cal_component_free_datetime (&dtend);

	/* To Translators: It will display "Time: ActualStartDateAndTime (DurationOfTheMeeting)"*/
	tmp2 = g_strdup_printf(_("Time: %s %s"), tmp1, tmp);
	hbox = gtk_hbox_new (FALSE, 0);
	gtk_box_pack_start ((GtkBox *)hbox, gtk_label_new_with_mnemonic (tmp2), FALSE, FALSE, 0);
	ebox = gtk_event_box_new ();
	gtk_container_add ((GtkContainer *)ebox, hbox);
	gtk_box_pack_start ((GtkBox *)box, ebox, FALSE, FALSE, 0);

	g_free (tmp);
	g_free (tmp2);
	g_free (tmp1);

	pevent->tooltip = gtk_window_new (GTK_WINDOW_POPUP);
	frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type ((GtkFrame *)frame, GTK_SHADOW_IN);

	gtk_window_move ((GtkWindow *)pevent->tooltip, pevent->x +16, pevent->y+16);
	gtk_container_add ((GtkContainer *)frame, box);
	gtk_container_add ((GtkContainer *)pevent->tooltip, frame);

	gtk_widget_show_all (pevent->tooltip);

	e_calendar_view_move_tip (pevent->tooltip, pevent->x +16, pevent->y+16);

	gdk_keyboard_grab (pevent->tooltip->window, FALSE, GDK_CURRENT_TIME);
	g_signal_connect (pevent->tooltip, "key-press-event", G_CALLBACK (tooltip_grab), data->cal_view);
	pevent->timeout = -1;

	g_object_set_data (G_OBJECT (data->cal_view), "tooltip-window", pevent->tooltip);
	g_object_unref (newcomp);
	g_free (data);

	return FALSE;
}

static gboolean
icalcomp_contains_category (icalcomponent *icalcomp, const gchar *category)
{
	icalproperty *property;

	g_return_val_if_fail (icalcomp != NULL && category != NULL, FALSE);

	for (property = icalcomponent_get_first_property (icalcomp, ICAL_CATEGORIES_PROPERTY);
	     property != NULL;
	     property = icalcomponent_get_next_property (icalcomp, ICAL_CATEGORIES_PROPERTY)) {
		char *value = icalproperty_get_value_as_string (property);

		if (value && strcmp (category, value) == 0){
			g_free (value);
			return TRUE;
		}
		g_free (value);
	}

	return FALSE;
}

/* e_calendar_view_get_icalcomponent_summary returns summary of calcomp,
 * and for type of birthday or anniversary it append number of years since
 * beginning. In this case, the free_text is set to TRUE and caller need
 * to g_free returned string, otherwise free_text is set to FALSE and
 * returned value is owned by calcomp.
 */

const gchar *
e_calendar_view_get_icalcomponent_summary (ECal *ecal, icalcomponent *icalcomp, gboolean *free_text)
{
	const gchar *summary;

	g_return_val_if_fail (icalcomp != NULL && free_text != NULL, NULL);

	*free_text = FALSE;
	summary = icalcomponent_get_summary (icalcomp);

	if (icalcomp_contains_category (icalcomp, _("Birthday")) ||
	    icalcomp_contains_category (icalcomp, _("Anniversary"))) {
		struct icaltimetype dtstart, dtnow;
		icalcomponent *item_icalcomp = NULL;

		if (e_cal_get_object (ecal,
				      icalcomponent_get_uid (icalcomp),
				      icalcomponent_get_relcalid (icalcomp),
				      &item_icalcomp,
				      NULL)) {
			dtstart = icalcomponent_get_dtstart (item_icalcomp);
			dtnow = icalcomponent_get_dtstart (icalcomp);

			if (dtnow.year - dtstart.year > 0) {
				summary = g_strdup_printf ("%s (%d)", summary ? summary : "", dtnow.year - dtstart.year);
				*free_text = summary != NULL;
			}
		}
	}

	return summary;
}

void
draw_curved_rectangle (cairo_t *cr, double x0, double y0,
			double rect_width, double rect_height,
			double radius)
{
	double x1, y1;

	x1 = x0 + rect_width;
	y1 = y0 + rect_height;

	if (!rect_width || !rect_height)
	    return;
	if (rect_width / 2 < radius) {
	    if (rect_height / 2 < radius) {
	        cairo_move_to  (cr, x0, (y0 + y1)/2);
        	cairo_curve_to (cr, x0 ,y0, x0, y0, (x0 + x1)/2, y0);
	        cairo_curve_to (cr, x1, y0, x1, y0, x1, (y0 + y1)/2);
	        cairo_curve_to (cr, x1, y1, x1, y1, (x1 + x0)/2, y1);
	        cairo_curve_to (cr, x0, y1, x0, y1, x0, (y0 + y1)/2);
	    } else {
        	cairo_move_to  (cr, x0, y0 + radius);
	        cairo_curve_to (cr, x0 ,y0, x0, y0, (x0 + x1)/2, y0);
        	cairo_curve_to (cr, x1, y0, x1, y0, x1, y0 + radius);
	        cairo_line_to (cr, x1 , y1 - radius);
	        cairo_curve_to (cr, x1, y1, x1, y1, (x1 + x0)/2, y1);
        	cairo_curve_to (cr, x0, y1, x0, y1, x0, y1- radius);
    		}
	} else {
	    if (rect_height / 2 < radius) {
        	cairo_move_to  (cr, x0, (y0 + y1)/2);
	        cairo_curve_to (cr, x0 , y0, x0 , y0, x0 + radius, y0);
	        cairo_line_to (cr, x1 - radius, y0);
	        cairo_curve_to (cr, x1, y0, x1, y0, x1, (y0 + y1)/2);
        	cairo_curve_to (cr, x1, y1, x1, y1, x1 - radius, y1);
	        cairo_line_to (cr, x0 + radius, y1);
        	cairo_curve_to (cr, x0, y1, x0, y1, x0, (y0 + y1)/2);
	    } else {
        	cairo_move_to  (cr, x0, y0 + radius);
	        cairo_curve_to (cr, x0 , y0, x0 , y0, x0 + radius, y0);
        	cairo_line_to (cr, x1 - radius, y0);
	        cairo_curve_to (cr, x1, y0, x1, y0, x1, y0 + radius);
        	cairo_line_to (cr, x1 , y1 - radius);
	        cairo_curve_to (cr, x1, y1, x1, y1, x1 - radius, y1);
        	cairo_line_to (cr, x0 + radius, y1);
	        cairo_curve_to (cr, x0, y1, x0, y1, x0, y1- radius);
    		}
	}
	cairo_close_path (cr);
}
