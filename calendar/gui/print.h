/* Evolution calendar - Print support
 *
 * Copyright (C) 2000 Ximian, Inc.
 *
 * Authors: Michael Zucchi <notzed@ximian.com>
 *          Federico Mena-Quintero <federico@ximian.com>
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

#ifndef PRINT_H
#define PRINT_H

#include "gnome-cal.h"



typedef enum {
	PRINT_VIEW_DAY,
	PRINT_VIEW_WEEK,
	PRINT_VIEW_MONTH,
	PRINT_VIEW_YEAR,
	PRINT_VIEW_LIST
} PrintView;

void		print_calendar                (GnomeCalendar *gcal,
                                               GtkPrintOperationAction action,
                                               time_t start);
void		print_comp                    (ECalComponent *comp,
                                               ECal *client,
                                               GtkPrintOperationAction action);
void		print_table                   (ETable *table,
                                               const gchar *dialog_title,
                                               const gchar *print_header,
                                               GtkPrintOperationAction action);

#endif
