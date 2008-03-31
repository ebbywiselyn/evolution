/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/*
 * Author :
 *  Damon Chaplin <damon@ximian.com>
 *
 * Copyright 1999, Ximian, Inc.
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
 * EDayViewTimeItem - canvas item which displays the times down the left of
 * the EDayView.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <glib.h>
#include <gtk/gtksignal.h>
#include <gtk/gtkradiomenuitem.h>
#include <gtk/gtkcheckmenuitem.h>
#include <glib/gi18n.h>
#include <misc/e-gui-utils.h>
#include "e-day-view-time-item.h"
#include "calendar-config.h"
#include <libecal/e-cal-time-util.h>


/* The spacing between items in the time column. GRID_X_PAD is the space down
   either side of the column, i.e. outside the main horizontal grid lines.
   HOUR_L_PAD & HOUR_R_PAD are the spaces on the left & right side of the
   big hour number (this is inside the horizontal grid lines).
   MIN_X_PAD is the spacing either side of the minute number. The smaller
   horizontal grid lines match with this.
   60_MIN_X_PAD is the space either side of the HH:MM display used when
   we are displaying 60 mins per row (inside the main grid lines).
   LARGE_HOUR_Y_PAD is the offset of the large hour string from the top of the
   row.
   SMALL_FONT_Y_PAD is the offset of the small time/minute string from the top
   of the row. */
#define E_DVTMI_TIME_GRID_X_PAD		4
#define E_DVTMI_HOUR_L_PAD		4
#define E_DVTMI_HOUR_R_PAD		2
#define E_DVTMI_MIN_X_PAD		2
#define E_DVTMI_60_MIN_X_PAD		4
#define E_DVTMI_LARGE_HOUR_Y_PAD	1
#define E_DVTMI_SMALL_FONT_Y_PAD	1

static void e_day_view_time_item_set_arg (GtkObject *o,
					  GtkArg *arg,
					  guint arg_id);

static void e_day_view_time_item_update (GnomeCanvasItem *item,
					 double *affine,
					 ArtSVP *clip_path, int flags);
static void e_day_view_time_item_draw (GnomeCanvasItem *item,
				       GdkDrawable *drawable,
				       int x, int y,
				       int width, int height);
static double e_day_view_time_item_point (GnomeCanvasItem *item,
					  double x, double y,
					  int cx, int cy,
					  GnomeCanvasItem **actual_item);
static gint e_day_view_time_item_event (GnomeCanvasItem *item,
					GdkEvent *event);
static void e_day_view_time_item_increment_time	(gint	*hour,
						 gint	*minute,
						 gint	 mins_per_row);
static void e_day_view_time_item_show_popup_menu (EDayViewTimeItem *dvtmitem,
						  GdkEvent *event);
static void e_day_view_time_item_on_set_divisions (GtkWidget *item,
						   EDayViewTimeItem *dvtmitem);
static void e_day_view_time_item_on_button_press (EDayViewTimeItem *dvtmitem,
						  GdkEvent *event);
static void e_day_view_time_item_on_button_release (EDayViewTimeItem *dvtmitem,
						    GdkEvent *event);
static void e_day_view_time_item_on_motion_notify (EDayViewTimeItem *dvtmitem,
						   GdkEvent *event);
static gint e_day_view_time_item_convert_position_to_row (EDayViewTimeItem *dvtmitem,
							  gint y);


/* The arguments we take */
enum {
	ARG_0,
	ARG_DAY_VIEW
};

G_DEFINE_TYPE (EDayViewTimeItem, e_day_view_time_item, GNOME_TYPE_CANVAS_ITEM)

static void
e_day_view_time_item_class_init (EDayViewTimeItemClass *class)
{
	GtkObjectClass  *object_class;
	GnomeCanvasItemClass *item_class;

	object_class = (GtkObjectClass *) class;
	item_class = (GnomeCanvasItemClass *) class;

	gtk_object_add_arg_type ("EDayViewTimeItem::day_view",
				 GTK_TYPE_POINTER, GTK_ARG_WRITABLE,
				 ARG_DAY_VIEW);

	object_class->set_arg = e_day_view_time_item_set_arg;

	/* GnomeCanvasItem method overrides */
	item_class->update      = e_day_view_time_item_update;
	item_class->draw        = e_day_view_time_item_draw;
	item_class->point       = e_day_view_time_item_point;
	item_class->event       = e_day_view_time_item_event;
}


static void
e_day_view_time_item_init (EDayViewTimeItem *dvtmitem)
{
	dvtmitem->dragging_selection = FALSE;
}


static void
e_day_view_time_item_set_arg (GtkObject *o, GtkArg *arg, guint arg_id)
{
	EDayViewTimeItem *dvtmitem;

	dvtmitem = E_DAY_VIEW_TIME_ITEM (o);

	switch (arg_id){
	case ARG_DAY_VIEW:
		dvtmitem->day_view = GTK_VALUE_POINTER (*arg);
		break;
	}
}


static void
e_day_view_time_item_update (GnomeCanvasItem *item,
			    double *affine,
			    ArtSVP *clip_path,
			    int flags)
{
	if (GNOME_CANVAS_ITEM_CLASS (e_day_view_time_item_parent_class)->update)
		(* GNOME_CANVAS_ITEM_CLASS (e_day_view_time_item_parent_class)->update) (item, affine, clip_path, flags);

	/* The item covers the entire canvas area. */
	item->x1 = 0;
	item->y1 = 0;
	item->x2 = INT_MAX;
	item->y2 = INT_MAX;
}


/* Returns the minimum width needed for the column, by adding up all the
   maximum widths of the strings. The string widths are all calculated in
   the style_set handlers of EDayView and EDayViewTimeCanvas. */
gint
e_day_view_time_item_get_column_width (EDayViewTimeItem *dvtmitem)
{
	EDayView *day_view;
	GtkStyle *style;
	gint digit, large_digit_width, max_large_digit_width = 0;
	gint max_suffix_width, max_minute_or_suffix_width;
	gint column_width_default, column_width_60_min_rows;

	day_view = dvtmitem->day_view;
	g_return_val_if_fail (day_view != NULL, 0);

	style = gtk_widget_get_style (GTK_WIDGET (day_view));
	g_return_val_if_fail (style != NULL, 0);

	/* Find the maximum width a digit can have. FIXME: We could use pango's
	 * approximation function, but I worry it won't be precise enough. Also
	 * it needs a language tag that I don't know where to get. */
	for (digit = '0'; digit <= '9'; digit++) {
		PangoLayout *layout;
		gchar digit_str [2];

		digit_str [0] = digit;
		digit_str [1] = '\0';

		layout = gtk_widget_create_pango_layout (GTK_WIDGET (day_view), digit_str);
		pango_layout_set_font_description (layout, day_view->large_font_desc);
		pango_layout_get_pixel_size (layout, &large_digit_width, NULL);

		g_object_unref (layout);

		max_large_digit_width = MAX (max_large_digit_width,
					     large_digit_width);
	}

	/* Calculate the width of each time column, using the maximum of the
	   default format with large hour numbers, and the 60-min divisions
	   format which uses small text. */
	max_suffix_width = MAX (day_view->am_string_width,
				day_view->pm_string_width);

	max_minute_or_suffix_width = MAX (max_suffix_width,
					  day_view->max_minute_width);

	column_width_default = max_large_digit_width * 2
		+ max_minute_or_suffix_width
		+ E_DVTMI_MIN_X_PAD * 2
		+ E_DVTMI_HOUR_L_PAD
		+ E_DVTMI_HOUR_R_PAD
		+ E_DVTMI_TIME_GRID_X_PAD * 2;

	column_width_60_min_rows = day_view->max_small_hour_width
		+ day_view->colon_width
		+ max_minute_or_suffix_width
		+ E_DVTMI_60_MIN_X_PAD * 2
		+ E_DVTMI_TIME_GRID_X_PAD * 2;

	dvtmitem->column_width = MAX (column_width_default,
				      column_width_60_min_rows);
	return dvtmitem->column_width;
}


/*
 * DRAWING ROUTINES - functions to paint the canvas item.
 */
static void
e_day_view_time_item_draw (GnomeCanvasItem *canvas_item,
			   GdkDrawable	   *drawable,
			   int		    x,
			   int		    y,
			   int		    width,
			   int		    height)
{
	EDayView *day_view;
	EDayViewTimeItem *dvtmitem;
	GtkStyle *style;
	gchar buffer[64], *suffix;
	gint hour, display_hour, minute, row;
	gint row_y, start_y, large_hour_y_offset, small_font_y_offset;
	gint long_line_x1, long_line_x2, short_line_x1;
	gint large_hour_x2, minute_x2;
	gint hour_width, minute_width, suffix_width;
	gint max_suffix_width, max_minute_or_suffix_width;
	PangoLayout *layout;
	PangoContext *context;
	PangoFontDescription *small_font_desc;
	PangoFontMetrics *large_font_metrics, *small_font_metrics;
	cairo_t *cr;
	GdkColor fg, dark;

	cr = gdk_cairo_create (drawable);

	dvtmitem = E_DAY_VIEW_TIME_ITEM (canvas_item);
	day_view = dvtmitem->day_view;
	g_return_if_fail (day_view != NULL);

	style = gtk_widget_get_style (GTK_WIDGET (day_view));
	small_font_desc = style->font_desc;

	context = gtk_widget_get_pango_context (GTK_WIDGET (day_view));
	large_font_metrics = pango_context_get_metrics (context, day_view->large_font_desc,
							pango_context_get_language (context));
	small_font_metrics = pango_context_get_metrics (context, small_font_desc,
							pango_context_get_language (context));

	fg = style->fg[GTK_STATE_NORMAL];
	dark = style->dark[GTK_STATE_NORMAL];

	/* The start and end of the long horizontal line between hours. */
	long_line_x1 = E_DVTMI_TIME_GRID_X_PAD - x;
	long_line_x2 = dvtmitem->column_width - E_DVTMI_TIME_GRID_X_PAD - x;

	if (day_view->mins_per_row == 60) {
		/* The right edge of the complete time string in 60-min
		   divisions, e.g. "14:00" or "2 pm". */
		minute_x2 = long_line_x2 - E_DVTMI_60_MIN_X_PAD;

		/* These aren't used for 60-minute divisions, but we initialize
		   them to keep gcc happy. */
		short_line_x1 = 0;
		large_hour_x2 = 0;
	} else {
		max_suffix_width = MAX (day_view->am_string_width,
					day_view->pm_string_width);

		max_minute_or_suffix_width = MAX (max_suffix_width,
						  day_view->max_minute_width);

		/* The start of the short horizontal line between the periods
		   within each hour. */
		short_line_x1 = long_line_x2 - E_DVTMI_MIN_X_PAD * 2
			- max_minute_or_suffix_width;

		/* The right edge of the large hour string. */
		large_hour_x2 = short_line_x1 - E_DVTMI_HOUR_R_PAD;

		/* The right edge of the minute part of the time. */
		minute_x2 = long_line_x2 - E_DVTMI_MIN_X_PAD;
	}

	/* Start with the first hour & minute shown in the EDayView. */
	hour = day_view->first_hour_shown;
	minute = day_view->first_minute_shown;

	/* The offset of the large hour string from the top of the row. */
	large_hour_y_offset = E_DVTMI_LARGE_HOUR_Y_PAD;

	/* The offset of the small time/minute string from top of row. */
	small_font_y_offset = E_DVTMI_SMALL_FONT_Y_PAD;

	/* Calculate the minimum y position of the first row we need to draw.
	   This is normally one row height above the 0 position, but if we
	   are using the large font we may have to go back a bit further. */
	start_y = 0 - MAX (day_view->row_height,
			   (pango_font_metrics_get_ascent (large_font_metrics) +
			    pango_font_metrics_get_descent (large_font_metrics)) / PANGO_SCALE +
			   E_DVTMI_LARGE_HOUR_Y_PAD);

	/* Draw the Marcus Bains Line first, so it appears under other elements. */
	if (e_day_view_get_show_marcus_bains (day_view)) {
		struct icaltimetype time_now;
		int marcus_bains_y;
		GdkColor mb_color;

		cairo_save (cr);
		gdk_cairo_set_source_color (cr, &day_view->colors[E_DAY_VIEW_COLOR_MARCUS_BAINS_LINE]);

		if (day_view->marcus_bains_time_bar_color && gdk_color_parse (day_view->marcus_bains_time_bar_color, &mb_color)) {
			GdkColormap *colormap;

			colormap = gtk_widget_get_colormap (GTK_WIDGET (day_view));
			if (gdk_colormap_alloc_color (colormap, &mb_color, TRUE, TRUE)) {
				gdk_cairo_set_source_color (cr, &mb_color);
			}
		}

		time_now = icaltime_current_time_with_zone (e_calendar_view_get_timezone (E_CALENDAR_VIEW (day_view)));
		marcus_bains_y = (time_now.hour * 60 + time_now.minute) * day_view->row_height / day_view->mins_per_row - y;
		cairo_set_line_width (cr, 1.5);
		cairo_move_to (cr, long_line_x1, marcus_bains_y);
		cairo_line_to (cr, long_line_x2, marcus_bains_y);
		cairo_stroke (cr);
		cairo_restore (cr);
	}

	/* Step through each row, drawing the times and the horizontal lines
	   between them. */
	for (row = 0, row_y = 0 - y;
	     row < day_view->rows && row_y < height;
	     row++, row_y += day_view->row_height) {

		/* If the row is above the first row we want to draw just
		   increment the time and skip to the next row. */
		if (row_y < start_y) {
			e_day_view_time_item_increment_time (&hour, &minute,
							     day_view->mins_per_row);
			continue;
		}

		/* Calculate the actual hour number to display. For 12-hour
		   format we convert 0-23 to 12-11am/12-11pm. */
		e_day_view_convert_time_to_display (day_view, hour,
						    &display_hour,
						    &suffix, &suffix_width);

		if (day_view->mins_per_row == 60) {
			/* 60 minute intervals - draw a long horizontal line
			   between hours and display as one long string,
			   e.g. "14:00" or "2 pm". */
			cairo_save (cr);
			gdk_cairo_set_source_color (cr, &dark);
			cairo_save (cr);
			cairo_set_line_width (cr, 0.7);
			cairo_move_to (cr, long_line_x1, row_y);
			cairo_line_to (cr, long_line_x2, row_y);
			cairo_stroke (cr);
			cairo_restore (cr);

			if (e_calendar_view_get_use_24_hour_format (E_CALENDAR_VIEW (day_view))) {
				g_snprintf (buffer, sizeof (buffer), "%i:%02i",
					    display_hour, minute);
			} else {
				g_snprintf (buffer, sizeof (buffer), "%i %s",
					    display_hour, suffix);
			}

			cairo_save (cr);
			gdk_cairo_set_source_color (cr, &fg);
			layout = pango_cairo_create_layout (cr);
			pango_layout_set_text (layout, buffer, -1);
			pango_layout_get_pixel_size (layout, &minute_width, NULL);
			cairo_translate (cr, minute_x2 - minute_width, row_y + small_font_y_offset);
			pango_cairo_update_layout (cr, layout);
			pango_cairo_show_layout (cr, layout);
			cairo_restore (cr);

			g_object_unref (layout);
		} else {
			/* 5/10/15/30 minute intervals. */

			if (minute == 0) {
				/* On the hour - draw a long horizontal line
				   before the hour and display the hour in the
				   large font. */

				cairo_save (cr);
				gdk_cairo_set_source_color (cr, &dark);
				g_snprintf (buffer, sizeof (buffer), "%i",
					    display_hour);

				cairo_set_line_width (cr, 0.7);
				cairo_move_to (cr, long_line_x1, row_y);
				cairo_line_to (cr, long_line_x2, row_y);
				cairo_stroke (cr);
				cairo_restore (cr);

				cairo_save (cr);
				gdk_cairo_set_source_color (cr, &fg);
				layout = pango_cairo_create_layout (cr);
				pango_layout_set_text (layout, buffer, -1);
				pango_layout_set_font_description (layout, day_view->large_font_desc);
				pango_layout_get_pixel_size (layout, &hour_width, NULL);
				cairo_translate (cr, large_hour_x2 - hour_width, row_y + large_hour_y_offset);
				pango_cairo_update_layout (cr, layout);
				pango_cairo_show_layout (cr, layout);
				cairo_restore (cr);

				g_object_unref (layout);
			} else {
				/* Within the hour - draw a short line before
				   the time. */
				cairo_save (cr);
				gdk_cairo_set_source_color (cr, &dark);
				cairo_set_line_width (cr, 0.7);
				cairo_move_to (cr, short_line_x1, row_y);
				cairo_line_to (cr, long_line_x2, row_y);
				cairo_stroke (cr);
				cairo_restore (cr);
			}

			/* Normally we display the minute in each
			   interval, but when using 30-minute intervals
			   we don't display the '30'. */
			if (day_view->mins_per_row != 30 || minute != 30) {
				/* In 12-hour format we display 'am' or 'pm'
				   instead of '00'. */
				if (minute == 0
				    && !e_calendar_view_get_use_24_hour_format (E_CALENDAR_VIEW (day_view))) {
					strcpy (buffer, suffix);
				} else {
					g_snprintf (buffer, sizeof (buffer),
						    "%02i", minute);
				}

				cairo_save (cr);
				gdk_cairo_set_source_color (cr, &fg);
				layout = pango_cairo_create_layout (cr);
				pango_layout_set_text (layout, buffer, -1);
				pango_layout_set_font_description (layout, day_view->small_font_desc);
				pango_layout_get_pixel_size (layout, &minute_width, NULL);
				cairo_translate (cr, minute_x2 - minute_width, row_y + small_font_y_offset);
				pango_cairo_update_layout (cr, layout);
				pango_cairo_show_layout (cr, layout);
				cairo_restore (cr);

				g_object_unref (layout);
			}
		}

		e_day_view_time_item_increment_time (&hour, &minute,
						     day_view->mins_per_row);
	}

	pango_font_metrics_unref (large_font_metrics);
	pango_font_metrics_unref (small_font_metrics);
	cairo_destroy (cr);
}

/* Increment the time by the 5/10/15/30/60 minute interval.
   Note that mins_per_row is never > 60, so we never have to
   worry about adding more than 60 minutes. */
static void
e_day_view_time_item_increment_time	(gint	*hour,
					 gint	*minute,
					 gint	 mins_per_row)
{
	*minute += mins_per_row;
	if (*minute >= 60) {
		*minute -= 60;
		/* Currently we never wrap around to the next day, but
		   we may do if we display extra timezones. */
		*hour = (*hour + 1) % 24;
	}
}


static double
e_day_view_time_item_point (GnomeCanvasItem *item, double x, double y,
			    int cx, int cy,
			    GnomeCanvasItem **actual_item)
{
	*actual_item = item;
	return 0.0;
}


static gint
e_day_view_time_item_event (GnomeCanvasItem *item,
			    GdkEvent *event)
{
	EDayViewTimeItem *dvtmitem;

	dvtmitem = E_DAY_VIEW_TIME_ITEM (item);

	switch (event->type) {
	case GDK_BUTTON_PRESS:
		if (event->button.button == 1) {
			e_day_view_time_item_on_button_press (dvtmitem, event);
		} else if (event->button.button == 3) {
			e_day_view_time_item_show_popup_menu (dvtmitem, event);
			return TRUE;
		}
		break;
	case GDK_BUTTON_RELEASE:
		if (event->button.button == 1)
			e_day_view_time_item_on_button_release (dvtmitem,
								event);
		break;

	case GDK_MOTION_NOTIFY:
		e_day_view_time_item_on_motion_notify (dvtmitem, event);
		break;

	default:
		break;
	}

	return FALSE;
}


static void
e_day_view_time_item_show_popup_menu (EDayViewTimeItem *dvtmitem,
				      GdkEvent *event)
{
	static gint divisions[] = { 60, 30, 15, 10, 5 };
	EDayView *day_view;
	gint num_divisions = sizeof (divisions) / sizeof (divisions[0]);
	GtkWidget *menu, *item;
	gchar buffer[256];
	GSList *group = NULL;
	gint current_divisions, i;

	day_view = dvtmitem->day_view;
	g_return_if_fail (day_view != NULL);

	current_divisions = e_day_view_get_mins_per_row (day_view);

	menu = gtk_menu_new ();

	/* Make sure the menu is destroyed when it disappears. */
	e_auto_kill_popup_menu_on_selection_done (GTK_MENU (menu));

	for (i = 0; i < num_divisions; i++) {
		g_snprintf (buffer, sizeof (buffer),
		/* TO TRANSLATORS: %02i is the number of minutes; this is a context menu entry
		 * to change the length of the time division in the calendar day view, e.g.
		 * a day is displayed in 24 "60 minute divisions" or 48 "30 minute divisions"
		 */
			    _("%02i minute divisions"), divisions[i]);
		item = gtk_radio_menu_item_new_with_label (group, buffer);
		group = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (item));
		gtk_widget_show (item);
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

		if (current_divisions == divisions[i])
			gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), TRUE);

		g_object_set_data (G_OBJECT (item), "divisions",
				   GINT_TO_POINTER (divisions[i]));

		g_signal_connect (item, "toggled",
				  G_CALLBACK (e_day_view_time_item_on_set_divisions), dvtmitem);
	}

	gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL,
			event->button.button, event->button.time);
}


static void
e_day_view_time_item_on_set_divisions (GtkWidget *item,
				       EDayViewTimeItem *dvtmitem)
{
	EDayView *day_view;
	gint divisions;

	day_view = dvtmitem->day_view;
	g_return_if_fail (day_view != NULL);

 	if (!gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (item)))
		return;

	divisions = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (item), "divisions"));
	e_day_view_set_mins_per_row (day_view, divisions);
	calendar_config_set_time_divisions (divisions);
}


static void
e_day_view_time_item_on_button_press (EDayViewTimeItem *dvtmitem,
				      GdkEvent *event)
{
	EDayView *day_view;
	GnomeCanvas *canvas;
	gint row;

	day_view = dvtmitem->day_view;
	g_return_if_fail (day_view != NULL);

	canvas = GNOME_CANVAS_ITEM (dvtmitem)->canvas;

	row = e_day_view_time_item_convert_position_to_row (dvtmitem,
							    event->button.y);

	if (row == -1)
		return;

	if (!GTK_WIDGET_HAS_FOCUS (day_view))
		gtk_widget_grab_focus (GTK_WIDGET (day_view));

	if (gdk_pointer_grab (GTK_LAYOUT (canvas)->bin_window, FALSE,
			      GDK_POINTER_MOTION_MASK
			      | GDK_BUTTON_RELEASE_MASK,
			      NULL, NULL, event->button.time) == 0) {
		e_day_view_start_selection (day_view, -1, row);
		dvtmitem->dragging_selection = TRUE;
	}
}


static void
e_day_view_time_item_on_button_release (EDayViewTimeItem *dvtmitem,
					GdkEvent *event)
{
	EDayView *day_view;

	day_view = dvtmitem->day_view;
	g_return_if_fail (day_view != NULL);

	if (dvtmitem->dragging_selection) {
		gdk_pointer_ungrab (event->button.time);
		e_day_view_finish_selection (day_view);
		e_day_view_stop_auto_scroll (day_view);
	}

	dvtmitem->dragging_selection = FALSE;
}

static void
e_day_view_time_item_on_motion_notify (EDayViewTimeItem *dvtmitem,
				       GdkEvent *event)
{
	EDayView *day_view;
	GnomeCanvas *canvas;
	gdouble window_y;
	gint y, row;

	if (!dvtmitem->dragging_selection)
		return;

	day_view = dvtmitem->day_view;
	g_return_if_fail (day_view != NULL);

	canvas = GNOME_CANVAS_ITEM (dvtmitem)->canvas;

	y = event->motion.y;
	row = e_day_view_time_item_convert_position_to_row (dvtmitem, y);

	if (row != -1) {
		gnome_canvas_world_to_window (canvas, 0, event->motion.y,
					      NULL, &window_y);
		e_day_view_update_selection (day_view, -1, row);
		e_day_view_check_auto_scroll (day_view, -1, (gint) window_y);
	}
}


/* Returns the row corresponding to the y position, or -1. */
static gint
e_day_view_time_item_convert_position_to_row (EDayViewTimeItem *dvtmitem,
					      gint y)
{
	EDayView *day_view;
	gint row;

	day_view = dvtmitem->day_view;
	g_return_val_if_fail (day_view != NULL, -1);

	if (y < 0)
		return -1;

	row = y / day_view->row_height;
	if (row >= day_view->rows)
		return -1;

	return row;
}
