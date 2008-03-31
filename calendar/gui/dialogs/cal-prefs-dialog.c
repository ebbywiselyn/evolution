/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * Authors :
 *  Damon Chaplin <damon@ximian.com>
 *  Ettore Perazzoli <ettore@ximian.com>
 *  David Trowbridge <trowbrds cs colorado edu>
 *
 * Copyright (C) 2005 Novell, Inc. (www.novell.com)
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "../e-cal-config.h"
#include "../e-timezone-entry.h"
#include "../calendar-config.h"
#include "cal-prefs-dialog.h"
#include <widgets/misc/e-dateedit.h>
#include <e-util/e-dialog-widgets.h>
#include <e-util/e-util-private.h>
#include <glib/gi18n.h>
#include <string.h>

static const int week_start_day_map[] = {
	1, 2, 3, 4, 5, 6, 0, -1
};

static const int time_division_map[] = {
	60, 30, 15, 10, 5, -1
};

/* The following two are kept separate in case we need to re-order each menu individually */
static const int hide_completed_units_map[] = {
	CAL_MINUTES, CAL_HOURS, CAL_DAYS, -1
};

static const int default_reminder_units_map[] = {
	CAL_MINUTES, CAL_HOURS, CAL_DAYS, -1
};

static GtkVBoxClass *parent_class = NULL;

GtkWidget *cal_prefs_dialog_create_time_edit (void);



static void
calendar_prefs_dialog_finalize (GObject *obj)
{
	CalendarPrefsDialog *prefs = (CalendarPrefsDialog *) obj;

	g_object_unref (prefs->gui);

	if (prefs->gconf) {
		g_object_unref (prefs->gconf);
		prefs->gconf = NULL;
	}

	((GObjectClass *)(parent_class))->finalize (obj);
}

static void
calendar_prefs_dialog_class_init (CalendarPrefsDialogClass *klass)
{
	GObjectClass *object_class;

	object_class = (GObjectClass *) klass;
	parent_class = g_type_class_ref (GTK_TYPE_VBOX);

	object_class->finalize = calendar_prefs_dialog_finalize;
}

static void
calendar_prefs_dialog_init (CalendarPrefsDialog *dialog)
{
}

static GtkWidget *
eccp_widget_glade (EConfig *ec, EConfigItem *item, GtkWidget *parent, GtkWidget *old, void *data)
{
	CalendarPrefsDialog *prefs = data;

	return glade_xml_get_widget (prefs->gui, item->label);
}

static void
working_days_changed (GtkWidget *widget, CalendarPrefsDialog *prefs)
{
	CalWeekdays working_days = 0;
	guint32 mask = 1;
	int day;

	for (day = 0; day < 7; day++) {
		if (e_dialog_toggle_get (prefs->working_days[day]))
			working_days |= mask;
		mask <<= 1;
	}

	calendar_config_set_working_days (working_days);
}

static void
timezone_changed (GtkWidget *widget, CalendarPrefsDialog *prefs)
{
	icaltimezone *zone;
	icalcomponent *icalcomp, *dl_comp;

	zone = e_timezone_entry_get_timezone (E_TIMEZONE_ENTRY (prefs->timezone));
	icalcomp = icaltimezone_get_component (zone);

	if (!(dl_comp = icalcomponent_get_first_component (icalcomp, ICAL_XDAYLIGHT_COMPONENT)))
		gtk_widget_set_sensitive ((GtkWidget *) prefs->daylight_saving, FALSE);
	else
		gtk_widget_set_sensitive ((GtkWidget *) prefs->daylight_saving, TRUE);

	calendar_config_set_timezone (icaltimezone_get_location (zone));
}

static void
daylight_saving_changed (GtkWidget *widget, CalendarPrefsDialog *prefs)
{
	gboolean set = gtk_toggle_button_get_active ((GtkToggleButton *) prefs->daylight_saving);
	calendar_config_set_daylight_saving (set);
	timezone_changed (widget, prefs);
}

static void
start_of_day_changed (GtkWidget *widget, CalendarPrefsDialog *prefs)
{
	int start_hour, start_minute, end_hour, end_minute;
	EDateEdit *start, *end;

	start = E_DATE_EDIT (prefs->start_of_day);
	end = E_DATE_EDIT (prefs->end_of_day);

	e_date_edit_get_time_of_day (start, &start_hour, &start_minute);
	e_date_edit_get_time_of_day (end, &end_hour, &end_minute);

	if ((start_hour > end_hour) || (start_hour == end_hour && start_minute > end_minute)) {
		if (start_hour < 23)
			e_date_edit_set_time_of_day (end, start_hour + 1, start_minute);
		else
			e_date_edit_set_time_of_day (end, 23, 59);

		return;
	}

	calendar_config_set_day_start_hour (start_hour);
	calendar_config_set_day_start_minute (start_minute);
}

static void
end_of_day_changed (GtkWidget *widget, CalendarPrefsDialog *prefs)
{
	int start_hour, start_minute, end_hour, end_minute;
	EDateEdit *start, *end;

	start = E_DATE_EDIT (prefs->start_of_day);
	end = E_DATE_EDIT (prefs->end_of_day);

	e_date_edit_get_time_of_day (start, &start_hour, &start_minute);
	e_date_edit_get_time_of_day (end, &end_hour, &end_minute);

	if ((end_hour < start_hour) || (end_hour == start_hour && end_minute < start_minute)) {
		if (end_hour < 1)
			e_date_edit_set_time_of_day (start, 0, 0);
		else
			e_date_edit_set_time_of_day (start, end_hour - 1, end_minute);

		return;
	}

	calendar_config_set_day_end_hour (end_hour);
	calendar_config_set_day_end_minute (end_minute);
}
static void
week_start_day_changed (GtkWidget *widget, CalendarPrefsDialog *prefs)
{
	int week_start_day;

	week_start_day = e_dialog_combo_box_get (prefs->week_start_day, week_start_day_map);
	calendar_config_set_week_start_day (week_start_day);
}

static void
use_24_hour_toggled (GtkToggleButton *toggle, CalendarPrefsDialog *prefs)
{
	gboolean use_24_hour;

	use_24_hour = gtk_toggle_button_get_active (toggle);

	e_date_edit_set_use_24_hour_format (E_DATE_EDIT (prefs->start_of_day), use_24_hour);
	e_date_edit_set_use_24_hour_format (E_DATE_EDIT (prefs->end_of_day), use_24_hour);

	calendar_config_set_24_hour_format (use_24_hour);
}

static void
time_divisions_changed (GtkWidget *widget, CalendarPrefsDialog *prefs)
{
	int time_divisions;

	time_divisions = e_dialog_combo_box_get (prefs->time_divisions, time_division_map);
	calendar_config_set_time_divisions (time_divisions);
}

static void
show_end_times_toggled (GtkToggleButton *toggle, CalendarPrefsDialog *prefs)
{
	calendar_config_set_show_event_end (gtk_toggle_button_get_active (toggle));
}

static void
compress_weekend_toggled (GtkToggleButton *toggle, CalendarPrefsDialog *prefs)
{
	calendar_config_set_compress_weekend (gtk_toggle_button_get_active (toggle));
}

static void
dnav_show_week_no_toggled (GtkToggleButton *toggle, CalendarPrefsDialog *prefs)
{
	calendar_config_set_dnav_show_week_no (gtk_toggle_button_get_active (toggle));
}

static void
hide_completed_tasks_toggled (GtkToggleButton *toggle, CalendarPrefsDialog *prefs)
{
	gboolean hide;

	hide = gtk_toggle_button_get_active (toggle);

	gtk_widget_set_sensitive (prefs->tasks_hide_completed_interval, hide);
	gtk_widget_set_sensitive (prefs->tasks_hide_completed_units, hide);

	calendar_config_set_hide_completed_tasks (hide);
}

static void
hide_completed_tasks_changed (GtkWidget *widget, CalendarPrefsDialog *prefs)
{
	calendar_config_set_hide_completed_tasks_value (e_dialog_spin_get_int (prefs->tasks_hide_completed_interval));
}

static void
hide_completed_tasks_units_changed (GtkWidget *widget, CalendarPrefsDialog *prefs)
{
	calendar_config_set_hide_completed_tasks_units (
		e_dialog_combo_box_get (prefs->tasks_hide_completed_units, hide_completed_units_map));
}

static void
tasks_due_today_set_color (GtkColorButton *color_button, CalendarPrefsDialog *prefs)
{
	GdkColor color;

	gtk_color_button_get_color (color_button, &color);
	calendar_config_set_tasks_due_today_color (&color);
}

static void
tasks_overdue_set_color (GtkColorButton *color_button, CalendarPrefsDialog *prefs)
{
	GdkColor color;

	gtk_color_button_get_color (color_button, &color);
	calendar_config_set_tasks_overdue_color (&color);
}

static void
confirm_delete_toggled (GtkToggleButton *toggle, CalendarPrefsDialog *prefs)
{
	calendar_config_set_confirm_delete (gtk_toggle_button_get_active (toggle));
}

static void
default_reminder_toggled (GtkToggleButton *toggle, CalendarPrefsDialog *prefs)
{
	calendar_config_set_use_default_reminder (gtk_toggle_button_get_active (toggle));
}

static void
default_reminder_interval_changed (GtkWidget *widget, CalendarPrefsDialog *prefs)
{
	const gchar *str;
	gdouble value;

	str = gtk_entry_get_text (GTK_ENTRY (widget));
	value = g_ascii_strtod (str, NULL);

	calendar_config_set_default_reminder_interval (value);
}

static void
default_reminder_units_changed (GtkWidget *widget, CalendarPrefsDialog *prefs)
{
	calendar_config_set_default_reminder_units (
		e_dialog_combo_box_get (prefs->default_reminder_units, default_reminder_units_map));
}

static void
alarms_selection_changed (ESourceSelector *selector, CalendarPrefsDialog *prefs)
{
	ESourceList *source_list = prefs->alarms_list;
	GSList *selection;
	GSList *l;
	GSList *groups;
	ESource *source;
	const gchar *alarm;

	/* first we clear all the alarm flags from all sources */
	g_message ("Clearing selection");
	for (groups = e_source_list_peek_groups (source_list); groups; groups = groups->next) {
		ESourceGroup *group = E_SOURCE_GROUP (groups->data);
		GSList *sources;
		for (sources = e_source_group_peek_sources (group); sources; sources = sources->next) {
			source = E_SOURCE (sources->data);

			alarm = e_source_get_property (source, "alarm");
			if (alarm && !g_ascii_strcasecmp (alarm, "never"))
				continue;

			g_message ("Unsetting for %s", e_source_peek_name (source));
			e_source_set_property (source, "alarm", "false");
		}
	}

	/* then we loop over the selector's selection, setting the
	   property on those sources */
	selection = e_source_selector_get_selection (selector);
	for (l = selection; l; l = l->next) {
		source = E_SOURCE (l->data);

		alarm = (gchar *)e_source_get_property (source, "alarm");
		if (alarm && !g_ascii_strcasecmp (alarm, "never"))
			continue;

		g_message ("Setting for %s", e_source_peek_name (E_SOURCE (l->data)));
		e_source_set_property (E_SOURCE (l->data), "alarm", "true");
	}
	e_source_selector_free_selection (selection);

	/* FIXME show an error if this fails? */
	e_source_list_sync (source_list, NULL);
}

static void
template_url_changed (GtkEntry *entry, CalendarPrefsDialog *prefs)
{
	calendar_config_set_free_busy_template (gtk_entry_get_text (entry));
}

static void
setup_changes (CalendarPrefsDialog *prefs)
{
	int i;

	for (i = 0; i < 7; i ++)
		g_signal_connect (G_OBJECT (prefs->working_days[i]), "toggled", G_CALLBACK (working_days_changed), prefs);

	g_signal_connect (G_OBJECT (prefs->timezone), "changed", G_CALLBACK (timezone_changed), prefs);
	g_signal_connect (G_OBJECT (prefs->daylight_saving), "toggled", G_CALLBACK (daylight_saving_changed), prefs);

	g_signal_connect (G_OBJECT (prefs->start_of_day), "changed", G_CALLBACK (start_of_day_changed), prefs);
	g_signal_connect (G_OBJECT (prefs->end_of_day), "changed", G_CALLBACK (end_of_day_changed), prefs);

	g_signal_connect (G_OBJECT (prefs->week_start_day), "changed", G_CALLBACK (week_start_day_changed), prefs);

	g_signal_connect (G_OBJECT (prefs->use_24_hour), "toggled", G_CALLBACK (use_24_hour_toggled), prefs);

	g_signal_connect (G_OBJECT (prefs->time_divisions), "changed", G_CALLBACK (time_divisions_changed), prefs);

	g_signal_connect (G_OBJECT (prefs->show_end_times), "toggled", G_CALLBACK (show_end_times_toggled), prefs);
	g_signal_connect (G_OBJECT (prefs->compress_weekend), "toggled", G_CALLBACK (compress_weekend_toggled), prefs);
	g_signal_connect (G_OBJECT (prefs->dnav_show_week_no), "toggled", G_CALLBACK (dnav_show_week_no_toggled), prefs);

	g_signal_connect (G_OBJECT (prefs->tasks_hide_completed), "toggled",
			  G_CALLBACK (hide_completed_tasks_toggled), prefs);
	g_signal_connect (G_OBJECT (prefs->tasks_hide_completed_interval), "value-changed",
			  G_CALLBACK (hide_completed_tasks_changed), prefs);
	g_signal_connect (G_OBJECT (prefs->tasks_hide_completed_units), "changed", G_CALLBACK (hide_completed_tasks_units_changed), prefs);
	g_signal_connect (G_OBJECT (prefs->tasks_due_today_color), "color-set",
			  G_CALLBACK (tasks_due_today_set_color), prefs);
	g_signal_connect (G_OBJECT (prefs->tasks_overdue_color), "color-set",
			  G_CALLBACK (tasks_overdue_set_color), prefs);

	g_signal_connect (G_OBJECT (prefs->confirm_delete), "toggled", G_CALLBACK (confirm_delete_toggled), prefs);
	g_signal_connect (G_OBJECT (prefs->default_reminder), "toggled", G_CALLBACK (default_reminder_toggled), prefs);
	g_signal_connect (G_OBJECT (prefs->default_reminder_interval), "changed",
			  G_CALLBACK (default_reminder_interval_changed), prefs);
	g_signal_connect (G_OBJECT (prefs->default_reminder_units), "changed", G_CALLBACK (default_reminder_units_changed), prefs);

	g_signal_connect (G_OBJECT (prefs->alarm_list_widget), "selection_changed", G_CALLBACK (alarms_selection_changed), prefs);


	g_signal_connect (G_OBJECT (prefs->template_url), "changed", G_CALLBACK (template_url_changed), prefs);
}

/* Shows the current Free/Busy settings in the dialog */
static void
show_fb_config (CalendarPrefsDialog *prefs)
{
	gchar *template_url;

	template_url = calendar_config_get_free_busy_template ();
	gtk_entry_set_text (GTK_ENTRY (prefs->template_url), (template_url ? template_url : ""));

	g_free (template_url);
}

/* Shows the current task list settings in the dialog */
static void
show_task_list_config (CalendarPrefsDialog *prefs)
{
	GtkColorButton *color_button;
	GdkColor color;
	CalUnits units;
	gboolean hide_completed_tasks;

	color_button = GTK_COLOR_BUTTON (prefs->tasks_due_today_color);
	calendar_config_get_tasks_due_today_color (&color);
	gtk_color_button_set_color (color_button, &color);

	color_button = GTK_COLOR_BUTTON (prefs->tasks_overdue_color);
	calendar_config_get_tasks_overdue_color (&color);
	gtk_color_button_set_color (color_button, &color);

	/* Hide Completed Tasks. */
	hide_completed_tasks = calendar_config_get_hide_completed_tasks ();
	e_dialog_toggle_set (prefs->tasks_hide_completed, hide_completed_tasks);

	/* Hide Completed Tasks Units. */
	units = calendar_config_get_hide_completed_tasks_units ();
	e_dialog_combo_box_set (prefs->tasks_hide_completed_units, units, hide_completed_units_map);

	/* Hide Completed Tasks Value. */
	e_dialog_spin_set (prefs->tasks_hide_completed_interval, calendar_config_get_hide_completed_tasks_value ());

	gtk_widget_set_sensitive (prefs->tasks_hide_completed_interval, hide_completed_tasks);
	gtk_widget_set_sensitive (prefs->tasks_hide_completed_units, hide_completed_tasks);
}

static void
initialize_selection (ESourceSelector *selector, ESourceList *source_list)
{
	GSList *groups;

	for (groups = e_source_list_peek_groups (source_list); groups; groups = groups->next) {
		ESourceGroup *group = E_SOURCE_GROUP (groups->data);
		GSList *sources;
		for (sources = e_source_group_peek_sources (group); sources; sources = sources->next) {
			ESource *source = E_SOURCE (sources->data);
			const char *completion = e_source_get_property (source, "alarm");
			if (!completion  || !g_ascii_strcasecmp (completion, "true")) {
				if (!completion)
					e_source_set_property (E_SOURCE (source), "alarm", "true");
				e_source_selector_select_source (selector, source);
			}
		}
	}
}

static void
show_alarms_config (CalendarPrefsDialog *prefs)
{
	if (!e_cal_get_sources (&prefs->alarms_list, E_CAL_SOURCE_TYPE_EVENT, NULL))
		return;

	prefs->alarm_list_widget = e_source_selector_new (prefs->alarms_list);
	atk_object_set_name (gtk_widget_get_accessible (prefs->alarm_list_widget), _("Selected Calendars for Alarms"));
	gtk_container_add (GTK_CONTAINER (prefs->scrolled_window), prefs->alarm_list_widget);
	gtk_widget_show (prefs->alarm_list_widget);
	initialize_selection (E_SOURCE_SELECTOR (prefs->alarm_list_widget), prefs->alarms_list);
}

/* Shows the current config settings in the dialog. */
static void
show_config (CalendarPrefsDialog *prefs)
{
	CalWeekdays working_days;
	gint mask, day, week_start_day, time_divisions;
	icaltimezone *zone;
	gboolean sensitive, set = FALSE;
	icalcomponent *icalcomp, *dl_comp;
	char *location;

	/* Timezone. */
	location = calendar_config_get_timezone ();
	zone = icaltimezone_get_builtin_timezone (location);
	e_timezone_entry_set_timezone (E_TIMEZONE_ENTRY (prefs->timezone), zone);
	g_free (location);

	icalcomp = icaltimezone_get_component (zone);
	if (!(dl_comp = icalcomponent_get_first_component (icalcomp, ICAL_XDAYLIGHT_COMPONENT)))
		gtk_widget_set_sensitive ((GtkWidget *) prefs->daylight_saving, FALSE);
	else
		gtk_widget_set_sensitive ((GtkWidget *) prefs->daylight_saving, TRUE);

	set = calendar_config_get_daylight_saving ();
	gtk_toggle_button_set_active ((GtkToggleButton *) prefs->daylight_saving, set);

	/* Working Days. */
	working_days = calendar_config_get_working_days ();
	mask = 1 << 0;
	for (day = 0; day < 7; day++) {
		e_dialog_toggle_set (prefs->working_days[day], (working_days & mask) ? TRUE : FALSE);
		mask <<= 1;
	}

	/* Week Start Day. */
	week_start_day = calendar_config_get_week_start_day ();
	e_dialog_combo_box_set (prefs->week_start_day, week_start_day, week_start_day_map);

	/* Start of Day. */
	e_date_edit_set_time_of_day (E_DATE_EDIT (prefs->start_of_day), calendar_config_get_day_start_hour (), calendar_config_get_day_start_minute ());

	/* End of Day. */
	e_date_edit_set_time_of_day (E_DATE_EDIT (prefs->end_of_day), calendar_config_get_day_end_hour (), calendar_config_get_day_end_minute ());

	/* 12/24 Hour Format. */
	if (calendar_config_get_24_hour_format ())
		e_dialog_toggle_set (prefs->use_24_hour, TRUE);
	else
		e_dialog_toggle_set (prefs->use_12_hour, TRUE);

	sensitive = calendar_config_locale_supports_12_hour_format ();
	gtk_widget_set_sensitive (prefs->use_12_hour, sensitive);
	gtk_widget_set_sensitive (prefs->use_24_hour, sensitive);


	/* Time Divisions. */
	time_divisions = calendar_config_get_time_divisions ();
	e_dialog_combo_box_set (prefs->time_divisions, time_divisions, time_division_map);

	/* Show Appointment End Times. */
	e_dialog_toggle_set (prefs->show_end_times, calendar_config_get_show_event_end ());

	/* Compress Weekend. */
	e_dialog_toggle_set (prefs->compress_weekend, calendar_config_get_compress_weekend ());

	/* Date Navigator - Show Week Numbers. */
	e_dialog_toggle_set (prefs->dnav_show_week_no, calendar_config_get_dnav_show_week_no ());

	/* Task list */
	show_task_list_config (prefs);

	/* Alarms list*/
	show_alarms_config (prefs);

	/* Free/Busy */
	show_fb_config (prefs);

	/* Other page */
	e_dialog_toggle_set (prefs->confirm_delete, calendar_config_get_confirm_delete ());
	e_dialog_toggle_set (prefs->default_reminder, calendar_config_get_use_default_reminder ());
	e_dialog_spin_set (prefs->default_reminder_interval, calendar_config_get_default_reminder_interval ());
	e_dialog_combo_box_set (prefs->default_reminder_units, calendar_config_get_default_reminder_units (), default_reminder_units_map);
}

/* plugin meta-data */
static ECalConfigItem eccp_items[] = {
	{ E_CONFIG_BOOK,          "",                             "toplevel-notebook",   eccp_widget_glade },
	{ E_CONFIG_PAGE,          "00.general",                   "general",             eccp_widget_glade },
	{ E_CONFIG_SECTION_TABLE, "00.general/00.time",           "time",                eccp_widget_glade },
	{ E_CONFIG_SECTION_TABLE, "00.general/10.workWeek",       "workWeek",            eccp_widget_glade },
	{ E_CONFIG_SECTION,       "00.general/20.alerts",         "alerts",              eccp_widget_glade },
	{ E_CONFIG_PAGE,          "10.display",                   "display",             eccp_widget_glade },
	{ E_CONFIG_SECTION,       "10.display/00.general",        "displayGeneral",      eccp_widget_glade },
	{ E_CONFIG_SECTION,       "10.display/10.taskList",       "taskList",            eccp_widget_glade },
	{ E_CONFIG_PAGE,	  "15.alarms",			  "alarms",		 eccp_widget_glade },
	{ E_CONFIG_PAGE,          "20.freeBusy",                  "freebusy",            eccp_widget_glade },
	{ E_CONFIG_SECTION,       "20.freeBusy/00.defaultServer", "defaultFBServer",     eccp_widget_glade },
};

static void
eccp_free (EConfig *ec, GSList *items, void *data)
{
	g_slist_free (items);
}

static void
calendar_prefs_dialog_construct (CalendarPrefsDialog *prefs)
{
	GladeXML *gui;
	ECalConfig *ec;
	ECalConfigTargetPrefs *target;
	int i;
	GtkWidget *toplevel;
	GSList *l;
	const char *working_day_names[] = {
		"sun_button",
		"mon_button",
		"tue_button",
		"wed_button",
		"thu_button",
		"fri_button",
		"sat_button",
	};
	char *gladefile;

	gladefile = g_build_filename (EVOLUTION_GLADEDIR,
				      "cal-prefs-dialog.glade",
				      NULL);
	gui = glade_xml_new (gladefile, "toplevel-notebook", NULL);
	g_free (gladefile);
	prefs->gui = gui;

	prefs->gconf = gconf_client_get_default ();

	/** @HookPoint-ECalConfig: Calendar Preferences Page
	 * @Id: org.gnome.evolution.calendar.prefs
	 * @Type: E_CONFIG_BOOK
	 * @Class: org.gnome.evolution.calendar.config:1.0
	 * @Target: ECalConfigTargetPrefs
	 *
	 * The mail calendar preferences page
	 */
	ec = e_cal_config_new (E_CONFIG_BOOK, "org.gnome.evolution.calendar.prefs");
	l = NULL;
	for (i = 0; i < G_N_ELEMENTS (eccp_items); i++)
		l = g_slist_prepend (l, &eccp_items[i]);
	e_config_add_items ((EConfig *) ec, l, NULL, NULL, eccp_free, prefs);

	/* General tab */
	prefs->timezone = glade_xml_get_widget (gui, "timezone");
	prefs->daylight_saving = glade_xml_get_widget (gui, "daylight_cb");
	for (i = 0; i < 7; i++)
		prefs->working_days[i] = glade_xml_get_widget (gui, working_day_names[i]);
	prefs->week_start_day = glade_xml_get_widget (gui, "week_start_day");
	prefs->start_of_day = glade_xml_get_widget (gui, "start_of_day");
	prefs->end_of_day = glade_xml_get_widget (gui, "end_of_day");
	prefs->use_12_hour = glade_xml_get_widget (gui, "use_12_hour");
	prefs->use_24_hour = glade_xml_get_widget (gui, "use_24_hour");
	prefs->confirm_delete = glade_xml_get_widget (gui, "confirm_delete");
	prefs->default_reminder = glade_xml_get_widget (gui, "default_reminder");
	prefs->default_reminder_interval = glade_xml_get_widget (gui, "default_reminder_interval");
	prefs->default_reminder_units = glade_xml_get_widget (gui, "default_reminder_units");

	/* Display tab */
	prefs->time_divisions = glade_xml_get_widget (gui, "time_divisions");
	prefs->show_end_times = glade_xml_get_widget (gui, "show_end_times");
	prefs->compress_weekend = glade_xml_get_widget (gui, "compress_weekend");
	prefs->dnav_show_week_no = glade_xml_get_widget (gui, "dnav_show_week_no");
	prefs->tasks_due_today_color = glade_xml_get_widget (gui, "tasks_due_today_color");
	prefs->tasks_overdue_color = glade_xml_get_widget (gui, "tasks_overdue_color");
	prefs->tasks_hide_completed = glade_xml_get_widget (gui, "tasks_hide_completed");
	prefs->tasks_hide_completed_interval = glade_xml_get_widget (gui, "tasks_hide_completed_interval");
	prefs->tasks_hide_completed_units = glade_xml_get_widget (gui, "tasks_hide_completed_units");


	/* Alarms tab */
	prefs->scrolled_window = glade_xml_get_widget (gui, "calendar-source-scrolled-window");

	/* Free/Busy tab */
	prefs->template_url = glade_xml_get_widget (gui, "template_url");
	target = e_cal_config_target_new_prefs (ec, prefs->gconf);
	e_config_set_target ((EConfig *)ec, (EConfigTarget *) target);
	toplevel = e_config_create_widget ((EConfig *)ec);
	gtk_container_add (GTK_CONTAINER (prefs), toplevel);

	show_config (prefs);
	/* FIXME: weakref? */
	setup_changes (prefs);
}

GType
calendar_prefs_dialog_get_type (void)
{
	static GType type = 0;

	if (!type) {
		static GTypeInfo type_info = {
			sizeof (CalendarPrefsDialogClass),
			NULL, NULL,
			(GClassInitFunc) calendar_prefs_dialog_class_init,
			NULL, NULL,
			sizeof (CalendarPrefsDialog),
			0,
			(GInstanceInitFunc) calendar_prefs_dialog_init,
		};

		type = g_type_register_static (GTK_TYPE_VBOX, "CalendarPrefsDialog", &type_info, 0);
	}

	return type;
}

GtkWidget *
calendar_prefs_dialog_new (void)
{
	CalendarPrefsDialog *dialog;

	dialog = (CalendarPrefsDialog *) g_object_new (calendar_prefs_dialog_get_type (), NULL);
	calendar_prefs_dialog_construct (dialog);

	return (GtkWidget *) dialog;
}

/* called by libglade to create our custom EDateEdit widgets. */
GtkWidget *
cal_prefs_dialog_create_time_edit (void)
{
	GtkWidget *dedit;

	dedit = e_date_edit_new ();

	gtk_widget_show (GTK_WIDGET (dedit));
	e_date_edit_set_use_24_hour_format (E_DATE_EDIT (dedit), calendar_config_get_24_hour_format ());
	e_date_edit_set_time_popup_range (E_DATE_EDIT (dedit), 0, 24);
	e_date_edit_set_show_date (E_DATE_EDIT (dedit), FALSE);

	return dedit;
}
