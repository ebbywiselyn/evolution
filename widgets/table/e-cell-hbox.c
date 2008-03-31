/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * e-cell-hbox.c - Hbox cell object.
 * Copyright 2006 Novell, Inc.
 *
 * Authors:
 *   Srinivasa Ragavan <sragavan@novell.com>
 *
 * A majority of code taken from:
 *
 * the ECellText renderer.
 * Copyright 1999, 2000, Ximian, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License, version 2, as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include <config.h>

#include <ctype.h>
#include <math.h>
#include <stdio.h>

#include <gtk/gtk.h>

/* #include "a11y/e-table/gal-a11y-e-cell-registry.h" */
/* #include "a11y/e-table/gal-a11y-e-cell-vbox.h" */
#include "e-util/e-util.h"

#include "e-cell-hbox.h"
#include "e-table-item.h"

G_DEFINE_TYPE (ECellHbox, e_cell_hbox, E_CELL_TYPE)

#define INDENT_AMOUNT 16
#define MAX_CELL_SIZE 25

/*
 * ECell::new_view method
 */
static ECellView *
ecv_new_view (ECell *ecell, ETableModel *table_model, void *e_table_item_view)
{
	ECellHbox *ecv = E_CELL_HBOX (ecell);
	ECellHboxView *hbox_view = g_new0 (ECellHboxView, 1);
	int i;

	hbox_view->cell_view.ecell = ecell;
	hbox_view->cell_view.e_table_model = table_model;
	hbox_view->cell_view.e_table_item_view = e_table_item_view;
        hbox_view->cell_view.kill_view_cb = NULL;
        hbox_view->cell_view.kill_view_cb_data = NULL;

	/* create our subcell view */
	hbox_view->subcell_view_count = ecv->subcell_count;
	hbox_view->subcell_views = g_new (ECellView *, hbox_view->subcell_view_count);
	hbox_view->model_cols = g_new (int, hbox_view->subcell_view_count);
	hbox_view->def_size_cols = g_new (int, hbox_view->subcell_view_count);

	for (i = 0; i < hbox_view->subcell_view_count; i++) {
		hbox_view->subcell_views[i] = e_cell_new_view (ecv->subcells[i], table_model, e_table_item_view /* XXX */);
		hbox_view->model_cols[i] = ecv->model_cols[i];
		hbox_view->def_size_cols[i] = ecv->def_size_cols[i];
	}

	return (ECellView *)hbox_view;
}

/*
 * ECell::kill_view method
 */
static void
ecv_kill_view (ECellView *ecv)
{
	ECellHboxView *hbox_view = (ECellHboxView *) ecv;
	int i;

        if (hbox_view->cell_view.kill_view_cb)
            (hbox_view->cell_view.kill_view_cb)(ecv, hbox_view->cell_view.kill_view_cb_data);

        if (hbox_view->cell_view.kill_view_cb_data)
            g_list_free(hbox_view->cell_view.kill_view_cb_data);

	/* kill our subcell view */
	for (i = 0; i < hbox_view->subcell_view_count; i++)
		e_cell_kill_view (hbox_view->subcell_views[i]);

	g_free (hbox_view->model_cols);
	g_free (hbox_view->def_size_cols);
	g_free (hbox_view->subcell_views);
	g_free (hbox_view);
}

/*
 * ECell::realize method
 */
static void
ecv_realize (ECellView *ecell_view)
{
	ECellHboxView *hbox_view = (ECellHboxView *) ecell_view;
	int i;

	/* realize our subcell view */
	for (i = 0; i < hbox_view->subcell_view_count; i++)
		e_cell_realize (hbox_view->subcell_views[i]);

	if (E_CELL_CLASS (e_cell_hbox_parent_class)->realize)
		(* E_CELL_CLASS (e_cell_hbox_parent_class)->realize) (ecell_view);
}

/*
 * ECell::unrealize method
 */
static void
ecv_unrealize (ECellView *ecv)
{
	ECellHboxView *hbox_view = (ECellHboxView *) ecv;
	int i;

	/* unrealize our subcell view. */
	for (i = 0; i < hbox_view->subcell_view_count; i++)
		e_cell_unrealize (hbox_view->subcell_views[i]);

	if (E_CELL_CLASS (e_cell_hbox_parent_class)->unrealize)
		(* E_CELL_CLASS (e_cell_hbox_parent_class)->unrealize) (ecv);
}

/*
 * ECell::draw method
 */
static void
ecv_draw (ECellView *ecell_view, GdkDrawable *drawable,
	  int model_col, int view_col, int row, ECellFlags flags,
	  int x1, int y1, int x2, int y2)
{
	ECellHboxView *hbox_view = (ECellHboxView *)ecell_view;

	int subcell_offset = 0;
	int i;
	int allotted_width = x2-x1;

	for (i = 0; i < hbox_view->subcell_view_count; i++) {
		/* Now cause our subcells to draw their contents,
		   shifted by subcell_offset pixels */
		int width = allotted_width * hbox_view->def_size_cols[i] / 100;
			//e_cell_max_width_by_row (hbox_view->subcell_views[i], hbox_view->model_cols[i], view_col, row);
//		if (width < hbox_view->def_size_cols[i])
	//		width = hbox_view->def_size_cols[i];
//		printf("width of %d %d of %d\n", width,hbox_view->def_size_cols[i], allotted_width );
		e_cell_draw (hbox_view->subcell_views[i], drawable,
			     hbox_view->model_cols[i], view_col, row, flags,
			     x1 + subcell_offset , y1, x1 + subcell_offset + width, y2);

		subcell_offset += width; //e_cell_max_width_by_row (hbox_view->subcell_views[i], hbox_view->model_cols[i], view_col, row);
	}
}

/*
 * ECell::event method
 */
static gint
ecv_event (ECellView *ecell_view, GdkEvent *event, int model_col, int view_col, int row, ECellFlags flags, ECellActions *actions)
{
	ECellHboxView *hbox_view = (ECellHboxView *)ecell_view;
	int y = 0;
	int i;
	int subcell_offset = 0;

	switch (event->type) {
	case GDK_BUTTON_PRESS:
	case GDK_BUTTON_RELEASE:
	case GDK_2BUTTON_PRESS:
	case GDK_3BUTTON_PRESS:
		y = event->button.y;
		break;
	case GDK_MOTION_NOTIFY:
		y = event->motion.y;
		break;
	default:
		/* nada */
		break;
	}

	for (i = 0; i < hbox_view->subcell_view_count; i++) {
		int width = e_cell_max_width_by_row (hbox_view->subcell_views[i], hbox_view->model_cols[i], view_col, row);
		if (width < hbox_view->def_size_cols[i])
			width = hbox_view->def_size_cols[i];
		if (y < subcell_offset + width)
			return e_cell_event(hbox_view->subcell_views[i], event, hbox_view->model_cols[i], view_col, row, flags, actions);
		subcell_offset += width;
	}
	return 0;
}

/*
 * ECell::height method
 */
static int
ecv_height (ECellView *ecell_view, int model_col, int view_col, int row)
{
	ECellHboxView *hbox_view = (ECellHboxView *)ecell_view;
	int height = 0, max_height = 0;
	int i;

	for (i = 0; i < hbox_view->subcell_view_count; i++) {
		height = e_cell_height (hbox_view->subcell_views[i], hbox_view->model_cols[i], view_col, row);
		max_height = MAX(max_height, height);
	}
	return max_height;
}

/*
 * ECell::max_width method
 */
static int
ecv_max_width (ECellView *ecell_view, int model_col, int view_col)
{
	ECellHboxView *hbox_view = (ECellHboxView *)ecell_view;
	int width = 0;
	int i;

	for (i = 0; i < hbox_view->subcell_view_count; i++) {
		int cell_width = e_cell_max_width (hbox_view->subcell_views[i], hbox_view->model_cols[i], view_col);

		if (cell_width < hbox_view->def_size_cols[i])
			cell_width = hbox_view->def_size_cols[i];
		width += cell_width;
	}

	return width;
}

/*
 * GObject::dispose method
 */
static void
ecv_dispose (GObject *object)
{
	ECellHbox *ecv = E_CELL_HBOX (object);
	int i;

	/* destroy our subcell */
	for (i = 0; i < ecv->subcell_count; i++)
		if (ecv->subcells[i])
			g_object_unref (ecv->subcells[i]);
	g_free (ecv->subcells);
	ecv->subcells = NULL;
	ecv->subcell_count = 0;

	G_OBJECT_CLASS (e_cell_hbox_parent_class)->dispose (object);
}

static void
e_cell_hbox_class_init (ECellHboxClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	ECellClass *ecc = E_CELL_CLASS (klass);

	object_class->dispose = ecv_dispose;

	ecc->new_view         = ecv_new_view;
	ecc->kill_view        = ecv_kill_view;
	ecc->realize          = ecv_realize;
	ecc->unrealize        = ecv_unrealize;
	ecc->draw             = ecv_draw;
	ecc->event            = ecv_event;
	ecc->height           = ecv_height;

	ecc->max_width        = ecv_max_width;

/* 	gal_a11y_e_cell_registry_add_cell_type (NULL, E_CELL_HBOX_TYPE, gal_a11y_e_cell_hbox_new); */
}

static void
e_cell_hbox_init (ECellHbox *ecv)
{
	ecv->subcells = NULL;
	ecv->subcell_count = 0;
}

/**
 * e_cell_hbox_new:
 *
 * Creates a new ECell renderer that can be used to render multiple
 * child cells.
 *
 * Return value: an ECell object that can be used to render multiple
 * child cells.
 **/
ECell *
e_cell_hbox_new (void)
{
	ECellHbox *ecv = g_object_new (E_CELL_HBOX_TYPE, NULL);

	return (ECell *) ecv;
}

void
e_cell_hbox_append (ECellHbox *hbox, ECell *subcell, int model_col, int size)
{
	hbox->subcell_count ++;

	hbox->subcells   = g_renew (ECell *, hbox->subcells,   hbox->subcell_count);
	hbox->model_cols = g_renew (int,     hbox->model_cols, hbox->subcell_count);
	hbox->def_size_cols = g_renew (int,     hbox->def_size_cols, hbox->subcell_count);

	hbox->subcells[hbox->subcell_count - 1]   = subcell;
	hbox->model_cols[hbox->subcell_count - 1] = model_col;
	hbox->def_size_cols[hbox->subcell_count - 1] = size;

	if (subcell)
		g_object_ref (subcell);
}
