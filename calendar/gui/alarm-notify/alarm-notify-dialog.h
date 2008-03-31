/* Evolution calendar - alarm notification dialog
 *
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Author: Federico Mena-Quintero <federico@ximian.com>
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

#ifndef ALARM_NOTIFY_DIALOG_H
#define ALARM_NOTIFY_DIALOG_H

#include <time.h>
#include <glib.h>
#include <libecal/e-cal-component.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtktreemodel.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtkliststore.h>



typedef enum {
	ALARM_NOTIFY_CLOSE,
	ALARM_NOTIFY_SNOOZE,
	ALARM_NOTIFY_EDIT
} AlarmNotifyResult;

typedef struct _AlarmNotificationsDialog AlarmNotificationsDialog;
struct _AlarmNotificationsDialog
{
	GtkWidget *dialog;
	GtkWidget *treeview;
};

typedef void (* AlarmNotifyFunc) (AlarmNotifyResult result, int snooze_mins, gpointer data);

AlarmNotificationsDialog *
notified_alarms_dialog_new (void);

GtkTreeIter
add_alarm_to_notified_alarms_dialog (AlarmNotificationsDialog *na, time_t trigger,
				time_t occur_start, time_t occur_end,
				ECalComponentVType vtype, const char *summary,
				const char *description, const char *location,
				AlarmNotifyFunc func, gpointer func_data);



#endif
