/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * Author :
 *  Damon Chaplin <damon@ximian.com>
 *
 * Copyright 1999, Ximian, Inc.
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
 * EWeekViewTitlesItem - displays the 'Monday', 'Tuesday' etc. at the top of
 * the Month calendar view.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <e-util/e-util.h>
#include "e-week-view-titles-item.h"

static void e_week_view_titles_item_set_arg	(GtkObject	 *o,
						 GtkArg		 *arg,
						 guint		  arg_id);
static void e_week_view_titles_item_update	(GnomeCanvasItem *item,
						 double		 *affine,
						 ArtSVP		 *clip_path,
						 int		  flags);
static void e_week_view_titles_item_draw	(GnomeCanvasItem *item,
						 GdkDrawable	 *drawable,
						 int		  x,
						 int		  y,
						 int		  width,
						 int		  height);
static double e_week_view_titles_item_point	(GnomeCanvasItem *item,
						 double		  x,
						 double		  y,
						 int		  cx,
						 int		  cy,
						 GnomeCanvasItem **actual_item);

/* The arguments we take */
enum {
	ARG_0,
	ARG_WEEK_VIEW
};

G_DEFINE_TYPE (EWeekViewTitlesItem, e_week_view_titles_item, GNOME_TYPE_CANVAS_ITEM)

static void
e_week_view_titles_item_class_init (EWeekViewTitlesItemClass *class)
{
	GtkObjectClass  *object_class;
	GnomeCanvasItemClass *item_class;

	object_class = (GtkObjectClass *) class;
	item_class = (GnomeCanvasItemClass *) class;

	gtk_object_add_arg_type ("EWeekViewTitlesItem::week_view",
				 GTK_TYPE_POINTER, GTK_ARG_WRITABLE,
				 ARG_WEEK_VIEW);

	object_class->set_arg = e_week_view_titles_item_set_arg;

	/* GnomeCanvasItem method overrides */
	item_class->update      = e_week_view_titles_item_update;
	item_class->draw        = e_week_view_titles_item_draw;
	item_class->point       = e_week_view_titles_item_point;
}


static void
e_week_view_titles_item_init (EWeekViewTitlesItem *wvtitem)
{
	wvtitem->week_view = NULL;
}


static void
e_week_view_titles_item_set_arg (GtkObject *o, GtkArg *arg, guint arg_id)
{
	EWeekViewTitlesItem *wvtitem;

	wvtitem = E_WEEK_VIEW_TITLES_ITEM (o);

	switch (arg_id){
	case ARG_WEEK_VIEW:
		wvtitem->week_view = GTK_VALUE_POINTER (*arg);
		break;
	}
}


static void
e_week_view_titles_item_update (GnomeCanvasItem *item,
				double	    *affine,
				ArtSVP	    *clip_path,
				int		     flags)
{
	if (GNOME_CANVAS_ITEM_CLASS (e_week_view_titles_item_parent_class)->update)
		(* GNOME_CANVAS_ITEM_CLASS (e_week_view_titles_item_parent_class)->update) (item, affine, clip_path, flags);

	/* The item covers the entire canvas area. */
	item->x1 = 0;
	item->y1 = 0;
	item->x2 = INT_MAX;
	item->y2 = INT_MAX;
}


/*
 * DRAWING ROUTINES - functions to paint the canvas item.
 */

static void
e_week_view_titles_item_draw (GnomeCanvasItem  *canvas_item,
			      GdkDrawable      *drawable,
			      int		x,
			      int		y,
			      int		width,
			      int		height)
{
	EWeekViewTitlesItem *wvtitem;
	EWeekView *week_view;
	GtkStyle *style;
	GdkGC *fg_gc, *light_gc, *dark_gc;
	gint canvas_width, canvas_height, col_width, col, date_width, date_x;
	gchar buffer[128];
	GdkRectangle clip_rect;
	gboolean abbreviated;
	gint weekday;
	PangoLayout *layout;

#if 0
	g_print ("In e_week_view_titles_item_draw %i,%i %ix%i\n",
		 x, y, width, height);
#endif

	wvtitem = E_WEEK_VIEW_TITLES_ITEM (canvas_item);
	week_view = wvtitem->week_view;
	g_return_if_fail (week_view != NULL);

	style = gtk_widget_get_style (GTK_WIDGET (week_view));
	fg_gc = style->fg_gc[GTK_STATE_NORMAL];
	light_gc = style->light_gc[GTK_STATE_NORMAL];
	dark_gc = style->dark_gc[GTK_STATE_NORMAL];
	canvas_width = GTK_WIDGET (canvas_item->canvas)->allocation.width;
	canvas_height = GTK_WIDGET (canvas_item->canvas)->allocation.height;
	layout = gtk_widget_create_pango_layout (GTK_WIDGET (week_view), NULL);

	/* Draw the shadow around the dates. */
	gdk_draw_line (drawable, light_gc,
		       1 - x, 1 - y,
		       canvas_width - 2 - x, 1 - y);
	gdk_draw_line (drawable, light_gc,
		       1 - x, 2 - y,
		       1 - x, canvas_height - 1 - y);

	gdk_draw_rectangle (drawable, dark_gc, FALSE,
			    0 - x, 0 - y,
			    canvas_width - 1, canvas_height);

	/* Determine the format to use. */
	col_width = canvas_width / week_view->columns;
	abbreviated = (week_view->max_day_width + 2 >= col_width);

	/* Shift right one pixel to account for the shadow around the main
	   canvas. */
	x--;

	/* Draw the date. Set a clipping rectangle so we don't draw over the
	   next day. */
	weekday = week_view->display_start_day;
	for (col = 0; col < week_view->columns; col++) {
		if (weekday == 5 && week_view->compress_weekend)
			g_snprintf (
				buffer, sizeof (buffer), "%s/%s",
				e_get_weekday_name (G_DATE_SATURDAY, TRUE),
				e_get_weekday_name (G_DATE_SUNDAY, TRUE));
		else
			g_snprintf (
				buffer, sizeof (buffer), "%s",
				e_get_weekday_name (weekday + 1, abbreviated));

		clip_rect.x = week_view->col_offsets[col] - x;
		clip_rect.y = 2 - y;
		clip_rect.width = week_view->col_widths[col];
		clip_rect.height = canvas_height - 2;
		gdk_gc_set_clip_rectangle (fg_gc, &clip_rect);

		if (weekday == 5 && week_view->compress_weekend)
			date_width = week_view->abbr_day_widths[5]
				+ week_view->slash_width
				+ week_view->abbr_day_widths[6];
		else if (abbreviated)
			date_width = week_view->abbr_day_widths[weekday];
		else
			date_width = week_view->day_widths[weekday];

		date_x = week_view->col_offsets[col]
			+ (week_view->col_widths[col] - date_width) / 2;
		date_x = MAX (date_x, week_view->col_offsets[col]);

		pango_layout_set_text (layout, buffer, -1);
		gdk_draw_layout (drawable, fg_gc,
				 date_x - x,
				 3 - y,
				 layout);

		gdk_gc_set_clip_rectangle (fg_gc, NULL);

		/* Draw the lines down the left and right of the date cols. */
		if (col != 0) {
			gdk_draw_line (drawable, light_gc,
				       week_view->col_offsets[col] - x,
				       4 - y,
				       week_view->col_offsets[col] - x,
				       canvas_height - 4 - y);

			gdk_draw_line (drawable, dark_gc,
				       week_view->col_offsets[col] - 1 - x,
				       4 - y,
				       week_view->col_offsets[col] - 1 - x,
				       canvas_height - 4 - y);
		}

		/* Draw the lines between each column. */
		if (col != 0) {
			gdk_draw_line (drawable, style->black_gc,
				       week_view->col_offsets[col] - x,
				       canvas_height - y,
				       week_view->col_offsets[col] - x,
				       canvas_height - y);
		}

		if (weekday == 5 && week_view->compress_weekend)
			weekday += 2;
		else
			weekday++;

		weekday = weekday % 7;
	}

	g_object_unref (layout);
}


/* This is supposed to return the nearest item the the point and the distance.
   Since we are the only item we just return ourself and 0 for the distance.
   This is needed so that we get button/motion events. */
static double
e_week_view_titles_item_point (GnomeCanvasItem *item, double x, double y,
			       int cx, int cy,
			       GnomeCanvasItem **actual_item)
{
	*actual_item = item;
	return 0.0;
}


