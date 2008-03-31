/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */
/* e-combo-button.c
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

#include "e-combo-button.h"
#include "ea-widgets.h"
#include <e-util/e-icon-factory.h>

#include <gtk/gtkarrow.h>
#include <gtk/gtkhbox.h>
#include <gtk/gtkvbox.h>
#include <gtk/gtklabel.h>
#include <gtk/gtkmain.h>
#include <gtk/gtkimage.h>
#include <gtk/gtksignal.h>

struct _EComboButtonPrivate {
	GdkPixbuf *icon;

	GtkWidget *icon_image;
	GtkWidget *label;
	GtkWidget *arrow_image;
	GtkWidget *hbox;
	GtkWidget *vbox;

	GtkMenu *menu;

	gboolean menu_popped_up;
	gboolean is_already_packed;
};

#define SPACING 2

enum {
	ACTIVATE_DEFAULT,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (EComboButton, e_combo_button, GTK_TYPE_BUTTON)

/* Utility functions.  */

static void
set_icon (EComboButton *combo_button,
	  GdkPixbuf *pixbuf)
{
	EComboButtonPrivate *priv;

	priv = combo_button->priv;

	if (priv->icon != NULL)
		g_object_unref (priv->icon);

	if (pixbuf == NULL) {
		priv->icon = NULL;
		gtk_widget_hide (priv->icon_image);
		return;
	}

	priv->icon = g_object_ref (pixbuf);

	gtk_image_set_from_pixbuf (GTK_IMAGE (priv->icon_image), priv->icon);

	gtk_widget_show (priv->icon_image);
}


/* Paint the borders.  */

static void
paint (EComboButton *combo_button,
       GdkRectangle *area)
{
	EComboButtonPrivate *priv = combo_button->priv;
	GtkWidget *widget = GTK_WIDGET (combo_button);
	GtkButton *button = GTK_BUTTON (combo_button);
	GtkShadowType shadow_type;
	gboolean interior_focus;
	int separator_x;
	int focus_width, focus_pad;
	int x, y, width, height;
	int border_width;

	if (GTK_BUTTON (widget)->depressed || priv->menu_popped_up) {
		shadow_type = GTK_SHADOW_IN;
		gtk_widget_set_state (widget, GTK_STATE_ACTIVE);
	} else if (GTK_BUTTON (widget)->relief == GTK_RELIEF_NONE &&
		   (GTK_WIDGET_STATE (widget) == GTK_STATE_NORMAL ||
		    GTK_WIDGET_STATE (widget) == GTK_STATE_INSENSITIVE))
		shadow_type = GTK_SHADOW_NONE;
	else
		shadow_type = GTK_SHADOW_OUT;

	border_width = GTK_CONTAINER (widget)->border_width;

	x = widget->allocation.x + border_width;
	y = widget->allocation.y + border_width;
	width = widget->allocation.width - border_width * 2;
	height = widget->allocation.height - border_width * 2;

	separator_x = (priv->label->allocation.width
		       + priv->label->allocation.x
		       + priv->arrow_image->allocation.x) / 2;

	gtk_widget_style_get (GTK_WIDGET (widget),
			      "focus-line-width", &focus_width,
			      "focus-padding", &focus_pad,
			      "interior-focus", &interior_focus,
			      NULL);

	if (GTK_WIDGET_HAS_DEFAULT (widget)
	    && GTK_BUTTON (widget)->relief == GTK_RELIEF_NORMAL)
		gtk_paint_box (widget->style, widget->window,
			       GTK_STATE_NORMAL, GTK_SHADOW_IN,
			       area, widget, "buttondefault",
			       x, y, width, height);

	if (!interior_focus && GTK_WIDGET_HAS_FOCUS (widget)) {
		x += focus_width + focus_pad;
		y += focus_width + focus_pad;
		width -= 2 * (focus_width + focus_pad);
		height -= 2 * (focus_width + focus_pad);
	}

	if (button->relief != GTK_RELIEF_NONE || button->depressed ||
	    priv->menu_popped_up ||
	    GTK_WIDGET_STATE (widget) == GTK_STATE_PRELIGHT) {

		gtk_paint_box (widget->style, widget->window,
			       GTK_WIDGET_STATE (widget), shadow_type,
			       area, widget, "button",
			       x, y, separator_x, height);

		if (width - separator_x > 0)
			gtk_paint_box (widget->style, widget->window,
				       GTK_WIDGET_STATE (widget), shadow_type,
				       area, widget, "button",
				       separator_x, y, width - separator_x, height);
	}

	if (GTK_WIDGET_HAS_FOCUS (widget)) {
		if (interior_focus) {
			x += widget->style->xthickness + focus_pad;
			y += widget->style->ythickness + focus_pad;
			width -= 2 * (widget->style->xthickness + focus_pad);
			height -=  2 * (widget->style->xthickness + focus_pad);
		} else {
			x -= focus_width + focus_pad;
			y -= focus_width + focus_pad;
			width += 2 * (focus_width + focus_pad);
			height += 2 * (focus_width + focus_pad);
		}

		gtk_paint_focus (widget->style, widget->window,
				 GTK_WIDGET_STATE (widget),
				 area, widget, "button",
				 x, y, width, height);
	}
}


/* Callbacks for the associated menu.  */

static void
menu_detacher (GtkWidget *widget,
	       GtkMenu *menu)
{
	EComboButton *combo_button;
	EComboButtonPrivate *priv;

	combo_button = E_COMBO_BUTTON (widget);
	priv = combo_button->priv;
	g_signal_handlers_disconnect_matched (menu,
					      G_SIGNAL_MATCH_DATA,
					      0, 0, NULL, NULL,
					      combo_button);
	priv->menu = NULL;
}

static void
menu_deactivate_callback (GtkMenuShell *menu_shell,
			  void *data)
{
	EComboButton *combo_button;
	EComboButtonPrivate *priv;

	combo_button = E_COMBO_BUTTON (data);
	priv = combo_button->priv;

	priv->menu_popped_up = FALSE;

	GTK_BUTTON (combo_button)->button_down = FALSE;
	GTK_BUTTON (combo_button)->in_button = FALSE;
	gtk_button_leave (GTK_BUTTON (combo_button));
	gtk_button_clicked (GTK_BUTTON (combo_button));
}

static void
menu_position_func (GtkMenu *menu,
		    gint *x_return,
		    gint *y_return,
		    gboolean *push_in,
		    void *data)
{
	EComboButton *combo_button;
	GtkAllocation *allocation;

	combo_button = E_COMBO_BUTTON (data);
	allocation = & (GTK_WIDGET (combo_button)->allocation);

	gdk_window_get_origin (GTK_WIDGET (combo_button)->window, x_return, y_return);

	*y_return += allocation->height;
}


/* GtkObject methods.  */

static void
impl_destroy (GtkObject *object)
{
	EComboButton *combo_button;
	EComboButtonPrivate *priv;

	combo_button = E_COMBO_BUTTON (object);
	priv = combo_button->priv;

	if (priv) {
		if (priv->arrow_image != NULL) {
			gtk_widget_destroy (priv->arrow_image);
			priv->arrow_image = NULL;
		}

		if (priv->icon != NULL) {
			g_object_unref (priv->icon);
			priv->icon = NULL;
		}

		g_free (priv);
		combo_button->priv = NULL;
	}

	(* GTK_OBJECT_CLASS (e_combo_button_parent_class)->destroy) (object);
}



static gboolean
e_combo_button_popup (EComboButton *combo_button, GdkEventButton *event)
{
	EComboButtonPrivate *priv;

	g_return_val_if_fail (combo_button != NULL, FALSE);
	g_return_val_if_fail (E_IS_COMBO_BUTTON (combo_button), FALSE);

	priv = combo_button->priv;

	priv->menu_popped_up = TRUE;

	if (event)
		gtk_menu_popup (GTK_MENU (priv->menu), NULL, NULL,
				menu_position_func, combo_button,
				event->button, event->time);
	else
		gtk_menu_popup (GTK_MENU (priv->menu), NULL, NULL,
				menu_position_func, combo_button,
				0, gtk_get_current_event_time());

	return TRUE;
}
/* GtkWidget methods.  */

static int
impl_button_press_event (GtkWidget *widget,
			 GdkEventButton *event)
{
	EComboButton *combo_button;
	EComboButtonPrivate *priv;

	combo_button = E_COMBO_BUTTON (widget);
	priv = combo_button->priv;

	if (event->type == GDK_BUTTON_PRESS &&
	    (event->button == 1 || event->button == 3)) {
		GTK_BUTTON (widget)->button_down = TRUE;

		if (event->button == 3 ||
		    event->x >= priv->arrow_image->allocation.x) {
			/* User clicked on the right side: pop up the menu.  */
			gtk_button_pressed (GTK_BUTTON (widget));

			e_combo_button_popup (combo_button, event);
		} else {
			/* User clicked on the left side: just behave like a
			   normal button (i.e. not a toggle).  */
			gtk_button_pressed (GTK_BUTTON (widget));
		}
	}

	return TRUE;
}

static int
impl_leave_notify_event (GtkWidget *widget,
			 GdkEventCrossing *event)
{
	EComboButton *combo_button;
	EComboButtonPrivate *priv;

	combo_button = E_COMBO_BUTTON (widget);
	priv = combo_button->priv;

	/* This is to override the standard behavior of deactivating the button
	   when the pointer gets out of the widget, in the case in which we
	   have just popped up the menu.  Otherwise, the button would look as
	   inactive when the menu is popped up.  */
	if (! priv->menu_popped_up)
		return (* GTK_WIDGET_CLASS (e_combo_button_parent_class)->leave_notify_event) (widget, event);

	return FALSE;
}

static int
impl_expose_event (GtkWidget *widget,
		   GdkEventExpose *event)
{
	GtkBin *bin;
	GdkEventExpose child_event;

	if (! GTK_WIDGET_DRAWABLE (widget))
		return FALSE;

	bin = GTK_BIN (widget);

	paint (E_COMBO_BUTTON (widget), &event->area);

	child_event = *event;
	if (bin->child && GTK_WIDGET_NO_WINDOW (bin->child) &&
	    gtk_widget_intersect (bin->child, &event->area, &child_event.area))
		gtk_container_propagate_expose (GTK_CONTAINER (widget), bin->child, &child_event);

	return FALSE;
}


/* GtkButton methods.  */

static void
impl_released (GtkButton *button)
{
	EComboButton *combo_button;
	EComboButtonPrivate *priv;

	combo_button = E_COMBO_BUTTON (button);
	priv = combo_button->priv;

	/* Massive cut & paste from GtkButton here...  The only change in
	   behavior here is that we want to emit ::activate_default when not
	   the menu hasn't been popped up.  */

	if (button->button_down) {
		int new_state;

		button->button_down = FALSE;

		if (button->in_button) {
			gtk_button_clicked (button);

			if (! priv->menu_popped_up)
				gtk_signal_emit (GTK_OBJECT (button), signals[ACTIVATE_DEFAULT]);
		}

		new_state = (button->in_button ? GTK_STATE_PRELIGHT : GTK_STATE_NORMAL);

		if (GTK_WIDGET_STATE (button) != new_state) {
			gtk_widget_set_state (GTK_WIDGET (button), new_state);

			/* We _draw () instead of queue_draw so that if the
			   operation blocks, the label doesn't vanish.  */
			/* XXX gtk_widget_draw() is deprecated.
			 *     Replace it with GTK's implementation. */
			gtk_widget_queue_draw (GTK_WIDGET (button));
			gdk_window_process_updates (
				GTK_WIDGET (button)->window, TRUE);
		}
	}
}


static void
e_combo_button_class_init (EComboButtonClass *combo_button_class)
{
	GtkObjectClass *object_class;
	GtkWidgetClass *widget_class;
	GtkButtonClass *button_class;

	object_class = GTK_OBJECT_CLASS (combo_button_class);
	object_class->destroy = impl_destroy;

	widget_class = GTK_WIDGET_CLASS (object_class);
	widget_class->button_press_event = impl_button_press_event;
	widget_class->leave_notify_event = impl_leave_notify_event;
	widget_class->expose_event       = impl_expose_event;

	button_class = GTK_BUTTON_CLASS (object_class);
	button_class->released = impl_released;

	signals[ACTIVATE_DEFAULT] = gtk_signal_new ("activate_default",
						    GTK_RUN_FIRST,
						    GTK_CLASS_TYPE (object_class),
						    G_STRUCT_OFFSET (EComboButtonClass, activate_default),
						    gtk_marshal_NONE__NONE,
						    GTK_TYPE_NONE, 0);

	e_combo_button_a11y_init ();
}

static void
e_combo_button_init (EComboButton *combo_button)
{
	EComboButtonPrivate *priv;

	priv = g_new (EComboButtonPrivate, 1);
	combo_button->priv = priv;

	priv->icon              = NULL;
	priv->menu              = NULL;
	priv->menu_popped_up    = FALSE;
	priv->is_already_packed = FALSE;
}

void
e_combo_button_pack_hbox (EComboButton *combo_button)
{
	EComboButtonPrivate *priv;

	priv = combo_button->priv;

	if(priv->is_already_packed){
		gtk_widget_destroy (priv->hbox);
	}

	priv->hbox = gtk_hbox_new (FALSE, SPACING);
	gtk_container_add (GTK_CONTAINER (combo_button), priv->hbox);
	gtk_widget_show (priv->hbox);

	priv->icon_image = e_icon_factory_get_image (NULL, E_ICON_SIZE_MENU);
	gtk_box_pack_start (GTK_BOX (priv->hbox), priv->icon_image, TRUE, TRUE, 0);
	gtk_widget_show (priv->icon_image);

	priv->label = gtk_label_new ("");
	gtk_box_pack_start (GTK_BOX (priv->hbox), priv->label, TRUE, TRUE,
			    0);
	gtk_widget_show (priv->label);

	priv->arrow_image = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_NONE);
	gtk_box_pack_end (GTK_BOX (priv->hbox), priv->arrow_image, TRUE, TRUE,
			  GTK_WIDGET (combo_button)->style->xthickness);
	gtk_widget_show (priv->arrow_image);

	gtk_widget_show (priv->hbox);

	priv->is_already_packed = TRUE;
}

void
e_combo_button_pack_vbox (EComboButton *combo_button)
{
	EComboButtonPrivate *priv;

	priv = combo_button->priv;

	if(priv->is_already_packed){
		gtk_widget_destroy (priv->hbox);
	}

	priv->hbox = gtk_hbox_new (FALSE, SPACING);
	gtk_container_add (GTK_CONTAINER (combo_button), priv->hbox);
	gtk_widget_show (priv->hbox);

   	priv->vbox = gtk_vbox_new (FALSE, 0);
	gtk_widget_show (priv->vbox);

	priv->icon_image = e_icon_factory_get_image (NULL, E_ICON_SIZE_MENU);
	gtk_box_pack_start (GTK_BOX (priv->vbox), priv->icon_image, TRUE, TRUE, 0);
	gtk_widget_show (priv->icon_image);

	priv->label = gtk_label_new ("");
	gtk_box_pack_start (GTK_BOX (priv->vbox), priv->label, TRUE, TRUE,
			    0);
	gtk_widget_show (priv->label);

	gtk_box_pack_start (GTK_BOX(priv->hbox),priv->vbox, TRUE, TRUE, 0);

	priv->arrow_image = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_NONE);
	gtk_box_pack_end (GTK_BOX (priv->hbox), priv->arrow_image, TRUE, TRUE,
			  GTK_WIDGET (combo_button)->style->xthickness);
	gtk_widget_show (priv->arrow_image);

	gtk_widget_show (priv->hbox);

	priv->is_already_packed = TRUE;
}


void
e_combo_button_construct (EComboButton *combo_button)
{
	EComboButtonPrivate *priv;

	g_return_if_fail (combo_button != NULL);
	g_return_if_fail (E_IS_COMBO_BUTTON (combo_button));

	priv = combo_button->priv;
	g_return_if_fail (priv->menu == NULL);

	GTK_WIDGET_UNSET_FLAGS (combo_button, GTK_CAN_FOCUS);

	gtk_button_set_relief (GTK_BUTTON (combo_button), GTK_RELIEF_NONE);
}

GtkWidget *
e_combo_button_new (void)
{
	EComboButton *new;

	new = g_object_new (e_combo_button_get_type (), NULL);
	e_combo_button_construct (new);

	return GTK_WIDGET (new);
}


void
e_combo_button_set_icon (EComboButton *combo_button,
			 GdkPixbuf *pixbuf)
{
	g_return_if_fail (combo_button != NULL);
	g_return_if_fail (E_IS_COMBO_BUTTON (combo_button));

	set_icon (combo_button, pixbuf);
}

void
e_combo_button_set_label (EComboButton *combo_button,
			  const char *label)
{
	EComboButtonPrivate *priv;

	g_return_if_fail (combo_button != NULL);
	g_return_if_fail (E_IS_COMBO_BUTTON (combo_button));

	priv = combo_button->priv;

	if (label == NULL)
		label = "";

	gtk_label_parse_uline (GTK_LABEL (priv->label), label);
}

void
e_combo_button_set_menu (EComboButton *combo_button,
			 GtkMenu *menu)
{
	EComboButtonPrivate *priv;

	g_return_if_fail (combo_button != NULL);
	g_return_if_fail (E_IS_COMBO_BUTTON (combo_button));
	g_return_if_fail (menu != NULL);
	g_return_if_fail (GTK_IS_MENU (menu));

	priv = combo_button->priv;

	if (priv->menu != NULL)
		gtk_menu_detach (priv->menu);

	priv->menu = menu;
	if (menu == NULL)
		return;

	gtk_menu_attach_to_widget (menu, GTK_WIDGET (combo_button), menu_detacher);

	g_signal_connect((menu), "deactivate",
			    G_CALLBACK (menu_deactivate_callback),
			    combo_button);
}

GtkWidget *
e_combo_button_get_label (EComboButton *combo_button)
{
	EComboButtonPrivate *priv;

	g_return_val_if_fail (combo_button != NULL, NULL);
	g_return_val_if_fail (E_IS_COMBO_BUTTON (combo_button), NULL);

	priv = combo_button->priv;

	return priv->label;
}

gboolean
e_combo_button_popup_menu (EComboButton *combo_button)
{
	return e_combo_button_popup (combo_button, NULL);
}
