/* Evolution calendar - Utilities for manipulating ECalComponent objects
 *
 * Copyright (C) 2000 Ximian, Inc.
 * Copyright (C) 2000 Ximian, Inc.
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

#ifndef COMP_UTIL_H
#define COMP_UTIL_H

#include <glib.h>
#include <gtk/gtkwidget.h>
#include <gtk/gtkselection.h>
#include <libecal/e-cal-component.h>
#include <libecal/e-cal.h>

void cal_comp_util_add_exdate (ECalComponent *comp, time_t t, icaltimezone *zone);


/* Returns TRUE if the component uses the given timezone for both DTSTART
   and DTEND, or if the UTC offsets of the start and end times are the same
   as in the given zone. */
gboolean cal_comp_util_compare_event_timezones (ECalComponent *comp,
						ECal *client,
						icaltimezone *zone);

/* Returns the number of icons owned by the ECalComponent */
gint     cal_comp_util_get_n_icons (ECalComponent *comp);

gboolean cal_comp_is_on_server (ECalComponent *comp,
				ECal *client);

ECalComponent *cal_comp_event_new_with_defaults (ECal *client);
ECalComponent *cal_comp_event_new_with_current_time (ECal *client, gboolean all_day);
ECalComponent *cal_comp_task_new_with_defaults (ECal *client);
ECalComponent *cal_comp_memo_new_with_defaults (ECal *client);

void    cal_comp_selection_set_string_list (GtkSelectionData *data, GSList *str_list);
GSList *cal_comp_selection_get_string_list (GtkSelectionData *data);

#endif
