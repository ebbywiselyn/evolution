/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * Authors :
 *  Damon Chaplin <damon@ximian.com>
 *  Rodrigo Moya <rodrigo@ximian.com>
 *
 * Copyright 2000, 2001, 2002, 2003 Novell, Inc.
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

/*
 * ECalendarTable - displays the ECalComponent objects in a table (an ETable).
 * Used for calendar events and tasks.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/stat.h>
#include <unistd.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gnome.h>
#include <misc/e-gui-utils.h>
#include <table/e-cell-checkbox.h>
#include <table/e-cell-toggle.h>
#include <table/e-cell-text.h>
#include <table/e-cell-combo.h>
#include <e-util/e-dialog-utils.h>
#include <e-util/e-util-private.h>
#include <misc/e-cell-date-edit.h>
#include <misc/e-cell-percent.h>

#include "calendar-component.h"
#include "calendar-config.h"
#include "dialogs/delete-comp.h"
#include "dialogs/delete-error.h"
#include "dialogs/task-editor.h"
#include "e-cal-model-tasks.h"
#include "e-calendar-table.h"
#include "e-cell-date-edit-text.h"
#include "e-comp-editor-registry.h"
#include "print.h"
#include <e-util/e-icon-factory.h>
#include "e-cal-popup.h"
#include "e-tasks.h"
#include "misc.h"

enum TargetType{
	TARGET_TYPE_VCALENDAR
};

static GtkTargetEntry target_types[] = {
	{ "text/x-calendar", 0, TARGET_TYPE_VCALENDAR },
	{ "text/calendar",   0, TARGET_TYPE_VCALENDAR }
};

static guint n_target_types = G_N_ELEMENTS (target_types);

extern ECompEditorRegistry *comp_editor_registry;

static void e_calendar_table_class_init		(ECalendarTableClass *class);
static void e_calendar_table_init		(ECalendarTable	*cal_table);
static void e_calendar_table_destroy		(GtkObject	*object);

static void e_calendar_table_on_double_click	(ETable		*table,
						 gint		 row,
						 gint		 col,
						 GdkEvent	*event,
						 ECalendarTable *cal_table);
static gint e_calendar_table_show_popup_menu    (ETable *table,
						 GdkEvent *gdk_event,
						 ECalendarTable *cal_table);

static gint e_calendar_table_on_right_click	(ETable		*table,
						 gint		 row,
						 gint		 col,
						 GdkEvent       *event,
						 ECalendarTable *cal_table);
static gboolean e_calendar_table_on_popup_menu  (GtkWidget *widget,
						 gpointer data);

static gint e_calendar_table_on_key_press	(ETable		*table,
						 gint		 row,
						 gint		 col,
						 GdkEventKey	*event,
						 ECalendarTable *cal_table);

static struct tm e_calendar_table_get_current_time (ECellDateEdit *ecde,
						    gpointer data);
static void mark_as_complete_cb (EPopup *ep, EPopupItem *pitem, void *data);

static void hide_completed_rows (ECalModel *model, GList *clients_list, char *hide_sexp, GPtrArray *comp_objects);
static void show_completed_rows (ECalModel *model, GList *clients_list, char *show_sexp, GPtrArray *comp_objects);

/* Signal IDs */
enum {
	USER_CREATED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

/* The icons to represent the task. */
#define E_CALENDAR_MODEL_NUM_ICONS	4
static const char* icon_names[E_CALENDAR_MODEL_NUM_ICONS] = {
	"stock_task", "stock_task-recurring", "stock_task-assigned", "stock_task-assigned-to"
};
static GdkPixbuf* icon_pixbufs[E_CALENDAR_MODEL_NUM_ICONS] = { NULL };

static GdkAtom clipboard_atom = GDK_NONE;

G_DEFINE_TYPE (ECalendarTable, e_calendar_table, GTK_TYPE_TABLE)

static void
e_calendar_table_class_init (ECalendarTableClass *class)
{
	GtkObjectClass *object_class;

	object_class = (GtkObjectClass *) class;

	/* Method override */
	object_class->destroy		= e_calendar_table_destroy;

	signals[USER_CREATED] =
		g_signal_new ("user_created",
			      G_TYPE_FROM_CLASS (class),
			      G_SIGNAL_RUN_LAST,
			      G_STRUCT_OFFSET (ECalendarTableClass, user_created),
			      NULL, NULL,
			      g_cclosure_marshal_VOID__VOID,
			      G_TYPE_NONE, 0);

	/* clipboard atom */
	if (!clipboard_atom)
		clipboard_atom = gdk_atom_intern ("CLIPBOARD", FALSE);
}

static gint
date_compare_cb (gconstpointer a, gconstpointer b)
{
	ECellDateEditValue *dv1 = (ECellDateEditValue *) a;
	ECellDateEditValue *dv2 = (ECellDateEditValue *) b;
	struct icaltimetype tt;

	/* First check if either is NULL. NULL dates sort last. */
	if (!dv1 || !dv2) {
		if (dv1 == dv2)
			return 0;
		else if (dv1)
			return -1;
		else
			return 1;
	}

	/* Copy the 2nd value and convert it to the same timezone as the
	   first. */
	tt = dv2->tt;

	icaltimezone_convert_time (&tt, dv2->zone, dv1->zone);

	/* Now we can compare them. */

	return icaltime_compare (dv1->tt, tt);
}

static gint
percent_compare_cb (gconstpointer a, gconstpointer b)
{
	int percent1 = GPOINTER_TO_INT (a);
	int percent2 = GPOINTER_TO_INT (b);
	int retval;

	if (percent1 > percent2)
		retval = 1;
	else if (percent1 < percent2)
		retval = -1;
	else
		retval = 0;

	return retval;
}

static gint
priority_compare_cb (gconstpointer a, gconstpointer b)
{
	int priority1, priority2;

	priority1 = e_cal_util_priority_from_string ((const char*) a);
	priority2 = e_cal_util_priority_from_string ((const char*) b);

	/* We change undefined priorities so they appear after 'Low'. */
	if (priority1 <= 0)
		priority1 = 10;
	if (priority2 <= 0)
		priority2 = 10;

	/* We'll just use the ordering of the priority values. */
	if (priority1 < priority2)
		return -1;
	else if (priority1 > priority2)
		return 1;
	else
		return 0;
}

static gint
status_from_string (const char *str)
{
	int status = -2;

	if (!str || !str[0])
		status = -1;
	else if (!g_utf8_collate (str, _("Not Started")))
		status = 0;
	else if (!g_utf8_collate (str, _("In Progress")))
		status = 1;
	else if (!g_utf8_collate (str, _("Completed")))
		status = 2;
	else if (!g_utf8_collate (str, _("Canceled")))
		status = 3;

	return status;
}

static gint
status_compare_cb (gconstpointer a, gconstpointer b)
{
	int sa = status_from_string ((const char *)a);
	int sb = status_from_string ((const char *)b);

	if (sa < sb)
		return -1;
	else if (sa > sb)
		return 1;

	return 0;
}

static void
row_appended_cb (ECalModel *model, ECalendarTable *cal_table)
{
	g_signal_emit (cal_table, signals[USER_CREATED], 0);
}

static void
e_calendar_table_init (ECalendarTable *cal_table)
{
	GtkWidget *table;
	ETable *e_table;
	ECell *cell, *popup_cell;
	ETableExtras *extras;
	gint i;
	GdkPixbuf *pixbuf;
	GList *strings;
	AtkObject *a11y;
	char *etspecfile;

	/* Create the model */

	cal_table->model = (ECalModel *) e_cal_model_tasks_new ();
	g_signal_connect (cal_table->model, "row_appended", G_CALLBACK (row_appended_cb), cal_table);

	cal_table->user_created_cal = NULL;

	/* Create the header columns */

	extras = e_table_extras_new();

	/*
	 * Normal string fields.
	 */
	cell = e_cell_text_new (NULL, GTK_JUSTIFY_LEFT);
	g_object_set (G_OBJECT (cell),
		      "strikeout_column", E_CAL_MODEL_TASKS_FIELD_STRIKEOUT,
		      "bold_column", E_CAL_MODEL_TASKS_FIELD_OVERDUE,
		      "bg_color_column", E_CAL_MODEL_FIELD_COLOR,
		      NULL);

	e_table_extras_add_cell (extras, "calstring", cell);


	/*
	 * Date fields.
	 */
	cell = e_cell_date_edit_text_new (NULL, GTK_JUSTIFY_LEFT);
	g_object_set (G_OBJECT (cell),
		      "strikeout_column", E_CAL_MODEL_TASKS_FIELD_STRIKEOUT,
		      "bold_column", E_CAL_MODEL_TASKS_FIELD_OVERDUE,
		      "bg_color_column", E_CAL_MODEL_FIELD_COLOR,
		      NULL);

	popup_cell = e_cell_date_edit_new ();
	e_cell_popup_set_child (E_CELL_POPUP (popup_cell), cell);
	g_object_unref (cell);
	e_table_extras_add_cell (extras, "dateedit", popup_cell);
	cal_table->dates_cell = E_CELL_DATE_EDIT (popup_cell);

	e_cell_date_edit_set_get_time_callback (E_CELL_DATE_EDIT (popup_cell),
						e_calendar_table_get_current_time,
						cal_table, NULL);


	/*
	 * Combo fields.
	 */

	/* Classification field. */
	cell = e_cell_text_new (NULL, GTK_JUSTIFY_LEFT);
	g_object_set (G_OBJECT (cell),
		      "strikeout_column", E_CAL_MODEL_TASKS_FIELD_STRIKEOUT,
		      "bold_column", E_CAL_MODEL_TASKS_FIELD_OVERDUE,
		      "bg_color_column", E_CAL_MODEL_FIELD_COLOR,
		      "editable", FALSE,
		      NULL);

	popup_cell = e_cell_combo_new ();
	e_cell_popup_set_child (E_CELL_POPUP (popup_cell), cell);
	g_object_unref (cell);

	strings = NULL;
	strings = g_list_append (strings, (char*) _("Public"));
	strings = g_list_append (strings, (char*) _("Private"));
	strings = g_list_append (strings, (char*) _("Confidential"));
	e_cell_combo_set_popdown_strings (E_CELL_COMBO (popup_cell),
					  strings);

	e_table_extras_add_cell (extras, "classification", popup_cell);

	/* Priority field. */
	cell = e_cell_text_new (NULL, GTK_JUSTIFY_LEFT);
	g_object_set (G_OBJECT (cell),
		      "strikeout_column", E_CAL_MODEL_TASKS_FIELD_STRIKEOUT,
		      "bold_column", E_CAL_MODEL_TASKS_FIELD_OVERDUE,
		      "bg_color_column", E_CAL_MODEL_FIELD_COLOR,
		      "editable", FALSE,
		      NULL);

	popup_cell = e_cell_combo_new ();
	e_cell_popup_set_child (E_CELL_POPUP (popup_cell), cell);
	g_object_unref (cell);

	strings = NULL;
	strings = g_list_append (strings, (char*) _("High"));
	strings = g_list_append (strings, (char*) _("Normal"));
	strings = g_list_append (strings, (char*) _("Low"));
	strings = g_list_append (strings, (char*) _("Undefined"));
	e_cell_combo_set_popdown_strings (E_CELL_COMBO (popup_cell),
					  strings);

	e_table_extras_add_cell (extras, "priority", popup_cell);

	/* Percent field. */
	cell = e_cell_percent_new (NULL, GTK_JUSTIFY_LEFT);
	g_object_set (G_OBJECT (cell),
		      "strikeout_column", E_CAL_MODEL_TASKS_FIELD_STRIKEOUT,
		      "bold_column", E_CAL_MODEL_TASKS_FIELD_OVERDUE,
		      "bg_color_column", E_CAL_MODEL_FIELD_COLOR,
		      NULL);

	popup_cell = e_cell_combo_new ();
	e_cell_popup_set_child (E_CELL_POPUP (popup_cell), cell);
	g_object_unref (cell);

	strings = NULL;
	strings = g_list_append (strings, (char*) _("0%"));
	strings = g_list_append (strings, (char*) _("10%"));
	strings = g_list_append (strings, (char*) _("20%"));
	strings = g_list_append (strings, (char*) _("30%"));
	strings = g_list_append (strings, (char*) _("40%"));
	strings = g_list_append (strings, (char*) _("50%"));
	strings = g_list_append (strings, (char*) _("60%"));
	strings = g_list_append (strings, (char*) _("70%"));
	strings = g_list_append (strings, (char*) _("80%"));
	strings = g_list_append (strings, (char*) _("90%"));
	strings = g_list_append (strings, (char*) _("100%"));
	e_cell_combo_set_popdown_strings (E_CELL_COMBO (popup_cell),
					  strings);

	e_table_extras_add_cell (extras, "percent", popup_cell);

	/* Transparency field. */
	cell = e_cell_text_new (NULL, GTK_JUSTIFY_LEFT);
	g_object_set (G_OBJECT (cell),
		      "strikeout_column", E_CAL_MODEL_TASKS_FIELD_STRIKEOUT,
		      "bold_column", E_CAL_MODEL_TASKS_FIELD_OVERDUE,
		      "bg_color_column", E_CAL_MODEL_FIELD_COLOR,
		      "editable", FALSE,
		      NULL);

	popup_cell = e_cell_combo_new ();
	e_cell_popup_set_child (E_CELL_POPUP (popup_cell), cell);
	g_object_unref (cell);

	strings = NULL;
	strings = g_list_append (strings, (char*) _("Free"));
	strings = g_list_append (strings, (char*) _("Busy"));
	e_cell_combo_set_popdown_strings (E_CELL_COMBO (popup_cell),
					  strings);

	e_table_extras_add_cell (extras, "transparency", popup_cell);

	/* Status field. */
	cell = e_cell_text_new (NULL, GTK_JUSTIFY_LEFT);
	g_object_set (G_OBJECT (cell),
		      "strikeout_column", E_CAL_MODEL_TASKS_FIELD_STRIKEOUT,
		      "bold_column", E_CAL_MODEL_TASKS_FIELD_OVERDUE,
		      "bg_color_column", E_CAL_MODEL_FIELD_COLOR,
		      "editable", FALSE,
		      NULL);

	popup_cell = e_cell_combo_new ();
	e_cell_popup_set_child (E_CELL_POPUP (popup_cell), cell);
	g_object_unref (cell);

	strings = NULL;
	strings = g_list_append (strings, (char*) _("Not Started"));
	strings = g_list_append (strings, (char*) _("In Progress"));
	strings = g_list_append (strings, (char*) _("Completed"));
	strings = g_list_append (strings, (char*) _("Canceled"));
	e_cell_combo_set_popdown_strings (E_CELL_COMBO (popup_cell),
					  strings);

	e_table_extras_add_cell (extras, "calstatus", popup_cell);

	e_table_extras_add_compare (extras, "date-compare",
				    date_compare_cb);
	e_table_extras_add_compare (extras, "percent-compare",
				    percent_compare_cb);
	e_table_extras_add_compare (extras, "priority-compare",
				    priority_compare_cb);
	e_table_extras_add_compare (extras, "status-compare",
				    status_compare_cb);

	/* Create pixmaps */

	if (!icon_pixbufs[0])
		for (i = 0; i < E_CALENDAR_MODEL_NUM_ICONS; i++) {
			icon_pixbufs[i] = e_icon_factory_get_icon (icon_names[i], E_ICON_SIZE_LIST);
		}

	cell = e_cell_toggle_new (0, E_CALENDAR_MODEL_NUM_ICONS, icon_pixbufs);
	e_table_extras_add_cell(extras, "icon", cell);
	e_table_extras_add_pixbuf(extras, "icon", icon_pixbufs[0]);

	pixbuf = e_icon_factory_get_icon ("stock_check-filled", E_ICON_SIZE_LIST);
	e_table_extras_add_pixbuf(extras, "complete", pixbuf);
	g_object_unref(pixbuf);

	/* Create the table */

	etspecfile = g_build_filename (EVOLUTION_ETSPECDIR,
				       "e-calendar-table.etspec",
				       NULL);
	table = e_table_scrolled_new_from_spec_file (E_TABLE_MODEL (cal_table->model),
						     extras,
						     etspecfile,
						     NULL);
	g_free (etspecfile);

	/* FIXME: this causes a message from GLib about 'extras' having only a floating
	   reference */
	/* g_object_unref (extras); */

	cal_table->etable = table;
	gtk_table_attach (GTK_TABLE (cal_table), table, 0, 1, 0, 1,
			  GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
	gtk_widget_show (table);


	e_table = e_table_scrolled_get_table (E_TABLE_SCROLLED (table));
	g_signal_connect (e_table, "double_click", G_CALLBACK (e_calendar_table_on_double_click), cal_table);
	g_signal_connect (e_table, "right_click", G_CALLBACK (e_calendar_table_on_right_click), cal_table);
	g_signal_connect (e_table, "key_press", G_CALLBACK (e_calendar_table_on_key_press), cal_table);
	g_signal_connect (e_table, "popup_menu", G_CALLBACK (e_calendar_table_on_popup_menu), cal_table);

	a11y = gtk_widget_get_accessible ((GtkWidget *)e_table);
	if (a11y)
		atk_object_set_name (a11y, _("Tasks"));
}


/**
 * e_calendar_table_new:
 * @Returns: a new #ECalendarTable.
 *
 * Creates a new #ECalendarTable.
 **/
GtkWidget *
e_calendar_table_new (void)
{
	GtkWidget *cal_table;

	cal_table = GTK_WIDGET (g_object_new (e_calendar_table_get_type (), NULL));

	return cal_table;
}


/**
 * e_calendar_table_get_model:
 * @cal_table: A calendar table.
 *
 * Queries the calendar data model that a calendar table is using.
 *
 * Return value: A calendar model.
 **/
ECalModel *
e_calendar_table_get_model (ECalendarTable *cal_table)
{
	g_return_val_if_fail (cal_table != NULL, NULL);
	g_return_val_if_fail (E_IS_CALENDAR_TABLE (cal_table), NULL);

	return cal_table->model;
}


static void
e_calendar_table_destroy (GtkObject *object)
{
	ECalendarTable *cal_table;

	cal_table = E_CALENDAR_TABLE (object);

	if (cal_table->model) {
		g_object_unref (cal_table->model);
		cal_table->model = NULL;
	}

	GTK_OBJECT_CLASS (e_calendar_table_parent_class)->destroy (object);
}

/**
 * e_calendar_table_get_table:
 * @cal_table: A calendar table.
 *
 * Queries the #ETable widget that the calendar table is using.
 *
 * Return value: The #ETable widget that the calendar table uses to display its
 * data.
 **/
ETable *
e_calendar_table_get_table (ECalendarTable *cal_table)
{
	g_return_val_if_fail (cal_table != NULL, NULL);
	g_return_val_if_fail (E_IS_CALENDAR_TABLE (cal_table), NULL);

	return e_table_scrolled_get_table (E_TABLE_SCROLLED (cal_table->etable));
}

void
e_calendar_table_open_selected (ECalendarTable *cal_table)
{
	ECalModelComponent *comp_data;
	icalproperty *prop;

	comp_data = e_calendar_table_get_selected_comp (cal_table);
	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_ATTENDEE_PROPERTY);
	if (comp_data != NULL)
		e_calendar_table_open_task (cal_table, comp_data->client, comp_data->icalcomp, prop ? TRUE : FALSE);
}

/**
 * e_calendar_table_complete_selected:
 * @cal_table: A calendar table
 *
 * Marks the selected items as completed
 **/
void
e_calendar_table_complete_selected (ECalendarTable *cal_table)
{
	g_return_if_fail (cal_table != NULL);
	g_return_if_fail (E_IS_CALENDAR_TABLE (cal_table));

	mark_as_complete_cb (NULL, NULL, cal_table);
}

/* Used from e_table_selected_row_foreach(); puts the selected row number in an
 * int pointed to by the closure data.
 */
static void
get_selected_row_cb (int model_row, gpointer data)
{
	int *row;

	row = data;
	*row = model_row;
}

/* Returns the component that is selected in the table; only works if there is
 * one and only one selected row.
 */
ECalModelComponent *
e_calendar_table_get_selected_comp (ECalendarTable *cal_table)
{
	ETable *etable;
	int row;

	etable = e_table_scrolled_get_table (E_TABLE_SCROLLED (cal_table->etable));
	if (e_table_selected_count (etable) != 1)
		return NULL;

	row = -1;
	e_table_selected_row_foreach (etable,
				      get_selected_row_cb,
				      &row);
	g_return_val_if_fail (row != -1, NULL);

	return e_cal_model_get_component_at (cal_table->model, row);
}

struct get_selected_uids_closure {
	ECalendarTable *cal_table;
	GSList *objects;
};

/* Used from e_table_selected_row_foreach(), builds a list of the selected UIDs */
static void
add_uid_cb (int model_row, gpointer data)
{
	struct get_selected_uids_closure *closure;
	ECalModelComponent *comp_data;

	closure = data;

	comp_data = e_cal_model_get_component_at (closure->cal_table->model, model_row);

	closure->objects = g_slist_prepend (closure->objects, comp_data);
}

static GSList *
get_selected_objects (ECalendarTable *cal_table)
{
	struct get_selected_uids_closure closure;
	ETable *etable;

	closure.cal_table = cal_table;
	closure.objects = NULL;

	etable = e_table_scrolled_get_table (E_TABLE_SCROLLED (cal_table->etable));
	e_table_selected_row_foreach (etable, add_uid_cb, &closure);

	return closure.objects;
}

/* Deletes all of the selected components in the table */
static void
delete_selected_components (ECalendarTable *cal_table)
{
	GSList *objs, *l;

	objs = get_selected_objects (cal_table);

	e_calendar_table_set_status_message (cal_table, _("Deleting selected objects"), -1);

	for (l = objs; l; l = l->next) {
		ECalModelComponent *comp_data = (ECalModelComponent *) l->data;
		GError *error = NULL;

		e_cal_remove_object (comp_data->client,
				     icalcomponent_get_uid (comp_data->icalcomp), &error);
		delete_error_dialog (error, E_CAL_COMPONENT_TODO);
		g_clear_error (&error);
	}

	e_calendar_table_set_status_message (cal_table, NULL, -1);

	g_slist_free (objs);
}
static void
add_retract_data (ECalComponent *comp, const char *retract_comment)
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

/**
 * e_calendar_table_delete_selected:
 * @cal_table: A calendar table.
 *
 * Deletes the selected components in the table; asks the user first.
 **/
void
e_calendar_table_delete_selected (ECalendarTable *cal_table)
{
	ETable *etable;
	int n_selected;
	ECalModelComponent *comp_data;
	ECalComponent *comp = NULL;
	gboolean  delete = FALSE;
	GError *error = NULL;

	g_return_if_fail (cal_table != NULL);
	g_return_if_fail (E_IS_CALENDAR_TABLE (cal_table));

	etable = e_table_scrolled_get_table (E_TABLE_SCROLLED (cal_table->etable));

	n_selected = e_table_selected_count (etable);
	if (n_selected <= 0)
		return;

	if (n_selected == 1)
		comp_data = e_calendar_table_get_selected_comp (cal_table);
	else
		comp_data = NULL;

	/* FIXME: this may be something other than a TODO component */

	if (comp_data) {
		comp = e_cal_component_new ();
		e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (comp_data->icalcomp));
	}

	if ((n_selected == 1) && comp && check_for_retract (comp, comp_data->client)) {
		char *retract_comment = NULL;
		gboolean retract = FALSE;

		retract = prompt_retract_dialog (comp, &retract_comment, GTK_WIDGET (cal_table));
		delete = retract;
		if (retract) {
			GList *users = NULL;
			icalcomponent *icalcomp = NULL, *mod_comp = NULL;

			add_retract_data (comp, retract_comment);
			icalcomp = e_cal_component_get_icalcomponent (comp);
			icalcomponent_set_method (icalcomp, ICAL_METHOD_CANCEL);
			if (!e_cal_send_objects (comp_data->client, icalcomp, &users,
						&mod_comp, &error))	{
				delete_error_dialog (error, E_CAL_COMPONENT_TODO);
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
	} else {
		delete = delete_component_dialog (comp, FALSE, n_selected, E_CAL_COMPONENT_TODO, GTK_WIDGET (cal_table));
	}

	if (delete)
		delete_selected_components (cal_table);

	/* free memory */
	if (comp)
		g_object_unref (comp);
}

/**
 * e_calendar_table_get_selected:
 * @cal_table:
 *
 * Get the currently selected ECalModelComponent's on the table.
 *
 * Return value: A GSList of the components, which should be
 * g_slist_free'd when finished with.
 **/
GSList *
e_calendar_table_get_selected (ECalendarTable *cal_table)
{
	return get_selected_objects(cal_table);
}

/**
 * e_calendar_table_cut_clipboard:
 * @cal_table: A calendar table.
 *
 * Cuts selected tasks in the given calendar table
 */
void
e_calendar_table_cut_clipboard (ECalendarTable *cal_table)
{
	g_return_if_fail (E_IS_CALENDAR_TABLE (cal_table));

	e_calendar_table_copy_clipboard (cal_table);
	delete_selected_components (cal_table);
}

static void
clipboard_get_calendar_cb (GtkClipboard *clipboard,
			   GtkSelectionData *selection_data,
			   guint info,
			   gpointer data)
{
	gchar *comp_str = (gchar *) data;

	switch (info) {
	case TARGET_TYPE_VCALENDAR:
		gtk_selection_data_set (selection_data,
					gdk_atom_intern (target_types[info].target, FALSE), 8,
					(const guchar *) comp_str,
					(gint) strlen (comp_str));
		break;
	default:
		break;
	}
}

/* callback for e_table_selected_row_foreach */
static void
copy_row_cb (int model_row, gpointer data)
{
	ECalendarTable *cal_table;
	ECalModelComponent *comp_data;
	gchar *comp_str;
	icalcomponent *child;

	cal_table = E_CALENDAR_TABLE (data);

	g_return_if_fail (cal_table->tmp_vcal != NULL);

	comp_data = e_cal_model_get_component_at (cal_table->model, model_row);
	if (!comp_data)
		return;

	/* add timezones to the VCALENDAR component */
	e_cal_util_add_timezones_from_component (cal_table->tmp_vcal, comp_data->icalcomp);

	/* add the new component to the VCALENDAR component */
	comp_str = icalcomponent_as_ical_string (comp_data->icalcomp);
	child = icalparser_parse_string (comp_str);
	if (child) {
		icalcomponent_add_component (cal_table->tmp_vcal,
					     icalcomponent_new_clone (child));
		icalcomponent_free (child);
	}
	g_free (child);
}

/**
 * e_calendar_table_copy_clipboard:
 * @cal_table: A calendar table.
 *
 * Copies selected tasks into the clipboard
 */
void
e_calendar_table_copy_clipboard (ECalendarTable *cal_table)
{
	ETable *etable;
	GtkClipboard *clipboard;
	char *comp_str;

	g_return_if_fail (E_IS_CALENDAR_TABLE (cal_table));

	/* create temporary VCALENDAR object */
	cal_table->tmp_vcal = e_cal_util_new_top_level ();

	etable = e_table_scrolled_get_table (E_TABLE_SCROLLED (cal_table->etable));
	e_table_selected_row_foreach (etable, copy_row_cb, cal_table);
	comp_str = icalcomponent_as_ical_string (cal_table->tmp_vcal);
	clipboard = gtk_widget_get_clipboard (GTK_WIDGET (cal_table), clipboard_atom);
	if (!gtk_clipboard_set_with_data(clipboard, target_types, n_target_types,
					 clipboard_get_calendar_cb,
					 NULL, comp_str)) {

		/* do not free this pointer, it owns libical */
		/* g_free (comp_str); */
	} else {
		gtk_clipboard_set_can_store (clipboard, target_types + 1, n_target_types - 1);
	}

	/* free memory */
	icalcomponent_free (cal_table->tmp_vcal);
	g_free (comp_str);
	cal_table->tmp_vcal = NULL;
}

static void
clipboard_get_calendar_data (ECalendarTable *cal_table, const gchar *text)
{
	icalcomponent *icalcomp;
	char *uid;
	ECalComponent *comp;
	ECal *client;
	icalcomponent_kind kind;

	g_return_if_fail (E_IS_CALENDAR_TABLE (cal_table));

	if (!text || !*text)
		return;

	icalcomp = icalparser_parse_string (text);
	if (!icalcomp)
		return;

	/* check the type of the component */
	kind = icalcomponent_isa (icalcomp);
	if (kind != ICAL_VCALENDAR_COMPONENT &&
	    kind != ICAL_VEVENT_COMPONENT &&
	    kind != ICAL_VTODO_COMPONENT &&
	    kind != ICAL_VJOURNAL_COMPONENT) {
		return;
	}

	client = e_cal_model_get_default_client (cal_table->model);

	e_calendar_table_set_status_message (cal_table, _("Updating objects"), -1);

	if (kind == ICAL_VCALENDAR_COMPONENT) {
		icalcomponent_kind child_kind;
		icalcomponent *subcomp;
		icalcomponent *vcal_comp;

		vcal_comp = icalcomp;
		subcomp = icalcomponent_get_first_component (
			vcal_comp, ICAL_ANY_COMPONENT);
		while (subcomp) {
			child_kind = icalcomponent_isa (subcomp);
			if (child_kind == ICAL_VEVENT_COMPONENT ||
			    child_kind == ICAL_VTODO_COMPONENT ||
			    child_kind == ICAL_VJOURNAL_COMPONENT) {
				ECalComponent *tmp_comp;

				uid = e_cal_component_gen_uid ();
				tmp_comp = e_cal_component_new ();
				e_cal_component_set_icalcomponent (
					tmp_comp, icalcomponent_new_clone (subcomp));
				e_cal_component_set_uid (tmp_comp, uid);
				free (uid);

				/* FIXME should we convert start/due/complete times? */
				/* FIXME Error handling */
				e_cal_create_object (client, e_cal_component_get_icalcomponent (tmp_comp), NULL, NULL);

				g_object_unref (tmp_comp);
			}
			subcomp = icalcomponent_get_next_component (
				vcal_comp, ICAL_ANY_COMPONENT);
		}
	} else {
		comp = e_cal_component_new ();
		e_cal_component_set_icalcomponent (comp, icalcomp);
		uid = e_cal_component_gen_uid ();
		e_cal_component_set_uid (comp, (const char *) uid);
		free (uid);

		e_cal_create_object (client, e_cal_component_get_icalcomponent (comp), NULL, NULL);

		g_object_unref (comp);
	}

	e_calendar_table_set_status_message (cal_table, NULL, -1);
}

static void
clipboard_paste_received_cb (GtkClipboard *clipboard,
			     GtkSelectionData *selection_data,
			     gpointer data)
{
	ECalendarTable *cal_table = E_CALENDAR_TABLE (data);
	ETable *e_table = e_table_scrolled_get_table (E_TABLE_SCROLLED (cal_table->etable));
	GnomeCanvas *canvas = e_table->table_canvas;
	GnomeCanvasItem *item = GNOME_CANVAS (canvas)->focused_item;

	if (gtk_clipboard_wait_is_text_available (clipboard) &&
	    GTK_WIDGET_HAS_FOCUS (canvas) &&
	    E_IS_TABLE_ITEM (item) &&
	    E_TABLE_ITEM (item)->editing_col >= 0 &&
	    E_TABLE_ITEM (item)->editing_row >= 0) {
		ETableItem *eti = E_TABLE_ITEM (item);
		ECellView *cell_view = eti->cell_views[eti->editing_col];
		e_cell_text_paste_clipboard (cell_view, eti->editing_col, eti->editing_row);
	} else {
		GdkAtom type = selection_data->type;
		if (type == gdk_atom_intern (target_types[TARGET_TYPE_VCALENDAR].target, TRUE)) {
			gchar *result = NULL;
			result = g_strndup ((const gchar *) selection_data->data,
					    selection_data->length);
			clipboard_get_calendar_data (cal_table, result);
			g_free (result);
		}
	}
	g_object_unref (cal_table);
}

/**
 * e_calendar_table_paste_clipboard:
 * @cal_table: A calendar table.
 *
 * Pastes tasks currently in the clipboard into the given calendar table
 */
void
e_calendar_table_paste_clipboard (ECalendarTable *cal_table)
{
	GtkClipboard *clipboard;
	g_return_if_fail (E_IS_CALENDAR_TABLE (cal_table));

	clipboard = gtk_widget_get_clipboard (GTK_WIDGET (cal_table), clipboard_atom);
	g_object_ref (cal_table);

	gtk_clipboard_request_contents (clipboard,
					gdk_atom_intern (target_types[0].target, FALSE),
					clipboard_paste_received_cb, cal_table);
}

/* Opens a task in the task editor */
void
e_calendar_table_open_task (ECalendarTable *cal_table, ECal *client, icalcomponent *icalcomp, gboolean assign)
{
	CompEditor *tedit;
	const char *uid;
	guint32 flags = 0;

	uid = icalcomponent_get_uid (icalcomp);

	tedit = e_comp_editor_registry_find (comp_editor_registry, uid);
	if (tedit == NULL) {
		ECalComponent *comp;

		comp = e_cal_component_new ();
		e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (icalcomp));

		if (assign) {
			flags |= COMP_EDITOR_IS_ASSIGNED;

			if (itip_organizer_is_user (comp, client) ||
					!e_cal_component_has_attendees (comp))
				flags |= COMP_EDITOR_USER_ORG;
		}

		tedit = COMP_EDITOR (task_editor_new (client, flags));
		comp_editor_edit_comp (tedit, comp);
		g_object_unref (comp);

		if (flags & COMP_EDITOR_IS_ASSIGNED)
			task_editor_show_assignment (TASK_EDITOR (tedit));

		e_comp_editor_registry_add (comp_editor_registry, tedit, FALSE);
	}
	comp_editor_focus (tedit);
}

/* Opens the task in the specified row */
static void
open_task_by_row (ECalendarTable *cal_table, int row)
{
	ECalModelComponent *comp_data;
	icalproperty *prop;

	comp_data = e_cal_model_get_component_at (cal_table->model, row);
	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_ATTENDEE_PROPERTY);
	e_calendar_table_open_task (cal_table, comp_data->client, comp_data->icalcomp, prop ? TRUE : FALSE);
}

static void
e_calendar_table_on_double_click (ETable *table,
				  gint row,
				  gint col,
				  GdkEvent *event,
				  ECalendarTable *cal_table)
{
	open_task_by_row (cal_table, row);
}

/* popup menu callbacks */

static void
e_calendar_table_on_open_task (EPopup *ep, EPopupItem *pitem, void *data)
{
	ECalendarTable *cal_table = data;
	ECalModelComponent *comp_data;
	icalproperty *prop;

	comp_data = e_calendar_table_get_selected_comp (cal_table);
	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_ATTENDEE_PROPERTY);
	if (comp_data)
		e_calendar_table_open_task (cal_table, comp_data->client, comp_data->icalcomp, prop ? TRUE : FALSE);
}

static void
e_calendar_table_on_save_as (EPopup *ep, EPopupItem *pitem, void *data)
{
	ECalendarTable *cal_table = data;
	ECalModelComponent *comp_data;
	char *filename;
	char *ical_string;

	comp_data = e_calendar_table_get_selected_comp (cal_table);
	if (comp_data == NULL)
		return;

	filename = e_file_dialog_save (_("Save as..."), NULL);
	if (filename == NULL)
		return;

	ical_string = e_cal_get_component_as_string (comp_data->client, comp_data->icalcomp);
	if (ical_string == NULL) {
		g_warning ("Couldn't convert item to a string");
		return;
	}

	e_write_file_uri (filename, ical_string);

	g_free (ical_string);
}

static void
e_calendar_table_on_print_task (EPopup *ep, EPopupItem *pitem, void *data)
{
	ECalendarTable *cal_table = data;
	ECalModelComponent *comp_data;
	ECalComponent *comp;

	comp_data = e_calendar_table_get_selected_comp (cal_table);
	if (comp_data == NULL)
		return;

	comp = e_cal_component_new ();
	e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (comp_data->icalcomp));
	print_comp (comp, comp_data->client, GTK_PRINT_OPERATION_ACTION_PRINT_DIALOG);

	g_object_unref (comp);
}

static void
e_calendar_table_on_cut (EPopup *ep, EPopupItem *pitem, void *data)
{
	ECalendarTable *cal_table = data;

	e_calendar_table_cut_clipboard (cal_table);
}

static void
e_calendar_table_on_copy (EPopup *ep, EPopupItem *pitem, void *data)
{
	ECalendarTable *cal_table = data;

	e_calendar_table_copy_clipboard (cal_table);
}

static void
e_calendar_table_on_paste (EPopup *ep, EPopupItem *pitem, void *data)
{
	ECalendarTable *cal_table = data;

	e_calendar_table_paste_clipboard (cal_table);
}

static void
e_calendar_table_on_assign (EPopup *ep, EPopupItem *pitem, void *data)
{
	ECalendarTable *cal_table = data;
	ECalModelComponent *comp_data;

	comp_data = e_calendar_table_get_selected_comp (cal_table);
	if (comp_data)
		e_calendar_table_open_task (cal_table, comp_data->client, comp_data->icalcomp, TRUE);
}

static void
e_calendar_table_on_forward (EPopup *ep, EPopupItem *pitem, void *data)
{
	ECalendarTable *cal_table = data;
	ECalModelComponent *comp_data;

	comp_data = e_calendar_table_get_selected_comp (cal_table);
	if (comp_data) {
		ECalComponent *comp;

		comp = e_cal_component_new ();
		e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (comp_data->icalcomp));
		itip_send_comp (E_CAL_COMPONENT_METHOD_PUBLISH, comp, comp_data->client, NULL, NULL, NULL);

		g_object_unref (comp);
	}
}

struct AffectedComponents {
	ECalendarTable *cal_table;
	GSList *components; /* contains pointers to ECalModelComponent */
};

/**
 * get_selected_components_cb
 * Helper function to fill list of selected components in ECalendarTable.
 * This function is called from e_table_selected_row_foreach.
 **/
static void
get_selected_components_cb (int model_row, gpointer data)
{
	struct AffectedComponents *ac = (struct AffectedComponents *) data;

	if (!ac || !ac->cal_table)
		return;

	ac->components = g_slist_prepend (ac->components, e_cal_model_get_component_at (E_CAL_MODEL (ac->cal_table->model), model_row));
}

/**
 * do_for_selected_components
 * Calls function func for all selected components in cal_table.
 *
 * @param cal_table Table with selected components of our interest
 * @param func Function to be called on each selected component from cal_table.
 *        The first parameter of this function is a pointer to ECalModelComponent and
 *        the second parameter of this function is pointer to cal_table
 **/
static void
do_for_selected_components (ECalendarTable *cal_table, GFunc func)
{
	ETable *etable;
	struct AffectedComponents ac;

	g_return_if_fail (cal_table != NULL);

	ac.cal_table = cal_table;
	ac.components = NULL;

	etable = e_table_scrolled_get_table (E_TABLE_SCROLLED (cal_table->etable));
	e_table_selected_row_foreach (etable, get_selected_components_cb, &ac);

	g_slist_foreach (ac.components, func, cal_table);
	g_slist_free (ac.components);
}

/**
 * mark_comp_complete_cb
 * Function used in call to @ref do_for_selected_components to mark each component as complete
 **/
static void
mark_comp_complete_cb (gpointer data, gpointer user_data)
{
	ECalendarTable *cal_table;
	ECalModelComponent *comp_data;

	comp_data = (ECalModelComponent *) data;
	cal_table = E_CALENDAR_TABLE (user_data);

	e_cal_model_tasks_mark_comp_complete (E_CAL_MODEL_TASKS (cal_table->model), comp_data);
}

/**
 * mark_comp_incomplete_cb
 * Function used in call to @ref do_for_selected_components to mark each component as incomplete
 **/
static void
mark_comp_incomplete_cb (gpointer data, gpointer user_data)
{
	ECalendarTable *cal_table;
	ECalModelComponent *comp_data;

	comp_data = (ECalModelComponent *) data;
	cal_table = E_CALENDAR_TABLE (user_data);

	e_cal_model_tasks_mark_comp_incomplete (E_CAL_MODEL_TASKS (cal_table->model), comp_data);
}

/* Callback used for the "mark tasks as incomplete" menu item */
static void
mark_as_incomplete_cb (EPopup *ep, EPopupItem *pitem, void *data)
{
	do_for_selected_components (data, mark_comp_incomplete_cb);
}

/* Callback used for the "mark tasks as complete" menu item */
static void
mark_as_complete_cb (EPopup *ep, EPopupItem *pitem, void *data)
{
	do_for_selected_components (data, mark_comp_complete_cb);
}

/* Opens the URL of the task */
static void
open_url_cb (EPopup *ep, EPopupItem *pitem, void *data)
{
	ECalendarTable *cal_table = data;
	ECalModelComponent *comp_data;
	icalproperty *prop;

	comp_data = e_calendar_table_get_selected_comp (cal_table);
	if (!comp_data)
		return;

	prop = icalcomponent_get_first_property (comp_data->icalcomp, ICAL_URL_PROPERTY);
	if (!prop)
		return;

	gnome_url_show (icalproperty_get_url (prop), NULL);
}

/* Opens a new task editor */
static void
on_new_task (EPopup *ep, EPopupItem *pitem, void *data)
{
	ECalendarTable *cal_table = data;
	ETasks *tasks = g_object_get_data (G_OBJECT (cal_table), "tasks");

	if (!tasks)
		return;

	e_tasks_new_task (tasks);

}

/* Callback for the "delete tasks" menu item */
static void
delete_cb (EPopup *ep, EPopupItem *pitem, void *data)
{
	ECalendarTable *cal_table = data;

	e_calendar_table_delete_selected (cal_table);
}

static EPopupItem tasks_popup_items [] = {
	{ E_POPUP_ITEM, "00.newtask", N_("New _Task"), on_new_task, NULL, "stock_task", 0, 0},
	{ E_POPUP_BAR, "01.bar" },

	{ E_POPUP_ITEM, "03.open", N_("_Open"), e_calendar_table_on_open_task, NULL, GTK_STOCK_OPEN, E_CAL_POPUP_SELECT_ONE },
	{ E_POPUP_ITEM, "05.openweb", N_("Open _Web Page"), open_url_cb, NULL, NULL, E_CAL_POPUP_SELECT_ONE, E_CAL_POPUP_SELECT_HASURL },
	{ E_POPUP_ITEM, "10.saveas", N_("_Save As..."), e_calendar_table_on_save_as, NULL, GTK_STOCK_SAVE_AS, E_CAL_POPUP_SELECT_ONE },
	{ E_POPUP_ITEM, "20.print", N_("P_rint..."), e_calendar_table_on_print_task, NULL, GTK_STOCK_PRINT, E_CAL_POPUP_SELECT_ONE },

	{ E_POPUP_BAR, "30.bar" },

	{ E_POPUP_ITEM, "40.cut", N_("C_ut"), e_calendar_table_on_cut, NULL, GTK_STOCK_CUT, 0, E_CAL_POPUP_SELECT_EDITABLE },
	{ E_POPUP_ITEM, "50.copy", N_("_Copy"), e_calendar_table_on_copy, NULL, GTK_STOCK_COPY, 0, 0 },
	{ E_POPUP_ITEM, "60.paste", N_("_Paste"), e_calendar_table_on_paste, NULL, GTK_STOCK_PASTE, 0, E_CAL_POPUP_SELECT_EDITABLE },

	{ E_POPUP_BAR, "70.bar" },

	{ E_POPUP_ITEM, "80.assign", N_("_Assign Task"), e_calendar_table_on_assign, NULL, NULL, E_CAL_POPUP_SELECT_ONE, E_CAL_POPUP_SELECT_EDITABLE|E_CAL_POPUP_SELECT_ASSIGNABLE },
	{ E_POPUP_ITEM, "90.forward", N_("_Forward as iCalendar"), e_calendar_table_on_forward, NULL, "mail-forward", E_CAL_POPUP_SELECT_ONE },
	{ E_POPUP_ITEM, "a0.markonecomplete", N_("_Mark as Complete"), mark_as_complete_cb, NULL, NULL, E_CAL_POPUP_SELECT_ONE, E_CAL_POPUP_SELECT_EDITABLE | E_CAL_POPUP_SELECT_NOTCOMPLETE},
	{ E_POPUP_ITEM, "b0.markmanycomplete", N_("_Mark Selected Tasks as Complete"), mark_as_complete_cb, NULL, NULL, E_CAL_POPUP_SELECT_MANY, E_CAL_POPUP_SELECT_EDITABLE | E_CAL_POPUP_SELECT_NOTCOMPLETE },
	{ E_POPUP_ITEM, "c0.markoneincomplete", N_("_Mark as Incomplete"), mark_as_incomplete_cb, NULL, NULL, E_CAL_POPUP_SELECT_ONE, E_CAL_POPUP_SELECT_EDITABLE|E_CAL_POPUP_SELECT_COMPLETE},
	{ E_POPUP_ITEM, "d0.markmanyincomplete", N_("_Mark Selected Tasks as Incomplete"), mark_as_incomplete_cb, NULL, NULL, E_CAL_POPUP_SELECT_MANY, E_CAL_POPUP_SELECT_EDITABLE | E_CAL_POPUP_SELECT_COMPLETE },

	{ E_POPUP_BAR, "e0.bar" },

	{ E_POPUP_ITEM, "f0.delete", N_("_Delete"), delete_cb, NULL, GTK_STOCK_DELETE, E_CAL_POPUP_SELECT_ONE, E_CAL_POPUP_SELECT_EDITABLE },
	{ E_POPUP_ITEM, "g0.deletemany", N_("_Delete Selected Tasks"), delete_cb, NULL, GTK_STOCK_DELETE, E_CAL_POPUP_SELECT_MANY, E_CAL_POPUP_SELECT_EDITABLE },
};

static void
ect_popup_free(EPopup *ep, GSList *items, void *data)
{
	g_slist_free(items);
}

static gint
e_calendar_table_show_popup_menu (ETable *table,
				  GdkEvent *gdk_event,
				  ECalendarTable *cal_table)
{
	GtkMenu *menu;
	GSList *selection, *l, *menus = NULL;
	GPtrArray *events;
	ECalPopup *ep;
	ECalPopupTargetSelect *t;
	int i;

	selection = get_selected_objects (cal_table);
	if (!selection)
		return TRUE;

	/** @HookPoint-ECalPopup: Tasks Table Context Menu
	 * @Id: org.gnome.evolution.tasks.table.popup
	 * @Class: org.gnome.evolution.calendar.popup:1.0
	 * @Target: ECalPopupTargetSelect
	 *
	 * The context menu on the tasks table.
	 */
	ep = e_cal_popup_new("org.gnome.evolution.tasks.table.popup");

	events = g_ptr_array_new();
	for (l=selection;l;l=g_slist_next(l))
		g_ptr_array_add(events, e_cal_model_copy_component_data((ECalModelComponent *)l->data));
	g_slist_free(selection);

	t = e_cal_popup_target_new_select(ep, cal_table->model, events);
	t->target.widget = (GtkWidget *)cal_table;

	for (i=0;i<sizeof(tasks_popup_items)/sizeof(tasks_popup_items[0]);i++)
		menus = g_slist_prepend(menus, &tasks_popup_items[i]);
	e_popup_add_items((EPopup *)ep, menus, NULL, ect_popup_free, cal_table);

	menu = e_popup_create_menu_once((EPopup *)ep, (EPopupTarget *)t, 0);

	gtk_menu_popup(menu, NULL, NULL, NULL, NULL, gdk_event?gdk_event->button.button:0,
		       gdk_event?gdk_event->button.time:gtk_get_current_event_time());

	return TRUE;
}

static gint
e_calendar_table_on_right_click (ETable *table,
				 gint row,
				 gint col,
				 GdkEvent *event,
				 ECalendarTable *cal_table)
{
	return e_calendar_table_show_popup_menu (table, event, cal_table);
}

static gboolean
e_calendar_table_on_popup_menu (GtkWidget *widget, gpointer data)
{
	ETable *table = E_TABLE(widget);
	g_return_val_if_fail(table, FALSE);

	return e_calendar_table_show_popup_menu (table, NULL,
						 E_CALENDAR_TABLE(data));
}

static gint
e_calendar_table_on_key_press (ETable *table,
			       gint row,
			       gint col,
			       GdkEventKey *event,
			       ECalendarTable *cal_table)
{
	if (event->keyval == GDK_Delete) {
		delete_cb (NULL, NULL, cal_table);
		return TRUE;
	} else if ((event->keyval == GDK_o)
		   &&(event->state & GDK_CONTROL_MASK)) {
		open_task_by_row (cal_table, row);
		return TRUE;
	}

	return FALSE;
}

static void
hide_completed_rows (ECalModel *model, GList *clients_list, char *hide_sexp, GPtrArray *comp_objects)
{
	GList *l, *m, *objects;
	ECal *client;
	int pos;

	for (l = clients_list; l != NULL; l = l->next) {
		client = l->data;

		if (!e_cal_get_object_list (client, hide_sexp, &objects, NULL)) {
			g_warning (G_STRLOC ": Could not get the objects");

			continue;
		}

		for (m = objects; m; m = m->next) {
			ECalModelComponent *comp_data;
			ECalComponentId *id;
			ECalComponent *comp = e_cal_component_new ();

			e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (m->data));
			id = e_cal_component_get_id (comp);

			if ((comp_data =  e_cal_model_get_component_for_uid (model, id))) {
				e_table_model_pre_change (E_TABLE_MODEL (model));
				pos = get_position_in_array (comp_objects, comp_data);
				e_table_model_row_deleted (E_TABLE_MODEL (model), pos);

				if (g_ptr_array_remove (comp_objects, comp_data))
					e_cal_model_free_component_data (comp_data);
			}
			e_cal_component_free_id (id);
			g_object_unref (comp);
		}

		g_list_foreach (objects, (GFunc) icalcomponent_free, NULL);
		g_list_free (objects);

		/* to notify about changes, because in call of row_deleted there are still all events */
		e_table_model_changed (E_TABLE_MODEL (model));
	}
}

static void
show_completed_rows (ECalModel *model, GList *clients_list, char *show_sexp, GPtrArray *comp_objects)
{
	GList *l, *m, *objects;
	ECal *client;

	for (l = clients_list; l != NULL; l = l->next) {
		client = l->data;

		if (!e_cal_get_object_list (client, show_sexp, &objects, NULL)) {
			g_warning (G_STRLOC ": Could not get the objects");

			continue;
		}

		for (m = objects; m; m = m->next) {
			ECalModelComponent *comp_data;
			ECalComponentId *id;
			ECalComponent *comp = e_cal_component_new ();

			e_cal_component_set_icalcomponent (comp, icalcomponent_new_clone (m->data));
			id = e_cal_component_get_id (comp);

			if (!(e_cal_model_get_component_for_uid (model, id))) {
				e_table_model_pre_change (E_TABLE_MODEL (model));
				comp_data = g_new0 (ECalModelComponent, 1);
				comp_data->client = g_object_ref (client);
				comp_data->icalcomp = icalcomponent_new_clone (m->data);
				e_cal_model_set_instance_times (comp_data,
						e_cal_model_get_timezone (model));
				comp_data->dtstart = comp_data->dtend = comp_data->due = comp_data->completed = NULL;
				comp_data->color = NULL;

				g_ptr_array_add (comp_objects, comp_data);
				e_table_model_row_inserted (E_TABLE_MODEL (model), comp_objects->len - 1);
			}
			e_cal_component_free_id (id);
			g_object_unref (comp);
		}
	}
}

/* Loads the state of the table (headers shown etc.) from the given file. */
void
e_calendar_table_load_state	(ECalendarTable *cal_table,
				 gchar		*filename)
{
	struct stat st;

	g_return_if_fail (E_IS_CALENDAR_TABLE (cal_table));

	if (g_stat (filename, &st) == 0 && st.st_size > 0
	    && S_ISREG (st.st_mode)) {
		e_table_load_state (e_table_scrolled_get_table (E_TABLE_SCROLLED (cal_table->etable)), filename);
	}
}


/* Saves the state of the table (headers shown etc.) to the given file. */
void
e_calendar_table_save_state (ECalendarTable	*cal_table,
			     gchar		*filename)
{
	g_return_if_fail (E_IS_CALENDAR_TABLE (cal_table));

	e_table_save_state (e_table_scrolled_get_table (E_TABLE_SCROLLED (cal_table->etable)),
			    filename);
}

/* Returns the current time, for the ECellDateEdit items.
   FIXME: Should probably use the timezone of the item rather than the
   current timezone, though that may be difficult to get from here. */
static struct tm
e_calendar_table_get_current_time (ECellDateEdit *ecde, gpointer data)
{
	icaltimezone *zone;
	struct tm tmp_tm = { 0 };
	struct icaltimetype tt;

	/* Get the current timezone. */
	zone = calendar_config_get_icaltimezone ();

	tt = icaltime_from_timet_with_zone (time (NULL), FALSE, zone);

	/* Now copy it to the struct tm and return it. */
	tmp_tm.tm_year  = tt.year - 1900;
	tmp_tm.tm_mon   = tt.month - 1;
	tmp_tm.tm_mday  = tt.day;
	tmp_tm.tm_hour  = tt.hour;
	tmp_tm.tm_min   = tt.minute;
	tmp_tm.tm_sec   = tt.second;
	tmp_tm.tm_isdst = -1;

	return tmp_tm;
}


#ifdef TRANSLATORS_ONLY

static char *test[] = {
    N_("Click to add a task")
};

#endif

/* Displays messages on the status bar */
#define EVOLUTION_TASKS_PROGRESS_IMAGE "stock_todo"
static GdkPixbuf *progress_icon = NULL;

void
e_calendar_table_set_activity_handler (ECalendarTable *cal_table, EActivityHandler *activity_handler)
{
	g_return_if_fail (E_IS_CALENDAR_TABLE (cal_table));

	cal_table->activity_handler = activity_handler;
}

void
e_calendar_table_set_status_message (ECalendarTable *cal_table, const gchar *message, int percent)
{
        g_return_if_fail (E_IS_CALENDAR_TABLE (cal_table));

	if (!cal_table->activity_handler)
		return;

        if (!message || !*message) {
		if (cal_table->activity_id != 0) {
			e_activity_handler_operation_finished (cal_table->activity_handler, cal_table->activity_id);
			cal_table->activity_id = 0;
		}
        } else if (cal_table->activity_id == 0) {
                char *client_id = g_strdup_printf ("%p", cal_table);

                if (progress_icon == NULL)
                        progress_icon = e_icon_factory_get_icon (EVOLUTION_TASKS_PROGRESS_IMAGE, E_ICON_SIZE_STATUS);

                cal_table->activity_id = e_activity_handler_operation_started (cal_table->activity_handler, client_id,
									       progress_icon, message, TRUE);

                g_free (client_id);
        } else {

		double progress;

		if (percent < 0)
			progress = -1.0;
		else {
			progress = ((double) percent / 100);
		}

                e_activity_handler_operation_progressing (cal_table->activity_handler, cal_table->activity_id, message, progress);
	}
}

/**
 * e_calendar_table_hide_completed_tasks:
 * @table: A calendar table model.
 * @client_list: Clients List
 *
 * Hide completed tasks.
 */
void
e_calendar_table_process_completed_tasks (ECalendarTable *table, GList *clients_list, gboolean config_changed)
{
	ECalModel *model;
	static GMutex *mutex = NULL;
	char *hide_sexp, *show_sexp;
	GPtrArray *comp_objects = NULL;

	if (!mutex)
		mutex = g_mutex_new ();

	g_mutex_lock (mutex);

	model = e_calendar_table_get_model (table);
	comp_objects = e_cal_model_get_object_array (model);

	hide_sexp = calendar_config_get_hide_completed_tasks_sexp (TRUE);
	show_sexp = calendar_config_get_hide_completed_tasks_sexp (FALSE);

	/* If hide option is unchecked */
	if (!(hide_sexp && show_sexp))
		show_sexp = g_strdup ("(is-completed?)");

	/* Delete rows from model*/
	if (hide_sexp) {
		hide_completed_rows (model, clients_list, hide_sexp, comp_objects);
	}

	/* Insert rows into model */
	if (config_changed) {
		show_completed_rows (model, clients_list, show_sexp, comp_objects);
	}

	g_free (hide_sexp);
	g_free (show_sexp);
	g_mutex_unlock (mutex);
}
