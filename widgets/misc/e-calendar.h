/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * Author :
 *  Damon Chaplin <damon@ximian.com>
 *
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
#ifndef _E_CALENDAR_H_
#define _E_CALENDAR_H_

#include <gtk/gtkwidget.h>
#include <misc/e-canvas.h>
#include "e-calendar-item.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*
 * ECalendar - displays a table of monthly calendars, allowing highlighting
 * and selection of one or more days. Like GtkCalendar with more features.
 * Most of the functionality is in the ECalendarItem canvas item, though
 * we also add GnomeCanvasWidget buttons to go to the previous/next month and
 * to got to the current day.
 */

#define E_CALENDAR(obj)          G_TYPE_CHECK_INSTANCE_CAST (obj, e_calendar_get_type (), ECalendar)
#define E_CALENDAR_CLASS(klass)  G_TYPE_CHECK_CLASS_CAST (klass, e_calendar_get_type (), ECalendarClass)
#define E_IS_CALENDAR(obj)       G_TYPE_CHECK_INSTANCE_TYPE (obj, e_calendar_get_type ())


typedef struct _ECalendar       ECalendar;
typedef struct _ECalendarClass  ECalendarClass;

struct _ECalendar
{
	ECanvas canvas;

	ECalendarItem *calitem;

	GnomeCanvasItem *prev_item;
	GnomeCanvasItem *next_item;

	gint min_rows;
	gint min_cols;

	gint max_rows;
	gint max_cols;

	/* These are all used when the prev/next buttons are held down.
	   moving_forward is TRUE if we are moving forward in time, i.e. the
	   next button is pressed. */
	gint timeout_id;
	gint timeout_delay;
	gboolean moving_forward;
};

struct _ECalendarClass
{
	ECanvasClass parent_class;
};


GType		   e_calendar_get_type		(void);
GtkWidget* e_calendar_new		(void);

void	   e_calendar_set_minimum_size	(ECalendar	*cal,
					 gint		 rows,
					 gint		 cols);
void	   e_calendar_set_maximum_size	(ECalendar	*cal,
					 gint		 rows,
					 gint		 cols);

/* Returns the border size on each side of the month displays. */
void	   e_calendar_get_border_size	(ECalendar	*cal,
					 gint		*top,
					 gint		*bottom,
					 gint		*left,
					 gint		*right);

void       e_calendar_set_focusable (ECalendar *cal, gboolean focusable);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* _E_CALENDAR_H_ */
