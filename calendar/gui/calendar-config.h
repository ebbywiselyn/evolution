/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * Authors :
 *  Damon Chaplin <damon@ximian.com>
 *  Rodrigo Moya <rodrigo@ximian.com>
 *
 * Copyright 2000, Ximian, Inc.
 * Copyright 2000, Ximian, Inc.
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
 * calendar-config.h - functions to load/save/get/set user settings.
 */

#ifndef _CALENDAR_CONFIG_H_
#define _CALENDAR_CONFIG_H_

#include <glib.h>
#include <gdk/gdk.h>
#include <libecal/e-cal.h>
#include <gconf/gconf-client.h>

/* These are used to get/set the working days in the week. The bit-flags are
   combined together. The bits must be from 0 (Sun) to 6 (Sat) to match the
   day values used by localtime etc. */
typedef enum
{
	CAL_SUNDAY	= 1 << 0,
	CAL_MONDAY	= 1 << 1,
	CAL_TUESDAY	= 1 << 2,
	CAL_WEDNESDAY	= 1 << 3,
	CAL_THURSDAY	= 1 << 4,
	CAL_FRIDAY	= 1 << 5,
	CAL_SATURDAY	= 1 << 6
} CalWeekdays;


/* Units for settings. */
typedef enum
{
	CAL_DAYS,
	CAL_HOURS,
	CAL_MINUTES
} CalUnits;


void calendar_config_remove_notification (guint id);

/*
 * Calendar Settings.
 */

/* The current list of calendars selected */
GSList   *calendar_config_get_calendars_selected (void);
void	  calendar_config_set_calendars_selected (GSList *selected);
guint	  calendar_config_add_notification_calendars_selected (GConfClientNotifyFunc func, gpointer data);

/* The primary calendar */
char     *calendar_config_get_primary_calendar (void);
void	  calendar_config_set_primary_calendar (const char *primary_uid);
guint	  calendar_config_add_notification_primary_calendar (GConfClientNotifyFunc func, gpointer data);

/* The current timezone, e.g. "Europe/London". */
gchar*	  calendar_config_get_timezone		(void);
icaltimezone *calendar_config_get_icaltimezone (void);
void	  calendar_config_set_timezone		(gchar	     *timezone);
guint calendar_config_add_notification_timezone (GConfClientNotifyFunc func, gpointer data);

/* The working days of the week, a bit-wise combination of flags. */
CalWeekdays calendar_config_get_working_days	(void);
void	  calendar_config_set_working_days	(CalWeekdays  days);
guint calendar_config_add_notification_working_days (GConfClientNotifyFunc func, gpointer data);

/* The start day of the week (0 = Sun to 6 = Sat). */
gint	  calendar_config_get_week_start_day	(void);
void	  calendar_config_set_week_start_day	(gint	      week_start_day);
guint calendar_config_add_notification_week_start_day (GConfClientNotifyFunc func, gpointer data);

/* The start and end times of the work-day. */
gint	  calendar_config_get_day_start_hour	(void);
void	  calendar_config_set_day_start_hour	(gint	      day_start_hour);
guint calendar_config_add_notification_day_start_hour (GConfClientNotifyFunc func, gpointer data);

gint	  calendar_config_get_day_start_minute	(void);
void	  calendar_config_set_day_start_minute	(gint	      day_start_min);
guint calendar_config_add_notification_day_start_minute (GConfClientNotifyFunc func, gpointer data);

gint	  calendar_config_get_day_end_hour	(void);
void	  calendar_config_set_day_end_hour	(gint	      day_end_hour);
guint calendar_config_add_notification_day_end_hour (GConfClientNotifyFunc func, gpointer data);

gint	  calendar_config_get_day_end_minute	(void);
void	  calendar_config_set_day_end_minute	(gint	      day_end_min);
guint calendar_config_add_notification_day_end_minute (GConfClientNotifyFunc func, gpointer data);

/* Whether we use 24-hour format or 12-hour format (AM/PM). */
gboolean  calendar_config_get_24_hour_format	(void);
void	  calendar_config_set_24_hour_format	(gboolean     use_24_hour);
guint calendar_config_add_notification_24_hour_format (GConfClientNotifyFunc func, gpointer data);

gboolean  calendar_config_get_show_status (void);
void	  calendar_config_set_show_status (gboolean	status);

gboolean  calendar_config_get_show_type (void);
void	  calendar_config_set_show_type (gboolean	status);

gboolean  calendar_config_get_show_rsvp (void);
void	  calendar_config_set_show_rsvp (gboolean	status);

gboolean  calendar_config_get_show_timezone (void);
void	  calendar_config_set_show_timezone (gboolean	status);

gboolean  calendar_config_get_show_categories (void);
void	  calendar_config_set_show_categories (gboolean	status);

gboolean calendar_config_get_show_role	(void);
void calendar_config_set_show_role	(gboolean state);


/* The time divisions in the Day/Work-Week view in minutes (5/10/15/30/60). */
gint	  calendar_config_get_time_divisions	(void);
void	  calendar_config_set_time_divisions	(gint	      divisions);
guint calendar_config_add_notification_time_divisions (GConfClientNotifyFunc func, gpointer data);

/* Whether we show the Marcus Bains Line, and in what colors. */
void calendar_config_get_marcus_bains (gboolean *show_line, const char **dayview_color, const char **timebar_color);
void calendar_config_add_notification_marcus_bains (GConfClientNotifyFunc func, gpointer data, gint *not_show, gint *not_dcolor, gint *not_tcolor);

/* Whether we show event end times. */
gboolean  calendar_config_get_show_event_end	(void);
void	  calendar_config_set_show_event_end	(gboolean     show_end);
guint calendar_config_add_notification_show_event_end (GConfClientNotifyFunc func, gpointer data);

/* Whether we compress the weekend in the week/month views. */
gboolean  calendar_config_get_compress_weekend	(void);
void	  calendar_config_set_compress_weekend	(gboolean     compress);
guint calendar_config_add_notification_compress_weekend (GConfClientNotifyFunc func, gpointer data);

/* Whether we show week numbers in the Date Navigator. */
gboolean  calendar_config_get_dnav_show_week_no	(void);
void	  calendar_config_set_dnav_show_week_no	(gboolean     show_week_no);
guint calendar_config_add_notification_dnav_show_week_no (GConfClientNotifyFunc func, gpointer data);

/* The positions of the panes in the normal and month views. */
gint      calendar_config_get_hpane_pos		(void);
void	  calendar_config_set_hpane_pos		(gint	      hpane_pos);

gint      calendar_config_get_vpane_pos		(void);
void	  calendar_config_set_vpane_pos		(gint	      vpane_pos);

gboolean  calendar_config_get_preview_state	(void);
void	  calendar_config_set_preview_state	(gboolean     state);
guint	  calendar_config_add_notification_preview_state (GConfClientNotifyFunc func, gpointer data);

gint      calendar_config_get_month_hpane_pos	(void);
void	  calendar_config_set_month_hpane_pos	(gint	      hpane_pos);

gint      calendar_config_get_month_vpane_pos	(void);
void	  calendar_config_set_month_vpane_pos	(gint	      vpane_pos);

float     calendar_config_get_tag_vpane_pos	(void);
void	  calendar_config_set_tag_vpane_pos	(float	      vpane_pos);

/* The current list of task lists selected */
GSList   *calendar_config_get_tasks_selected (void);
void	  calendar_config_set_tasks_selected (GSList *selected);
guint	  calendar_config_add_notification_tasks_selected (GConfClientNotifyFunc func, gpointer data);

/* The primary calendar */
char     *calendar_config_get_primary_tasks (void);
void	  calendar_config_set_primary_tasks (const char *primary_uid);
guint	  calendar_config_add_notification_primary_tasks (GConfClientNotifyFunc func, gpointer data);

/* The pane position */
gint      calendar_config_get_task_vpane_pos    (void);
void      calendar_config_set_task_vpane_pos    (gint         vpane_pos);


/* The current list of memo lists selected */
GSList   *calendar_config_get_memos_selected (void);
void	  calendar_config_set_memos_selected (GSList *selected);
guint	  calendar_config_add_notification_memos_selected (GConfClientNotifyFunc func, gpointer data);

/* The primary calendar */
char     *calendar_config_get_primary_memos (void);
void	  calendar_config_set_primary_memos (const char *primary_uid);
guint	  calendar_config_add_notification_primary_memos (GConfClientNotifyFunc func, gpointer data);

/* Colors for the task list */
void      calendar_config_get_tasks_due_today_color (GdkColor *color);
void	  calendar_config_set_tasks_due_today_color (GdkColor *color);

void      calendar_config_get_tasks_overdue_color (GdkColor *color);
void	  calendar_config_set_tasks_overdue_color (GdkColor *color);

/* Settings to hide completed tasks. */
gboolean  calendar_config_get_hide_completed_tasks	(void);
void	  calendar_config_set_hide_completed_tasks	(gboolean	hide);
guint	  calendar_config_add_notification_hide_completed_tasks (GConfClientNotifyFunc func, gpointer data);

CalUnits  calendar_config_get_hide_completed_tasks_units(void);
void	  calendar_config_set_hide_completed_tasks_units(CalUnits	units);
guint	  calendar_config_add_notification_hide_completed_tasks_units (GConfClientNotifyFunc func, gpointer data);

gint	  calendar_config_get_hide_completed_tasks_value(void);
void	  calendar_config_set_hide_completed_tasks_value(gint		value);
guint	  calendar_config_add_notification_hide_completed_tasks_value (GConfClientNotifyFunc func, gpointer data);

char *	  calendar_config_get_hide_completed_tasks_sexp (gboolean get_completed);

/* Confirmation options */
gboolean  calendar_config_get_confirm_delete (void);
void      calendar_config_set_confirm_delete (gboolean confirm);

gboolean  calendar_config_get_confirm_purge (void);
void      calendar_config_set_confirm_purge (gboolean confirm);

/* Default reminder options */
gboolean calendar_config_get_use_default_reminder (void);
void     calendar_config_set_use_default_reminder (gboolean value);

int      calendar_config_get_default_reminder_interval (void);
void     calendar_config_set_default_reminder_interval (int interval);

CalUnits calendar_config_get_default_reminder_units (void);
void     calendar_config_set_default_reminder_units (CalUnits units);

/* Free/Busy Settings */
GSList * calendar_config_get_free_busy (void);
void calendar_config_set_free_busy (GSList * url_list);

gchar *calendar_config_get_free_busy_template (void);
void calendar_config_set_free_busy_template (const gchar *template);
guint calendar_config_add_notification_free_busy_template (GConfClientNotifyFunc func,
							   gpointer data);

/* Shows the timezone dialog if the user hasn't set a default timezone. */
void	  calendar_config_check_timezone_set	(void);

/* Returns TRUE if the locale has 'am' and 'pm' strings defined, i.e. it
   supports 12-hour time format. */
gboolean  calendar_config_locale_supports_12_hour_format(void);

void	  calendar_config_set_dir_path (const char *);
char *	  calendar_config_get_dir_path (void);

gboolean calendar_config_get_daylight_saving (void);
void calendar_config_set_daylight_saving (gboolean daylight_saving);
guint calendar_config_add_notification_daylight_saving (GConfClientNotifyFunc func, gpointer data);

#endif /* _CALENDAR_CONFIG_H_ */
