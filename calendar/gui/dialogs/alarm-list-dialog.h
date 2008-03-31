/* Evolution calendar - Alarm page of the calendar component dialogs
 *
 * Copyright (C) 2001-2003 Ximian, Inc.
 *
 * Authors: Federico Mena-Quintero <federico@ximian.com>
 *          Miguel de Icaza <miguel@ximian.com>
 *          Seth Alves <alves@hungry.com>
 *          JP Rosevear <jpr@ximian.com>
 *          Hans Petter Jansson <hpj@ximian.com>
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

#ifndef ALARM_LIST_DIALOG_H
#define ALARM_LIST_DIALOG_H

#include <libecal/e-cal.h>
#include <libecal/e-cal-component.h>
#include "../e-alarm-list.h"

G_BEGIN_DECLS

gboolean alarm_list_dialog_run (GtkWidget *parent, ECal *ecal, EAlarmList *list_store);
GtkWidget *alarm_list_dialog_peek (ECal *ecal, EAlarmList *list_store);
void alarm_list_dialog_set_client (GtkWidget *dlg_box, ECal *client);

G_END_DECLS

#endif
