/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-task-widget.c
 *
 * Copyright (C) 2001  Ximian, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 * Author: Ettore Perazzoli <ettore@ximian.com>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "e-task-widget.h"
#include "e-spinner.h"
#include <e-util/e-icon-factory.h>

#include <gtk/gtkframe.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkimage.h>
#include <gtk/gtktooltips.h>
#include <gtk/gtktoolbutton.h>
#include <gtk/gtkbox.h>

#include <glib/gi18n.h>


#define SPACING 2

struct _ETaskWidgetPrivate {
	char *component_id;

	GtkTooltips *tooltips;

	GdkPixbuf *icon_pixbuf;
	GtkWidget *label;
	GtkWidget *box;
	GtkWidget *image;

	void (*cancel_func) (gpointer data);
	gpointer data;
};

G_DEFINE_TYPE (ETaskWidget, e_task_widget, GTK_TYPE_EVENT_BOX)

/* GObject methods.  */

static void
impl_dispose (GObject *object)
{
	ETaskWidget *task_widget;
	ETaskWidgetPrivate *priv;

	task_widget = E_TASK_WIDGET (object);

	priv = task_widget->priv;

	if (priv->tooltips != NULL) {
		g_object_unref (priv->tooltips);
		priv->tooltips = NULL;
	}

	if (priv->icon_pixbuf != NULL) {
		g_object_unref (priv->icon_pixbuf);
		priv->icon_pixbuf = NULL;
	}

	(* G_OBJECT_CLASS (e_task_widget_parent_class)->dispose) (object);
}

static void
impl_finalize (GObject *object)
{
	ETaskWidget *task_widget;
	ETaskWidgetPrivate *priv;

	task_widget = E_TASK_WIDGET (object);
	priv = task_widget->priv;

	g_free (priv->component_id);
	g_free (priv);

	(* G_OBJECT_CLASS (e_task_widget_parent_class)->finalize) (object);
}


static void
e_task_widget_class_init (ETaskWidgetClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose  = impl_dispose;
	object_class->finalize = impl_finalize;
}

static void
e_task_widget_init (ETaskWidget *task_widget)
{
	ETaskWidgetPrivate *priv;

	priv = g_new (ETaskWidgetPrivate, 1);

	priv->component_id = NULL;
	priv->tooltips     = NULL;
	priv->icon_pixbuf  = NULL;
	priv->label        = NULL;
	priv->image        = NULL;
	priv->box	   = NULL;

	task_widget->priv = priv;
	task_widget->id = 0;
}

static gboolean
button_press_event_cb (GtkWidget *w, gpointer data)
{
	ETaskWidget *tw = (ETaskWidget *) data;
	ETaskWidgetPrivate *priv = tw->priv;

	priv->cancel_func (priv->data);

	return TRUE;
}

static gboolean
prepare_popup (ETaskWidget *widget, GdkEventButton *event)
{
	if (event->type != GDK_BUTTON_PRESS)
		return FALSE;

	if (event->button != 3)
		return FALSE;
	
	/* FIXME: Implement Cancel */

	return TRUE;
}


void
e_task_widget_construct (ETaskWidget *task_widget,
			 GdkPixbuf *icon_pixbuf,
			 const char *component_id,
			 const char *information,
			 void (*cancel_func) (gpointer data),
			 gpointer data)
{
	ETaskWidgetPrivate *priv;
	/*GdkPixmap *pixmap;
	GdkBitmap *mask;*/
	GtkWidget *box;
	GtkWidget *frame;

	g_return_if_fail (task_widget != NULL);
	g_return_if_fail (E_IS_TASK_WIDGET (task_widget));
	g_return_if_fail (component_id != NULL);
	g_return_if_fail (information != NULL);

	priv = task_widget->priv;

	priv->component_id = g_strdup (component_id);

	frame = gtk_frame_new (NULL);
	gtk_frame_set_shadow_type (GTK_FRAME (frame), GTK_SHADOW_IN);
	gtk_container_add (GTK_CONTAINER (task_widget), frame);
	gtk_widget_show (frame);

	box = gtk_hbox_new (FALSE, 0);
	gtk_container_add (GTK_CONTAINER (frame), box);
	gtk_widget_show (box);

	gtk_widget_set_size_request (box, 1, -1);

	/* FIXME: Experimenting Spinner widget instead of an image. REWORK THIS */
	/* priv->icon_pixbuf = g_object_ref (icon_pixbuf); */

	/* gdk_pixbuf_render_pixmap_and_mask (icon_pixbuf, &pixmap, &mask, 128); */
	priv->box = gtk_hbox_new (FALSE, 0);
	priv->image = e_spinner_new ();
	e_spinner_set_size (E_SPINNER (priv->image), GTK_ICON_SIZE_SMALL_TOOLBAR);
	e_spinner_start (E_SPINNER (priv->image));
	/* gtk_image_new_from_pixmap (pixmap, mask); */
	gtk_widget_show (priv->image);
	gtk_widget_show (priv->box);
	gtk_box_pack_start (GTK_BOX (priv->box), priv->image, FALSE, TRUE, 0);
	gtk_box_pack_start (GTK_BOX (box), priv->box, FALSE, TRUE, 0);
	priv->label = gtk_label_new ("");
	gtk_misc_set_alignment (GTK_MISC (priv->label), 0.0, 0.5);
	gtk_widget_show (priv->label);
	gtk_box_pack_start (GTK_BOX (box), priv->label, TRUE, TRUE, 0);
	if (cancel_func) {
		GtkWidget *image = e_icon_factory_get_image ("gtk-stop", E_ICON_SIZE_MENU);
		GtkWidget *tool;

		tool = (GtkWidget *) gtk_tool_button_new (image, NULL);
		gtk_box_pack_end (GTK_BOX (box), tool, FALSE, TRUE, 0);
		gtk_widget_show_all (tool);

		gtk_widget_set_sensitive (tool, cancel_func != NULL);
		priv->cancel_func = cancel_func;
		priv->data = data;
		g_signal_connect (tool, "clicked",  G_CALLBACK (button_press_event_cb), task_widget);
		/* g_object_unref (pixmap);
		if (mask)
			g_object_unref (mask); */
 		g_signal_connect (task_widget, "button-press-event", G_CALLBACK (prepare_popup), task_widget);

	}

	priv->tooltips = gtk_tooltips_new ();
	g_object_ref_sink (priv->tooltips);

	e_task_widget_update (task_widget, information, -1.0);
}

GtkWidget *
e_task_widget_new_with_cancel (GdkPixbuf *icon_pixbuf,
		   const char *component_id,
		   const char *information,
		   void (*cancel_func) (gpointer data),
		   gpointer data)
{
	ETaskWidget *task_widget;

	g_return_val_if_fail (information != NULL, NULL);

	task_widget = g_object_new (e_task_widget_get_type (), NULL);
	e_task_widget_construct (task_widget, icon_pixbuf, component_id, information, cancel_func, data);

	return GTK_WIDGET (task_widget);
}

GtkWidget *
e_task_widget_new (GdkPixbuf *icon_pixbuf,
		   const char *component_id,
		   const char *information)
{
	ETaskWidget *task_widget;

	g_return_val_if_fail (icon_pixbuf != NULL, NULL);
	g_return_val_if_fail (information != NULL, NULL);

	task_widget = g_object_new (e_task_widget_get_type (), NULL);
	e_task_widget_construct (task_widget, icon_pixbuf, component_id, information, NULL, NULL);

	return GTK_WIDGET (task_widget);
}

GtkWidget *
e_task_widget_update_image (ETaskWidget *task_widget,
			    const char *stock, const char *text)
{
	GtkWidget *img, *tool;

	img = e_icon_factory_get_image (stock, E_ICON_SIZE_MENU);
	tool = (GtkWidget *) gtk_tool_button_new (img, NULL);
	gtk_box_pack_start (GTK_BOX(task_widget->priv->box), tool, FALSE, TRUE, 0);
	gtk_widget_show_all (task_widget->priv->box);
	gtk_widget_hide (task_widget->priv->image);
	task_widget->priv->image = img;
	gtk_label_set_text (GTK_LABEL (task_widget->priv->label), text);

	return tool;
}


void
e_task_widget_update (ETaskWidget *task_widget,
		      const char *information,
		      double completion)
{
	ETaskWidgetPrivate *priv;
	char *text;

	g_return_if_fail (task_widget != NULL);
	g_return_if_fail (E_IS_TASK_WIDGET (task_widget));
	g_return_if_fail (information != NULL);

	priv = task_widget->priv;

	if (completion < 0.0) {
		/* For Translator only: %s is status message that is displayed (eg "moving items", "updating objects") */
		text = g_strdup_printf (_("%s (...)"), information);
	} else {
		int percent_complete;
		percent_complete = (int) (completion * 100.0 + .5);
		/* For Translator only: %s is status message that is displayed (eg "moving items", "updating objects");
		   %d is a number between 0 and 100, describing the percentage of operation complete */
		text = g_strdup_printf (_("%s (%d%% complete)"), information, percent_complete);
	}

	gtk_label_set_text (GTK_LABEL (priv->label), text);

	gtk_tooltips_set_tip (priv->tooltips, GTK_WIDGET (task_widget), text, NULL);

	g_free (text);
}

void
e_task_wiget_alert (ETaskWidget *task_widget)
{
	g_return_if_fail (task_widget != NULL);
	g_return_if_fail (E_IS_TASK_WIDGET (task_widget));
}

void
e_task_wiget_unalert (ETaskWidget *task_widget)
{
	g_return_if_fail (task_widget != NULL);
	g_return_if_fail (E_IS_TASK_WIDGET (task_widget));
}


const char *
e_task_widget_get_component_id  (ETaskWidget *task_widget)
{
	g_return_val_if_fail (task_widget != NULL, NULL);
	g_return_val_if_fail (E_IS_TASK_WIDGET (task_widget), NULL);

	return task_widget->priv->component_id;
}

